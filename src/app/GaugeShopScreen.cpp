// ZEHRA KINIK - Kadran dukkani: analog / dijital / ikisi birden kadran ve turbo basinc gostergesi. Ustte canli onizleme
// (secili satir, aracin gercek kesici / boost degerleriyle devir taramasi), altta satin al / sec.
#include "Screens.h"
#include "Ui.h"
#include "Gauges.h"
#include "app/Hints.h"
#include "sim/VehicleSim.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace zk {

namespace {
constexpr float kRowY0 = 262, kRowH = 46;
Rect rowR(int i) { return {8, kRowY0 + i * kRowH, 352, kRowY0 + i * kRowH + kRowH - 6}; }
const Rect kApply{8, 500, 352, 544}, kBackG{8, 596, 352, 634};
}

GaugeShopScreen::GaugeShopScreen(App& app) : app_(app) {
    app_.hint(HintGauges, hintTexts()[HintGauges]);
    const OwnedCar& oc = app_.career.car();
    VehicleSimConfig cfg; cfg.car = findVehicle(oc.carId); cfg.tune = &oc.tune;
    const VehicleSim s(cfg);
    redline_ = (float)s.engineSpec().redlineRpm; shift_ = (float)s.shiftRpm(); boostMax_ = (float)s.boostMax();
    sel_ = std::max(1, oc.gauge);
}

void GaugeShopScreen::render(Renderer& r) {
    const OwnedCar& oc = app_.career.car();
    r.begin(360, 640, kUiBg);
    r.rect(0, 0, 360, 58, kUiPanel);
    r.text(8, 8, "KADRAN", 2, kUiGold);
    r.text(8, 34, "GOSTERGELER (YOL VE DRAG EKRANI)", 1, kUiDim);
    const std::string m = money(app_.career.money);
    r.text(352 - r.textWidth(m, 2), 8, m, 2, {0.4f, 1.0f, 0.5f});
    // Canli onizleme: devir taramasi (kalkis -> vites -> kesici)
    GaugeData d;
    const float ph = (float)std::fmod(t_, 4.0) / 4.0f;
    d.redline = redline_; d.shiftRpm = shift_;
    d.rpm = 1000.0f + (redline_ - 800.0f) * (ph < 0.8f ? ph / 0.8f : 1.0f - (ph - 0.8f) / 0.2f * 0.4f);
    d.speed = 30.0f + 150.0f * ph; d.speedMax = 260; d.unit = app_.settings.speedUnit();
    d.gear = std::to_string(1 + (int)(ph * 4)); d.t = t_;
    d.boostMax = boostMax_ > 0 ? boostMax_ : 1.0f;                     // atmosferikte de onizlemede goster
    d.boost = d.boostMax * std::clamp((d.rpm / redline_ - 0.3f) / 0.3f, 0.0f, 1.0f) - (ph > 0.8f ? 0.6f : 0.0f);
    r.rect(8, 66, 352, 246, {0.03f, 0.03f, 0.05f});
    const bool previewBoost = sel_ == 4 ? true : oc.boostGauge;
    const int style = sel_ == 4 ? std::max(1, oc.gauge) : sel_;
    drawGaugeCluster(r, 16, 76, 344, 236, style, previewBoost, d);
    char b[64];
    for (int i = 0; i < 5; ++i) {
        const Rect R = rowR(i);
        const bool owned = i == 0 || (i < 4 ? (oc.gaugeOwned >> i) & 1 : (oc.gaugeOwned >> 8) & 1);
        const bool active = i < 4 ? oc.gauge == i : oc.boostGauge;
        r.rect(R.x0, R.y0, R.x1, R.y1, i == sel_ ? Color{0.2f, 0.24f, 0.36f} : active ? Color{0.12f, 0.3f, 0.16f} : kUiPanel);
        if (i == sel_) r.rect(R.x0, R.y0, R.x0 + 4, R.y1, kUiOrange);
        r.text(18, R.y0 + 8, i < 4 ? gaugeName(i) : "TURBO BASINC GOSTERGESI", 1, {1, 1, 1});
        if (i == 4) r.text(18, R.y0 + 22, boostMax_ > 0 ? "VAKUM / BOOST, BAR" : "ATMOSFERIK MOTORDA GORUNMEZ", 1, boostMax_ > 0 ? kUiDim : Color{0.8f, 0.5f, 0.35f});
        else if (i == 1) r.text(18, R.y0 + 22, "IBRELI DEVIR + HIZ", 1, kUiDim);
        else if (i == 2) r.text(18, R.y0 + 22, "RAKAM + DEVIR SERIDI + VITES ISIGI", 1, kUiDim);
        else if (i == 3) r.text(18, R.y0 + 22, "IBRELI DEVIR + DIJITAL HIZ", 1, kUiDim);
        else r.text(18, R.y0 + 22, "UST SERITTE HIZ / DEVIR", 1, kUiDim);
        std::string right = active ? (i == 4 ? "ACIK" : "TAKILI") : owned ? (i == 4 ? "KAPALI" : "SAHIP") : money(i < 4 ? gaugeStylePrice(i) : kBoostGaugePrice);
        r.text(344 - r.textWidth(right, 2), R.y0 + 12, right, 2, active ? Color{0.4f, 1.0f, 0.5f} : owned ? kUiText : kUiGold);
    }
    {
        const bool owned = sel_ == 0 || (sel_ < 4 ? (oc.gaugeOwned >> sel_) & 1 : (oc.gaugeOwned >> 8) & 1);
        std::string lbl;
        if (sel_ == 4) lbl = !owned ? "SATIN AL " + money(kBoostGaugePrice) : oc.boostGauge ? "KAPAT" : "AC";
        else if (oc.gauge == sel_) lbl = "TAKILI";
        else lbl = owned ? "TAK" : "SATIN AL VE TAK " + money(gaugeStylePrice(sel_));
        std::snprintf(b, sizeof b, "%s", lbl.c_str());
        button(r, kApply, b, oc.gauge == sel_ && sel_ < 4 ? Color{0.25f, 0.25f, 0.28f} : kUiGreen, 2);
    }
    button(r, kBackG, "< GARAJ", kUiBtn, 2);
    if (msgT_ > 0) { r.rect(0, 556, 360, 588, {0.02f, 0.02f, 0.04f, 0.92f}); r.textFit(180, 566, msg_, 2, 344, kUiGold, true); }
}

void GaugeShopScreen::pointerDown(int, float x, float y) {
    if (kBackG.hit(x, y)) { app_.goGarage(); return; }
    for (int i = 0; i < 5; ++i) if (rowR(i).hit(x, y)) { sel_ = i; return; }
    if (kApply.hit(x, y)) {
        std::string why;
        const bool ok = sel_ == 4 ? app_.career.toggleBoostGauge(&why) : app_.career.selectGauge(sel_, &why);
        if (ok) { app_.saveCareer(); msg_ = sel_ == 4 ? (app_.career.car().boostGauge ? "TURBO GOSTERGESI ACIK" : "TURBO GOSTERGESI KAPALI") : std::string(gaugeName(sel_)) + " TAKILDI"; }
        else msg_ = why;
        msgT_ = 1.6;
    }
}

void GaugeShopScreen::key(Key k, bool down) {
    if (!down) return;
    if (k == Key::Back) app_.goGarage();
    else if (k == Key::PageUp) sel_ = std::max(0, sel_ - 1);
    else if (k == Key::PageDown) sel_ = std::min(4, sel_ + 1);
}

} // namespace zk
