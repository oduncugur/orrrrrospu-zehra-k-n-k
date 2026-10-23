// ZEHRA KINIK - Boyahane: renk (16), cila (parlak / metalik / mat / sedef), serit (orta / cift / yan) ve rengi, jant rengi.
// Dikey 360x640 ve Yatay 640x360 adaptif tasarim.
#include "Screens.h"
#include "Ui.h"
#include "app/Looks.h"
#include "garage/VehicleCatalog.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace zk {

namespace {
// Dikey 360x640 koordinatlar
constexpr float kSwWP = 41, kSwHP = 22;
const Rect kFactoryPaintP{8, 300, 84, 322};
const Rect kApplyP{8, 556, 352, 594};
const Rect kBackP{8, 600, 352, 634};
constexpr float kPaintYP = 326, kFinishYP = 392, kStripeYP = 438, kStripeColYP = 464, kRimYP = 510;

Rect swatchP(int i, float y0) { return {8.0f + (i % 8) * (kSwWP + 2), y0 + (i / 8) * (kSwHP + 2), 8.0f + (i % 8) * (kSwWP + 2) + kSwWP, y0 + (i / 8) * (kSwHP + 2) + kSwHP}; }
Rect choiceP(int i, float y0) { return {8.0f + i * 87.0f, y0, 8.0f + i * 87.0f + 83.0f, y0 + 22}; }

// Yatay 640x360 koordinatlar
const Rect kFactoryPaintL{350, 48, 420, 68};
constexpr float kPaintYL = 72, kFinishYL = 126, kStripeYL = 168, kStripeColYL = 192, kRimYL = 228;
const Rect kApplyL{350, 268, 632, 304};
const Rect kBackL{350, 310, 632, 348};

Rect swatchL(int i, float y0) {
    constexpr float w = 33.0f, h = 18.0f;
    return {350.0f + (i % 8) * (w + 2.5f), y0 + (i / 8) * (h + 2.0f), 350.0f + (i % 8) * (w + 2.5f) + w, y0 + (i / 8) * (h + 2.0f) + h};
}
Rect choiceL(int i, float y0) {
    constexpr float w = 68.0f;
    return {350.0f + i * (w + 3.0f), y0, 350.0f + i * (w + 3.0f) + w, y0 + 19.0f};
}

constexpr int kPricePaint = 900, kPriceFinish[4] = {0, 500, 700, 1600}, kPriceStripe = 350, kPriceRim = 300;
constexpr int kStripePal[8] = {0, 1, 2, 4, 6, 7, 11, 15};
constexpr int kRimPal[8] = {-1, 1, 2, 3, 0, 15, 4, 11};
Color colOf(int i) { float c[3]; rgbOf(paintPalette()[i].rgb, c); return {c[0], c[1], c[2]}; }
} // namespace

BodyShopScreen::BodyShopScreen(App& app) : app_(app) {
    const OwnedCar& oc = app_.career.car();
    paint_ = oc.paint; finish_ = oc.finish; stripe_ = oc.stripe; stripeCol_ = oc.stripeCol; rimCol_ = oc.rimCol;
}

int BodyShopScreen::cost() const {
    const OwnedCar& oc = app_.career.car();
    int c = 0;
    if (paint_ != oc.paint || finish_ != oc.finish) c += kPricePaint + kPriceFinish[std::clamp(finish_, 0, 3)];
    if (stripe_ != oc.stripe || (stripe_ && stripeCol_ != oc.stripeCol)) c += stripe_ ? kPriceStripe : 100;
    if (rimCol_ != oc.rimCol) c += kPriceRim;
    return c;
}

void BodyShopScreen::render(Renderer& r) {
    const OwnedCar& oc = app_.career.car();
    const VehicleDef& v = *findVehicle(oc.carId);
    const bool land = app_.settings.menuLandscape;
    r.begin(land ? 640 : 360, land ? 360 : 640, kUiBg);

    OwnedCar d = oc;
    d.paint = paint_; d.finish = finish_; d.stripe = stripe_; d.stripeCol = stripeCol_; d.rimCol = rimCol_;
    const float L = (float)v.lengthM;
    r.setCarLook(lookOf(d));

    const int c = cost();
    char b[64];
    if (c > 0) std::snprintf(b, sizeof b, "UYGULA  %s", money(c).c_str());
    else std::snprintf(b, sizeof b, "DEGISIKLIK YOK");

    if (land) {
        // ==========================================
        // YATAY 640x360 NATIVE BOYAHANE
        // ==========================================
        r.rect(0, 0, 640, 42, kUiPanel);
        r.text(8, 6, "BOYAHANE", 2, kUiGold);
        r.text(8, 25, upper(v.model).substr(0, 24), 1, kUiText);
        const std::string m = money(app_.career.money);
        r.text(632 - r.textWidth(m, 2), 8, m, 2, {0.4f, 1.0f, 0.5f});

        // Sol Kisim: 3B Dönen Stüdyo
        studio(r, 48, 336, 300);
        const Mat4 proj = matPerspective(0.70f, 336.0f / 300.0f, 0.1f, 50.0f);
        const Mat4 view = matLookAt(0.0f, 1.0f + 0.2f * L, 1.05f * L + 1.0f, 0.0f, 0.55f, 0.0f);
        r.drawCar(v.id, 8, 48, 336, 300, proj, view, matRotY(spin_));

        // Sag Kisim: Kontroller
        button(r, kFactoryPaintL, "FABRIKA", paint_ < 0 ? kUiOrange : kUiBtn, 1);
        r.text(428, 52, "RENK:", 1, kUiDim);
        r.text(470, 52, paint_ >= 0 ? paintPalette()[paint_].name : "FABRIKA RENGI", 1, {1, 1, 1});

        for (int i = 0; i < kPaints; ++i) {
            const Rect s = swatchL(i, kPaintYL);
            if (i == paint_) r.rect(s.x0 - 2, s.y0 - 2, s.x1 + 2, s.y1 + 2, {1, 1, 1});
            r.rect(s.x0, s.y0, s.x1, s.y1, colOf(i));
        }

        r.text(350, kFinishYL - 10, "CILA", 1, kUiDim);
        for (int i = 0; i < 4; ++i) {
            const Rect ch = choiceL(i, kFinishYL);
            button(r, ch, finishName(i), i == finish_ ? kUiOrange : kUiBtn, 1);
        }

        r.text(350, kStripeYL - 10, "SERIT", 1, kUiDim);
        for (int i = 0; i < 4; ++i) button(r, choiceL(i, kStripeYL), stripeName(i), i == stripe_ ? kUiOrange : kUiBtn, 1);
        for (int i = 0; i < 8; ++i) {
            const Rect s = swatchL(i, kStripeColYL);
            const int ci = kStripePal[i];
            if (stripe_ && ci == stripeCol_) r.rect(s.x0 - 2, s.y0 - 2, s.x1 + 2, s.y1 + 2, {1, 1, 1});
            r.rect(s.x0, s.y0, s.x1, s.y1, stripe_ ? colOf(ci) : Color{0.2f, 0.2f, 0.22f});
        }

        r.text(350, kRimYL - 10, "JANT RENGI", 1, kUiDim);
        for (int i = 0; i < 8; ++i) {
            const Rect s = swatchL(i, kRimYL);
            const int ci = kRimPal[i];
            if (ci == rimCol_) r.rect(s.x0 - 2, s.y0 - 2, s.x1 + 2, s.y1 + 2, {1, 1, 1});
            if (ci < 0) { r.rect(s.x0, s.y0, s.x1, s.y1, kUiBtn); r.textCentered(s.cx(), s.y0 + 5, "FAB", 1, {1, 1, 1}); }
            else r.rect(s.x0, s.y0, s.x1, s.y1, colOf(ci));
        }

        button(r, kApplyL, b, c > 0 ? (app_.career.money >= c ? kUiOrange : Color{0.4f, 0.15f, 0.15f}) : Color{0.25f, 0.25f, 0.28f}, 2);
        button(r, kBackL, "< GARAJ", kUiBtn, 2);

        if (msgT_ > 0) {
            r.rect(140, 160, 500, 200, {0.02f, 0.02f, 0.04f, 0.95f});
            r.textCentered(320, 175, msg_, 2, kUiGold);
        }
    } else {
        // ==========================================
        // DIKEY 360x640 KLASIK BOYAHANE
        // ==========================================
        studio(r, 60, 296, 230);
        r.rect(0, 0, 360, 58, kUiPanel);
        r.text(8, 8, "BOYAHANE", 2, kUiGold);
        r.text(8, 32, upper(v.model).substr(0, 24), 1, kUiText);
        const std::string m = money(app_.career.money);
        r.text(352 - r.textWidth(m, 2), 8, m, 2, {0.4f, 1.0f, 0.5f});

        const Mat4 proj = matPerspective(0.75f, 360.0f / 230.0f, 0.1f, 50.0f);
        const Mat4 view = matLookAt(0.0f, 1.0f + 0.2f * L, 1.05f * L + 1.0f, 0.0f, 0.55f, 0.0f);
        r.drawCar(v.id, 0, 64, 360, 230, proj, view, matRotY(spin_));

        r.text(92, 305, "RENK", 1, kUiDim);
        button(r, kFactoryPaintP, "FABRIKA", paint_ < 0 ? kUiOrange : kUiBtn, 1);
        for (int i = 0; i < kPaints; ++i) {
            const Rect s = swatchP(i, kPaintYP);
            if (i == paint_) r.rect(s.x0 - 2, s.y0 - 2, s.x1 + 2, s.y1 + 2, {1, 1, 1});
            r.rect(s.x0, s.y0, s.x1, s.y1, colOf(i));
        }
        r.text(140, 305, paint_ >= 0 ? paintPalette()[paint_].name : "", 1, {1, 1, 1});
        r.text(8, kFinishYP - 12, "CILA", 1, kUiDim);
        for (int i = 0; i < 4; ++i) {
            const Rect ch = choiceP(i, kFinishYP);
            button(r, ch, finishName(i), i == finish_ ? kUiOrange : kUiBtn, 1);
        }
        r.text(8, kStripeYP - 12, "SERIT", 1, kUiDim);
        for (int i = 0; i < 4; ++i) button(r, choiceP(i, kStripeYP), stripeName(i), i == stripe_ ? kUiOrange : kUiBtn, 1);
        for (int i = 0; i < 8; ++i) {
            const Rect s = swatchP(i, kStripeColYP);
            const int ci = kStripePal[i];
            if (stripe_ && ci == stripeCol_) r.rect(s.x0 - 2, s.y0 - 2, s.x1 + 2, s.y1 + 2, {1, 1, 1});
            r.rect(s.x0, s.y0, s.x1, s.y1, stripe_ ? colOf(ci) : Color{0.2f, 0.2f, 0.22f});
        }
        r.text(8, kRimYP - 12, "JANT", 1, kUiDim);
        for (int i = 0; i < 8; ++i) {
            const Rect s = swatchP(i, kRimYP);
            const int ci = kRimPal[i];
            if (ci == rimCol_) r.rect(s.x0 - 2, s.y0 - 2, s.x1 + 2, s.y1 + 2, {1, 1, 1});
            if (ci < 0) { r.rect(s.x0, s.y0, s.x1, s.y1, kUiBtn); r.textCentered(s.cx(), s.y0 + 7, "FAB", 1, {1, 1, 1}); }
            else r.rect(s.x0, s.y0, s.x1, s.y1, colOf(ci));
        }
        button(r, kApplyP, b, c > 0 ? (app_.career.money >= c ? kUiOrange : Color{0.4f, 0.15f, 0.15f}) : Color{0.25f, 0.25f, 0.28f}, 2);
        button(r, kBackP, "< GARAJ", kUiBtn, 2);
        if (msgT_ > 0) { r.rect(0, 250, 360, 280, {0.02f, 0.02f, 0.04f, 0.88f}); r.textCentered(180, 258, msg_, 2, kUiGold); }
    }
}

void BodyShopScreen::apply() {
    const int c = cost();
    if (c <= 0) return;
    Career& k = app_.career;
    if (k.money < c) { msg_ = "PARA YETMIYOR"; msgT_ = 1.6; return; }
    k.money -= c;
    OwnedCar& oc = k.cars[k.current];
    oc.paint = paint_; oc.finish = finish_; oc.stripe = stripe_; oc.stripeCol = stripeCol_; oc.rimCol = rimCol_;
    app_.saveCareer();
    msg_ = "HAZIR! -" + money(c); msgT_ = 1.8;
}

void BodyShopScreen::pointerDown(int, float x, float y) {
    const bool land = app_.settings.menuLandscape;
    const Rect& backBtn = land ? kBackL : kBackP;
    const Rect& applyBtn = land ? kApplyL : kApplyP;
    const Rect& facBtn = land ? kFactoryPaintL : kFactoryPaintP;

    if (backBtn.hit(x, y)) { app_.goGarage(); return; }
    if (applyBtn.hit(x, y)) { apply(); return; }
    if (facBtn.hit(x, y)) { paint_ = -1; return; }

    if (land) {
        for (int i = 0; i < kPaints; ++i) if (swatchL(i, kPaintYL).hit(x, y)) { paint_ = i; return; }
        for (int i = 0; i < 4; ++i) if (choiceL(i, kFinishYL).hit(x, y)) { finish_ = i; return; }
        for (int i = 0; i < 4; ++i) if (choiceL(i, kStripeYL).hit(x, y)) { stripe_ = i; return; }
        for (int i = 0; i < 8; ++i) if (swatchL(i, kStripeColYL).hit(x, y)) { stripeCol_ = kStripePal[i]; if (!stripe_) stripe_ = 1; return; }
        for (int i = 0; i < 8; ++i) if (swatchL(i, kRimYL).hit(x, y)) { rimCol_ = kRimPal[i]; return; }
    } else {
        for (int i = 0; i < kPaints; ++i) if (swatchP(i, kPaintYP).hit(x, y)) { paint_ = i; return; }
        for (int i = 0; i < 4; ++i) if (choiceP(i, kFinishYP).hit(x, y)) { finish_ = i; return; }
        for (int i = 0; i < 4; ++i) if (choiceP(i, kStripeYP).hit(x, y)) { stripe_ = i; return; }
        for (int i = 0; i < 8; ++i) if (swatchP(i, kStripeColYP).hit(x, y)) { stripeCol_ = kStripePal[i]; if (!stripe_) stripe_ = 1; return; }
        for (int i = 0; i < 8; ++i) if (swatchP(i, kRimYP).hit(x, y)) { rimCol_ = kRimPal[i]; return; }
    }
}

void BodyShopScreen::key(Key k, bool down) {
    if (!down) return;
    if (k == Key::Back) app_.goGarage();
    else if (k == Key::Enter) apply();
    else if (k == Key::Left) paint_ = paint_ <= -1 ? kPaints - 1 : paint_ - 1;
    else if (k == Key::Right) paint_ = paint_ >= kPaints - 1 ? -1 : paint_ + 1;
}

} // namespace zk
