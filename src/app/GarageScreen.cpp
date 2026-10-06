#include "Screens.h"
#include "app/Hints.h"
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
// Yerlesim (v2, kullanici dostu): ust serit (para / un), 3B arac (iki yanda ok, sag ustte ayar), bilgi kartlari,
// buyuk YARIS dugmesi, iki sira ikonlu kutucuk. Devir testi: 3B gorunumde GAZ dugmesi (basili tut).
const Rect kPrev{4, 140, 42, 214}, kNext{318, 140, 356, 214};
const Rect kSettingsG{316, 64, 354, 100}, kSellG{316, 106, 354, 132}, kRestoreG{6, 64, 118, 88}, kCashG{6, 260, 58, 286};
const Rect kGas{282, 246, 354, 288};
const Rect kRace{8, 386, 352, 436};
Rect tileR(int i) { return {8 + (i % 4) * 87.0f, 444 + (i / 4) * 92.0f, 8 + (i % 4) * 87.0f + 82, 444 + (i / 4) * 92.0f + 86}; }
enum GTile { TParts, TSetup, TGauge, TPaint, TDyno, TRoad, TGallery, TStreet, TCount };
#ifdef ZK_OPENWORLD
const char* const kTileName[TCount] = {"PARCA", "KURULUM", "KADRAN", "BOYA", "DYNO", "ACIK DUNYA", "GALERI", "SOKAK"};   // acik dunya surumu
#else
const char* const kTileName[TCount] = {"PARCA", "KURULUM", "KADRAN", "BOYA", "DYNO", "SERBEST YOL", "GALERI", "SOKAK"};
#endif
const int kTileIcon[TCount] = {IconParts, IconSetup, IconGauge, IconPaint, IconDyno, IconRoad, IconGallery, IconStreet};

double peakHp(const EngineSpec& e) {
    double hp = 0;
    for (auto* c : {&e.lowCam, &e.highCam})
        for (auto& p : *c) if (p.first <= e.redlineRpm) hp = std::max(hp, p.second * p.first * 2 * 3.14159265 / 60 / 745.7);
    return hp;
}
} // namespace

GarageScreen::GarageScreen(App& app) : app_(app) {
    app_.hint(HintGarage, hintTexts()[HintGarage]);
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
    torqueNm_ = 0;
    for (auto* c : {&sim.engineSpec().lowCam, &sim.engineSpec().highCam}) for (auto& p : *c) torqueNm_ = std::max(torqueNm_, p.second);
    index_ = performanceIndex(v, oc.tune);
    estEt_ = v.streetLegal ? estimatedEt(v, oc.tune) : 0.0;
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
    char b[128];
    r.begin(360, 640, kUiBg);
    // ---- 3B arac (studyo) ----
    studio(r, 58, 294, 230);
    const float L = (float)v.lengthM;
    const Mat4 proj = matPerspective(0.75f, 360.0f / 236.0f, 0.1f, 50.0f);
    const Mat4 view = matLookAt(0.0f, 1.1f + 0.2f * L, 1.35f * L + 1.2f, 0.0f, 0.55f, 0.0f);
    r.setCarLook(lookOf(oc));
    r.drawCar(v.id, 0, 58, 360, 236, proj, view, matRotY(spin_));
    if (c.cars.size() > 1) {                                            // arac degistir: iki yanda buyuk ok
        button(r, kPrev, "<", {0.15f, 0.16f, 0.2f, 0.75f}, 3);
        button(r, kNext, ">", {0.15f, 0.16f, 0.2f, 0.75f}, 3);
        std::snprintf(b, sizeof b, "%d / %zu", c.current + 1, c.cars.size());
        r.textCentered(180, 66, b, 1, kUiDim);
    }
    button(r, kSettingsG, "", {0.15f, 0.16f, 0.2f, 0.8f}, 1);
    icon(r, IconSettings, kSettingsG.cx(), kSettingsG.cy(), 11, {0.9f, 0.9f, 0.95f});
    if (c.cars.size() > 1 && oc.jobHp == 0) button(r, kSellG, "SAT", Color{0.5f, 0.12f, 0.12f, 0.9f}, 1);
    if (worn()) button(r, kRestoreG, "RESTORASYON", Color{0.45f, 0.30f, 0.12f, 0.95f}, 1);
    button(r, kCashG, "+10K", Color{0.1f, 0.4f, 0.15f, 0.9f}, 1);         // TEST: yayin oncesi kaldirilacak
    if (!v.streetLegal) r.textCentered(180, 80, "YARIS ARACI - ROMORK", 1, {1.0f, 0.3f, 0.3f});
    // Devir testi: GAZ dugmesi + devir + ince serit (kesici cizgisi)
    const float rpm = (float)pt_->rpm(), red = (float)pt_->engine().redlineRpm;
    button(r, kGas, "GAZ", throttle_ > 0.05f ? Color{0.9f, 0.5f, 0.1f} : Color{0.3f, 0.22f, 0.12f, 0.9f}, 2);
    std::snprintf(b, sizeof b, "%5.0f RPM", rpm);
    r.text(66, 266, b, 2, pt_->limiterHit() ? Color{1.0f, 0.5f, 0.1f} : Color{1, 1, 1});
    if (pt_->vtecActive()) r.text(196, 266, "VTEC", 2, {1.0f, 0.2f, 0.2f});
    const float fill = std::clamp(rpm / (red * 1.08f), 0.0f, 1.0f);
    r.rect(0, 290, 360, 294, {0.15f, 0.15f, 0.18f});
    r.rect(0, 290, 360 * fill, 294, rpm > red * 0.9f ? Color{0.95f, 0.2f, 0.3f} : Color{0.2f, 0.85f, 0.3f});
    r.rect(360 / 1.08f, 288, 360 / 1.08f + 2, 296, {1, 0.2f, 0.2f});
    // ---- ust serit ----
    r.rect(0, 0, 360, 58, kUiPanel);
    r.text(8, 7, upper(v.brand), 1, kUiGold);
    r.textFit(8, 22, upper(v.model), 2, 200, {1, 1, 1});
    std::snprintf(b, sizeof b, "%d %s %s  %.0f KM", v.year, bodyName(v.body), driveName(v.drive), app_.career.car().odo());
    r.text(8, 42, upper(b), 1, kUiDim);
    const std::string m = money(c.money);
    r.text(352 - r.textWidth(m, 2), 8, m, 2, {0.4f, 1.0f, 0.5f});
    std::snprintf(b, sizeof b, "UN %d   GALIBIYET %d", c.rep, c.wins);
    r.text(352 - r.textWidth(b, 1), 30, b, 1, kUiDim);
    if (app_.settings.showFps) { std::snprintf(b, sizeof b, "%2.0f FPS", app_.fps()); r.text(352 - r.textWidth(b, 1), 42, b, 1, {0.45f, 0.5f, 0.45f}); }
    // ---- bilgi kartlari ----
    std::snprintf(b, sizeof b, "%s  %.0f KG  KESICI %.0f", engineDisplayName(e).c_str(), v.massKg - oc.tune.totalWeightKg(), pt_->engine().redlineRpm);
    r.textFit(8, 302, upper(b), 1, 344, kUiText);
    struct Chip { const char* label; std::string val; Color col; };
    char hp[24], tq[24], ix[24], et[24];
    std::snprintf(hp, sizeof hp, "%.0f", tunedHp_); std::snprintf(tq, sizeof tq, "%.0f", torqueNm_); std::snprintf(ix, sizeof ix, "%.0f", index_);
    if (oc.bestEt > 0) std::snprintf(et, sizeof et, "%.2f", oc.bestEt); else if (estEt_ > 0) std::snprintf(et, sizeof et, "~%.1f", estEt_); else std::snprintf(et, sizeof et, "-");
    const Chip chips[4] = {{"GUC HP", hp, tunedHp_ > stockHp_ + 0.5 ? Color{0.4f, 1.0f, 0.5f} : Color{1, 1, 1}}, {"TORK NM", tq, {1, 1, 1}},
                           {"ENDEKS", ix, kUiGold}, {oc.bestEt > 0 ? "EN IYI 1/4" : "TAHMINI 1/4", et, {0.55f, 0.8f, 1.0f}}};
    for (int i = 0; i < 4; ++i) {
        const float x0 = 8 + i * 87.0f;
        r.rect(x0, 316, x0 + 82, 356, kUiPanel);
        r.rect(x0, 316, x0 + 82, 318, chips[i].col);
        r.textCentered(x0 + 41, 322, chips[i].label, 1, kUiDim);
        r.textFit(x0 + 41, 335, chips[i].val, 2, 76, chips[i].col, true);
    }
    // Durum satiri: hasar / musteri / parca ozeti
    if (oc.jobHp > 0) {
        std::snprintf(b, sizeof b, "MUSTERI: %.0f / %d HP  ODEME %s", tunedHp_, oc.jobHp, money(oc.jobReward).c_str());
        r.textFit(8, 366, b, 1, 344, tunedHp_ + 0.5 >= oc.jobHp ? Color{0.4f, 1.0f, 0.5f} : kUiGold);
    } else if (!oc.raceable()) {
        std::string d = "HASAR:";
        if (oc.axleBroken) d += " AKS KIRIK";
        if (oc.gearboxBroken) d += " SANZIMAN KIRIK";
        if (oc.engineWear > 0.02) { char e2[32]; std::snprintf(e2, sizeof e2, " MOTOR %%%d", (int)(oc.engineWear * 100 + 0.5)); d += e2; }
        r.textFit(8, 366, d, 1, 344, {1.0f, 0.35f, 0.3f});
    } else r.textFit(8, 366, tuneSummary(oc.tune), 1, 344, kUiGold);
    // ---- buyuk YARIS dugmesi ----
    if (oc.jobHp > 0) {
        const bool ok = tunedHp_ + 0.5 >= oc.jobHp;
        button(r, kRace, ok ? "TESLIM ET" : "MUSTERI ARACI: IPTAL", ok ? kUiGreen : Color{0.45f, 0.20f, 0.15f}, 2);
    } else if (!oc.raceable()) button(r, kRace, "TAMIR ET >", Color{0.75f, 0.15f, 0.12f}, 3);
    else {
        button(r, kRace, "", kUiOrange, 3);
        r.textCentered(kRace.cx() + 1, kRace.y0 + 9, "YARIS", 3, {0, 0, 0, 0.4f});
        r.textCentered(kRace.cx(), kRace.y0 + 8, "YARIS", 3, {1, 1, 1});
        r.textCentered(kRace.cx(), kRace.y1 - 14, "LIG  TURNUVA  HARITA", 1, {1.0f, 0.85f, 0.7f});
    }
    // ---- kutucuklar ----
    static const Color kTileCol[TCount] = {{0.22f, 0.26f, 0.34f}, {0.15f, 0.30f, 0.40f}, {0.40f, 0.30f, 0.10f}, {0.35f, 0.18f, 0.45f},
                                           {0.20f, 0.32f, 0.22f}, {0.15f, 0.35f, 0.55f}, {0.30f, 0.30f, 0.34f}, {0.30f, 0.15f, 0.40f}};
    for (int i = 0; i < TCount; ++i) {
        std::string badge;
        if (i == TStreet && c.meetAvailable()) badge = "!";                 // bu gece bulusma var
        if (i == TSetup && oc.tune.nitrous > 0 && oc.nosFill < 0.999) badge = "NOS";
        tile(r, tileR(i), kTileIcon[i], kTileName[i], kTileCol[i], badge);
    }
#ifndef __ANDROID__
#ifdef ZK_OPENWORLD
    r.textCentered(180, 630, "<> ARAC  ENTER YARIS  PGUP ACIK DUNYA  W GAZ  O AYAR", 1, {0.45f, 0.6f, 0.85f});
#else
    r.textCentered(180, 630, "<> ARAC  ENTER YARIS  PGUP YOL  W GAZ  O AYAR", 1, {0.45f, 0.6f, 0.85f});
#endif
#endif
    if (msgT_ > 0) {
        r.rect(0, 240, 360, 270, {0.02f, 0.02f, 0.04f, 0.88f});
        r.textFit(180, 248, msg_, 2, 344, kUiGold, true);
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
    Career& c = app_.career;
    if (c.cars.size() > 1 && c.car().jobHp == 0 && kSellG.hit(x, y)) { selling_ = true; return; }
    if (kSettingsG.hit(x, y)) { app_.goSettings(); return; }
    if (worn() && kRestoreG.hit(x, y)) { app_.goRestore(); return; }
    if (kCashG.hit(x, y)) {                                      // TEST: para
        c.money += 10000;
        app_.saveCareer();
        msg_ = "TEST: +$10,000"; msgT_ = 1.2;
        return;
    }
    if (kGas.hit(x, y)) { throttlePtr_ = id; throttle_ = 1.0f; return; }   // basili tut: devir
    if (c.cars.size() > 1 && kPrev.hit(x, y)) { select(c.current - 1); cancelArm_ = false; return; }
    if (c.cars.size() > 1 && kNext.hit(x, y)) { select(c.current + 1); cancelArm_ = false; return; }
    if (kRace.hit(x, y)) { raceOrRepair(); return; }
    for (int i = 0; i < TCount; ++i) {
        if (!tileR(i).hit(x, y)) continue;
        switch (i) {
        case TParts: app_.goParts(); break;
        case TSetup: app_.goSetup(); break;
        case TGauge: app_.goGauges(); break;
        case TPaint: app_.goBodyShop(); break;
        case TDyno: app_.goDyno(); break;
#ifdef ZK_OPENWORLD
        case TRoad: if (c.car().raceable()) app_.goWorld();   // acik dunya surumu: serbest yol = acik dunya
#else
        case TRoad: if (c.car().raceable()) app_.goRoad();
#endif else { msg_ = c.car().jobHp ? "MUSTERI ARACIYLA OLMAZ" : "ARAC HASARLI - TAMIR"; msgT_ = 2.0; } break;
        case TGallery: app_.goGallery(); break;
        case TStreet: app_.goStreet(); break;
        default: break;
        }
        return;
    }
}
// Hasarli arac yarisamaz: once tamir. Hafif motor asinmasi yarisa engel degil ama buton tamir teklif eder;
// ikinci basista (tamir parasi yoksa) yine de yarisa girilir.
void GarageScreen::raceOrRepair() {
    Career& c = app_.career;
    if (c.car().jobHp > 0) {                                              // musteri araci: teslim ya da (ikinci basista) iptal
        long paid = 0; std::string why;
        if (c.deliverJob(&paid, &why)) { app_.saveCareer(); msg_ = "TESLIM EDILDI +" + money(paid); msgT_ = 2.5; select(c.current); return; }
        if (cancelArm_) { c.cancelJob(); app_.saveCareer(); msg_ = "IS IPTAL: ARAC GERI VERILDI"; msgT_ = 2.0; cancelArm_ = false; select(c.current); return; }
        cancelArm_ = true; msg_ = why + " - IPTAL ICIN TEKRAR BAS"; msgT_ = 2.5;
        return;
    }
    if (c.car().raceable()) { app_.goCareerRace(); return; }
    app_.goRestore();                                                     // yarisamaz: restorasyon atolyesi
}

bool GarageScreen::worn() const {
    const OwnedCar& c = app_.career.car();
    for (int k = 0; k < kRestoreParts; ++k) if (restoreStepCost(c, k) > 0) return true;
    return false;
}

void GarageScreen::pointerMove(int, float, float) {}
void GarageScreen::pointerUp(int id) { if (id == throttlePtr_) throttlePtr_ = -1; }

void GarageScreen::key(Key k, bool down) {
    if (k == Key::Throttle) throttleKey_ = down;
    if (!down) return;
    if (selling_) { if (k == Key::Enter) sellConfirmed(); else if (k == Key::Back) selling_ = false; return; }
    if (k == Key::Left) select(app_.career.current - 1);
    else if (k == Key::Right) select(app_.career.current + 1);
    else if (k == Key::Enter) raceOrRepair();
#ifdef ZK_OPENWORLD
    else if (k == Key::PageUp && app_.career.car().raceable()) app_.goWorld();
#else
    else if (k == Key::PageUp && app_.career.car().raceable()) app_.goRoad();
#endif
    else if (k == Key::Settings) app_.goSettings();
}

} // namespace zk
