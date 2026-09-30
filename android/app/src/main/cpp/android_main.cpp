// ZEHRA KINIK - Android platform katmani (NativeActivity): EGL/GLES3 baglami, AAudio, dokunmatik girdi,
// ekran yonu (JNI) ve geri tusu. Oyun/cizim mantigi platformdan bagimsiz App'tedir (masaustu ile ortak).
#include "app/App.h"

#include <jni.h>

#include <EGL/egl.h>
#include <aaudio/AAudio.h>
#include <android/log.h>
#include <android_native_app_glue.h>

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

// Activity.setRequestedOrientation: 6 = SENSOR_LANDSCAPE, 7 = SENSOR_PORTRAIT
void requestOrientation(android_app* app, bool landscape) {
    JavaVM* vm = app->activity->vm;
    JNIEnv* env = nullptr;
    if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK || !env) return;
    jobject activity = app->activity->clazz;
    jclass cls = env->GetObjectClass(activity);
    jmethodID mid = env->GetMethodID(cls, "setRequestedOrientation", "(I)V");
    if (mid) env->CallVoidMethod(activity, mid, landscape ? 6 : 7);
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(cls);
    vm->DetachCurrentThread();
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
    p.game.onOrientation = [app](bool landscape) { requestOrientation(app, landscape); };
    requestOrientation(app, p.game.landscape());
    auto last = std::chrono::steady_clock::now();

    while (true) {
        int events = 0;
        android_poll_source* src = nullptr;
        while (ALooper_pollOnce(p.running ? 0 : -1, nullptr, &events, (void**)&src) >= 0) {
            if (src) src->process(app, src);
            if (app->destroyRequested) { stopAudio(p); termEGL(p); return; }
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
