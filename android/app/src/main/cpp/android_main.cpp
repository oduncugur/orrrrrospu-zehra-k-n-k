// ZEHRA KINIK - Android platform katmani (NativeActivity): EGL/GLES3 baglami, AAudio, dokunmatik girdi,
// ekran yonu (JNI) ve geri tusu. Oyun/cizim mantigi platformdan bagimsiz App'tedir (masaustu ile ortak).
#include "app/App.h"

#include <jni.h>

#include <EGL/egl.h>
#include <aaudio/AAudio.h>
#include <android/log.h>
#include <android_native_app_glue.h>

#include <algorithm>
#include <chrono>

using namespace zk;

namespace {

struct Platform {
    android_app* app = nullptr;
    App game;
    EGLDisplay dpy = EGL_NO_DISPLAY; EGLSurface surf = EGL_NO_SURFACE; EGLContext ctx = EGL_NO_CONTEXT;
    AAudioStream* stream = nullptr;
    bool running = false;
};

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
    AAudioStreamBuilder_setDataCallback(b, audioCb, &p.game);
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
    p.game.resize(w, h);
    return p.game.initGraphics();
}

void termEGL(Platform& p) {
    p.game.shutdownGraphics();
    if (p.dpy != EGL_NO_DISPLAY) {
        eglMakeCurrent(p.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (p.ctx != EGL_NO_CONTEXT) eglDestroyContext(p.dpy, p.ctx);
        if (p.surf != EGL_NO_SURFACE) eglDestroySurface(p.dpy, p.surf);
        eglTerminate(p.dpy);
    }
    p.dpy = EGL_NO_DISPLAY; p.surf = EGL_NO_SURFACE; p.ctx = EGL_NO_CONTEXT;
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
        if (!p.game.landscape()) return 0;                          // garajda: varsayilan (uygulamadan cik)
        if (AKeyEvent_getAction(ev) == AKEY_EVENT_ACTION_UP) p.game.key(Key::Back, true);
        return 1;
    }
    if (AInputEvent_getType(ev) != AINPUT_EVENT_TYPE_MOTION) return 0;
    const int32_t action = AMotionEvent_getAction(ev);
    const int32_t act = action & AMOTION_EVENT_ACTION_MASK;
    const size_t idx = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    switch (act) {
    case AMOTION_EVENT_ACTION_DOWN:
    case AMOTION_EVENT_ACTION_POINTER_DOWN:
        p.game.pointerDown(AMotionEvent_getPointerId(ev, idx), AMotionEvent_getX(ev, idx), AMotionEvent_getY(ev, idx));
        break;
    case AMOTION_EVENT_ACTION_MOVE:
        for (size_t i = 0; i < AMotionEvent_getPointerCount(ev); ++i)
            p.game.pointerMove(AMotionEvent_getPointerId(ev, i), AMotionEvent_getX(ev, i), AMotionEvent_getY(ev, i));
        break;
    case AMOTION_EVENT_ACTION_UP:
    case AMOTION_EVENT_ACTION_POINTER_UP:
        p.game.pointerUp(AMotionEvent_getPointerId(ev, idx));
        break;
    case AMOTION_EVENT_ACTION_CANCEL:
        for (size_t i = 0; i < AMotionEvent_getPointerCount(ev); ++i) p.game.pointerUp(AMotionEvent_getPointerId(ev, i));
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
    case APP_CMD_CONFIG_CHANGED:
    case APP_CMD_WINDOW_RESIZED:
        if (p.dpy != EGL_NO_DISPLAY) {
            EGLint w = 1, h = 1;
            eglQuerySurface(p.dpy, p.surf, EGL_WIDTH, &w);
            eglQuerySurface(p.dpy, p.surf, EGL_HEIGHT, &h);
            p.game.resize(w, h);
        }
        break;
    default: break;
    }
}

} // namespace

void android_main(android_app* app) {
    Platform p;
    p.app = app;
    app->userData = &p;
    app->onAppCmd = onCmd;
    app->onInputEvent = onInput;
    Jni jni;
    jniInit(jni, app);
    p.game.onOrientation = [&jni](bool landscape) { requestOrientation(jni, landscape); };
    p.game.onHaptic = [&jni](int ms, int amp) { vibrate(jni, ms, amp); };
    requestOrientation(jni, p.game.landscape());
    auto last = std::chrono::steady_clock::now();

    while (true) {
        int events = 0;
        android_poll_source* src = nullptr;
        while (ALooper_pollOnce(p.running ? 0 : -1, nullptr, &events, (void**)&src) >= 0) {
            if (src) src->process(app, src);
            if (app->destroyRequested) { stopAudio(p); termEGL(p); jniShutdown(jni); return; }
        }
        const auto now = std::chrono::steady_clock::now();
        const double dt = std::chrono::duration<double>(now - last).count();
        last = now;
        if (!p.running) continue;
        p.game.update(dt);
        p.game.render();
        eglSwapBuffers(p.dpy, p.surf);
    }
}
