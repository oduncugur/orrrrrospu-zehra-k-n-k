// ZEHRA KINIK - Masaustu platform katmani (Windows / Linux): SDL3 pencere + OpenGL 3.3 core + SDL3 ses.
// Oyun/cizim mantigi Android ile ortak GarageApp'tedir.
//   Fare: sag kizak = gaz, alttaki tuslar = arac secimi
//   Klavye: YUKARI/W/BOSLUK = gaz, SOL/SAG = onceki/sonraki arac, PAGE UP/DOWN = 10'ar arac, ESC = cikis
//   --screenshot=dosya.ppm --frames=N --throttle-frames=M : test icin ekran goruntusu alip cikar
#include "app/GLApi.h"
#include "app/GarageApp.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace zk;

static void SDLCALL audioCallback(void* user, SDL_AudioStream* stream, int additional, int) {
    GarageApp* app = static_cast<GarageApp*>(user);
    static std::vector<float> buf;
    const int frames = additional / (int)sizeof(float);
    if (frames <= 0) return;
    buf.resize(frames);
    app->renderAudio(buf.data(), frames);
    SDL_PutAudioStreamData(stream, buf.data(), frames * (int)sizeof(float));
}

static void* getProc(const char* name) { return reinterpret_cast<void*>(SDL_GL_GetProcAddress(name)); }

int main(int argc, char** argv) {
    std::string shot; int frames = 0, thrFrames = 0, carDelta = 0;
    for (int i = 1; i < argc; ++i) {
        if (!std::strncmp(argv[i], "--screenshot=", 13)) shot = argv[i] + 13;
        else if (!std::strncmp(argv[i], "--frames=", 9)) frames = std::atoi(argv[i] + 9);
        else if (!std::strncmp(argv[i], "--throttle-frames=", 18)) thrFrames = std::atoi(argv[i] + 18);
        else if (!std::strncmp(argv[i], "--car-offset=", 13)) carDelta = std::atoi(argv[i] + 13);
    }
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        if (!SDL_Init(SDL_INIT_VIDEO)) { std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1; }
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_Window* win = SDL_CreateWindow("Zehra Kinik - Garaj", 450, 800,
                                       SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!win) { std::fprintf(stderr, "Pencere: %s\n", SDL_GetError()); return 1; }
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    if (!ctx || !zkLoadGL(getProc)) { std::fprintf(stderr, "OpenGL 3.3 baglami: %s\n", SDL_GetError()); return 1; }
    SDL_GL_SetSwapInterval(1);

    GarageApp game;
    if (carDelta) game.selectRelative(carDelta);
    int pw = 0, ph = 0;
    SDL_GetWindowSizeInPixels(win, &pw, &ph);
    game.resize(pw, ph);
    game.initGraphics();

    SDL_AudioStream* audio = nullptr;
    if (SDL_WasInit(SDL_INIT_AUDIO)) {
        const SDL_AudioSpec spec{SDL_AUDIO_F32, 1, GarageApp::kSampleRate};
        audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audioCallback, &game);
        if (audio) SDL_ResumeAudioStreamDevice(audio);
    }

    auto pixelScale = [&]() {
        int ww = 1, wh = 1; SDL_GetWindowSize(win, &ww, &wh);
        SDL_GetWindowSizeInPixels(win, &pw, &ph);
        return (float)pw / (float)std::max(ww, 1);
    };

    Uint64 last = SDL_GetTicksNS();
    bool quit = false; int frame = 0;
    while (!quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_EVENT_QUIT: quit = true; break;
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                SDL_GetWindowSizeInPixels(win, &pw, &ph); game.resize(pw, ph); break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (e.button.button == SDL_BUTTON_LEFT) { const float s = pixelScale(); game.pointerDown(0, e.button.x * s, e.button.y * s); }
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (e.motion.state & SDL_BUTTON_LMASK) { const float s = pixelScale(); game.pointerMove(0, e.motion.x * s, e.motion.y * s); }
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (e.button.button == SDL_BUTTON_LEFT) game.pointerUp(0);
                break;
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP: {
                const bool down = e.type == SDL_EVENT_KEY_DOWN;
                const SDL_Keycode k = e.key.key;
                if (k == SDLK_UP || k == SDLK_W || k == SDLK_SPACE) game.setThrottleKey(down);
                else if (down && !e.key.repeat) {
                    if (k == SDLK_LEFT) game.selectRelative(-1);
                    else if (k == SDLK_RIGHT) game.selectRelative(+1);
                    else if (k == SDLK_PAGEUP) game.selectRelative(-10);
                    else if (k == SDLK_PAGEDOWN) game.selectRelative(+10);
                    else if (k == SDLK_ESCAPE) quit = true;
                }
                break;
            }
            default: break;
            }
        }
        const Uint64 now = SDL_GetTicksNS();
        double dt = (now - last) / 1e9;
        last = now;
        if (!shot.empty()) { dt = 1.0 / 60.0; game.setThrottleKey(frame < thrFrames); }
        game.update(dt);
        game.render();
        ++frame;
        if (!shot.empty() && frame >= frames) {
            std::vector<unsigned char> rgb; int w, h;
            game.readPixelsRGB(rgb, w, h);
            if (FILE* f = std::fopen(shot.c_str(), "wb")) {
                std::fprintf(f, "P6\n%d %d\n255\n", w, h);
                std::fwrite(rgb.data(), 1, rgb.size(), f);
                std::fclose(f);
            }
            quit = true;
        }
        SDL_GL_SwapWindow(win);
    }
    if (audio) SDL_DestroyAudioStream(audio);
    game.shutdownGraphics();
    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
