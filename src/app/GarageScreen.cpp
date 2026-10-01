#include "Screens.h"
#include "Ui.h"
#include "app/Looks.h"
#include "garage/VehicleCatalog.h"
#include "sim/PartTables.h"
#include "sim/VehicleSim.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace zk {

namespace {
const Rect kPrev{8, 470, 60, 508}, kNext{214, 470, 266, 508};
const Rect kRace{8, 516, 128, 562}, kRoad{132, 516, 188, 562}, kSettings{192, 516, 266, 562};
const Rect kParts{8, 570, 90, 632}, kGallery{96, 570, 178, 632}, kDyno{184, 570, 266, 632};
const float kSl[4] = {292, 380, 352, 632};
const Rect kSellG{284, 66, 352, 94};          // 3B gorunumun sag ustu (2+ arac varken)
const Rect kRestoreG{8, 66, 112, 94};         // sol ust: hasar / yipranma varsa
const Rect kPaintG{284, 100, 352, 128};       // sag ust: boyahane

double peakHp(const EngineSpec& e) {
    double hp = 0;
    for (auto* c : {&e.lowCam, &e.highCam})
        for (auto& p : *c) if (p.first <= e.redlineRpm) hp = std::max(hp, p.second * p.first * 2 * 3.14159265 / 60 / 745.7);
    return hp;
}
} // namespace

GarageScreen::GarageScreen(App& app) : app_(app) {
    app_.hint(0, "GARAJA HOSGELDIN!\nYARIS: KARIYER LIGLERI VE ETKINLIKLER\nYOL: SERBEST SURUS, YOL YARISI, TOUGE\nPARCA: MODIFIYE (MOTOR, TURBO, SASI...)\nBOYA: RENK, SERIT, JANT\nGALERI: YENI ARAC   DYNO: GUC EGRISI");
    app_.setVoice(1, nullptr);
    select(app_.career.current);
    if (!app_.startupMsg.empty()) { msg_ = app_.startupMsg; msgT_ = 4.0; app_.startupMsg.clear(); }
}

void GarageScreen::refreshEngine() {
    const OwnedCar& oc = app_.career.car();
    const VehicleDef& v = *findVehicle(oc.carId);
    VehicleSimConfig cfg; cfg.car = &v; cfg.tune = &oc.tune;
    const VehicleSim sim(cfg);                                   // parcalar uygulanmis motor + sanziman
    pt_ = std::make_unique<PowertrainCore>(sim.engineSpec(), ClutchSpec{}, DiffSpec{}, sim.gearboxSpec());
    pt_->setGear(0);
    pt_->setClutchPedal(1.0);
    tunedHp_ = peakHp(sim.engineSpec());
    stockHp_ = peakPowerHp(v);
}

void GarageScreen::select(int idx) {
    Career& c = app_.career;
    const int n = (int)c.cars.size();
    c.current = ((idx % n) + n) % n;
    app_.selectedCar = c.car().carId;
    refreshEngine();
    app_.setVoiceTuned(0, c.car().carId, &c.car().tune);
}

void GarageScreen::update(double dt) {
    if (throttleKey_) throttle_ = std::min(1.0f, throttle_ + (float)dt / 0.12f);
    else if (throttlePtr_ < 0) throttle_ = std::max(0.0f, throttle_ - (float)dt / 0.08f);
    pt_->setThrottle(throttle_);
    acc_ += dt;
    while (acc_ >= 0.001) { pt_->step(0.001, 0.0, 0.0, 1.0); acc_ -= 0.001; }   // bosta, 1 ms
    pt_->drainEvents();
    app_.voice(0, pt_->rpm(), throttle_, pt_->limiterHit(), false, 1.0f);
    app_.tire(0, 0.0); app_.tire(1, 0.0);
    hapT_ -= dt;
    if (pt_->limiterHit() && hapT_ <= 0) { app_.haptic(10, 80); hapT_ = 0.06; }   // kesici tirtiklamasi
    spin_ += (float)dt * 0.6f;
    msgT_ -= dt;
}

void GarageScreen::render(Renderer& r) {
    const Career& c = app_.career;
    const OwnedCar& oc = c.car();
    const VehicleDef& v = *findVehicle(oc.carId);
    const EngineDef& e = engineTable()[effectiveEngine(v, &oc.tune)];   // motor swap
    r.begin(360, 640, kUiBg);
    studio(r, 60, 296, 196);
    r.rect(0, 0, 360, 58, kUiPanel);
    r.text(8, 8, upper(v.brand), 2, kUiGold);
    r.text(8, 30, upper(v.model).substr(0, 20), 2, {1, 1, 1});
    const std::string m = money(c.money);
    r.text(352 - r.textWidth(m, 2), 8, m, 2, {0.4f, 1.0f, 0.5f});
    char b[128];
    std::snprintf(b, sizeof b, "YARIS %d  GAL %d", c.races, c.wins);
    r.text(352 - r.textWidth(b, 1), 32, b, 1, kUiDim);
    if (app_.settings.showFps) {
        std::snprintf(b, sizeof b, "%2.0f FPS", app_.fps());
        r.text(352 - r.textWidth(b, 1), 44, b, 1, {0.45f, 0.5f, 0.45f});
    }

    const float L = (float)v.lengthM;
    const Mat4 proj = matPerspective(0.75f, 360.0f / 230.0f, 0.1f, 50.0f);
    const Mat4 view = matLookAt(0.0f, 1.1f + 0.2f * L, 1.35f * L + 1.2f, 0.0f, 0.55f, 0.0f);
    r.setCarLook(lookOf(oc));
    r.drawCar(v.id, 0, 64, 360, 230, proj, view, matRotY(spin_));

    std::snprintf(b, sizeof b, "%d %s %s %.0fKG", v.year, bodyName(v.body), driveName(v.drive), v.massKg - Tune::weightKg(oc.tune.weight));
    r.text(8, 298, upper(b), 2, kUiText);
    std::snprintf(b, sizeof b, "%s %s %.1fL %s", e.code, layoutName(e.layout), e.displacementL, inductionName(e.induction));
    r.text(8, 318, upper(b).substr(0, 23), 2, kUiText);
    if (tunedHp_ > stockHp_ + 0.5) std::snprintf(b, sizeof b, "%.0f HP  (STOK %.0f)", tunedHp_, stockHp_);
    else std::snprintf(b, sizeof b, "%.0f HP  %s %zuV", tunedHp_, upper(gearboxName(gearboxTable()[v.gearbox].type)).c_str(), gearboxTable()[v.gearbox].ratios.size());
    r.text(8, 338, b, 2, tunedHp_ > stockHp_ + 0.5 ? Color{0.4f, 1.0f, 0.5f} : kUiText);
    r.text(8, 358, tuneSummary(oc.tune).substr(0, 46), 1, kUiGold);
    if (oc.bestEt > 0) { std::snprintf(b, sizeof b, "EN IYI 1/4: %.3f S", oc.bestEt); r.text(8, 368, b, 1, kUiDim); }
    if (!v.streetLegal) r.text(8, 280, "YARIS ARACI - ROMORK", 2, {1.0f, 0.3f, 0.3f});

    // Devir testi (bosta)
    const float rpm = (float)pt_->rpm(), red = (float)pt_->engine().redlineRpm;
    r.rect(8, 384, 280, 414, {0.15f, 0.15f, 0.18f});
    const float fill = std::clamp(rpm / (red * 1.08f), 0.0f, 1.0f);
    const bool hot = rpm > red * 0.9f;
    r.rect(8, 384, 8 + 272 * fill, 414, hot ? Color{0.95f, 0.2f, 0.3f} : Color{0.2f, 0.85f, 0.3f});
    r.rect(8 + 272 / 1.08f, 380, 10 + 272 / 1.08f, 418, {1, 0.2f, 0.2f});
    std::snprintf(b, sizeof b, "%5.0f RPM", rpm);
    r.text(8, 424, b, 3, pt_->limiterHit() ? Color{1.0f, 0.5f, 0.1f} : Color{1, 1, 1});
    if (pt_->vtecActive()) r.text(196, 424, "VTEC", 3, {1.0f, 0.2f, 0.2f});
    std::snprintf(b, sizeof b, "YAG %.1f BAR", pt_->oilPressureBar());
    r.text(8, 452, b, 1, {0.7f, 0.8f, 0.7f});
#ifndef __ANDROID__
    r.text(112, 452, "<> ARAC ENTER YARIS PGUP YOL", 1, {0.55f, 0.75f, 1.0f});
#endif

    // Garajdaki araclar
    button(r, kPrev, "<", kUiBtn, 3);
    button(r, kNext, ">", kUiBtn, 3);
    std::snprintf(b, sizeof b, "ARAC %d/%zu", c.current + 1, c.cars.size());
    r.textCentered((kPrev.x1 + kNext.x0) / 2, 482, b, 2, {1, 1, 1});

    if (!c.car().raceable()) {
        button(r, kRace, "TAMIR ET", Color{0.75f, 0.15f, 0.12f}, 2);
        std::string d = "HASAR:";
        if (c.car().axleBroken) d += " AKS KIRIK";
        if (c.car().engineWear > 0.02) { char e[32]; std::snprintf(e, sizeof e, " MOTOR YATAK %%%d", (int)(c.car().engineWear * 100 + 0.5)); d += e; }
        r.text(8, 462, d, 1, {1.0f, 0.35f, 0.3f});
    } else button(r, kRace, "YARIS >", kUiOrange, 2);
    button(r, kRoad, "YOL", Color{0.15f, 0.45f, 0.7f}, 2);
    button(r, kSettings, "AYAR", kUiBtn, 2);
#ifndef __ANDROID__
    r.textCentered(kSettings.cx(), kSettings.y1 - 13, "(O)", 1, {0.55f, 0.75f, 1.0f});
#endif
    button(r, kParts, "PARCA", kUiBtn, 2);
    button(r, kGallery, "GALERI", kUiBtn, 2);
    button(r, kDyno, "DYNO", kUiBtn, 2);

    r.rect(kSl[0], kSl[1], kSl[2], kSl[3], {0.18f, 0.18f, 0.22f});
    r.rect(kSl[0], kSl[3] - (kSl[3] - kSl[1]) * throttle_, kSl[2], kSl[3], {0.9f, 0.55f, 0.1f});
    r.textCentered((kSl[0] + kSl[2]) / 2, kSl[1] - 16, "GAZ", 2, {1, 1, 1});
#ifndef __ANDROID__
    r.textCentered((kSl[0] + kSl[2]) / 2, kSl[1] - 28, "(W)", 1, {0.55f, 0.75f, 1.0f});
#endif

    if (c.cars.size() > 1) button(r, kSellG, "SAT", Color{0.5f, 0.12f, 0.12f, 0.9f}, 2);
    if (worn()) button(r, kRestoreG, "RESTORASYON", Color{0.45f, 0.30f, 0.12f, 0.95f}, 1);
    button(r, kPaintG, "BOYA", Color{0.35f, 0.18f, 0.45f, 0.95f}, 2);
    if (msgT_ > 0) {
        r.rect(0, 250, 360, 280, {0.02f, 0.02f, 0.04f, 0.85f});
        r.textCentered(180, 258, msg_, 2, kUiGold);
    }
    if (selling_) drawSaleDialog(r, c.car());
}

void GarageScreen::sellConfirmed() {
    selling_ = false;
    const int got = sellPrice(app_.career.car());
    std::string why;
    if (!app_.career.sellCurrent(&why)) { msg_ = why; msgT_ = 1.8; return; }
    app_.saveCareer();
    select(app_.career.current);
    msg_ = "SATILDI +" + money(got); msgT_ = 2.0;
}

void GarageScreen::pointerDown(int id, float x, float y) {
    if (selling_) {                                              // modal onay
        if (kDlgYes.hit(x, y)) sellConfirmed();
        else if (kDlgNo.hit(x, y)) selling_ = false;
        return;
    }
    if (app_.career.cars.size() > 1 && kSellG.hit(x, y)) { selling_ = true; return; }
    if (worn() && kRestoreG.hit(x, y)) { app_.goRestore(); return; }
    if (kPaintG.hit(x, y)) { app_.goBodyShop(); return; }
    if (x >= kSl[0] - 10 && y >= kSl[1] - 20) {
        throttlePtr_ = id;
        throttle_ = std::clamp((kSl[3] - y) / (kSl[3] - kSl[1]), 0.0f, 1.0f);
        return;
    }
    if (kPrev.hit(x, y)) select(app_.career.current - 1);
    else if (kNext.hit(x, y)) select(app_.career.current + 1);
    else if (kSettings.hit(x, y)) app_.goSettings();
    else if (kRace.hit(x, y)) raceOrRepair();
    else if (kRoad.hit(x, y)) { if (app_.career.car().raceable()) app_.goRoad(); else { msg_ = "ARAC HASARLI - TAMIR"; msgT_ = 2.0; } }
    else if (kParts.hit(x, y)) app_.goParts();
    else if (kGallery.hit(x, y)) app_.goGallery();
    else if (kDyno.hit(x, y)) app_.goDyno();
}
// Hasarli arac yarisamaz: once tamir. Hafif motor asinmasi yarisa engel degil ama buton tamir teklif eder;
// ikinci basista (tamir parasi yoksa) yine de yarisa girilir.
void GarageScreen::raceOrRepair() {
    Career& c = app_.career;
    if (c.car().raceable()) { app_.goCareerRace(); return; }
    app_.goRestore();                                                     // yarisamaz: restorasyon atolyesi
}

bool GarageScreen::worn() const {
    const OwnedCar& c = app_.career.car();
    for (int k = 0; k < kRestoreParts; ++k) if (restoreStepCost(c, k) > 0) return true;
    return false;
}

void GarageScreen::pointerMove(int id, float, float y) {
    if (id == throttlePtr_) throttle_ = std::clamp((kSl[3] - y) / (kSl[3] - kSl[1]), 0.0f, 1.0f);
}
void GarageScreen::pointerUp(int id) { if (id == throttlePtr_) throttlePtr_ = -1; }

void GarageScreen::key(Key k, bool down) {
    if (k == Key::Throttle) throttleKey_ = down;
    if (!down) return;
    if (selling_) { if (k == Key::Enter) sellConfirmed(); else if (k == Key::Back) selling_ = false; return; }
    if (k == Key::Left) select(app_.career.current - 1);
    else if (k == Key::Right) select(app_.career.current + 1);
    else if (k == Key::Enter) raceOrRepair();
    else if (k == Key::PageUp && app_.career.car().raceable()) app_.goRoad();
    else if (k == Key::Settings) app_.goSettings();
}

} // namespace zk
