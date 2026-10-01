// ZEHRA KINIK - Ekranlar
#pragma once
#include "app/App.h"
#include "app/Cockpit.h"
#include "app/Ui.h"
#include "game/DragRace.h"
#include "game/RoadSession.h"
#include "sim/PowertrainCore.h"

#include <memory>
#include <vector>

namespace zk {

// Garaj: dikey 360x640. Donen arac, teknik bilgi, bosta devirlenme, arac secimi, yarisa gecis.
class GarageScreen : public Screen {
public:
    explicit GarageScreen(App& app);
    bool landscape() const override { return false; }
    void update(double dt) override;
    void render(Renderer& r) override;
    void pointerDown(int id, float x, float y) override;
    void pointerMove(int id, float x, float y) override;
    void pointerUp(int id) override;
    void key(Key k, bool down) override;

private:
    void raceOrRepair();
    void select(int ownedIndex);
    void refreshEngine();
    App& app_;
    std::unique_ptr<PowertrainCore> pt_;
    double tunedHp_ = 0, stockHp_ = 0;
    std::string msg_; double msgT_ = 0;
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
    void shifterFromPoint(float x, float y);
    void drawWorld(Renderer& r);
    void drawCarAt(Renderer& r, int lane, float screenX, float groundY, float scale);
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
    int pendingGear_ = -1, pendingPaddle_ = 0;
    std::vector<Smoke> smoke_;
    std::vector<std::string> ticker_;              // son olaylar
    double tickerT_ = 0, finishedT_ = 0, t_ = 0;
    std::string flash_; Color flashColor_{1, 1, 1}; double flashT_ = 0;   // kritik an: buyuk yazi
    float camX_ = 0;
    bool autopilot_ = false;
    bool career_ = false, rewarded_ = false; long prize_ = 0;
    Tune tunes_[2]; bool hasTune_[2] = {false, false};
    // Haptik izleme (onceki kare durumu)
    int  hapGear_ = 1, hapFlat_ = 0; bool hapLeft_ = false, hapBroke_ = false, hapRed_ = false;
    double hapLimiterT_ = 0;
};

// Parca dukkani: dikey. Kategori listesi -> secenekler; fiyat, takili parca, aks riski uyarisi.
class PartsScreen : public Screen {
public:
    explicit PartsScreen(App& app);
    bool landscape() const override { return false; }
    void update(double dt) override { msgT_ -= dt; }
    void render(Renderer& r) override;
    void pointerDown(int id, float x, float y) override;
    void key(Key k, bool down) override;
private:
    void recompute();
    App& app_;
    int cat_ = -1;                 // -1: kategori listesi
    std::string msg_; double msgT_ = 0;
    double hpNow_ = 0, nmNow_ = 0, idx_ = 0, axleRisk_ = 0;
};

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
    App& app_;
    int carId_;
    float spin_ = 0;
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
    enum class Item { FpsCap, VSync, ShowFps, Fullscreen, IntScale, RoadView, Master, Engine, Tire,
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
    Rect free_{}, flow_{}, race_{}, touge_{}, karma_{}, assistBtn_{}, tiltBtn_{};
    double camBlend_ = 1.0;                      // 0: drag gorunumu (yandan), 1: takip kamerasi (karma gecisi)
    bool menu_ = true, rewarded_ = false, record_ = false;
    long prize_ = 0;
    double steer_ = 0;
    bool kL_ = false, kR_ = false;
    bool autopilot_ = false;
    double camPsi_ = 0, finT_ = 0;
    std::string msg_; double msgT_ = 0;
};

} // namespace zk
