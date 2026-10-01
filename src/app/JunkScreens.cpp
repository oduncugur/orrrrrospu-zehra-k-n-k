// ZEHRA KINIK - Hurdalik (kelepir hasarli araclar) ve restorasyon atolyesi (bilesen bilesen, adim adim).
#include "Screens.h"
#include "Ui.h"
#include "garage/VehicleCatalog.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace zk {

namespace {
const Rect kBackJ{8, 596, 352, 634};
const Rect kBuyJ{8, 548, 352, 590};
const Rect kYesJ{8, 548, 176, 590}, kNoJ{184, 548, 352, 590};

void headerJ(Renderer& r, const App& app, const char* title, const char* sub) {
    r.rect(0, 0, 360, 58, kUiPanel);
    r.text(8, 8, title, 2, kUiGold);
    r.text(8, 32, sub, 1, kUiText);
    const std::string m = money(app.career.money);
    r.text(352 - r.textWidth(m, 2), 8, m, 2, {0.4f, 1.0f, 0.5f});
}
Color dmgColor(double d) { return d < 0.25 ? Color{0.4f, 1.0f, 0.5f} : d < 0.6 ? kUiGold : Color{1.0f, 0.3f, 0.25f}; }
} // namespace

// ================================================================== HURDALIK
JunkyardScreen::JunkyardScreen(App& app) : app_(app) { reroll(); }

void JunkyardScreen::reroll() {
    offers_ = junkyardOffers((uint32_t)(app_.career.races * 131 + app_.junkSalt * 7919 + 17));
    sel_ = -1; confirm_ = false;
}

void JunkyardScreen::render(Renderer& r) {
    r.begin(360, 640, kUiBg);
    headerJ(r, app_, "HURDALIK", "KELEPIR AMA DOKUK. HER YARISTAN SONRA YENI ARACLAR");
    char b[96];
    for (int i = 0; i < (int)offers_.size(); ++i) {
        const JunkCar& j = offers_[i];
        const VehicleDef& v = *findVehicle(j.carId);
        const float y = 64.0f + i * 80.0f;
        r.rect(8, y, 352, y + 76, i == sel_ ? Color{0.2f, 0.24f, 0.36f} : kUiPanel);
        if (i == sel_) r.rect(8, y, 12, y + 76, kUiOrange);
        r.text(16, y + 5, upper(v.fullName()).substr(0, 30), 1, {1, 1, 1});
        std::snprintf(b, sizeof b, "%d  DEGERI %s", v.year, money(carPrice(v)).c_str());
        r.text(16, y + 17, b, 1, kUiDim);
        const std::string p = money(j.price);
        r.text(344 - r.textWidth(p, 2), y + 6, p, 2, app_.career.money >= j.price ? kUiGold : kUiDim);
        // Ariza ozeti
        std::string d;
        if (j.engineWear >= 1.0) d += "MOTOR OLU  "; else if (j.engineWear > 0.3) d += "MOTOR YORGUN  ";
        if (j.gearboxBroken) d += "SANZ. KIRIK  ";
        if (j.axleBroken) d += "AKS KIRIK";
        r.text(16, y + 30, d.empty() ? "MEKANIK CALISIYOR" : d, 1, d.empty() ? Color{0.6f, 0.8f, 0.6f} : Color{1.0f, 0.4f, 0.3f});
        // Bilesen cubuklari
        const double w[6] = {j.tune.wearEngine, j.tune.wearTires, j.tune.wearBrakes, j.tune.wearSusp, j.tune.wearBody, j.tune.wearElec};
        static const char* n[6] = {"KOMP", "LAST", "FREN", "SUSP", "PAS", "ELEK"};
        for (int k = 0; k < 6; ++k) {
            const float x = 16.0f + k * 56.0f;
            r.text(x, y + 44, n[k], 1, kUiDim);
            r.rect(x, y + 56, x + 48, y + 62, {0.15f, 0.15f, 0.18f});
            r.rect(x, y + 56, x + 48 * (float)w[k], y + 62, dmgColor(w[k]));
        }
    }
    if (sel_ >= 0) {
        const JunkCar& j = offers_[sel_];
        if (confirm_) {
            button(r, kYesJ, "AL " + money(j.price), kUiGreen, 2);
            button(r, kNoJ, "VAZGEC", kUiRed, 2);
        } else button(r, kBuyJ, "SATIN AL " + money(j.price), app_.career.money >= j.price ? kUiGreen : Color{0.25f, 0.25f, 0.28f}, 2);
    } else r.textCentered(180, 562, "BIR ARAC SEC", 1, kUiDim);
    button(r, kBackJ, "< GALERI", kUiBtn, 2);
    if (msgT_ > 0) { r.rect(0, 500, 360, 528, {0.02f, 0.02f, 0.04f, 0.92f}); r.textCentered(180, 507, msg_, 2, kUiGold); }
}

void JunkyardScreen::pointerDown(int, float x, float y) {
    if (confirm_ && sel_ >= 0) {
        if (kYesJ.hit(x, y)) {
            std::string why;
            if (app_.career.buyJunk(offers_[sel_], &why)) {
                app_.junkSalt++;
                app_.saveCareer();
                app_.goRestore();                                         // dogrudan restorasyona
                return;
            }
            msg_ = why; msgT_ = 1.8; confirm_ = false;
            return;
        }
        if (kNoJ.hit(x, y)) { confirm_ = false; return; }
    }
    if (kBackJ.hit(x, y)) { app_.goGallery(); return; }
    if (sel_ >= 0 && !confirm_ && kBuyJ.hit(x, y)) {
        if (app_.career.money < offers_[sel_].price) { msg_ = "PARA YETMIYOR"; msgT_ = 1.8; return; }
        confirm_ = true; return;
    }
    for (int i = 0; i < (int)offers_.size(); ++i)
        if (Rect{8, 64.0f + i * 80.0f, 352, 140.0f + i * 80.0f}.hit(x, y)) { sel_ = i; confirm_ = false; return; }
}

void JunkyardScreen::key(Key k, bool down) {
    if (!down) return;
    if (k == Key::Back) { if (confirm_) confirm_ = false; else app_.goGallery(); }
    if (k == Key::Right) sel_ = std::min((int)offers_.size() - 1, sel_ + 1);
    if (k == Key::Left) sel_ = std::max(0, sel_ - 1);
    if (k == Key::Enter && sel_ >= 0) { if (confirm_) pointerDown(0, kYesJ.cx(), kYesJ.cy()); else confirm_ = true; }
}

// ================================================================== RESTORASYON
void RestoreScreen::render(Renderer& r) {
    const OwnedCar& oc = app_.career.car();
    const VehicleDef& v = *findVehicle(oc.carId);
    r.begin(360, 640, kUiBg);
    headerJ(r, app_, "RESTORASYON", upper(v.fullName()).substr(0, 40).c_str());
    r.text(8, 64, "HER ISCILIK ADIMI HASARI YARIYA INDIRIR", 1, kUiDim);
    r.text(8, 76, "TAM TOPARLAMAK ICIN BIRKAC KEZ UGRAS", 1, kUiDim);
    char b[64];
    for (int k = 0; k < kRestoreParts; ++k) {
        const float y = 94.0f + k * 62.0f;
        const double d = restoreDamage(oc, k);
        const long cost = restoreStepCost(oc, k);
        r.rect(8, y, 352, y + 58, kUiPanel);
        r.text(16, y + 6, restoreName(k), 2, {1, 1, 1});
        r.rect(16, y + 30, 200, y + 40, {0.15f, 0.15f, 0.18f});
        r.rect(16, y + 30, 16 + 184 * (float)std::min(1.0, d), y + 40, dmgColor(d));
        std::snprintf(b, sizeof b, "HASAR %%%.0f", d * 100);
        r.text(16, y + 44, b, 1, dmgColor(d));
        const Rect bt{220, y + 10, 344, y + 48};
        if (cost <= 0) { r.rect(bt.x0, bt.y0, bt.x1, bt.y1, {0.12f, 0.3f, 0.16f}); r.textCentered(bt.cx(), bt.cy() - 4, "SAGLAM", 1, {0.4f, 1.0f, 0.5f}); }
        else button(r, bt, money(cost), app_.career.money >= cost ? kUiOrange : Color{0.25f, 0.25f, 0.28f}, 1);
    }
    std::snprintf(b, sizeof b, "SATIS DEGERI: %s", money(sellPrice(oc)).c_str());
    r.textCentered(180, 534, b, 2, kUiGold);
    if (!oc.raceable()) r.textCentered(180, 556, "BU HALIYLE YARISAMAZ", 1, {1.0f, 0.4f, 0.3f});
    button(r, kBackJ, "< GARAJ", kUiBtn, 2);
    if (msgT_ > 0) { r.rect(0, 500, 360, 528, {0.02f, 0.02f, 0.04f, 0.92f}); r.textCentered(180, 507, msg_, 2, kUiGold); }
}

void RestoreScreen::pointerDown(int, float x, float y) {
    if (kBackJ.hit(x, y)) { app_.goGarage(); return; }
    for (int k = 0; k < kRestoreParts; ++k) {
        const float yy = 94.0f + k * 62.0f;
        if (Rect{220, yy + 10, 344, yy + 48}.hit(x, y)) {
            std::string why;
            if (app_.career.restoreStep(k, &why)) { app_.saveCareer(); msg_ = std::string(restoreName(k)) + " TAMAM"; msgT_ = 1.4; app_.haptic(30, 160); }
            else { msg_ = why; msgT_ = 1.6; }
            return;
        }
    }
}

void RestoreScreen::key(Key k, bool down) { if (down && k == Key::Back) app_.goGarage(); }

} // namespace zk
