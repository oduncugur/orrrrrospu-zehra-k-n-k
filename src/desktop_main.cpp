// ZEHRA KINIK - Masaustu platform katmani (Windows / Linux): SDL3 pencere + OpenGL 3.3 core + SDL3 ses.
// Oyun/cizim mantigi Android ile ortak App'tedir.
//   Fare: sag kizak = gaz, alttaki tuslar = arac secimi
//   Klavye: W/YUKARI = gaz, S/ASAGI = fren, BOSLUK/SHIFT = debriyaj, 1-6/N = vites (H), E/Q = vites +/-,
//           SOL/SAG = arac, PAGE UP/DOWN = 10'ar arac, ENTER = yaris/stage/tekrar, ESC = geri / cikis,
//           O / F1 = ayarlar, F11 = tam ekran
//   --screenshot=dosya.ppm --frames=N --throttle-frames=M : test icin ekran goruntusu alip cikar
//   Ayarlar (FPS siniri, dikey esitleme, tam ekran) her karede App::settings'ten okunur.
#include "app/GLApi.h"
#include "app/App.h"
#include "app/FramePacer.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>

using namespace zk;

static void SDLCALL audioCallback(void* user, SDL_AudioStream* stream, int additional, int) {
    App* app = static_cast<App*>(user);
    static std::vector<float> buf;
    const int frames = additional / (int)sizeof(float);
    if (frames <= 0) return;
    buf.resize(frames);
    app->renderAudio(buf.data(), frames);
    SDL_PutAudioStreamData(stream, buf.data(), frames * (int)sizeof(float));
}

static void* getProc(const char* name) { return reinterpret_cast<void*>(SDL_GL_GetProcAddress(name)); }

// Dikey esitleme: uyarlamali (-1) desteklenmiyorsa normal acik (1)
static void applyVSync(VSync v) {
    if (v == VSync::Adaptive && SDL_GL_SetSwapInterval(-1)) return;
    SDL_GL_SetSwapInterval(v == VSync::Off ? 0 : 1);
}

int main(int argc, char** argv) {
    std::string shot; int frames = 0, thrFrames = 0, carDelta = 0;
    double runSeconds = 0;                         // olcum: gercek zamanli N s kos, ortalama FPS'i yaz, cik
    for (int i = 1; i < argc; ++i) {
        if (!std::strncmp(argv[i], "--run-seconds=", 14)) runSeconds = std::atof(argv[i] + 14);
        if (!std::strncmp(argv[i], "--screenshot=", 13)) shot = argv[i] + 13;
        else if (!std::strncmp(argv[i], "--frames=", 9)) frames = std::atoi(argv[i] + 9);
        else if (!std::strncmp(argv[i], "--throttle-frames=", 18)) thrFrames = std::atoi(argv[i] + 18);
        else if (!std::strncmp(argv[i], "--car-offset=", 13)) carDelta = std::atoi(argv[i] + 13);
    }
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD) && !SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
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

    // Kayit klasoru: ZK_SAVE_DIR (test) ya da SDL kullanici klasoru
    std::string saveDir;
    if (const char* sd = std::getenv("ZK_SAVE_DIR")) saveDir = sd;
    else if (char* pp = SDL_GetPrefPath("ZehraKinik", "ZehraKinik")) { saveDir = pp; SDL_free(pp); }
    App game(saveDir);
    if (carDelta) game.selectedCar = 5 + carDelta;
    const bool startDrag = std::getenv("ZK_START_DRAG") != nullptr;   // test: dogrudan yarisa
    game.onOrientation = [&](bool landscape) {
        int w = 0, h = 0; SDL_GetWindowSize(win, &w, &h);
        const int lo = std::min(w, h), hi = std::max(w, h);
        if (landscape) SDL_SetWindowSize(win, hi, lo); else SDL_SetWindowSize(win, lo, hi);
    };
    // Goruntu ayarlari: degisince uygula (ayarlar ekrani, F11)
    VSync vsyncNow = game.settings.vsync;
    bool fullNow = game.settings.fullscreen;
    // Bazi suruculer (ornek: AMD "Dikey yenilemeyi bekle: Her zaman kapali") istegi kabul edip yok sayar.
    // Gercek araligi oku; esitleme istenip yoksa kare hizi yazilimla ekran yenileme hizina sinirlanir.
    auto syncVSync = [&]() {
        applyVSync(vsyncNow);
        int actual = 0; SDL_GL_GetSwapInterval(&actual);
        game.vsyncUnavailable = vsyncNow != VSync::Off && actual == 0;
        const SDL_DisplayMode* dm = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(win));
        game.displayHz = dm && dm->refresh_rate > 1.0f ? dm->refresh_rate : 60.0f;
    };
    syncVSync();
    if (fullNow) SDL_SetWindowFullscreen(win, true);
    FramePacer pacer;
    // Test: zamanli tus betigi "sure:tus:1/0,..." (ornek: ZK_KEYS="0.5:Enter:1,0.6:Enter:0")
    struct KeyEv { double t; Key k; bool down; };
    std::vector<KeyEv> script;
    if (const char* ks = std::getenv("ZK_KEYS")) {
        static const struct { const char* n; Key k; } names[] = {
            {"Throttle", Key::Throttle}, {"Brake", Key::Brake}, {"Clutch", Key::Clutch}, {"ShiftUp", Key::ShiftUp},
            {"ShiftDown", Key::ShiftDown}, {"Gear0", Key::Gear0}, {"Gear1", Key::Gear1}, {"Gear2", Key::Gear2},
            {"Gear3", Key::Gear3}, {"Gear4", Key::Gear4}, {"Gear5", Key::Gear5}, {"Gear6", Key::Gear6}, {"Enter", Key::Enter}, {"Left", Key::Left}, {"Right", Key::Right},
            {"Settings", Key::Settings}, {"Back", Key::Back}, {"PageUp", Key::PageUp}, {"PageDown", Key::PageDown}};
        std::string all = ks;
        size_t pos = 0;
        while (pos < all.size()) {
            size_t end = all.find(',', pos); if (end == std::string::npos) end = all.size();
            const std::string item = all.substr(pos, end - pos);
            const size_t a = item.find(':'), b = item.rfind(':');
            if (a != std::string::npos && b > a) {
                const std::string name = item.substr(a + 1, b - a - 1);
                for (const auto& n : names)
                    if (name == n.n) script.push_back({std::atof(item.c_str()), n.k, item[b + 1] == '1'});
            }
            pos = end + 1;
        }
    }
    size_t scriptPos = 0;
    // Test: zamanli dokunus betigi, sanal koordinat "sure:x:y,..." (ornek: ZK_TAPS="0.5:180:170,1.0:180:170")
    struct TapEv { double t; float x, y; };
    std::vector<TapEv> taps;
    if (const char* ts = std::getenv("ZK_TAPS")) {
        double t; float x, y; int n = 0; const char* p = ts;
        while (std::sscanf(p, "%lf:%f:%f%n", &t, &x, &y, &n) == 3) {
            taps.push_back({t, x, y}); p += n;
            if (*p != ',') break;
            ++p;
        }
    }
    size_t tapPos = 0;
    game.onSetClipboard = [](const std::string& t) { SDL_SetClipboardText(t.c_str()); };
    game.onGetClipboard = []() { char* t = SDL_GetClipboardText(); std::string r = t ? t : ""; SDL_free(t); return r; };
    if (const char* tv = std::getenv("ZK_TUNE")) parseTuneV2(tv, game.career.cars[game.career.current].tune);   // test: "aero:6,susp:3"
    if (const char* lk = std::getenv("ZK_LOOK")) {                // test: "boya;cila;serit;seritRenk;jant"
        OwnedCar& oc = game.career.cars[game.career.current];
        std::sscanf(lk, "%d;%d;%d;%d;%d", &oc.paint, &oc.finish, &oc.stripe, &oc.stripeCol, &oc.rimCol);
    }
    if (const char* ss = std::getenv("ZK_START_SCREEN")) {       // test: dogrudan bir ekran
        const std::string n = ss;
        if (n == "parts") game.goParts(); else if (n == "gallery") game.goGallery();
        else if (n == "dyno") game.goDyno(); else if (n == "race") game.goCareerRace();
        else if (n == "road") game.goRoad(); else if (n == "settings") game.goSettings();
        else if (n == "fabricate") game.goFabricate((int)PartCat::Turbo);
        else if (n == "junk") game.goJunkyard(); else if (n == "paint") game.goBodyShop(); else if (n == "ach") game.goAchievements(); else if (n == "ecu") game.goEcu(); else if (n == "map") game.goMap(); else if (n == "league") game.goLeague(); else if (n == "restore") game.goRestore();
    }
    if (startDrag) game.goDrag(game.selectedCar, 227, std::getenv("ZK_AUTOPILOT") != nullptr);
    int pw = 0, ph = 0;
    SDL_GetWindowSizeInPixels(win, &pw, &ph);
    game.resize(pw, ph);
    game.initGraphics();

    SDL_AudioStream* audio = nullptr;
    if (SDL_WasInit(SDL_INIT_AUDIO)) {
        const SDL_AudioSpec spec{SDL_AUDIO_F32, 1, App::kSampleRate};
        audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audioCallback, &game);
        if (audio) SDL_ResumeAudioStreamDevice(audio);
    }

    auto pixelScale = [&]() {
        int ww = 1, wh = 1; SDL_GetWindowSize(win, &ww, &wh);
        SDL_GetWindowSizeInPixels(win, &pw, &ph);
        return (float)pw / (float)std::max(ww, 1);
    };

    Uint64 last = SDL_GetTicksNS();
    const Uint64 runStart = last;
    bool quit = false; int frame = 0;
    // Oyun kolu: tetikler gaz / fren, sol cubuk direksiyon, LB debriyaj, A / X vites yukari / asagi, RB bos,
    // B geri, Start onay, Back ayarlar, D-pad sol / sag / yukari (yol) / asagi. Eksenler esikli dijital tusa cevrilir.
    bool padThr = false, padBrk = false, padL = false, padR = false;
    auto padAxis = [&](bool& st, bool now, Key k) { if (now != st) { st = now; game.key(k, now); } };
    float padT = 0.0f, padB = 0.0f;
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
                if (down && e.key.repeat) break;
                const SDL_Keycode k = e.key.key;
                if (k == SDLK_ESCAPE) { if (down && !game.back()) quit = true; break; }
                if (k == SDLK_F11) {
                    if (down) { game.settings.fullscreen = !game.settings.fullscreen; game.saveSettings(); }
                    break;
                }
                struct Map { SDL_Keycode sdl; Key key; };
                static const Map map[] = {
                    {SDLK_W, Key::Throttle}, {SDLK_UP, Key::Throttle}, {SDLK_S, Key::Brake}, {SDLK_DOWN, Key::Brake},
                    {SDLK_SPACE, Key::Clutch}, {SDLK_LSHIFT, Key::Clutch}, {SDLK_E, Key::ShiftUp}, {SDLK_Q, Key::ShiftDown},
                    {SDLK_0, Key::Gear0}, {SDLK_N, Key::Gear0}, {SDLK_1, Key::Gear1}, {SDLK_2, Key::Gear2}, {SDLK_3, Key::Gear3},
                    {SDLK_4, Key::Gear4}, {SDLK_5, Key::Gear5}, {SDLK_6, Key::Gear6}, {SDLK_LEFT, Key::Left}, {SDLK_RIGHT, Key::Right},
                    {SDLK_PAGEUP, Key::PageUp}, {SDLK_PAGEDOWN, Key::PageDown}, {SDLK_RETURN, Key::Enter}, {SDLK_ESCAPE, Key::Back},
                    {SDLK_O, Key::Settings}, {SDLK_F1, Key::Settings}, {SDLK_A, Key::Left}, {SDLK_D, Key::Right}};
                for (const Map& m : map) if (m.sdl == k) game.key(m.key, down);
                break;
            }
            case SDL_EVENT_GAMEPAD_ADDED: SDL_OpenGamepad(e.gdevice.which); break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP: {
                const bool down = e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
                switch (e.gbutton.button) {
                case SDL_GAMEPAD_BUTTON_EAST: if (down) game.back(); break;          // garajda cikis yok (yalniz klavye ESC)
                case SDL_GAMEPAD_BUTTON_SOUTH: game.key(Key::ShiftUp, down); break;
                case SDL_GAMEPAD_BUTTON_WEST: game.key(Key::ShiftDown, down); break;
                case SDL_GAMEPAD_BUTTON_NORTH: game.key(Key::PageUp, down); break;
                case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: game.key(Key::Clutch, down); break;
                case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: game.key(Key::Gear0, down); break;
                case SDL_GAMEPAD_BUTTON_START: game.key(Key::Enter, down); break;
                case SDL_GAMEPAD_BUTTON_BACK: game.key(Key::Settings, down); break;
                case SDL_GAMEPAD_BUTTON_DPAD_LEFT: game.key(Key::Left, down); break;
                case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: game.key(Key::Right, down); break;
                case SDL_GAMEPAD_BUTTON_DPAD_UP: game.key(Key::PageUp, down); break;
                case SDL_GAMEPAD_BUTTON_DPAD_DOWN: game.key(Key::PageDown, down); break;
                default: break;
                }
                break;
            }
            case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
                const float v = e.gaxis.value / 32767.0f;
                switch (e.gaxis.axis) {
                // Tetikler analog (surus ekraninda pedal tetige oranli); yalniz dibe yakin basis tus olayi (drag vb. ekranlar)
                case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: padT = std::max(0.0f, v); game.setPadPedals(padT, padB); padAxis(padThr, v > (padThr ? 0.80f : 0.90f), Key::Throttle); break;
                case SDL_GAMEPAD_AXIS_LEFT_TRIGGER: padB = std::max(0.0f, v); game.setPadPedals(padT, padB); padAxis(padBrk, v > (padBrk ? 0.80f : 0.90f), Key::Brake); break;
                case SDL_GAMEPAD_AXIS_LEFTX:
                    padAxis(padL, v < (padL ? -0.3f : -0.4f), Key::Left);
                    padAxis(padR, v > (padR ? 0.3f : 0.4f), Key::Right);
                    break;
                default: break;
                }
                break;
            }
            default: break;
            }
        }
        const Uint64 now = SDL_GetTicksNS();
        double dt = (now - last) / 1e9;
        last = now;
        if (!shot.empty()) {
            dt = 1.0 / 60.0;
            const bool thr = frame < thrFrames;
            if (thrFrames > 0 && (frame == 0 || frame == thrFrames)) game.key(Key::Throttle, thr);
            const double tNow = frame / 60.0;
            while (scriptPos < script.size() && script[scriptPos].t <= tNow) {
                game.key(script[scriptPos].k, script[scriptPos].down); ++scriptPos;
            }
            while (tapPos < taps.size() && taps[tapPos].t <= tNow) { game.tapVirtual(taps[tapPos].x, taps[tapPos].y); ++tapPos; }
        }
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

        // Ayar degisiklikleri (bir sonraki kareden gecerli)
        if (game.settings.vsync != vsyncNow) { vsyncNow = game.settings.vsync; syncVSync(); }
        if (game.settings.fullscreen != fullNow) {
            fullNow = game.settings.fullscreen;
            SDL_SetWindowFullscreen(win, fullNow);
            if (!fullNow && game.onOrientation) game.onOrientation(game.landscape());   // pencere yonunu geri yukle
        }
        // FPS siniri (test ekran goruntusu modunda yok: sabit adimla hizli kosar)
        if (shot.empty()) {
            const Sint64 waitNs = pacer.wait((Sint64)SDL_GetTicksNS(), game.framePeriodNs());
            if (waitNs > 0) SDL_DelayPrecise((Uint64)waitNs);
        }
        if (runSeconds > 0 && (SDL_GetTicksNS() - runStart) / 1e9 >= runSeconds) {
            const double secs = (SDL_GetTicksNS() - runStart) / 1e9;
            int swapNow = -99; SDL_GL_GetSwapInterval(&swapNow);
            std::printf("kare %d, sure %.2f s, ortalama %.1f FPS (siniri %d, vsync %d, swap %d%s, ekran %.0f Hz)\n", frame, secs,
                        frame / secs, game.settings.fpsCap, (int)game.settings.vsync, swapNow,
                        game.vsyncUnavailable ? " surucu reddetti" : "", game.displayHz);
            std::fflush(stdout);
            quit = true;
        }
    }
    if (audio) SDL_DestroyAudioStream(audio);
    game.shutdownGraphics();
    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
