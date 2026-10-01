// ZEHRA KINIK - Uygulama cekirdegi: ekranlar (garaj, drag yarisi), ses karistirma, girdi yonlendirme.
// Platform katmani (Android / SDL3) yalnizca GL baglami, ses cihazi, girdi ve ekran yonu saglar.
#pragma once
#include "app/Renderer.h"
#include "audio/ProceduralEngineAudio.h"
#include "audio/TireAudio.h"
#include "game/Career.h"
#include "game/Settings.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace zk {

enum class Key { Throttle, Brake, Clutch, ShiftUp, ShiftDown, Gear0, Gear1, Gear2, Gear3, Gear4, Gear5, Gear6,
                 Left, Right, PageUp, PageDown, Enter, Back, Settings };

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
    explicit App(const std::string& saveDir = "");   // kayit klasoru (bossa kayit yok)
    ~App();

    bool initGraphics();
    void shutdownGraphics();
    void resize(int w, int h) { sw_ = w > 0 ? w : 1; sh_ = h > 0 ? h : 1; }
    void update(double dt);
    void render();
    void pointerDown(int id, float px, float py);
    void pointerMove(int id, float px, float py);
    void pointerUp(int id);
    void tapVirtual(float x, float y) { screen_->pointerDown(9, x, y); screen_->pointerUp(9); }   // test betigi (sanal koordinat)
    void key(Key k, bool down);
    bool back();                                  // geri: garaj disindaysa ekran isler (true); garajda false (uygulamadan cik)
    void renderAudio(float* out, int frames);
    bool readPixelsRGB(std::vector<unsigned char>& rgb, int& w, int& h);

    // Platform geri cagrilari: ekran yonu (true = yatay), titresim (sure ms, siddet 1..255)
    std::function<void(bool)> onOrientation;
    std::function<void(int, int)> onHaptic;
    void haptic(int ms, int amplitude);           // ayarlardaki titresim gucuyle olceklenir (0 = kapali)
    // Egim (ivmeolcer) direksiyonu: platform yazar (-1 sag .. +1 sol); yoksa tiltAvailable=false
    void setTilt(float t) { tilt_ = t; tiltAvailable = true; }
    float tilt() const { return tilt_; }
    bool tiltAvailable = false;
    bool treePro = false;                         // agac tipi (ayarlarda secilir, kariyer kaydinda saklanir)
    std::string startupMsg;                       // acilista garajda bir kez gosterilir

    // Ayarlar: ekran degistirir, sonra applySettings() (ses/olcek hemen) + saveSettings().
    // Dikey esitleme, tam ekran ve FPS siniri platform katmaninca her karede okunur.
    Settings settings;
    void applySettings();
    void saveSettings();
    const std::string& settingsPath() const { return settingsPath_; }
    // Platform yazar: dikey esitleme istendi ama surucu uygulamiyor -> yazilimla ekran hizina sinirla
    bool  vsyncUnavailable = false;
    float displayHz = 60.0f;
    // Kare suresi hedefi (ns, 0 = sinirsiz): FPS siniri ve gerekirse yazilim dikey esitlemesi
    long long framePeriodNs() const {
        long long p = settings.frameNs();
        if (vsyncUnavailable && displayHz > 1.0f) p = std::max(p, (long long)(1e9 / displayHz));
        return p;
    }
    // Performans olcumu (ekranda kucuk gosterge): kare hizi ve kare basina guncelleme (fizik) suresi
    double fps() const { return fps_; }
    double updateMs() const { return updMs_; }
    bool landscape() const { return screen_ && screen_->landscape(); }

    // Ekranlar icin
    void goGarage();
    void goDrag(int playerCarId, int opponentCarId, bool autopilot = false);   // serbest (kariyer disi) yaris
    void goCareerRace();                        // kariyer araci + parcalari vs eslesen rakip
    void goParts(int cat = -1);                  // cat: dogrudan kategori (atolyeden donus)
    void goFabricate(int cat);                   // ozel uretim atolyesi
    void goJunkyard();
    void goRestore();
    int junkSalt = 0;                            // hurdalik teklifleri: alimdan sonra yenilenir
    void goGallery();
    void goDyno();
    void goRoad();                                // acik yol (serbest surus)
    void goSettings();
    Career career;
    void saveCareer();
    int  selectedCar = 5;                       // serbest mod / test icin
    void setVoice(int i, const VehicleDef* v, bool turboKit = false);   // nullptr = sessiz
    void voice(int i, double rpm, double throttle, bool cut, bool inGear, float gain);
    // Parcali arac sesi: motor swap varsa takili motorun sesi, turbo / kompresor kiti sesi
    void setVoiceTuned(int i, int carId, const Tune* tune);
    void tire(int i, double slipSpeed) { voices_[i].slip = (float)slipSpeed; }   // m/s, lastik cigligi
    Opponent lastOpp;                              // son kariyer rakibi (tahmini ET'ler: odul zorlugu, ekranda gosterim)
    void wind(double speed) { windSpeed_ = (float)speed; }   // m/s, ruzgar ugultusu (ekran degisince 0)

private:
    std::atomic<float> tilt_{0.0f};
    std::atomic<float> engineVol_{1.0f}, tireVol_{1.0f};   // ses thread'i okur (ana ses x kanal)
    std::atomic<float> windSpeed_{0.0f};
    float windLp1_ = 0, windLp2_ = 0, windPh_ = 0; uint32_t windRng_ = 22222;   // yalniz ses thread'i
    struct Voice {
        std::unique_ptr<ProceduralEngineAudio> synth;
        std::atomic<float> rpm{900}, thr{0}, gain{1};
        std::atomic<bool> cut{false}, inGear{false};
        std::atomic<float> slip{0};
        // Lastik cigligi sentez durumu (yalnizca ses thread'i)
        TireAudio tireAudio{kSampleRate};
    };
    void setScreen(std::unique_ptr<Screen> s);

    Renderer renderer_;
    std::unique_ptr<Screen> screen_, pending_;
    int sw_ = 1, sh_ = 1;
    std::mutex audioLock_;
    Voice voices_[2];
    std::string savePath_, settingsPath_;
    uint32_t raceSeed_ = 1;
    std::vector<float> mix_;
    float fade_ = 0.0f;                          // ekran gecis karartmasi (1 -> 0)
    double fps_ = 0, updMs_ = 0, fpsAcc_ = 0; int fpsFrames_ = 0;
};

} // namespace zk
