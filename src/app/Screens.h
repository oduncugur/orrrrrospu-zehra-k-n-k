// ZEHRA KINIK - Ekranlar
#pragma once
#include "app/App.h"
#include "app/Cockpit.h"
#include "app/Ui.h"
#include "game/DragRace.h"
#include "game/RoadSession.h"
#include "sim/PowertrainCore.h"

#include <atomic>
#include <memory>
#include <vector>

namespace zk {

// Garaj: dikey 360x640. Donen arac, teknik bilgi, bosta devirlenme, arac secimi, yarisa gecis.
class GarageScreen : public Screen {
public:
    explicit GarageScreen(App& app);
    bool landscape() const override { return false; }
    bool modal() const { return selling_; }      // onay penceresi acik (Android geri tusu kapatir)
    void update(double dt) override;
    void render(Renderer& r) override;
    void pointerDown(int id, float x, float y) override;
    void pointerMove(int id, float x, float y) override;
    void pointerUp(int id) override;
    void key(Key k, bool down) override;

private:
    void raceOrRepair();
    bool worn() const;                           // restorasyon gereken bilesen var
    void sellConfirmed();
    void select(int ownedIndex);
    void refreshEngine();
    App& app_;
    std::unique_ptr<PowertrainCore> pt_;
    double tunedHp_ = 0, stockHp_ = 0;
    std::string msg_; double msgT_ = 0;
    bool selling_ = false;                       // satis onay penceresi acik
    float throttle_ = 0; int throttlePtr_ = -1; bool throttleKey_ = false;
    float spin_ = 0; double acc_ = 0; double hapT_ = 0;
};

// Drag yarisi: yatay 640x360.
class DragScreen : public Screen {
public:
    DragScreen(App& app, int playerCar, int opponentCar, const Tune* playerTune, const Tune* opponentTune, bool career);
    void setAutopilot(bool on) { autopilot_ = on; race_->setPlayerAutopilot(on); }
    bool landscape() const override { return true; }
    void update(double dt) override;
    void render(Renderer& r) override;
    void pointerDown(int id, float x, float y) override;
    void pointerMove(int id, float x, float y) override;
    void pointerUp(int id) override;
    void key(Key k, bool down) override;

private:
    enum class Ctl { None, Clutch, Throttle, Shifter, Brake, PaddleUp, PaddleDown };
    struct Touch { int id; Ctl ctl; };
    struct Smoke { float x, lane, y, vx, vy, life, size; };
    void restart();
    void shifterFromPoint(float x, float y, bool release = false);
    void drawWorld(Renderer& r);
    void drawCarAt(Renderer& r, int lane, float screenX, float groundY, float scale, float alpha = 1.0f);
    void drawHud(Renderer& r);
    void drawResults(Renderer& r);
    Ctl hit(float x, float y) const;

    App& app_;
    int carIds_[2];
    std::unique_ptr<DragRace> race_;
    uint32_t seed_;
    std::vector<Touch> touches_;
    PlayerControls pc_;
    float clutchUi_ = 0, throttleUi_ = 0;         // dokunmatik degerleri
    bool keyThr_ = false, keyClutch_ = false, keyBrake_ = false, brakeBtn_ = false;
    float keyClutchVal_ = 0, keyThrVal_ = 0;       // klavye analog rampalari
    float knobX_ = 460, knobY_ = 311;              // H-desen vites kolu
    bool knobDrag_ = false; float dragX_ = 0, dragY_ = 0, lastShX_ = 0, lastShY_ = 0;   // surukleme (gorsel)
    int pendingGear_ = -1, pendingPaddle_ = 0;
    std::vector<Smoke> smoke_;
    std::vector<std::string> ticker_;              // son olaylar
    double tickerT_ = 0, finishedT_ = 0, t_ = 0;
    std::string flash_; Color flashColor_{1, 1, 1}; double flashT_ = 0;   // kritik an: buyuk yazi
    float camX_ = 0;
    bool autopilot_ = false;
    bool career_ = false, rewarded_ = false; long prize_ = 0;
    Tune tunes_[2]; bool hasTune_[2] = {false, false};
    // Telemetri (sonuc ekrani grafigi): yesilden bitise 30 Hz ornek
    struct TelPt { float t, v, rpm, ov, slip; int gear; };
    std::vector<TelPt> tel_;
    double telT_ = 0, telAcc_ = 0;
    bool showGraph_ = false;
    void drawGraph(Renderer& r);
    void adjustLaunch(int delta);                // 2-step kalkis devri (+/- 250)
    // Hayalet: bu yarista gosterilen onceki en iyi kosu (kopya) + bu kosunun izi
    std::vector<float> ghost_, run_; double ghostEt_ = 0, runAcc_ = 0; bool ghostSaved_ = false;
    double ghostTimeAt(double d) const;          // hayaletin d mesafesine vardigi sure (kalkistan)
    int lastGear_ = -2;                          // vites sesi icin
    // Haptik izleme (onceki kare durumu)
    int  hapGear_ = 1, hapFlat_ = 0; bool hapLeft_ = false, hapBroke_ = false, hapRed_ = false;
    double hapLimiterT_ = 0;
};

// Parca setinin ozeti (dukkan / atolye onizlemesi): yuk oranlari 1.0 = sinirda (tepe tork / dayanim, guc / sogutma)
struct TuneStats {
    double hp = 0, nm = 0, idx = 0, axleRisk = 0, mass = 0, redline = 7000;
    double engineLoad = 0, gearboxLoad = 0, heatLoad = 0;
    double octane = 100, octaneReq = 0;          // yakit oktani / motorun istedigi (vuruntu)
    std::vector<std::pair<double, double>> hpCurve;
};
TuneStats tuneStats(const VehicleDef& v, const Tune& t);

// Modifiye dukkani: dikey. 5 sekme (motor / besleme / aktarma / sasi / ecu+sogutma) -> kategori -> kaydirmali secenekler;
// ilk dokunus onizleme (guc, endeks, motor/sanziman/isi yuku, 1/4 mil), ikinci dokunus onay. Atolye satiri -> FabricateScreen.
class PartsScreen : public Screen {
public:
    PartsScreen(App& app, int cat = -1);
    bool landscape() const override { return false; }
    void update(double dt) override { msgT_ -= dt; }
    void render(Renderer& r) override;
    void pointerDown(int id, float x, float y) override;
    void pointerMove(int id, float x, float y) override;
    void pointerUp(int id) override;
    void key(Key k, bool down) override;
private:
    struct EtJob { std::atomic<bool> done{false}; QuarterEstimate cur, nxt; };
    void drawStatsBar(Renderer& r);
    void drawPreview(Renderer& r, float py);
    void recompute();
    void select(int option);
    void buySelected(bool used = false);
    void tap(float x, float y);
    App& app_;
    int tab_ = 0, cat_ = -1, sel_ = -1;
    bool confirm_ = false;
    float scroll_ = 0;
    int dragId_ = -1; bool dragging_ = false; float downX_ = 0, downY_ = 0, lastX_ = 0, lastY_ = 0;
    TuneStats prev_, now_;
    std::shared_ptr<EtJob> et_;
    std::string msg_; double msgT_ = 0;
};

// Ozel uretim atolyesi: kategoriye gore kaydiricilar (turbo kompresor capi + A/R, kam suresi, stroker, son disli,
// vites oranlari, kanat baski kuvveti); canli sonuc (guc, yukler); "URET VE TAK" onayla.
class FabricateScreen : public Screen {
public:
    FabricateScreen(App& app, int cat);
    bool landscape() const override { return false; }
    void update(double dt) override;
    void render(Renderer& r) override;
    void pointerDown(int id, float x, float y) override;
    void pointerMove(int id, float x, float y) override;
    void pointerUp(int id) override;
    void key(Key k, bool down) override;
private:
    struct Slider { const char* name; double lo, hi, step; const char* fmt; };
    std::vector<Slider> sliders() const;
    Tune built() const;
    int price() const;
    void refresh();
    void nudge(int i, int dir);
    void make();
    App& app_;
    int cat_;
    Tune t_;
    std::vector<double> vals_;
    TuneStats now_, next_;
    bool confirm_ = false;
    int ptr_ = -1, held_ = -1, heldDir_ = 0, dragSlider_ = -1;
    double holdT_ = 0;
    std::string msg_; double msgT_ = 0;
};

// Ortak onay penceresi (dikey ekranlar): evet / vazgec dugmeleri ve arac satis dokumu (galeri + garaj)
inline const Rect kDlgYes{36, 400, 176, 450}, kDlgNo{184, 400, 324, 450};
void drawSaleDialog(Renderer& r, const OwnedCar& oc);

// Galeri: tum katalog, fiyat, satin alma; mevcut araci satma.
class GalleryScreen : public Screen {
public:
    explicit GalleryScreen(App& app);
    bool landscape() const override { return false; }
    void update(double dt) override { spin_ += (float)dt * 0.6f; msgT_ -= dt; }
    void render(Renderer& r) override;
    void pointerDown(int id, float x, float y) override;
    void key(Key k, bool down) override;
private:
    enum class Confirm { None, Buy, Sell };
    void confirmAction();
    App& app_;
    int carId_;
    Confirm confirm_ = Confirm::None;   // acik onay penceresi
    float spin_ = 0;
    std::string msg_; double msgT_ = 0;
};

// Kariyer: lig sekmeleri, etkinlik listesi (rakip, mod, odul, un), secili etkinlik ayrintisi / gunluk gorevler.
class LeagueScreen : public Screen {
public:
    explicit LeagueScreen(App& app);
    bool landscape() const override { return false; }
    void update(double dt) override { msgT_ -= dt; }
    void render(Renderer& r) override;
    void pointerDown(int id, float x, float y) override;
    void key(Key k, bool down) override;
private:
    std::vector<int> rows() const;
    App& app_;
    int tab_ = 0, sel_ = -1;
    bool confirm_ = false;
    std::string msg_; double msgT_ = 0;
};

// Basarimlar: liste (kazanilan altin, odul), ilerleme
class AchievementsScreen : public Screen {
public:
    explicit AchievementsScreen(App& app) : app_(app) {}
    bool landscape() const override { return false; }
    void update(double) override {}
    void render(Renderer& r) override;
    void pointerDown(int id, float x, float y) override;
    void key(Key k, bool down) override;
private:
    App& app_;
};

// Boyahane: renk, cila, serit, jant rengi (taslak onizleme, UYGULA ile odenir).
class BodyShopScreen : public Screen {
public:
    explicit BodyShopScreen(App& app);
    bool landscape() const override { return false; }
    void update(double dt) override { msgT_ -= dt; spin_ += (float)dt * 0.5f; }
    void render(Renderer& r) override;
    void pointerDown(int id, float x, float y) override;
    void key(Key k, bool down) override;
private:
    int cost() const;
    void apply();
    App& app_;
    int paint_ = -1, finish_ = 0, stripe_ = 0, stripeCol_ = 0, rimCol_ = -1;
    float spin_ = 0.6f;
    std::string msg_; double msgT_ = 0;
};

// Hurdalik: 6 hasarli arac kelepir fiyata (her yaristan sonra yenilenir); alinca restorasyona gider.
class JunkyardScreen : public Screen {
public:
    explicit JunkyardScreen(App& app);
    bool landscape() const override { return false; }
    void update(double dt) override { msgT_ -= dt; }
    void render(Renderer& r) override;
    void pointerDown(int id, float x, float y) override;
    void key(Key k, bool down) override;
private:
    void reroll();
    App& app_;
    std::vector<JunkCar> offers_;
    int sel_ = -1; bool confirm_ = false;
    std::string msg_; double msgT_ = 0;
};

// Restorasyon: secili aracin bilesenleri (motor, sanziman+aks, lastik, fren, suspansiyon, kaporta, elektrik), adim adim.
class RestoreScreen : public Screen {
public:
    explicit RestoreScreen(App& app) : app_(app) {}
    bool landscape() const override { return false; }
    void update(double dt) override { msgT_ -= dt; }
    void render(Renderer& r) override;
    void pointerDown(int id, float x, float y) override;
    void key(Key k, bool down) override;
private:
    App& app_;
    std::string msg_; double msgT_ = 0;
};

// Dyno: guc/tork egrisi (parcali vs stok), sesli cekis.
class DynoScreen : public Screen {
public:
    explicit DynoScreen(App& app);
    bool landscape() const override { return false; }
    void update(double dt) override;
    void render(Renderer& r) override;
    void pointerDown(int id, float x, float y) override;
    void key(Key k, bool down) override;
private:
    App& app_;
    std::vector<std::pair<double, double>> tuned_, stock_;   // (rpm, Nm) etkin egri (kam gecisi dahil)
    double redline_ = 7000, idle_ = 850, maxNm_ = 1, maxHp_ = 1, peakHp_ = 0, peakHpRpm_ = 0, peakNm_ = 0, peakNmRpm_ = 0;
    double stockPeakHp_ = 0;
    double pullRpm_ = -1;          // cekis sirasinda devir (-1 = yok)
};

// Ayarlar: dikey. Goruntu (FPS siniri, dikey esitleme, gosterge, tam ekran, olcek), ses, kontrol, birim.
// Her degisiklik aninda uygulanir ve ayarlar.cfg'ye yazilir.
class SettingsScreen : public Screen {
public:
    explicit SettingsScreen(App& app);
    bool landscape() const override { return false; }
    void update(double dt) override { msgT_ -= dt; }
    void render(Renderer& r) override;
    void pointerDown(int id, float x, float y) override;
    void key(Key k, bool down) override;
    enum class Item { Language, FpsCap, VSync, ShowFps, Fullscreen, IntScale, Graphics, RoadView, Master, Engine, Tire,
                      Haptics, Tilt, TiltSens, Assist, Gears, Speed, Tree };
private:
    struct Row { int section; Item item; float y; };   // section >= 0: bu satirdan once bolum basligi
    void change(Item it, int dir);                     // dir: +1 / -1; 0 = dongusel ileri
    std::string value(Item it) const;
    App& app_;
    std::vector<Row> rows_;
    int sel_ = 0;
    std::string msg_; double msgT_ = 0;
};

// Acik yol: yatay 640x360, arkadan takip kamerasi, duzlemsel dinamik. Once mod secimi (serbest / akis / yaris).
// Kontroller Cockpit'te (analog pedallar + sanzimana gore vites kolu); direksiyon egim / klavye.
class RoadScreen : public Screen {
public:
    RoadScreen(App& app, int carId, const Tune* tune);
    bool landscape() const override { return land_; }
    void update(double dt) override;
    void render(Renderer& r) override;
    void pointerDown(int id, float x, float y) override;
    void pointerMove(int id, float x, float y) override;
    void pointerUp(int id) override;
    void key(Key k, bool down) override;

private:
    void setupLayout();                          // ayardan yon: yatay 640x360 / dikey 360x640
    float fov() const { return land_ ? 0.85f : 1.05f; }
    void start(RoadSession::Mode m, RoadSession::Kind kind = RoadSession::Kind::Highway);
    void finishRace();
    void toggleAssist();
    void toggleTilt();
    bool autoClutchPenalty() const;              // H-desen + otomatik debriyaj: virajda vites yok, odul %75
    void drawMenu(Renderer& r);
    void drawWorld(Renderer& r);
    void drawHud(Renderer& r);
    void drawResults(Renderer& r);
    void flash(const std::string& m, double t = 1.8) { msg_ = m; msgT_ = t; }
    App& app_;
    int carId_;
    Tune tune_;
    std::unique_ptr<RoadSession> ses_;
    Cockpit cockpit_;
    bool land_ = true;
    int W = 640, H = 360;
    Rect free_{}, flow_{}, race_{}, touge_{}, karma_{}, chase_{}, assistBtn_{}, tiltBtn_{};
    double camBlend_ = 1.0;                      // 0: drag gorunumu (yandan), 1: takip kamerasi (karma gecisi)
    bool menu_ = true, rewarded_ = false, record_ = false;
    long prize_ = 0;
    double steer_ = 0;
    bool kL_ = false, kR_ = false;
    bool autopilot_ = false;
    double camPsi_ = 0, finT_ = 0;
    std::string msg_; double msgT_ = 0;
    // Teker donusu (rad) ve lastik dumani (dunya koordinati)
    double spinP_ = 0, spinR_ = 0;
    bool night_ = false, rain_ = false;          // ortam: gece / yagmur (yaris basinda tohumdan)
    double envT_ = 0;                            // yagmur damlasi animasyonu
    int lastGear_ = -2;                          // vites sesi icin
    std::string msgNote_;                        // ortam notu (HUD, ilk saniyeler)
    struct Puff { double x, y, z, vx, vy, vz, life, size; };
    std::vector<Puff> smoke_;
    std::vector<Puff> sparks_;                   // carpisma kivilcimlari (size kullanilmaz)
    double shakeT_ = 0, popT_ = 0, prevThr_ = 0; // kamera sarsintisi, gaz kesme patlamasi (egzoz alevi)
    void spawnSmoke(const RoadCar& car, double dt);
    int zoneAt(double s) const;                  // 0 kir, 1 sehir, 2 tunel
    double Pc0z() const;                         // oyuncunun yol yuksekligi (kivilcim zemini)
};

} // namespace zk
