// ZEHRA KINIK - Android platform katmani (NativeActivity): EGL/GLES3 baglami, AAudio, dokunmatik girdi,
// ekran yonu (JNI) ve geri tusu. Oyun/cizim mantigi platformdan bagimsiz App'tedir (masaustu ile ortak).
#include <cmath>
#include <algorithm>
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
    int rotation = 0;                // ekran donusu (Surface.ROTATION_0..270): egim ekseni buna gore
    bool rotationDirty = true;
    int surfW = 0, surfH = 0;                // son bilinen yuzey boyutu
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
    jobject display = nullptr; jmethodID getRotation = nullptr;   // Display.getRotation()
    jobject vibrator = nullptr; jclass effectCls = nullptr; jmethodID createOneShot = nullptr, vibrate = nullptr;
    jobject clipboard = nullptr;             // android.content.ClipboardManager
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
    {   // ClipboardManager bir Handler olusturur: bu native thread'de Java Looper hazirlanir (zaten hazirsa istisna yutulur)
        jclass looper = env->FindClass("android/os/Looper");
        clearEx(env);
        if (looper) {
            jmethodID prep = env->GetStaticMethodID(looper, "prepare", "()V");
            clearEx(env);
            if (prep) { env->CallStaticVoidMethod(looper, prep); clearEx(env); }
            env->DeleteLocalRef(looper);
        }
        if (getSys) {
            jstring name = env->NewStringUTF("clipboard");
            jobject cb = env->CallObjectMethod(j.activity, getSys, name);
            clearEx(env);
            env->DeleteLocalRef(name);
            if (cb) { j.clipboard = env->NewGlobalRef(cb); env->DeleteLocalRef(cb); }
        }
    }
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
    // Ekran donusu: Activity.getWindowManager().getDefaultDisplay() -> Display.getRotation()
    jmethodID getWm = env->GetMethodID(actCls, "getWindowManager", "()Landroid/view/WindowManager;");
    clearEx(env);
    if (getWm) {
        jobject wm = env->CallObjectMethod(j.activity, getWm);
        clearEx(env);
        if (wm) {
            jclass wmCls = env->GetObjectClass(wm);
            jmethodID getDisp = env->GetMethodID(wmCls, "getDefaultDisplay", "()Landroid/view/Display;");
            clearEx(env);
            jobject disp = getDisp ? env->CallObjectMethod(wm, getDisp) : nullptr;
            clearEx(env);
            if (disp) {
                j.display = env->NewGlobalRef(disp);
                jclass dCls = env->GetObjectClass(disp);
                j.getRotation = env->GetMethodID(dCls, "getRotation", "()I");
                clearEx(env);
                env->DeleteLocalRef(dCls);
                env->DeleteLocalRef(disp);
            }
            env->DeleteLocalRef(wmCls);
            env->DeleteLocalRef(wm);
        }
    }
    env->DeleteLocalRef(actCls);
}

// Surface.ROTATION_0/90/180/270 -> 0..3 (bilinmiyorsa 0)
int queryRotation(Jni& j) {
    if (!j.env || !j.display || !j.getRotation) return 0;
    const jint r = j.env->CallIntMethod(j.display, j.getRotation);
    clearEx(j.env);
    return (r >= 0 && r <= 3) ? (int)r : 0;
}

void jniShutdown(Jni& j) {
    if (!j.env) return;
    if (j.vibrator) j.env->DeleteGlobalRef(j.vibrator);
    if (j.effectCls) j.env->DeleteGlobalRef(j.effectCls);
    if (j.display) j.env->DeleteGlobalRef(j.display);
    if (j.clipboard) j.env->DeleteGlobalRef(j.clipboard);
    j.vm->DetachCurrentThread();
    j.env = nullptr;
}

// Pano: ClipData.newPlainText -> setPrimaryClip; okuma getPrimaryClip().getItemAt(0).getText()
void setClipboard(Jni& j, const std::string& text) {
    JNIEnv* env = j.env;
    if (!env || !j.clipboard) return;
    jclass cd = env->FindClass("android/content/ClipData");
    clearEx(env);
    if (!cd) return;
    jmethodID mk = env->GetStaticMethodID(cd, "newPlainText", "(Ljava/lang/CharSequence;Ljava/lang/CharSequence;)Landroid/content/ClipData;");
    jclass cmCls = env->GetObjectClass(j.clipboard);
    jmethodID setClip = env->GetMethodID(cmCls, "setPrimaryClip", "(Landroid/content/ClipData;)V");
    clearEx(env);
    if (mk && setClip) {
        jstring label = env->NewStringUTF("ZEHRA KINIK"), t = env->NewStringUTF(text.c_str());
        jobject clip = env->CallStaticObjectMethod(cd, mk, label, t);
        clearEx(env);
        if (clip) { env->CallVoidMethod(j.clipboard, setClip, clip); clearEx(env); env->DeleteLocalRef(clip); }
        env->DeleteLocalRef(label); env->DeleteLocalRef(t);
    }
    env->DeleteLocalRef(cmCls); env->DeleteLocalRef(cd);
}
std::string getClipboard(Jni& j) {
    JNIEnv* env = j.env;
    std::string out;
    if (!env || !j.clipboard) return out;
    jclass cmCls = env->GetObjectClass(j.clipboard);
    jmethodID getClip = env->GetMethodID(cmCls, "getPrimaryClip", "()Landroid/content/ClipData;");
    clearEx(env);
    jobject clip = getClip ? env->CallObjectMethod(j.clipboard, getClip) : nullptr;
    clearEx(env);
    if (clip) {
        jclass cd = env->GetObjectClass(clip);
        jmethodID itemAt = env->GetMethodID(cd, "getItemAt", "(I)Landroid/content/ClipData$Item;");
        clearEx(env);
        jobject item = itemAt ? env->CallObjectMethod(clip, itemAt, 0) : nullptr;
        clearEx(env);
        if (item) {
            jclass ic = env->GetObjectClass(item);
            jmethodID getText = env->GetMethodID(ic, "getText", "()Ljava/lang/CharSequence;");
            clearEx(env);
            jobject cs = getText ? env->CallObjectMethod(item, getText) : nullptr;
            clearEx(env);
            if (cs) {
                jclass csCls = env->GetObjectClass(cs);
                jmethodID toStr = env->GetMethodID(csCls, "toString", "()Ljava/lang/String;");
                clearEx(env);
                jstring js = toStr ? (jstring)env->CallObjectMethod(cs, toStr) : nullptr;
                clearEx(env);
                if (js) {
                    const char* c = env->GetStringUTFChars(js, nullptr);
                    if (c) { out = c; env->ReleaseStringUTFChars(js, c); }
                    env->DeleteLocalRef(js);
                }
                env->DeleteLocalRef(csCls); env->DeleteLocalRef(cs);
            }
            env->DeleteLocalRef(ic); env->DeleteLocalRef(item);
        }
        env->DeleteLocalRef(cd); env->DeleteLocalRef(clip);
    }
    env->DeleteLocalRef(cmCls);
    return out;
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

// Oyun kolu (masaustuyle ayni dizilim): tetikler gaz / fren, sol cubuk / hat direksiyon, L1 debriyaj, A / X vites,
// B geri, Start onay, Select ayarlar, R1 bos, Y yol. Eksenler esikli dijital tusa cevrilir.
bool padKey(zk::App& g, int32_t code, bool down) {
    using zk::Key;
    switch (code) {
    case AKEYCODE_BUTTON_A: g.key(Key::ShiftUp, down); return true;
    case AKEYCODE_BUTTON_X: g.key(Key::ShiftDown, down); return true;
    case AKEYCODE_BUTTON_Y: g.key(Key::PageUp, down); return true;
    case AKEYCODE_BUTTON_L1: g.key(Key::Clutch, down); return true;
    case AKEYCODE_BUTTON_R1: g.key(Key::Gear0, down); return true;
    case AKEYCODE_BUTTON_START: g.key(Key::Enter, down); return true;
    case AKEYCODE_BUTTON_SELECT: g.key(Key::Settings, down); return true;
    case AKEYCODE_BUTTON_B: if (!down) g.back(); return true;
    case AKEYCODE_DPAD_LEFT: g.key(Key::Left, down); return true;
    case AKEYCODE_DPAD_RIGHT: g.key(Key::Right, down); return true;
    case AKEYCODE_DPAD_UP: g.key(Key::PageUp, down); return true;
    case AKEYCODE_DPAD_DOWN: g.key(Key::PageDown, down); return true;
    default: return false;
    }
}
void padAxes(zk::App& g, const AInputEvent* ev) {
    static bool thr = false, brk = false, l = false, r = false;
    auto set = [&](bool& st, bool now, zk::Key k) { if (now != st) { st = now; g.key(k, now); } };
    const float rt = std::max(AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_RTRIGGER, 0), AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_GAS, 0));
    const float lt = std::max(AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_LTRIGGER, 0), AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_BRAKE, 0));
    float x = AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_X, 0);
    const float hx = AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_HAT_X, 0);
    if (std::fabs(hx) > std::fabs(x)) x = hx;
    g.setPadPedals(std::clamp(rt, 0.0f, 1.0f), std::clamp(lt, 0.0f, 1.0f));   // analog pedal (surus); dipte tus olayi
    set(thr, rt > (thr ? 0.80f : 0.90f), zk::Key::Throttle);
    set(brk, lt > (brk ? 0.80f : 0.90f), zk::Key::Brake);
    set(l, x < (l ? -0.3f : -0.4f), zk::Key::Left);
    set(r, x > (r ? 0.3f : 0.4f), zk::Key::Right);
}

int32_t onInput(android_app* app, AInputEvent* ev) {
    Platform& p = *static_cast<Platform*>(app->userData);
    const int32_t src = AInputEvent_getSource(ev);
    const bool pad = (src & AINPUT_SOURCE_GAMEPAD) == AINPUT_SOURCE_GAMEPAD || (src & AINPUT_SOURCE_JOYSTICK) == AINPUT_SOURCE_JOYSTICK
                  || (src & AINPUT_SOURCE_DPAD) == AINPUT_SOURCE_DPAD;
    if (AInputEvent_getType(ev) == AINPUT_EVENT_TYPE_KEY && pad && AKeyEvent_getKeyCode(ev) != AKEYCODE_BACK) {
        const int32_t a = AKeyEvent_getAction(ev);
        if (AKeyEvent_getRepeatCount(ev) > 0) return 1;
        return padKey(*p.game, AKeyEvent_getKeyCode(ev), a == AKEY_EVENT_ACTION_DOWN) ? 1 : 0;
    }
    if (AInputEvent_getType(ev) == AINPUT_EVENT_TYPE_MOTION && (src & AINPUT_SOURCE_JOYSTICK) == AINPUT_SOURCE_JOYSTICK) {
        padAxes(*p.game, ev);
        return 1;
    }
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
        p.rotationDirty = true;
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
    p.game->onOrientation = [&jni, &p](bool landscape) { requestOrientation(jni, landscape); p.rotationDirty = true; };
    p.game->onHaptic = [&jni](int ms, int amp) { vibrate(jni, ms, amp); };
    p.game->onSetClipboard = [&jni](const std::string& t) { setClipboard(jni, t); };
    p.game->onGetClipboard = [&jni]() { return getClipboard(jni); };
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
                    // Ekranin saga dogru ekseni (cihaz ekseninde; Android remapCoordinateSystem): dikeyde +x, 90da +y,
                    // 180de -x, 270te -y. Sola egim +. Ayarlarda TERS secilirse isaret doner.
                    const float ax = ev.acceleration.x, ay = ev.acceleration.y;
                    const float side = p.rotation == 1 ? ay : p.rotation == 2 ? -ax : p.rotation == 3 ? -ay : ax;
                    const float t = std::clamp(side / (9.81f * 0.5f), -1.0f, 1.0f);
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
        // 90 <-> 270 cevirmede Android yapilandirma degisikligi bildirmez: donus yarim saniyede bir de okunur
        static int rotFrames = 0;
        if (++rotFrames >= 30) { rotFrames = 0; p.rotationDirty = true; }
        if (p.rotationDirty) { p.rotation = queryRotation(jni); p.rotationDirty = false; }
        {   // Donuste CONFIG_CHANGED eski boyutu verebiliyor: yuzey boyutu her karede okunur, degisince uyarlanir
            EGLint w = 1, h = 1;
            eglQuerySurface(p.dpy, p.surf, EGL_WIDTH, &w);
            eglQuerySurface(p.dpy, p.surf, EGL_HEIGHT, &h);
            if (w != p.surfW || h != p.surfH) { p.surfW = w; p.surfH = h; p.game->resize(w, h); }
        }
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
