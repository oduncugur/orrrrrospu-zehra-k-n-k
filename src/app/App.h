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
#include <map>
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
    // Oyun kolu tetikleri (analog 0..1): platform yazar; gaz / fren pedali tetige oranli
    void setPadPedals(float thr, float brk) { padThr_ = thr; padBrk_ = brk; }
    float padThrottle() const { return padThr_; }
    float padBrake() const { return padBrk_; }
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
    void goBodyShop();
    void goAchievements();
    void goEcu();
    void goSetup();                              // kurulum (suspansiyon / lastik / profiller)
    void goStreet();
    void goGauges();                             // kadran dukkani                             // sokak: bulusma / dyno yarismasi / musteri isleri
    void startMeet();                            // gece bulusmasi: bahisli drag
    // Sehirler arasi THE RUN seyahati: hedefe kadar her ara bir etap (yakit tasinir). Etap bitince sonraki etap ya da varis.
    struct RunPlan { bool active = false; int from = 0, target = 0; double fuelL = -1; std::vector<RunEntrant> field; double realKm = 300; };
    RunPlan runPlan;
    void startTravel(int target);                // ilk etap
    void continueTravel();                       // sonraki etap ekrani
    void nextTravelLeg(double fuelLeft);         // etap bitti (bitirdiyse): sehir ilerler, sonraki etap / varis
    bool activeMeet = false;
    void goSaveCode();
    // Pano (platform baglar: masaustu SDL, Android ClipboardManager). Yoksa bos / etkisiz.
    std::function<void(const std::string&)> onSetClipboard;
    std::function<std::string()> onGetClipboard;
    void toast(const std::string& msg) { toasts_.push_back(msg); }   // ust bildirim (sirayla, ~2.6 s)
    // Ilk giris ipucu: her kimlik bir kez (ayarlarda saklanir); kart acikken ekran durur, dokunus / Enter kapatir.
    // Satirlar '\n' ile ayrilir. Test calistirmalarinda (ZK_START_SCREEN / ZK_AUTOPILOT / ZK_START_DRAG) kapali.
    void hint(int id, const char* text);
    bool hintOpen() const { return !hint_.empty(); }
    void goLeague(int tab = -1);
    void goMap();                                // bolge haritasi (kariyer girisi)
    void startTour();                            // haftalik turnuva: siradaki tur
    bool activeTour = false;
    // Lig etkinligi baslat: rakip (isimli ya da dengi), mod; sonuc ekranindan donus lig ekranina
    void startEvent(int idx);
    int activeEvent = -1;                        // suren lig etkinligi (-1: yok)
    double eventHandicap = 1.6;                  // etkinlik rakibinin hata payi
    std::string eventNote;                       // lig ekranina donuste gosterilecek not (pink slip vb.)
    // Drag hayaleti: arac basina en iyi kosunun mesafe izi (kalkistan itibaren 20 Hz); hayalet.zkg dosyasinda saklanir
    struct Ghost { std::vector<float> d; double et = 0; };
    std::map<int, Ghost> ghosts;
    void saveGhosts() const;
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
    // m/s, lastik cigligi; lockSpeed > 0: kilitli teker (fren) - arac hizi, perde hizla duser (burnout degil)
    void tire(int i, double slipSpeed, double lockSpeed = 0.0) { voices_[i].slip = (float)slipSpeed; voices_[i].lock = (float)lockSpeed; }
    Opponent lastOpp;                              // son kariyer rakibi (tahmini ET'ler: odul zorlugu, ekranda gosterim)
    void wind(double speed) { windSpeed_ = (float)speed; }   // m/s, ruzgar ugultusu (ekran degisince 0)
    void sfxShift() { clunk_.fetch_add(1); }                     // vites gecisi: mekanik "tok"
    void nitrousSound(bool on) { nos_ = on; }                     // nitro tislamasi
    void rainSound(bool on) { rain_ = on; }                       // yagmur ambiyansi (ekran degisince kapanir)
    void siren(float level) { siren_ = level; }                   // polis sireni 0..1 (mesafeyle; ekran degisince 0)

private:
    std::atomic<float> tilt_{0.0f}, padThr_{0.0f}, padBrk_{0.0f};
    std::atomic<float> engineVol_{1.0f}, tireVol_{1.0f};   // ses thread'i okur (ana ses x kanal)
    std::atomic<float> windSpeed_{0.0f};
    float windLp1_ = 0, windLp2_ = 0, windPh_ = 0; uint32_t windRng_ = 22222;   // yalniz ses thread'i
    std::atomic<int> clunk_{0}; std::atomic<bool> nos_{false}, rain_{false};
    std::atomic<float> siren_{0.0f}; float sirenPh_ = 0, sirenT_ = 0, sirenLv_ = 0;
    float clunkT_ = -1, nosEnv_ = 0, nosLp_ = 0, rainLp1_ = 0, rainLp2_ = 0, dripT_ = -1, dripF_ = 0; uint32_t fxRng_ = 777;
    struct Voice {
        std::unique_ptr<ProceduralEngineAudio> synth;
        std::atomic<float> rpm{900}, thr{0}, gain{1};
        std::atomic<bool> cut{false}, inGear{false};
        std::atomic<float> slip{0}, lock{0};
        // Lastik cigligi sentez durumu (yalnizca ses thread'i)
        TireAudio tireAudio{kSampleRate};
    };
    void setScreen(std::unique_ptr<Screen> s);

    Renderer renderer_;
    std::unique_ptr<Screen> screen_, pending_;
    int sw_ = 1, sh_ = 1;
    std::mutex audioLock_;
    Voice voices_[2];
    std::string savePath_, settingsPath_, ghostPath_;
    void loadGhosts();
    uint32_t raceSeed_ = 1;
    std::vector<float> mix_;
    float fade_ = 0.0f;                          // ekran gecis karartmasi (1 -> 0)
    std::vector<std::string> toasts_; double toastT_ = 0;
    std::string hint_;
    double fps_ = 0, updMs_ = 0, fpsAcc_ = 0; int fpsFrames_ = 0;
};

} // namespace zk
