// ZEHRA KINIK - Android platform katmani (NativeActivity): EGL/GLES3 baglami, AAudio, dokunmatik girdi,
// ekran yonu (JNI) ve geri tusu. Oyun/cizim mantigi platformdan bagimsiz App'tedir (masaustu ile ortak).
#include "app/App.h"
#include "app/FramePacer.h"

#include <jni.h>

#include <EGL/egl.h>
#include <aaudio/AAudio.h>
#include <android/log.h>
#include <android/sensor.h>
#include <android_native_app_glue.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <thread>

using namespace zk;

namespace {

struct Platform {
    android_app* app = nullptr;
    std::unique_ptr<App> game;       // kayit klasoru bilinince olusturulur (android_main)
    EGLDisplay dpy = EGL_NO_DISPLAY; EGLSurface surf = EGL_NO_SURFACE; EGLContext ctx = EGL_NO_CONTEXT;
    AAudioStream* stream = nullptr;
    // Ivmeolcer (egim direksiyonu)
    ASensorManager* sensorMgr = nullptr;
    const ASensor* accel = nullptr;
    ASensorEventQueue* sensorQ = nullptr;
    float tiltLp = 0.0f;
    bool running = false;
    int swapInterval = -99;          // uygulanan dikey esitleme (-99: henuz yok; yeni EGL yuzeyinde yeniden)
};

// Dikey esitleme: 0 kapali, 1 acik; uyarlamali GLES'te yok -> acik
void applySwapInterval(Platform& p) {
    const int want = p.game->settings.vsync == VSync::Off ? 0 : 1;
    if (want == p.swapInterval || p.dpy == EGL_NO_DISPLAY) return;
    eglSwapInterval(p.dpy, want);
    p.swapInterval = want;
}

aaudio_data_callback_result_t audioCb(AAudioStream*, void* user, void* data, int32_t frames) {
    static_cast<App*>(user)->renderAudio(static_cast<float*>(data), frames);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void startAudio(Platform& p) {
    if (p.stream) return;
    AAudioStreamBuilder* b = nullptr;
    if (AAudio_createStreamBuilder(&b) != AAUDIO_OK) return;
    AAudioStreamBuilder_setFormat(b, AAUDIO_FORMAT_PCM_FLOAT);
    AAudioStreamBuilder_setChannelCount(b, 1);
    AAudioStreamBuilder_setSampleRate(b, App::kSampleRate);
    AAudioStreamBuilder_setPerformanceMode(b, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setDataCallback(b, audioCb, p.game.get());
    if (AAudioStreamBuilder_openStream(b, &p.stream) == AAUDIO_OK) AAudioStream_requestStart(p.stream);
    else p.stream = nullptr;
    AAudioStreamBuilder_delete(b);
}
void stopAudio(Platform& p) {
    if (!p.stream) return;
    AAudioStream_requestStop(p.stream);
    AAudioStream_close(p.stream);
    p.stream = nullptr;
}

bool initEGL(Platform& p) {
    p.dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(p.dpy, nullptr, nullptr);
    const EGLint attr[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RED_SIZE, 8,
                           EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 16, EGL_NONE};
    EGLConfig cfg; EGLint n = 0;
    if (!eglChooseConfig(p.dpy, attr, &cfg, 1, &n) || n < 1) return false;
    p.surf = eglCreateWindowSurface(p.dpy, cfg, p.app->window, nullptr);
    const EGLint ctxAttr[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    p.ctx = eglCreateContext(p.dpy, cfg, EGL_NO_CONTEXT, ctxAttr);
    if (eglMakeCurrent(p.dpy, p.surf, p.surf, p.ctx) == EGL_FALSE) return false;
    EGLint w = 1, h = 1;
    eglQuerySurface(p.dpy, p.surf, EGL_WIDTH, &w);
    eglQuerySurface(p.dpy, p.surf, EGL_HEIGHT, &h);
    p.game->resize(w, h);
    return p.game->initGraphics();
}

void termEGL(Platform& p) {
    p.game->shutdownGraphics();
    if (p.dpy != EGL_NO_DISPLAY) {
        eglMakeCurrent(p.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (p.ctx != EGL_NO_CONTEXT) eglDestroyContext(p.dpy, p.ctx);
        if (p.surf != EGL_NO_SURFACE) eglDestroySurface(p.dpy, p.surf);
        eglTerminate(p.dpy);
    }
    p.dpy = EGL_NO_DISPLAY; p.surf = EGL_NO_SURFACE; p.ctx = EGL_NO_CONTEXT;
    p.swapInterval = -99;
}

// Ana thread icin JNI onbellegi (android_main thread'i bir kez baglanir)
struct Jni {
    JavaVM* vm = nullptr; JNIEnv* env = nullptr; jobject activity = nullptr;
    jmethodID setOrientation = nullptr;
    jobject vibrator = nullptr; jclass effectCls = nullptr; jmethodID createOneShot = nullptr, vibrate = nullptr;
};

void clearEx(JNIEnv* env) { if (env->ExceptionCheck()) env->ExceptionClear(); }

void jniInit(Jni& j, android_app* app) {
    j.vm = app->activity->vm;
    if (j.vm->AttachCurrentThread(&j.env, nullptr) != JNI_OK || !j.env) { j.env = nullptr; return; }
    JNIEnv* env = j.env;
    j.activity = app->activity->clazz;
    jclass actCls = env->GetObjectClass(j.activity);
    j.setOrientation = env->GetMethodID(actCls, "setRequestedOrientation", "(I)V");
    jmethodID getSys = env->GetMethodID(actCls, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    clearEx(env);
    if (getSys) {
        jstring name = env->NewStringUTF("vibrator");
        jobject vib = env->CallObjectMethod(j.activity, getSys, name);
        clearEx(env);
        env->DeleteLocalRef(name);
        jclass eff = env->FindClass("android/os/VibrationEffect");
        clearEx(env);
        if (vib && eff) {
            j.vibrator = env->NewGlobalRef(vib);
            j.effectCls = (jclass)env->NewGlobalRef(eff);
            j.createOneShot = env->GetStaticMethodID(j.effectCls, "createOneShot", "(JI)Landroid/os/VibrationEffect;");
            jclass vibCls = env->GetObjectClass(vib);
            j.vibrate = env->GetMethodID(vibCls, "vibrate", "(Landroid/os/VibrationEffect;)V");
            clearEx(env);
            env->DeleteLocalRef(vibCls);
        }
        if (vib) env->DeleteLocalRef(vib);
        if (eff) env->DeleteLocalRef(eff);
    }
    env->DeleteLocalRef(actCls);
}

void jniShutdown(Jni& j) {
    if (!j.env) return;
    if (j.vibrator) j.env->DeleteGlobalRef(j.vibrator);
    if (j.effectCls) j.env->DeleteGlobalRef(j.effectCls);
    j.vm->DetachCurrentThread();
    j.env = nullptr;
}

// Activity.setRequestedOrientation: 6 = SENSOR_LANDSCAPE, 7 = SENSOR_PORTRAIT
void requestOrientation(Jni& j, bool landscape) {
    if (!j.env || !j.setOrientation) return;
    j.env->CallVoidMethod(j.activity, j.setOrientation, landscape ? 6 : 7);
    clearEx(j.env);
}

// VibrationEffect.createOneShot(ms, genlik 1..255) -> Vibrator.vibrate
void vibrate(Jni& j, int ms, int amplitude) {
    if (!j.env || !j.vibrator || !j.createOneShot || !j.vibrate) return;
    jobject e = j.env->CallStaticObjectMethod(j.effectCls, j.createOneShot, (jlong)ms, (jint)std::clamp(amplitude, 1, 255));
    clearEx(j.env);
    if (!e) return;
    j.env->CallVoidMethod(j.vibrator, j.vibrate, e);
    clearEx(j.env);
    j.env->DeleteLocalRef(e);
}

int32_t onInput(android_app* app, AInputEvent* ev) {
    Platform& p = *static_cast<Platform*>(app->userData);
    if (AInputEvent_getType(ev) == AINPUT_EVENT_TYPE_KEY) {
        if (AKeyEvent_getKeyCode(ev) != AKEYCODE_BACK) return 0;
        if (AKeyEvent_getAction(ev) != AKEY_EVENT_ACTION_UP) return 1;
        return p.game->back() ? 1 : 0;                                // garajda: varsayilan (uygulamadan cik)
    }
    if (AInputEvent_getType(ev) != AINPUT_EVENT_TYPE_MOTION) return 0;
    const int32_t action = AMotionEvent_getAction(ev);
    const int32_t act = action & AMOTION_EVENT_ACTION_MASK;
    const size_t idx = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    switch (act) {
    case AMOTION_EVENT_ACTION_DOWN:
    case AMOTION_EVENT_ACTION_POINTER_DOWN:
        p.game->pointerDown(AMotionEvent_getPointerId(ev, idx), AMotionEvent_getX(ev, idx), AMotionEvent_getY(ev, idx));
        break;
    case AMOTION_EVENT_ACTION_MOVE:
        for (size_t i = 0; i < AMotionEvent_getPointerCount(ev); ++i)
            p.game->pointerMove(AMotionEvent_getPointerId(ev, i), AMotionEvent_getX(ev, i), AMotionEvent_getY(ev, i));
        break;
    case AMOTION_EVENT_ACTION_UP:
    case AMOTION_EVENT_ACTION_POINTER_UP:
        p.game->pointerUp(AMotionEvent_getPointerId(ev, idx));
        break;
    case AMOTION_EVENT_ACTION_CANCEL:
        for (size_t i = 0; i < AMotionEvent_getPointerCount(ev); ++i) p.game->pointerUp(AMotionEvent_getPointerId(ev, i));
        break;
    default: break;
    }
    return 1;
}

void onCmd(android_app* app, int32_t cmd) {
    Platform& p = *static_cast<Platform*>(app->userData);
    switch (cmd) {
    case APP_CMD_INIT_WINDOW:
        if (app->window && initEGL(p)) { p.running = true; startAudio(p); }
        break;
    case APP_CMD_TERM_WINDOW:
        p.running = false; stopAudio(p); termEGL(p);
        break;
    case APP_CMD_PAUSE: stopAudio(p); break;
    case APP_CMD_RESUME: if (p.dpy != EGL_NO_DISPLAY) startAudio(p); break;
    case APP_CMD_GAINED_FOCUS:   // sensor yalniz odaktayken acik (pil)
        if (p.accel && p.sensorQ) { ASensorEventQueue_enableSensor(p.sensorQ, p.accel); ASensorEventQueue_setEventRate(p.sensorQ, p.accel, 16667); }
        break;
    case APP_CMD_LOST_FOCUS:
        if (p.accel && p.sensorQ) ASensorEventQueue_disableSensor(p.sensorQ, p.accel);
        break;
    case APP_CMD_CONFIG_CHANGED:
    case APP_CMD_WINDOW_RESIZED:
        if (p.dpy != EGL_NO_DISPLAY) {
            EGLint w = 1, h = 1;
            eglQuerySurface(p.dpy, p.surf, EGL_WIDTH, &w);
            eglQuerySurface(p.dpy, p.surf, EGL_HEIGHT, &h);
            p.game->resize(w, h);
        }
        break;
    default: break;
    }
}

} // namespace

void android_main(android_app* app) {
    Platform p;
    p.app = app;
    p.game = std::make_unique<App>(app->activity->internalDataPath ? app->activity->internalDataPath : "");
    app->userData = &p;
    app->onAppCmd = onCmd;
    app->onInputEvent = onInput;
    Jni jni;
    jniInit(jni, app);
    p.game->onOrientation = [&jni](bool landscape) { requestOrientation(jni, landscape); };
    p.game->onHaptic = [&jni](int ms, int amp) { vibrate(jni, ms, amp); };
    requestOrientation(jni, p.game->landscape());
    p.sensorMgr = ASensorManager_getInstance();
    if (p.sensorMgr) {
        p.accel = ASensorManager_getDefaultSensor(p.sensorMgr, ASENSOR_TYPE_ACCELEROMETER);
        if (p.accel) p.sensorQ = ASensorManager_createEventQueue(p.sensorMgr, app->looper, LOOPER_ID_USER, nullptr, nullptr);
    }
    auto last = std::chrono::steady_clock::now();
    const auto t0 = last;
    FramePacer pacer;

    while (true) {
        int events = 0;
        android_poll_source* src = nullptr;
        int ident;
        while ((ident = ALooper_pollOnce(p.running ? 0 : -1, nullptr, &events, (void**)&src)) >= 0) {
            if (src) src->process(app, src);
            if (ident == LOOPER_ID_USER && p.sensorQ) {
                // Dikey tutus: telefon sola yatinca x ivmesi +; ~30 derece = tam direksiyon; alcak geciren suzgec
                ASensorEvent ev;
                while (ASensorEventQueue_getEvents(p.sensorQ, &ev, 1) > 0) {
                    const float t = std::clamp(ev.acceleration.x / (9.81f * 0.5f), -1.0f, 1.0f);
                    p.tiltLp += (t - p.tiltLp) * 0.25f;
                    p.game->setTilt(p.tiltLp);
                }
            }
            if (app->destroyRequested) {
                if (p.sensorQ) ASensorManager_destroyEventQueue(p.sensorMgr, p.sensorQ);
                stopAudio(p); termEGL(p); jniShutdown(jni); return;
            }
        }
        const auto now = std::chrono::steady_clock::now();
        const double dt = std::chrono::duration<double>(now - last).count();
        last = now;
        if (!p.running) continue;
        p.game->update(dt);
        p.game->render();
        applySwapInterval(p);
        eglSwapBuffers(p.dpy, p.surf);
        // FPS siniri (ayarlar): pil ve isinma icin
        const int64_t nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count();
        const int64_t waitNs = pacer.wait(nowNs, p.game->framePeriodNs());
        if (waitNs > 0) std::this_thread::sleep_for(std::chrono::nanoseconds(waitNs));
    }
}
