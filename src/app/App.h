// ZEHRA KINIK - Uygulama cekirdegi: ekranlar (garaj, drag yarisi), ses karistirma, girdi yonlendirme.
// Platform katmani (Android / SDL3) yalnizca GL baglami, ses cihazi, girdi ve ekran yonu saglar.
#pragma once
#include "app/Renderer.h"
#include "audio/ProceduralEngineAudio.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace zk {

enum class Key { Throttle, Brake, Clutch, ShiftUp, ShiftDown, Gear0, Gear1, Gear2, Gear3, Gear4, Gear5, Gear6,
                 Left, Right, PageUp, PageDown, Enter, Back };

class App;

class Screen {
public:
    virtual ~Screen() = default;
    virtual bool landscape() const = 0;
    virtual void update(double dt) = 0;
    virtual void render(Renderer& r) = 0;
    // Girdi: sanal piksel koordinatlari
    virtual void pointerDown(int, float, float) {}
    virtual void pointerMove(int, float, float) {}
    virtual void pointerUp(int) {}
    virtual void key(Key, bool) {}
};

class App {
public:
    static constexpr int kSampleRate = 48000;
    App();
    ~App();

    bool initGraphics();
    void shutdownGraphics();
    void resize(int w, int h) { sw_ = w > 0 ? w : 1; sh_ = h > 0 ? h : 1; }
    void update(double dt);
    void render();
    void pointerDown(int id, float px, float py);
    void pointerMove(int id, float px, float py);
    void pointerUp(int id);
    void key(Key k, bool down);
    void renderAudio(float* out, int frames);
    bool readPixelsRGB(std::vector<unsigned char>& rgb, int& w, int& h);

    // Platform geri cagrisi: ekran yonu degisti (true = yatay)
    std::function<void(bool)> onOrientation;
    bool landscape() const { return screen_ && screen_->landscape(); }

    // Ekranlar icin
    void goGarage();
    void goDrag(int playerCarId, int opponentCarId, bool autopilot = false);
    int  selectedCar = 5;
    void setVoice(int i, const VehicleDef* v);   // nullptr = sessiz
    void voice(int i, double rpm, double throttle, bool cut, bool inGear, float gain);

private:
    struct Voice {
        std::unique_ptr<ProceduralEngineAudio> synth;
        std::atomic<float> rpm{900}, thr{0}, gain{1};
        std::atomic<bool> cut{false}, inGear{false};
    };
    void setScreen(std::unique_ptr<Screen> s);

    Renderer renderer_;
    std::unique_ptr<Screen> screen_, pending_;
    int sw_ = 1, sh_ = 1;
    std::mutex audioLock_;
    Voice voices_[2];
    std::vector<float> mix_;
};

} // namespace zk
