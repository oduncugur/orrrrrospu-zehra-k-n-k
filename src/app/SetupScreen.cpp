// ZEHRA KINIK - Kurulum ekrani: lastik basinci, ayarli suspansiyon (yukseklik / yay / amortisor / viraj demirleri),
// LSD on yuku, NOS memesi; DRAG / YOL / PIST profilleri. Ayarlar parcalarla acilir (Tune::set*, PartTables clampSetup).
#include "Screens.h"
#include "Ui.h"
#include "sim/PartTables.h"

#include <algorithm>
#include <cstdio>

namespace zk {

namespace {
enum Row { RPsi, RRide, RSpring, RDamp, RArbF, RArbR, RPreload, RNos, RCount };
const char* const kRowName[RCount] = {"LASTIK BASINCI", "YUKSEKLIK", "YAY SERTLIGI", "AMORTISOR", "ON VIRAJ DEMIRI", "ARKA VIRAJ DEMIRI",
                                      "LSD ON YUKU", "NOS MEMESI"};
const char* const kProfName[3] = {"DRAG", "YOL", "PIST"};
constexpr float kY0 = 66, kH = 44;
Rect minusR(int i) { return {214, kY0 + i * kH + 6, 254, kY0 + i * kH + 36}; }
Rect plusR(int i) { return {312, kY0 + i * kH + 6, 352, kY0 + i * kH + 36}; }
Rect loadR(int k) { return {8 + k * 116.0f, 444, 120 + k * 116.0f, 474}; }
Rect saveR(int k) { return {8 + k * 116.0f, 480, 120 + k * 116.0f, 510}; }
const Rect kReset{8, 530, 176, 562}, kRefill{184, 530, 352, 562}, kBackS{8, 596, 352, 634};

double defaultPsi(const Tune& t) { return t.tires == TireType::DragSlick ? 16.0 : t.tires == TireType::SemiSlick ? 26.0 : 32.0; }
// Satirin kilidi (bos: acik)
const char* lockOf(const Tune& t, int row) {
    if (row >= RRide && row <= RArbR && !suspAdjustable(t)) return "AYARLI SUSPANSIYON GEREKLI";
    if (row == RPreload && !lsdAdjustable(t)) return "PLAKALI LSD GEREKLI";
    if (row == RNos && t.nitrous <= 0) return "NOS KITI GEREKLI";
    return "";
}
}

SetupScreen::SetupScreen(App& app) : app_(app) {}

void SetupScreen::render(Renderer& r) {
    const OwnedCar& oc = app_.career.car();
    const Tune& t = oc.tune;
    r.begin(360, 640, kUiBg);
    r.rect(0, 0, 360, 58, kUiPanel);
    r.text(8, 8, "KURULUM", 2, kUiGold);
    r.textFit(8, 34, findVehicle(oc.carId)->fullName(), 1, 344, kUiText);
    char b[64];
    for (int i = 0; i < RCount; ++i) {
        const float y = kY0 + i * kH;
        const char* lock = lockOf(t, i);
        r.rect(8, y, 352, y + kH - 4, kUiPanel);
        r.text(16, y + 6, kRowName[i], 1, *lock ? kUiDim : Color{1, 1, 1});
        if (*lock) { r.text(16, y + 22, lock, 1, {0.6f, 0.4f, 0.35f}); continue; }
        switch (i) {
        case RPsi: std::snprintf(b, sizeof b, "%.0f PSI", t.psi > 0 ? t.psi : defaultPsi(t)); break;
        case RRide: std::snprintf(b, sizeof b, "%+d MM", t.setRide); break;
        case RSpring: std::snprintf(b, sizeof b, "%+d%%", 5 * t.setSpring); break;
        case RDamp: std::snprintf(b, sizeof b, "%+d%%", 8 * t.setDamp); break;
        case RArbF: std::snprintf(b, sizeof b, "%+d%%", 20 * t.setArbF); break;
        case RArbR: std::snprintf(b, sizeof b, "%+d%%", 20 * t.setArbR); break;
        case RPreload: std::snprintf(b, sizeof b, "%+d%%", 15 * t.setPreload); break;
        default: std::snprintf(b, sizeof b, "%%%d", t.setNos ? t.setNos : 100); break;
        }
        const bool def = i == RPsi ? t.psi <= 0 : std::string(b).find_first_of("123456789") == std::string::npos || (i == RNos && !t.setNos);
        r.textFit(283, y + 15, b, 1, 54, def ? kUiText : kUiGold, true);
        static const char* const kHint[RCount] = {"DUSUK: KALKIS TUTUSU, YUKSEK: HIZ", "ALCAK: VIRAJ, YUKSEK: DRAG YUK AKTARIMI",
                                                  "SERT: VIRAJ TEPKISI", "SERT: AZ YALPA", "SERT: ONDEN KAYAR", "SERT: ARKADAN KAYAR",
                                                  "YUKSEK: KILITLI, CEKISLI", "KUCUK MEME: UZUN TUP"};
        r.textFit(16, y + 22, kHint[i], 1, 192, kUiDim);
        button(r, minusR(i), "-", kUiBtn, 2);
        button(r, plusR(i), "+", kUiBtn, 2);
    }
    r.text(8, 428, "PROFILLER", 1, kUiGold);
    for (int k = 0; k < 3; ++k) {
        const bool has = oc.profile[k].size() > 0;
        button(r, loadR(k), std::string(kProfName[k]) + (has ? " YUKLE" : " BOS"), has ? kUiGreen : Color{0.25f, 0.25f, 0.28f}, 1);
        button(r, saveR(k), std::string(kProfName[k]) + " KAYDET", kUiBtn, 1);
    }
    button(r, kReset, "SIFIRLA", Color{0.45f, 0.20f, 0.15f}, 2);
    if (t.nitrous > 0) {                                               // NOS tupu: doluluk + dolum
        const int p = app_.career.nosRefillPrice();
        std::snprintf(b, sizeof b, p > 0 ? "NOS %%%.0f DOLDUR %s" : "NOS TUPU DOLU", oc.nosFill * 100.0, money(p).c_str());
        button(r, kRefill, b, p > 0 ? (app_.career.money >= p ? kUiGreen : Color{0.3f, 0.3f, 0.32f}) : Color{0.18f, 0.30f, 0.20f}, 1);
    }
    button(r, kBackS, "< GARAJ", kUiBtn, 2);
    if (msgT_ > 0) { r.rect(0, 560, 360, 590, {0.02f, 0.02f, 0.04f, 0.92f}); r.textCentered(180, 568, msg_, 2, kUiGold); }
}

void SetupScreen::change(int row, int dir) {
    Tune& t = app_.career.cars[app_.career.current].tune;
    switch (row) {
    case RPsi: t.psi = std::clamp((t.psi > 0 ? t.psi : defaultPsi(t)) + dir, 10.0, 40.0); break;
    case RRide: t.setRide += 5 * dir; break;
    case RSpring: t.setSpring += dir; break;
    case RDamp: t.setDamp += dir; break;
    case RArbF: t.setArbF += dir; break;
    case RArbR: t.setArbR += dir; break;
    case RPreload: t.setPreload += dir; break;
    default: t.setNos = std::clamp((t.setNos ? t.setNos : 100) + 5 * dir, 50, 100); if (t.setNos == 100) t.setNos = 0; break;
    }
    clampSetup(t);
    app_.saveCareer();
}

void SetupScreen::pointerDown(int, float x, float y) {
    if (kBackS.hit(x, y)) { app_.goGarage(); return; }
    Career& c = app_.career;
    const Tune& t = c.car().tune;
    for (int i = 0; i < RCount; ++i) {
        if (*lockOf(t, i)) continue;
        if (minusR(i).hit(x, y)) { change(i, -1); return; }
        if (plusR(i).hit(x, y)) { change(i, +1); return; }
    }
    for (int k = 0; k < 3; ++k) {
        if (saveR(k).hit(x, y)) { c.saveProfile(k); app_.saveCareer(); msg_ = std::string(kProfName[k]) + " KAYDEDILDI"; msgT_ = 1.4; return; }
        if (loadR(k).hit(x, y)) {
            if (c.loadProfile(k)) { app_.saveCareer(); msg_ = std::string(kProfName[k]) + " YUKLENDI"; }
            else msg_ = "PROFIL BOS";
            msgT_ = 1.4;
            return;
        }
    }
    if (kRefill.hit(x, y) && t.nitrous > 0) {
        std::string why;
        if (c.refillNos(&why)) { app_.saveCareer(); msg_ = "NOS TUPU DOLDU"; } else msg_ = why;
        msgT_ = 1.4; return;
    }
    if (kReset.hit(x, y)) {
        Tune& m = c.cars[c.current].tune;
        m.psi = 0; m.setRide = m.setSpring = m.setDamp = m.setArbF = m.setArbR = m.setPreload = m.setNos = 0;
        app_.saveCareer(); msg_ = "FABRIKA AYARI"; msgT_ = 1.4;
    }
}

void SetupScreen::key(Key k, bool down) { if (down && k == Key::Back) app_.goGarage(); }

} // namespace zk
