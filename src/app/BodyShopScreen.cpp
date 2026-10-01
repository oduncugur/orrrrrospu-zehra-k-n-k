// ZEHRA KINIK - Boyahane: renk (16), cila (parlak / metalik / mat / sedef), serit (orta / cift / yan) ve rengi, jant rengi.
// Taslak onizlemede doner; UYGULA yalniz degisenlerin parasini alir.
#include "Screens.h"
#include "Ui.h"
#include "app/Looks.h"
#include "garage/VehicleCatalog.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace zk {

namespace {
constexpr float kSwW = 41, kSwH = 22;
const Rect kFactoryPaint{8, 300, 84, 322};
const Rect kApply{8, 556, 352, 594};
const Rect kBack{8, 600, 352, 634};
constexpr float kPaintY = 326, kFinishY = 392, kStripeY = 438, kStripeColY = 464, kRimY = 510;
constexpr int kPricePaint = 900, kPriceFinish[4] = {0, 500, 700, 1600}, kPriceStripe = 350, kPriceRim = 300;

Rect swatch(int i, float y0) { return {8.0f + (i % 8) * (kSwW + 2), y0 + (i / 8) * (kSwH + 2), 8.0f + (i % 8) * (kSwW + 2) + kSwW, y0 + (i / 8) * (kSwH + 2) + kSwH}; }
Rect choice(int i, float y0) { return {8.0f + i * 87.0f, y0, 8.0f + i * 87.0f + 83.0f, y0 + 22}; }
constexpr int kStripePal[8] = {0, 1, 2, 4, 6, 7, 11, 15};               // beyaz siyah gumus kirmizi turuncu sari mavi altin
constexpr int kRimPal[8] = {-1, 1, 2, 3, 0, 15, 4, 11};                 // fabrika siyah gumus antrasit beyaz altin kirmizi mavi
Color colOf(int i) { float c[3]; rgbOf(paintPalette()[i].rgb, c); return {c[0], c[1], c[2]}; }
} // namespace

BodyShopScreen::BodyShopScreen(App& app) : app_(app) {
    const OwnedCar& oc = app_.career.car();
    paint_ = oc.paint; finish_ = oc.finish; stripe_ = oc.stripe; stripeCol_ = oc.stripeCol; rimCol_ = oc.rimCol;
}

int BodyShopScreen::cost() const {
    const OwnedCar& oc = app_.career.car();
    int c = 0;
    if (paint_ != oc.paint || finish_ != oc.finish) c += kPricePaint + kPriceFinish[std::clamp(finish_, 0, 3)];   // yeniden boya
    if (stripe_ != oc.stripe || (stripe_ && stripeCol_ != oc.stripeCol)) c += stripe_ ? kPriceStripe : 100;
    if (rimCol_ != oc.rimCol) c += kPriceRim;
    return c;
}

void BodyShopScreen::render(Renderer& r) {
    const OwnedCar& oc = app_.career.car();
    const VehicleDef& v = *findVehicle(oc.carId);
    r.begin(360, 640, kUiBg);
    studio(r, 60, 296, 230);
    r.rect(0, 0, 360, 58, kUiPanel);
    r.text(8, 8, "BOYAHANE", 2, kUiGold);
    r.text(8, 32, upper(v.model).substr(0, 24), 1, kUiText);
    const std::string m = money(app_.career.money);
    r.text(352 - r.textWidth(m, 2), 8, m, 2, {0.4f, 1.0f, 0.5f});
    // Onizleme: taslak gorunum
    OwnedCar d = oc;
    d.paint = paint_; d.finish = finish_; d.stripe = stripe_; d.stripeCol = stripeCol_; d.rimCol = rimCol_;
    const float L = (float)v.lengthM;
    const Mat4 proj = matPerspective(0.75f, 360.0f / 230.0f, 0.1f, 50.0f);
    const Mat4 view = matLookAt(0.0f, 1.0f + 0.2f * L, 1.05f * L + 1.0f, 0.0f, 0.55f, 0.0f);
    r.setCarLook(lookOf(d));
    r.drawCar(v.id, 0, 64, 360, 230, proj, view, matRotY(spin_));

    r.text(92, 305, "RENK", 1, kUiDim);
    button(r, kFactoryPaint, "FABRIKA", paint_ < 0 ? kUiOrange : kUiBtn, 1);
    for (int i = 0; i < kPaints; ++i) {
        const Rect s = swatch(i, kPaintY);
        if (i == paint_) r.rect(s.x0 - 2, s.y0 - 2, s.x1 + 2, s.y1 + 2, {1, 1, 1});
        r.rect(s.x0, s.y0, s.x1, s.y1, colOf(i));
    }
    r.text(140, 305, paint_ >= 0 ? paintPalette()[paint_].name : "", 1, {1, 1, 1});
    r.text(8, kFinishY - 12, "CILA", 1, kUiDim);
    for (int i = 0; i < 4; ++i) {
        const Rect c = choice(i, kFinishY);
        button(r, c, finishName(i), i == finish_ ? kUiOrange : kUiBtn, 1);
    }
    r.text(8, kStripeY - 12, "SERIT", 1, kUiDim);
    for (int i = 0; i < 4; ++i) button(r, choice(i, kStripeY), stripeName(i), i == stripe_ ? kUiOrange : kUiBtn, 1);
    for (int i = 0; i < 8; ++i) {
        const Rect s = swatch(i, kStripeColY);
        const int ci = kStripePal[i];
        if (stripe_ && ci == stripeCol_) r.rect(s.x0 - 2, s.y0 - 2, s.x1 + 2, s.y1 + 2, {1, 1, 1});
        r.rect(s.x0, s.y0, s.x1, s.y1, stripe_ ? colOf(ci) : Color{0.2f, 0.2f, 0.22f});
    }
    r.text(8, kRimY - 12, "JANT", 1, kUiDim);
    for (int i = 0; i < 8; ++i) {
        const Rect s = swatch(i, kRimY);
        const int ci = kRimPal[i];
        if (ci == rimCol_) r.rect(s.x0 - 2, s.y0 - 2, s.x1 + 2, s.y1 + 2, {1, 1, 1});
        if (ci < 0) { r.rect(s.x0, s.y0, s.x1, s.y1, kUiBtn); r.textCentered(s.cx(), s.y0 + 7, "FAB", 1, {1, 1, 1}); }
        else r.rect(s.x0, s.y0, s.x1, s.y1, colOf(ci));
    }
    const int c = cost();
    char b[64];
    if (c > 0) std::snprintf(b, sizeof b, "UYGULA  %s", money(c).c_str());
    else std::snprintf(b, sizeof b, "DEGISIKLIK YOK");
    button(r, kApply, b, c > 0 ? (app_.career.money >= c ? kUiOrange : Color{0.4f, 0.15f, 0.15f}) : Color{0.25f, 0.25f, 0.28f}, 2);
    button(r, kBack, "< GARAJ", kUiBtn, 2);
    if (msgT_ > 0) { r.rect(0, 250, 360, 280, {0.02f, 0.02f, 0.04f, 0.88f}); r.textCentered(180, 258, msg_, 2, kUiGold); }
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
    if (kBack.hit(x, y)) { app_.goGarage(); return; }
    if (kApply.hit(x, y)) { apply(); return; }
    if (kFactoryPaint.hit(x, y)) { paint_ = -1; return; }
    for (int i = 0; i < kPaints; ++i) if (swatch(i, kPaintY).hit(x, y)) { paint_ = i; return; }
    for (int i = 0; i < 4; ++i) if (choice(i, kFinishY).hit(x, y)) { finish_ = i; return; }
    for (int i = 0; i < 4; ++i) if (choice(i, kStripeY).hit(x, y)) { stripe_ = i; return; }
    for (int i = 0; i < 8; ++i) if (swatch(i, kStripeColY).hit(x, y)) { stripeCol_ = kStripePal[i]; if (!stripe_) stripe_ = 1; return; }
    for (int i = 0; i < 8; ++i) if (swatch(i, kRimY).hit(x, y)) { rimCol_ = kRimPal[i]; return; }
}

void BodyShopScreen::key(Key k, bool down) {
    if (!down) return;
    if (k == Key::Back) app_.goGarage();
    else if (k == Key::Enter) apply();
    else if (k == Key::Left) paint_ = paint_ <= -1 ? kPaints - 1 : paint_ - 1;
    else if (k == Key::Right) paint_ = paint_ >= kPaints - 1 ? -1 : paint_ + 1;
}

} // namespace zk
