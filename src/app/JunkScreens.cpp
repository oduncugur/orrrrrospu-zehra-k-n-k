// ZEHRA KINIK - Hurdalik (kelepir hasarli araclar) ve restorasyon atolyesi.
// Dikey 360x640 ve Yatay 640x360 adaptif tasarim.
#include "Screens.h"
#include "Ui.h"
#include "garage/VehicleCatalog.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace zk {

namespace {
const Rect kBackJP{8, 596, 352, 634};
const Rect kBuyJP{8, 548, 352, 590};
const Rect kYesJP{8, 548, 176, 590}, kNoJP{184, 548, 352, 590};

const Rect kBackJL{448, 305, 632, 348};
const Rect kBuyJL{448, 250, 632, 295};
const Rect kYesJL{448, 250, 536, 295}, kNoJL{544, 250, 632, 295};

const Rect kBackRL{324, 300, 632, 348};

void headerJ(Renderer& r, const App& app, const char* title, const char* sub, bool land) {
    if (land) {
        r.rect(0, 0, 640, 44, kUiPanel);
        r.text(8, 8, title, 2, kUiGold);
        r.text(8, 28, sub, 1, kUiText);
        const std::string m = money(app.career.money);
        r.text(632 - r.textWidth(m, 2), 8, m, 2, {0.4f, 1.0f, 0.5f});
    } else {
        r.rect(0, 0, 360, 58, kUiPanel);
        r.text(8, 8, title, 2, kUiGold);
        r.text(8, 32, sub, 1, kUiText);
        const std::string m = money(app.career.money);
        r.text(352 - r.textWidth(m, 2), 8, m, 2, {0.4f, 1.0f, 0.5f});
    }
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
    const bool land = app_.settings.menuLandscape;
    r.begin(land ? 640 : 360, land ? 360 : 640, kUiBg);
    headerJ(r, app_, "HURDALIK", "KELEPIR AMA DOKUK. HER YARISTAN SONRA YENI ARACLAR", land);
    char b[96];

    if (land) {
        // ==========================================
        // YATAY 640x360 NATIVE HURDALIK
        // ==========================================
        for (int i = 0; i < (int)offers_.size(); ++i) {
            const JunkCar& j = offers_[i];
            const VehicleDef& v = *findVehicle(j.carId);
            const int col = i % 2;
            const int row = i / 2;
            const float x = 8.0f + col * 216.0f;
            const float y = 50.0f + row * 96.0f;

            r.rect(x, y, x + 210.0f, y + 90.0f, i == sel_ ? Color{0.2f, 0.24f, 0.36f} : kUiPanel);
            if (i == sel_) r.rect(x, y, x + 4.0f, y + 90.0f, kUiOrange);

            const std::string p = money(j.price);
            r.textFit(x + 8.0f, y + 4.0f, upper(v.fullName()), 1, 190.0f - r.textWidth(p, 2), {1, 1, 1});   // fiyatla cakismaz
            std::snprintf(b, sizeof b, "%d  %s", v.year, money(carPrice(v)).c_str());
            r.text(x + 8.0f, y + 16.0f, b, 1, kUiDim);

            r.text(x + 204.0f - r.textWidth(p, 2), y + 5.0f, p, 2, app_.career.money >= j.price ? kUiGold : kUiDim);

            // Ariza
            std::string d;
            if (j.engineWear >= 1.0) d += "MOTOR OLU  "; else if (j.engineWear > 0.3) d += "MOTOR YORGUN  ";
            if (j.gearboxBroken) d += "SANZ. KIRIK  ";
            if (j.axleBroken) d += "AKS KIRIK";
            r.textFit(x + 8.0f, y + 30.0f, d.empty() ? "MEKANIK CALISIYOR" : d, 1, 194.0f, d.empty() ? Color{0.6f, 0.8f, 0.6f} : Color{1.0f, 0.4f, 0.3f});

            // 6 Bilesen cubugu
            const double w[6] = {j.tune.wearEngine, j.tune.wearTires, j.tune.wearBrakes, j.tune.wearSusp, j.tune.wearBody, j.tune.wearElec};
            static const char* n[6] = {"KOMP", "LAST", "FREN", "SUSP", "PAS", "ELEK"};
            for (int k = 0; k < 6; ++k) {
                const float bx = x + 8.0f + k * 32.0f;
                r.text(bx, y + 46.0f, n[k], 1, kUiDim);
                r.rect(bx, y + 58.0f, bx + 28.0f, y + 64.0f, {0.15f, 0.15f, 0.18f});
                r.rect(bx, y + 58.0f, bx + 28.0f * (float)w[k], y + 64.0f, dmgColor(w[k]));
            }
        }

        // Sag Detay / Satin Alma Paneli
        r.rect(448, 50, 632, 235, kUiPanel);
        if (sel_ >= 0 && sel_ < (int)offers_.size()) {
            const JunkCar& j = offers_[sel_];
            const VehicleDef& v = *findVehicle(j.carId);
            r.textFit(458, 60, upper(v.fullName()), 2, 164.0f, kUiGold);
            std::snprintf(b, sizeof b, "MODEL YILI: %d", v.year);
            r.text(458, 86, b, 1, kUiText);
            std::snprintf(b, sizeof b, "FABRIKA DEGERI: %s", money(carPrice(v)).c_str());
            r.text(458, 104, b, 1, kUiText);
            std::snprintf(b, sizeof b, "HURDA FIYATI: %s", money(j.price).c_str());
            r.text(458, 122, b, 2, kUiGreen);

            r.text(458, 150, "RESTORASYON ILE", 1, kUiDim);
            r.text(458, 166, "DEGERI %100 GERI KAZANILIR", 1, kUiDim);

            if (confirm_) {
                button(r, kYesJL, "AL " + money(j.price), kUiGreen, 2);
                button(r, kNoJL, "VAZGEC", kUiRed, 2);
            } else {
                button(r, kBuyJL, "SATIN AL " + money(j.price), app_.career.money >= j.price ? kUiGreen : Color{0.25f, 0.25f, 0.28f}, 2);
            }
        } else {
            r.textCentered(540, 120, "LISTEDEN BIR", 2, kUiDim);
            r.textCentered(540, 144, "ARAC SEC", 2, kUiDim);
        }

        button(r, kBackJL, "< GALERI", kUiBtn, 2);

        if (msgT_ > 0) {
            r.rect(140, 160, 500, 200, {0.02f, 0.02f, 0.04f, 0.95f});
            r.textCentered(320, 175, msg_, 2, kUiGold);
        }
    } else {
        // ==========================================
        // DIKEY 360x640 KLASIK HURDALIK
        // ==========================================
        for (int i = 0; i < (int)offers_.size(); ++i) {
            const JunkCar& j = offers_[i];
            const VehicleDef& v = *findVehicle(j.carId);
            const float y = 64.0f + i * 80.0f;
            r.rect(8, y, 352, y + 76, i == sel_ ? Color{0.2f, 0.24f, 0.36f} : kUiPanel);
            if (i == sel_) r.rect(8, y, 12, y + 76, kUiOrange);
            r.textFit(16, y + 5, upper(v.fullName()), 1, 330.0f, {1, 1, 1});
            std::snprintf(b, sizeof b, "%d  DEGERI %s", v.year, money(carPrice(v)).c_str());
            r.text(16, y + 17, b, 1, kUiDim);
            const std::string p = money(j.price);
            r.text(344 - r.textWidth(p, 2), y + 6, p, 2, app_.career.money >= j.price ? kUiGold : kUiDim);

            std::string d;
            if (j.engineWear >= 1.0) d += "MOTOR OLU  "; else if (j.engineWear > 0.3) d += "MOTOR YORGUN  ";
            if (j.gearboxBroken) d += "SANZ. KIRIK  ";
            if (j.axleBroken) d += "AKS KIRIK";
            r.text(16, y + 30, d.empty() ? "MEKANIK CALISIYOR" : d, 1, d.empty() ? Color{0.6f, 0.8f, 0.6f} : Color{1.0f, 0.4f, 0.3f});

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
                button(r, kYesJP, "AL " + money(j.price), kUiGreen, 2);
                button(r, kNoJP, "VAZGEC", kUiRed, 2);
            } else button(r, kBuyJP, "SATIN AL " + money(j.price), app_.career.money >= j.price ? kUiGreen : Color{0.25f, 0.25f, 0.28f}, 2);
        } else r.textCentered(180, 562, "BIR ARAC SEC", 1, kUiDim);
        button(r, kBackJP, "< GALERI", kUiBtn, 2);
        if (msgT_ > 0) { r.rect(0, 500, 360, 528, {0.02f, 0.02f, 0.04f, 0.92f}); r.textCentered(180, 507, msg_, 2, kUiGold); }
    }
}

void JunkyardScreen::pointerDown(int, float x, float y) {
    const bool land = app_.settings.menuLandscape;
    const Rect& backBtn = land ? kBackJL : kBackJP;
    const Rect& buyBtn = land ? kBuyJL : kBuyJP;
    const Rect& yesBtn = land ? kYesJL : kYesJP;
    const Rect& noBtn = land ? kNoJL : kNoJP;

    if (confirm_ && sel_ >= 0) {
        if (yesBtn.hit(x, y)) {
            std::string why;
            if (app_.career.buyJunk(offers_[sel_], &why)) {
                app_.junkSalt++;
                app_.saveCareer();
                app_.goRestore();
                return;
            }
            msg_ = why; msgT_ = 1.6; confirm_ = false; return;
        }
        if (noBtn.hit(x, y)) { confirm_ = false; return; }
    }

    if (backBtn.hit(x, y)) { app_.goGallery(); return; }

    if (land) {
        for (int i = 0; i < (int)offers_.size(); ++i) {
            const int col = i % 2;
            const int row = i / 2;
            const float cx = 8.0f + col * 216.0f;
            const float cy = 50.0f + row * 96.0f;
            if (Rect{cx, cy, cx + 210.0f, cy + 90.0f}.hit(x, y)) { sel_ = i; confirm_ = false; return; }
        }
    } else {
        for (int i = 0; i < (int)offers_.size(); ++i) {
            const float cy = 64.0f + i * 80.0f;
            if (Rect{8, cy, 352, cy + 76}.hit(x, y)) { sel_ = i; confirm_ = false; return; }
        }
    }

    if (sel_ >= 0 && buyBtn.hit(x, y)) {
        if (app_.career.money >= offers_[sel_].price) confirm_ = true;
        else { msg_ = "PARA YETMIYOR"; msgT_ = 1.4; }
    }
}

void JunkyardScreen::key(Key k, bool down) {
    if (!down) return;
    if (k == Key::Back) { if (confirm_) confirm_ = false; else app_.goGallery(); return; }
    if (k == Key::Throttle && sel_ > 0) { --sel_; confirm_ = false; }
    else if (k == Key::Brake && sel_ + 1 < (int)offers_.size()) { ++sel_; confirm_ = false; }
    if (k == Key::Enter && sel_ >= 0) {
        const Rect& yesBtn = app_.settings.menuLandscape ? kYesJL : kYesJP;
        if (confirm_) pointerDown(0, yesBtn.cx(), yesBtn.cy()); else confirm_ = true;
    }
}

// ================================================================== RESTORASYON
void RestoreScreen::render(Renderer& r) {
    const OwnedCar& oc = app_.career.car();
    const VehicleDef& v = *findVehicle(oc.carId);
    const bool land = app_.settings.menuLandscape;
    r.begin(land ? 640 : 360, land ? 360 : 640, kUiBg);
    headerJ(r, app_, "RESTORASYON", upper(v.fullName()).substr(0, 40).c_str(), land);
    char b[64];

    if (land) {
        // ==========================================
        // YATAY 640x360 NATIVE RESTORASYON
        // ==========================================
        for (int k = 0; k < kRestoreParts; ++k) {
            const bool isLeft = k < 4;
            const float x = isLeft ? 8.0f : 324.0f;
            const float y = 50.0f + (isLeft ? k : (k - 4)) * 56.0f;
            const double d = restoreDamage(oc, k);
            const long cost = restoreStepCost(oc, k);

            r.rect(x, y, x + 308.0f, y + 50.0f, kUiPanel);
            r.textFit(x + 10.0f, y + 5.0f, restoreName(k), 2, 176.0f, {1, 1, 1});

            // Hasar Cubugu
            r.rect(x + 10.0f, y + 26.0f, x + 180.0f, y + 34.0f, {0.15f, 0.15f, 0.18f});
            r.rect(x + 10.0f, y + 26.0f, x + 10.0f + 170.0f * (float)std::min(1.0, d), y + 34.0f, dmgColor(d));
            std::snprintf(b, sizeof b, "HASAR %%%.0f", d * 100);
            r.text(x + 10.0f, y + 36.0f, b, 1, dmgColor(d));

            // Onarim butonu
            const Rect bt{x + 195.0f, y + 8.0f, x + 300.0f, y + 42.0f};
            if (cost <= 0) {
                r.rect(bt.x0, bt.y0, bt.x1, bt.y1, {0.12f, 0.3f, 0.16f});
                r.textCentered(bt.cx(), bt.cy() - 4, "SAGLAM", 1, {0.4f, 1.0f, 0.5f});
            } else {
                button(r, bt, money(cost), app_.career.money >= cost ? kUiOrange : Color{0.25f, 0.25f, 0.28f}, 1);
            }
        }

        // Sol Alt Subtext
        r.text(12, 276, "HER ISCILIK ADIMI HASARI YARIYA INDIRIR", 1, kUiDim);
        r.text(12, 290, "TAM TOPARLAMAK ICIN BIRKAC KEZ UGRAS", 1, kUiDim);

        // Sag Alt Satis Degeri + Buton
        std::snprintf(b, sizeof b, "SATIS DEGERI: %s", money(sellPrice(oc)).c_str());
        r.text(330, 240, b, 2, kUiGold);
        if (!oc.raceable()) r.text(330, 264, "BU HALIYLE YARISAMAZ (MOTOR/FREN HASARLI)", 1, {1.0f, 0.4f, 0.3f});
        else r.text(330, 264, "YARISA HAZIR", 1, {0.4f, 1.0f, 0.5f});

        button(r, kBackRL, "< GARAJ", kUiBtn, 2);

        if (msgT_ > 0) {
            r.rect(140, 160, 500, 200, {0.02f, 0.02f, 0.04f, 0.95f});
            r.textCentered(320, 175, msg_, 2, kUiGold);
        }
    } else {
        // ==========================================
        // DIKEY 360x640 KLASIK RESTORASYON
        // ==========================================
        r.text(8, 64, "HER ISCILIK ADIMI HASARI YARIYA INDIRIR", 1, kUiDim);
        r.text(8, 76, "TAM TOPARLAMAK ICIN BIRKAC KEZ UGRAS", 1, kUiDim);
        for (int k = 0; k < kRestoreParts; ++k) {
            const float y = 94.0f + k * 62.0f;
            const double d = restoreDamage(oc, k);
            const long cost = restoreStepCost(oc, k);
            r.rect(8, y, 352, y + 58, kUiPanel);
            r.textFit(16, y + 6, restoreName(k), 2, 190.0f, {1, 1, 1});
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
        button(r, kBackJP, "< GARAJ", kUiBtn, 2);
        if (msgT_ > 0) { r.rect(0, 500, 360, 528, {0.02f, 0.02f, 0.04f, 0.92f}); r.textCentered(180, 507, msg_, 2, kUiGold); }
    }
}

void RestoreScreen::pointerDown(int, float x, float y) {
    const bool land = app_.settings.menuLandscape;
    const Rect& backBtn = land ? kBackRL : kBackJP;
    if (backBtn.hit(x, y)) { app_.goGarage(); return; }

    for (int k = 0; k < kRestoreParts; ++k) {
        Rect bt;
        if (land) {
            const bool isLeft = k < 4;
            const float bx = isLeft ? 8.0f : 324.0f;
            const float by = 50.0f + (isLeft ? k : (k - 4)) * 56.0f;
            bt = {bx + 195.0f, by + 8.0f, bx + 300.0f, by + 42.0f};
        } else {
            const float yy = 94.0f + k * 62.0f;
            bt = {220, yy + 10, 344, yy + 48};
        }

        if (bt.hit(x, y)) {
            std::string why;
            if (app_.career.restoreStep(k, &why)) {
                app_.saveCareer();
                msg_ = std::string(restoreName(k)) + " TAMAM";
                msgT_ = 1.4;
                app_.haptic(30, 160);
            } else {
                msg_ = why; msgT_ = 1.6;
            }
            return;
        }
    }
}

void RestoreScreen::key(Key k, bool down) { if (down && k == Key::Back) app_.goGarage(); }

} // namespace zk
