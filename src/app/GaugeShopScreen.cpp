// ZEHRA KINIK - Kadran dukkani: analog / dijital / ikisi birden kadran ve turbo basinc gostergesi.
// Ustte/solda canli onizleme, altta/sagda satin al / sec.
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
constexpr float kRowY0P = 262, kRowHP = 46;
Rect rowRP(int i) { return {8, kRowY0P + i * kRowHP, 352, kRowY0P + i * kRowHP + kRowHP - 6}; }
const Rect kApplyP{8, 500, 352, 544}, kBackGP{8, 596, 352, 634};

constexpr float kRowY0L = 52, kRowHL = 40;
Rect rowRL(int i) { return {358, kRowY0L + i * kRowHL, 632, kRowY0L + i * kRowHL + kRowHL - 4}; }
const Rect kApplyL{358, 258, 632, 298}, kBackGL{358, 306, 632, 348};
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
    const bool land = app_.settings.menuLandscape;
    r.begin(land ? 640 : 360, land ? 360 : 640, kUiBg);

    // Canli onizleme verileri
    GaugeData d;
    const float ph = (float)std::fmod(t_, 4.0) / 4.0f;
    d.redline = redline_; d.shiftRpm = shift_;
    d.rpm = 1000.0f + (redline_ - 800.0f) * (ph < 0.8f ? ph / 0.8f : 1.0f - (ph - 0.8f) / 0.2f * 0.4f);
    d.speed = 30.0f + 150.0f * ph; d.speedMax = 260; d.unit = app_.settings.speedUnit();
    d.gear = std::to_string(1 + (int)(ph * 4)); d.t = t_;
    d.boostMax = boostMax_ > 0 ? boostMax_ : 1.0f;
    d.boost = d.boostMax * std::clamp((d.rpm / redline_ - 0.3f) / 0.3f, 0.0f, 1.0f) - (ph > 0.8f ? 0.6f : 0.0f);

    const bool previewBoost = sel_ == 4 ? true : oc.boostGauge;
    const int style = sel_ == 4 ? std::max(1, oc.gauge) : sel_;

    const bool owned = sel_ == 0 || (sel_ < 4 ? (oc.gaugeOwned >> sel_) & 1 : (oc.gaugeOwned >> 8) & 1);
    std::string applyLbl;
    if (sel_ == 4) applyLbl = !owned ? "SATIN AL " + money(kBoostGaugePrice) : oc.boostGauge ? "KAPAT" : "AC";
    else if (oc.gauge == sel_) applyLbl = "TAKILI";
    else applyLbl = owned ? "TAK" : "SATIN AL VE TAK " + money(gaugeStylePrice(sel_));

    if (land) {
        // ==========================================
        // YATAY 640x360 NATIVE TASARIM
        // ==========================================
        r.rect(0, 0, 640, 44, kUiPanel);
        r.text(8, 8, "KADRAN", 2, kUiGold);
        r.text(8, 28, "GOSTERGELER (YOL VE DRAG EKRANI)", 1, kUiDim);
        const std::string m = money(app_.career.money);
        r.text(632 - r.textWidth(m, 2), 8, m, 2, {0.4f, 1.0f, 0.5f});

        // Sol Kisim: Buyuk Canli Onizleme
        r.rect(8, 52, 350, 348, {0.03f, 0.03f, 0.05f});
        drawGaugeCluster(r, 16, 68, 342, 332, style, previewBoost, d);

        // Sag Kisim: 5 Secenek
        for (int i = 0; i < 5; ++i) {
            const Rect R = rowRL(i);
            const bool rowOwned = i == 0 || (i < 4 ? (oc.gaugeOwned >> i) & 1 : (oc.gaugeOwned >> 8) & 1);
            const bool active = i < 4 ? oc.gauge == i : oc.boostGauge;
            r.rect(R.x0, R.y0, R.x1, R.y1, i == sel_ ? Color{0.2f, 0.24f, 0.36f} : active ? Color{0.12f, 0.3f, 0.16f} : kUiPanel);
            if (i == sel_) r.rect(R.x0, R.y0, R.x0 + 4, R.y1, kUiOrange);

            r.text(R.x0 + 10, R.y0 + 5, i < 4 ? gaugeName(i) : "TURBO BASINC GOSTERGESI", 1, {1, 1, 1});
            if (i == 4) r.text(R.x0 + 10, R.y0 + 19, boostMax_ > 0 ? "VAKUM / BOOST, BAR" : "ATMOSFERIK MOTORDA GORUNMEZ", 1, boostMax_ > 0 ? kUiDim : Color{0.8f, 0.5f, 0.35f});
            else if (i == 1) r.text(R.x0 + 10, R.y0 + 19, "IBRELI DEVIR + HIZ", 1, kUiDim);
            else if (i == 2) r.text(R.x0 + 10, R.y0 + 19, "RAKAM + DEVIR SERIDI + VITES ISIGI", 1, kUiDim);
            else if (i == 3) r.text(R.x0 + 10, R.y0 + 19, "IBRELI DEVIR + DIJITAL HIZ", 1, kUiDim);
            else r.text(R.x0 + 10, R.y0 + 19, "UST SERITTE HIZ / DEVIR", 1, kUiDim);

            std::string right = active ? (i == 4 ? "ACIK" : "TAKILI") : rowOwned ? (i == 4 ? "KAPALI" : "SAHIP") : money(i < 4 ? gaugeStylePrice(i) : kBoostGaugePrice);
            r.text(R.x1 - 10 - r.textWidth(right, 2), R.y0 + 10, right, 2, active ? Color{0.4f, 1.0f, 0.5f} : rowOwned ? kUiText : kUiGold);
        }

        button(r, kApplyL, applyLbl, oc.gauge == sel_ && sel_ < 4 ? Color{0.25f, 0.25f, 0.28f} : kUiGreen, 2);
        button(r, kBackGL, "< GARAJ", kUiBtn, 2);

        if (msgT_ > 0) {
            r.rect(8, 306, 350, 348, {0.02f, 0.02f, 0.04f, 0.95f});
            r.textFit(179, 320, msg_, 2, 330, kUiGold, true);
        }
    } else {
        // ==========================================
        // DIKEY 360x640 KLASIK TASARIM
        // ==========================================
        r.rect(0, 0, 360, 58, kUiPanel);
        r.text(8, 8, "KADRAN", 2, kUiGold);
        r.text(8, 34, "GOSTERGELER (YOL VE DRAG EKRANI)", 1, kUiDim);
        const std::string m = money(app_.career.money);
        r.text(352 - r.textWidth(m, 2), 8, m, 2, {0.4f, 1.0f, 0.5f});

        r.rect(8, 66, 352, 246, {0.03f, 0.03f, 0.05f});
        drawGaugeCluster(r, 16, 76, 344, 236, style, previewBoost, d);

        for (int i = 0; i < 5; ++i) {
            const Rect R = rowRP(i);
            const bool rowOwned = i == 0 || (i < 4 ? (oc.gaugeOwned >> i) & 1 : (oc.gaugeOwned >> 8) & 1);
            const bool active = i < 4 ? oc.gauge == i : oc.boostGauge;
            r.rect(R.x0, R.y0, R.x1, R.y1, i == sel_ ? Color{0.2f, 0.24f, 0.36f} : active ? Color{0.12f, 0.3f, 0.16f} : kUiPanel);
            if (i == sel_) r.rect(R.x0, R.y0, R.x0 + 4, R.y1, kUiOrange);
            r.text(18, R.y0 + 8, i < 4 ? gaugeName(i) : "TURBO BASINC GOSTERGESI", 1, {1, 1, 1});
            if (i == 4) r.text(18, R.y0 + 22, boostMax_ > 0 ? "VAKUM / BOOST, BAR" : "ATMOSFERIK MOTORDA GORUNMEZ", 1, boostMax_ > 0 ? kUiDim : Color{0.8f, 0.5f, 0.35f});
            else if (i == 1) r.text(18, R.y0 + 22, "IBRELI DEVIR + HIZ", 1, kUiDim);
            else if (i == 2) r.text(18, R.y0 + 22, "RAKAM + DEVIR SERIDI + VITES ISIGI", 1, kUiDim);
            else if (i == 3) r.text(18, R.y0 + 22, "IBRELI DEVIR + DIJITAL HIZ", 1, kUiDim);
            else r.text(18, R.y0 + 22, "UST SERITTE HIZ / DEVIR", 1, kUiDim);
            std::string right = active ? (i == 4 ? "ACIK" : "TAKILI") : rowOwned ? (i == 4 ? "KAPALI" : "SAHIP") : money(i < 4 ? gaugeStylePrice(i) : kBoostGaugePrice);
            r.text(344 - r.textWidth(right, 2), R.y0 + 12, right, 2, active ? Color{0.4f, 1.0f, 0.5f} : rowOwned ? kUiText : kUiGold);
        }

        button(r, kApplyP, applyLbl, oc.gauge == sel_ && sel_ < 4 ? Color{0.25f, 0.25f, 0.28f} : kUiGreen, 2);
        button(r, kBackGP, "< GARAJ", kUiBtn, 2);
        if (msgT_ > 0) { r.rect(0, 556, 360, 588, {0.02f, 0.02f, 0.04f, 0.92f}); r.textFit(180, 566, msg_, 2, 344, kUiGold, true); }
    }
}

void GaugeShopScreen::pointerDown(int, float x, float y) {
    const bool land = app_.settings.menuLandscape;
    const Rect& backBtn = land ? kBackGL : kBackGP;
    const Rect& applyBtn = land ? kApplyL : kApplyP;

    if (backBtn.hit(x, y)) { app_.goGarage(); return; }
    for (int i = 0; i < 5; ++i) {
        if ((land ? rowRL(i) : rowRP(i)).hit(x, y)) { sel_ = i; return; }
    }
    if (applyBtn.hit(x, y)) {
        std::string why;
        const bool ok = sel_ == 4 ? app_.career.toggleBoostGauge(&why) : app_.career.selectGauge(sel_, &why);
        if (ok) {
            app_.saveCareer();
            msg_ = sel_ == 4 ? (app_.career.car().boostGauge ? "TURBO GOSTERGESI ACIK" : "TURBO GOSTERGESI KAPALI") : std::string(gaugeName(sel_)) + " TAKILDI";
        } else {
            msg_ = why;
        }
        msgT_ = 1.6;
    }
}

void GaugeShopScreen::key(Key k, bool down) {
    if (!down) return;
    if (k == Key::Back) app_.goGarage();
    else if (k == Key::Throttle) sel_ = (sel_ + 4) % 5;
    else if (k == Key::Brake) sel_ = (sel_ + 1) % 5;
    else if (k == Key::Enter) {
        const Rect& applyBtn = app_.settings.menuLandscape ? kApplyL : kApplyP;
        pointerDown(0, applyBtn.cx(), applyBtn.cy());
    }
}

} // namespace zk
