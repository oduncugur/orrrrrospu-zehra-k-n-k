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
    double tunedHp_ = 0, stockHp_ = 0, torqueNm_ = 0, index_ = 0, estEt_ = 0;
    bool cancelArm_ = false;                     // musteri isi iptali: ikinci basis onaylar
    std::string msg_; double msgT_ = 0;
    bool selling_ = false;                       // satis onay penceresi acik
    float throttle_ = 0; int throttlePtr_ = -1; bool throttleKey_ = false;
    float spin_ = 0; double acc_ = 0; double hapT_ = 0;
};

// Acilis sinematigi: yatay 640x360 (IntroScreen.cpp)
class IntroScreen : public Screen {
public:
    explicit IntroScreen(App& app);
    bool landscape() const override { return false; }
    void update(double dt) override;
    void render(Renderer& r) override;
    void pointerDown(int id, float x, float y) override;
    void key(Key k, bool down) override;

private:
    void finish();
    float carX(double t) const;
    App& app_;
    bool full_ = true, done_ = false;
    int carId_ = 122;
    Renderer::CarLook look_;
    double t_ = 0, dur_ = 7.2, red_ = 7000;
    float spin_ = 0, halfL_ = 2.25f;
};

// Drag yarisi: yatay 640x360.
class DragScreen : public Screen {
public:
    DragScreen(App& app, int playerCar, int opponentCar, const Tune* playerTune, const Tune* opponentTune, bool career);
    void setAutopilot(bool on) { autopilot_ = on; race_->setPlayerAutopilot(on); }
    bool backLeaves() const override { return true; }
    int shifter() const override;
    int shifterGear() const override;
    bool landscape() const override { return true; }
    void update(double dt) override;
    void render(Renderer& r) override;
    void pointerDown(int id, float x, float y) override;
    void pointerMove(int id, float x, float y) override;
    void pointerUp(int id) override;
    void key(Key k, bool down) override;

private:
    float spinD_[2] = {0, 0};                    // gorsel teker donusu
    float boostShown_ = 0;                       // kadran: turbo ibresi (gecikmeli)
    enum class Ctl { None, Clutch, Throttle, Shifter, Brake, PaddleUp, PaddleDown, AutoD, AutoS, AutoM };
    int autoMode_ = 1;                           // otomatik / DCT drag: 0 D, 1 S, 2 M (+/-)
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
    bool fuelCapped = false;                     // guc yakit sistemiyle kirpiliyor
    double valveSafe = 0;                        // motorun guvenli devri (supap / kam / kafa; ECU bu kadar acabilir)
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
    double gearKmhPerRatio_ = 0;                 // vites ekrani: son hiz = bu / (vites orani) (kesici, son disli, teker)
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
    int listing_ = 0;                   // -1 sifir, 0..kListings-1 ikinci el ilan
    UsedListing listing() const { return usedListing(carId_, app_.career.marketWeek(), std::max(0, listing_)); }
    long priceNow() const;
    void pickCar(int id) { carId_ = id; listing_ = soldNew(*findVehicle(id)) ? -1 : 0; }
    float spin_ = 0;
    std::string msg_; double msgT_ = 0;
};

// Kariyer: lig sekmeleri, etkinlik listesi (rakip, mod, odul, un), secili etkinlik ayrintisi / gunluk gorevler.
class LeagueScreen : public Screen {
public:
    explicit LeagueScreen(App& app, int tab = -1);
    bool landscape() const override { return false; }
    void update(double dt) override { msgT_ -= dt; }
    void render(Renderer& r) override;
    void pointerDown(int id, float x, float y) override;
    void key(Key k, bool down) override;
private:
    std::vector<int> rows() const;
    int side_ = -1;                     // sehir secici: -1 bulundugun sehir (ligdeyse), 0 / 1 ligin sehirleri
    int viewCity() const { return side_ >= 0 ? tab_ * 2 + side_ : Career::cityLeague(app_.career.city) == tab_ ? app_.career.city : tab_ * 2; }
    App& app_;
    int tab_ = 0, sel_ = -1;
    int wagerStep_ = 0;                         // bahis kademesi (Career::wagerFor)
    bool confirm_ = false;
    std::string msg_; double msgT_ = 0;
};

// Bolge haritasi: 5 bolge (lig) yollarla bagli; kilitli gri, patronu yenilen altin. Dokunus: o ligin etkinlikleri.
// Altta haftalik turnuva (3 tur drag eleme).
class RegionMapScreen : public Screen {
public:
    explicit RegionMapScreen(App& app);
    bool landscape() const override { return false; }
    void update(double dt) override { msgT_ -= dt; t_ += dt; }
    void render(Renderer& r) override;
    void pointerDown(int id, float x, float y) override;
    void key(Key k, bool down) override;
private:
    App& app_;
    int travelTo_ = -1;                          // seyahat penceresi (hedef sehir)
    std::string msg_; double msgT_ = 0, t_ = 0;
};

// Kayit kodu: kariyeri panoya kod olarak kopyala / panodaki kodu yukle (onayli). Telefon <-> bilgisayar tasima.
class SaveCodeScreen : public Screen {
public:
    explicit SaveCodeScreen(App& app) : app_(app) {}
    bool landscape() const override { return false; }
    void update(double dt) override { msgT_ -= dt; }
    void render(Renderer& r) override;
    void pointerDown(int id, float x, float y) override;
    void key(Key k, bool down) override;
private:
    void paste();
    App& app_;
    std::string pending_;                        // dogrulanmis, onay bekleyen kayit metni
    std::string msg_; double msgT_ = 0;
};

// ECU yazilim: donanimin yuva / seviye sinirlari icinde moduller (harita, devir, launch, flat shift, anti-lag, flex, vuruntu)
class EcuScreen : public Screen {
public:
    explicit EcuScreen(App& app);
    bool landscape() const override { return false; }
    void update(double dt) override { msgT_ -= dt; }
    void render(Renderer& r) override;
    void pointerDown(int id, float x, float y) override;
    void key(Key k, bool down) override;
private:
    App& app_;
    TuneStats now_;
    std::string msg_; double msgT_ = 0;
};

// Kurulum: lastik basinci, ayarli suspansiyon, LSD on yuku, NOS memesi; DRAG / YOL / PIST profilleri
class SetupScreen : public Screen {
public:
    explicit SetupScreen(App& app);
    bool landscape() const override { return false; }
    void update(double dt) override { msgT_ -= dt; }
    void render(Renderer& r) override;
    void pointerDown(int id, float x, float y) override;
    void key(Key k, bool down) override;
private:
    void change(int row, int dir);
    App& app_;
    std::string msg_; double msgT_ = 0;
};

// Sokak: gece bulusmasi, haftalik dyno yarismasi, musteri isleri (sekmeler)
class StreetScreen : public Screen {
public:
    explicit StreetScreen(App& app);
    bool landscape() const override { return false; }
    void update(double dt) override { msgT_ -= dt; }
    void render(Renderer& r) override;
    void pointerDown(int id, float x, float y) override;
    void key(Key k, bool down) override;
private:
    App& app_;
    int tab_ = 0;
    std::vector<Career::DynoEntry> board_, showBoard_;
    bool pink_ = false;                          // bulusma pink slip
    std::string msg_; double msgT_ = 0;
};

// Kadran dukkani: analog / dijital / ikisi kadran + turbo basinc gostergesi (canli onizleme)
class GaugeShopScreen : public Screen {
public:
    explicit GaugeShopScreen(App& app);
    bool landscape() const override { return false; }
    void update(double dt) override { msgT_ -= dt; t_ += dt; }
    void render(Renderer& r) override;
    void pointerDown(int id, float x, float y) override;
    void key(Key k, bool down) override;
private:
    App& app_;
    int sel_ = 1;
    float redline_ = 7000, shift_ = 6700, boostMax_ = 0;
    double t_ = 0;
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
    bool tuneMode_ = false;        // ECU ince ayar paneli
    void recompute();              // egriler / tepe degerler (ayar degisince)
    double octane_ = 100, octReq_ = 0, valveSafe_ = 0;
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
                      Haptics, Tilt, TiltSens, Assist, Esp, Gears, Speed, Tree };
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
    bool backLeaves() const override { return true; }
    int shifter() const override { return cockpit_.lever() == Cockpit::Lever::HPattern ? 1 : 2; }
    int shifterGear() const override { return cockpit_.knobGear(); }
    void update(double dt) override;
    void render(Renderer& r) override;
    void pointerDown(int id, float x, float y) override;
    void pointerMove(int id, float x, float y) override;
    void pointerUp(int id) override;
    void key(Key k, bool down) override;

private:
    void leaveResults();
    // Sehir turu (acik dunya): yol kenari noktalari, polis / serseri sataşmasi -> kovalamaca / kapisma -> geri donus
    std::vector<Poi> cruisePois() const;
    void activatePoi(int k);
    bool cruiseRet_ = false, worldChase_ = false; double cruiseResume_ = 0; int forceRival_ = 0; Tune forceTune_{};
    Rect poiBtn_{0, 0, 0, 0};
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
    Rect free_{}, flow_{}, race_{}, touge_{}, karma_{}, chase_{}, marathon_{}, assistBtn_{}, tiltBtn_{}, adasBtn_{};
    // Surus yardimi: 0 kapali, 1 hiz sabitleyici / adaptif, 2 + serit takip, 3 otonom
    int adasMode_ = 0; int adasLevel_ = 0; double ccSpeed_ = 0, ccI_ = 0;
    bool autoDrive_ = false, prevManual_ = false, ov_ = false, ovBack_ = false;   // otonom: vites devri, sollama
    double ovLat_ = 0, ovUntil_ = 0;
    void cycleAdas();
    void applyAdas(RoadControls& c, double dt, bool steerInput);
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
    float boostShown_ = 0;                       // kadran: turbo ibresi (gecikmeli)
    int runLane_ = 1; double laneCool_ = 0;      // The Run: duzlukte secilen serit
    double tiltF_ = 0;                           // egim: ek yumusatma
    double l100_ = 0, lastRunS_ = 0, lastFuel_ = 0, usedRun_ = 0;   // The Run: anlik L/100, harcanan yakit
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

// Acik dunya (zehra_world): otoban + sehir izgaralari, kavsakta donus, trafik (polis / serseri), benzinlik (fiyat),
// bulusma meydani (modifiyeli araclar), yaris baslangiclari, hurdalik; harita + waypoint + otonom rota.
class WorldScreen : public Screen {
public:
    explicit WorldScreen(App& app);
    ~WorldScreen() override;
    bool landscape() const override { return land_; }
    bool backLeaves() const override;
    int shifter() const override;
    int shifterGear() const override;
    void update(double dt) override;
    void render(Renderer& r) override;
    void pointerDown(int id, float x, float y) override;
    void pointerMove(int id, float x, float y) override;
    void pointerUp(int id) override;
    void key(Key k, bool down) override;
private:
    struct Impl;
    std::unique_ptr<Impl> m_;
    bool land_ = true;
};

} // namespace zk
