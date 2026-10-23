// ZEHRA KINIK - Kurulum ekrani: lastik basinci, ayarli suspansiyon (yukseklik / yay / amortisor / viraj demirleri),
// LSD on yuku, NOS memesi; DRAG / YOL / PIST profilleri. Dikey 360x640 ve Yatay 640x360.
#include "Screens.h"
#include "Ui.h"
#include "sim/PartTables.h"
#include "app/Hints.h"

#include <algorithm>
#include <cstdio>

namespace zk {

namespace {
enum Row { RPsi, RRide, RRideR, RSpring, RSpringR, RDamp, RDampR, RArbF, RArbR, RPreload, RNos, RCount };
const char* const kRowName[RCount] = {"LASTIK BASINCI", "ON YUKSEKLIK", "ARKA YUKSEKLIK", "ON YAY", "ARKA YAY", "ON AMORTISOR", "ARKA AMORTISOR",
                                      "ON VIRAJ DEMIRI", "ARKA VIRAJ DEMIRI", "LSD ON YUKU", "NOS MEMESI"};
const char* const kProfName[3] = {"DRAG", "YOL", "PIST"};

// Dikey koordinatlar
constexpr float kY0P = 62, kHP = 37;
Rect minusRP(int i) { return {214, kY0P + i * kHP + 4, 254, kY0P + i * kHP + 31}; }
Rect plusRP(int i) { return {312, kY0P + i * kHP + 4, 352, kY0P + i * kHP + 31}; }
Rect loadRP(int k) { return {8 + k * 116.0f, 486, 120 + k * 116.0f, 512}; }
Rect saveRP(int k) { return {8 + k * 116.0f, 516, 120 + k * 116.0f, 542}; }
const Rect kResetP{8, 550, 176, 584}, kRefillP{184, 550, 352, 584}, kBackSP{8, 598, 352, 634};

// Yatay koordinatlar
constexpr float kY0L = 48, kHL = 38;
inline bool isLeftL(int i) { return i <= 5; }
inline float rowXL(int i) { return isLeftL(i) ? 8.0f : 324.0f; }
inline float rowYL(int i) { return kY0L + (isLeftL(i) ? i : (i - 6)) * kHL; }
Rect minusRL(int i) { float x = rowXL(i), y = rowYL(i); return {x + 188.0f, y + 4, x + 224.0f, y + 31}; }
Rect plusRL(int i) { float x = rowXL(i), y = rowYL(i); return {x + 270.0f, y + 4, x + 306.0f, y + 31}; }

Rect loadRL(int k) { return {8 + k * 102.0f, 286, 104 + k * 102.0f, 314}; }
Rect saveRL(int k) { return {8 + k * 102.0f, 320, 104 + k * 102.0f, 348}; }
const Rect kResetL{324, 250, 474, 292}, kRefillL{482, 250, 632, 292}, kBackSL{324, 302, 632, 348};

double defaultPsi(const Tune& t) { return t.tires == TireType::DragSlick ? 16.0 : t.tires == TireType::SemiSlick ? 26.0 : 32.0; }

const char* lockOf(const Tune& t, int row) {
    if (row >= RRide && row <= RArbR && !suspAdjustable(t)) return "AYARLI SUSPANSIYON GEREKLI";
    if (row == RPreload && !lsdAdjustable(t)) return "PLAKALI LSD GEREKLI";
    if (row == RNos && t.nitrous <= 0) return "NOS KITI GEREKLI";
    return "";
}
}

SetupScreen::SetupScreen(App& app) : app_(app) { app_.hint(HintSetup, hintTexts()[HintSetup]); }

void SetupScreen::render(Renderer& r) {
    const OwnedCar& oc = app_.career.car();
    const Tune& t = oc.tune;
    const bool land = app_.settings.menuLandscape;
    r.begin(land ? 640 : 360, land ? 360 : 640, kUiBg);

    char b[64];
    if (land) {
        // ==========================================
        // YATAY 640x360 NATIVE KURULUM
        // ==========================================
        r.rect(0, 0, 640, 42, kUiPanel);
        r.text(8, 6, "KURULUM", 2, kUiGold);
        r.textFit(8, 25, findVehicle(oc.carId)->fullName(), 1, 340, kUiText);
        const std::string m = money(app_.career.money);
        r.text(632 - r.textWidth(m, 2), 8, m, 2, {0.4f, 1.0f, 0.5f});

        for (int i = 0; i < RCount; ++i) {
            const float x = rowXL(i);
            const float y = rowYL(i);
            const char* lock = lockOf(t, i);
            r.rect(x, y, x + 308.0f, y + kHL - 3.0f, kUiPanel);
            r.text(x + 8.0f, y + 4.0f, kRowName[i], 1, *lock ? kUiDim : Color{1, 1, 1});

            if (*lock) {
                r.text(x + 8.0f, y + 18.0f, lock, 1, {0.6f, 0.4f, 0.35f});
                continue;
            }

            switch (i) {
            case RPsi: std::snprintf(b, sizeof b, "%.0f PSI", t.psi > 0 ? t.psi : defaultPsi(t)); break;
            case RRide: std::snprintf(b, sizeof b, "%+d MM", t.setRide); break;
            case RRideR: std::snprintf(b, sizeof b, "%+d MM", t.setRideR); break;
            case RSpring: std::snprintf(b, sizeof b, "%+d%%", 5 * t.setSpring); break;
            case RSpringR: std::snprintf(b, sizeof b, "%+d%%", 5 * t.setSpringR); break;
            case RDamp: std::snprintf(b, sizeof b, "%+d%%", 8 * t.setDamp); break;
            case RDampR: std::snprintf(b, sizeof b, "%+d%%", 8 * t.setDampR); break;
            case RArbF: std::snprintf(b, sizeof b, "%+d%%", 20 * t.setArbF); break;
            case RArbR: std::snprintf(b, sizeof b, "%+d%%", 20 * t.setArbR); break;
            case RPreload: std::snprintf(b, sizeof b, "%+d%%", 15 * t.setPreload); break;
            default: std::snprintf(b, sizeof b, "%%%d", t.setNos ? t.setNos : 100); break;
            }

            const bool def = i == RPsi ? t.psi <= 0 : std::string(b).find_first_of("123456789") == std::string::npos || (i == RNos && !t.setNos);
            r.textFit(x + 247.0f, y + 12.0f, b, 1, 42.0f, def ? kUiText : kUiGold, true);

            static const char* const kHint[RCount] = {
                "DUSUK: TUTUS, YUKSEK: HIZ", "ALCAK: AGIRLIK MERKEZI DUSER", "YUKSEK ARKA: DRAG YUK AKTARIMI",
                "SERT ON: ONDEN KAYAR", "SERT ARKA: ARKADAN KAYAR", "SERT: AZ DALIS (FREN)", "YUMUSAK: KALKISTA ARKA COKER",
                "SERT: ONDEN KAYAR", "SERT: ARKADAN KAYAR", "YUKSEK: KILITLI, CEKISLI", "KUCUK MEME: UZUN TUP"};
            r.textFit(x + 8.0f, y + 18.0f, kHint[i], 1, 180.0f, kUiDim);

            button(r, minusRL(i), "-", kUiBtn, 2);
            button(r, plusRL(i), "+", kUiBtn, 2);
        }

        // Sol Alt: Profiller
        for (int k = 0; k < 3; ++k) {
            const bool has = oc.profile[k].size() > 0;
            button(r, loadRL(k), std::string(kProfName[k]) + (has ? " YUKLE" : " BOS"), has ? kUiGreen : Color{0.25f, 0.25f, 0.28f}, 1);
            button(r, saveRL(k), std::string(kProfName[k]) + " KAYDET", kUiBtn, 1);
        }

        // Sag Alt: Sifirla, NOS, Geri
        button(r, kResetL, "SIFIRLA", Color{0.45f, 0.20f, 0.15f}, 2);
        if (t.nitrous > 0) {
            const int p = app_.career.nosRefillPrice();
            std::snprintf(b, sizeof b, p > 0 ? "NOS %%%.0f DOLDUR %s" : "NOS TUPU DOLU", oc.nosFill * 100.0, money(p).c_str());
            button(r, kRefillL, b, p > 0 ? (app_.career.money >= p ? kUiGreen : Color{0.3f, 0.3f, 0.32f}) : Color{0.18f, 0.30f, 0.20f}, 1);
        } else {
            r.rect(kRefillL.x0, kRefillL.y0, kRefillL.x1, kRefillL.y1, {0.12f, 0.12f, 0.14f});
            r.textCentered(kRefillL.cx(), kRefillL.cy() - 4, "NOS KITI YOK", 1, kUiDim);
        }

        button(r, kBackSL, "< GARAJ", kUiBtn, 2);

        if (msgT_ > 0) {
            r.rect(140, 160, 500, 200, {0.02f, 0.02f, 0.04f, 0.95f});
            r.textCentered(320, 175, msg_, 2, kUiGold);
        }
    } else {
        // ==========================================
        // DIKEY 360x640 KLASIK KURULUM
        // ==========================================
        r.rect(0, 0, 360, 58, kUiPanel);
        r.text(8, 8, "KURULUM", 2, kUiGold);
        r.textFit(8, 34, findVehicle(oc.carId)->fullName(), 1, 344, kUiText);

        for (int i = 0; i < RCount; ++i) {
            const float y = kY0P + i * kHP;
            const char* lock = lockOf(t, i);
            r.rect(8, y, 352, y + kHP - 3, kUiPanel);
            r.text(16, y + 5, kRowName[i], 1, *lock ? kUiDim : Color{1, 1, 1});
            if (*lock) { r.text(16, y + 19, lock, 1, {0.6f, 0.4f, 0.35f}); continue; }
            switch (i) {
            case RPsi: std::snprintf(b, sizeof b, "%.0f PSI", t.psi > 0 ? t.psi : defaultPsi(t)); break;
            case RRide: std::snprintf(b, sizeof b, "%+d MM", t.setRide); break;
            case RRideR: std::snprintf(b, sizeof b, "%+d MM", t.setRideR); break;
            case RSpring: std::snprintf(b, sizeof b, "%+d%%", 5 * t.setSpring); break;
            case RSpringR: std::snprintf(b, sizeof b, "%+d%%", 5 * t.setSpringR); break;
            case RDamp: std::snprintf(b, sizeof b, "%+d%%", 8 * t.setDamp); break;
            case RDampR: std::snprintf(b, sizeof b, "%+d%%", 8 * t.setDampR); break;
            case RArbF: std::snprintf(b, sizeof b, "%+d%%", 20 * t.setArbF); break;
            case RArbR: std::snprintf(b, sizeof b, "%+d%%", 20 * t.setArbR); break;
            case RPreload: std::snprintf(b, sizeof b, "%+d%%", 15 * t.setPreload); break;
            default: std::snprintf(b, sizeof b, "%%%d", t.setNos ? t.setNos : 100); break;
            }
            const bool def = i == RPsi ? t.psi <= 0 : std::string(b).find_first_of("123456789") == std::string::npos || (i == RNos && !t.setNos);
            r.textFit(283, y + 13, b, 1, 54, def ? kUiText : kUiGold, true);
            static const char* const kHint[RCount] = {
                "DUSUK: TUTUS, YUKSEK: HIZ", "ALCAK: AGIRLIK MERKEZI DUSER", "YUKSEK ARKA: DRAG YUK AKTARIMI",
                "SERT ON: ONDEN KAYAR", "SERT ARKA: ARKADAN KAYAR", "SERT: AZ DALIS (FREN)", "YUMUSAK: KALKISTA ARKA COKER",
                "SERT: ONDEN KAYAR", "SERT: ARKADAN KAYAR", "YUKSEK: KILITLI, CEKISLI", "KUCUK MEME: UZUN TUP"};
            r.textFit(16, y + 19, kHint[i], 1, 192, kUiDim);
            button(r, minusRP(i), "-", kUiBtn, 2);
            button(r, plusRP(i), "+", kUiBtn, 2);
        }
        r.text(8, 474, "PROFILLER", 1, kUiGold);
        for (int k = 0; k < 3; ++k) {
            const bool has = oc.profile[k].size() > 0;
            button(r, loadRP(k), std::string(kProfName[k]) + (has ? " YUKLE" : " BOS"), has ? kUiGreen : Color{0.25f, 0.25f, 0.28f}, 1);
            button(r, saveRP(k), std::string(kProfName[k]) + " KAYDET", kUiBtn, 1);
        }
        button(r, kResetP, "SIFIRLA", Color{0.45f, 0.20f, 0.15f}, 2);
        if (t.nitrous > 0) {
            const int p = app_.career.nosRefillPrice();
            std::snprintf(b, sizeof b, p > 0 ? "NOS %%%.0f DOLDUR %s" : "NOS TUPU DOLU", oc.nosFill * 100.0, money(p).c_str());
            button(r, kRefillP, b, p > 0 ? (app_.career.money >= p ? kUiGreen : Color{0.3f, 0.3f, 0.32f}) : Color{0.18f, 0.30f, 0.20f}, 1);
        }
        button(r, kBackSP, "< GARAJ", kUiBtn, 2);
        if (msgT_ > 0) { r.rect(0, 560, 360, 590, {0.02f, 0.02f, 0.04f, 0.92f}); r.textCentered(180, 568, msg_, 2, kUiGold); }
    }
}

void SetupScreen::change(int row, int dir) {
    Tune& t = app_.career.cars[app_.career.current].tune;
    switch (row) {
    case RPsi: t.psi = std::clamp((t.psi > 0 ? t.psi : defaultPsi(t)) + dir, 10.0, 40.0); break;
    case RRide: t.setRide += 5 * dir; break;
    case RRideR: t.setRideR += 5 * dir; break;
    case RSpring: t.setSpring += dir; break;
    case RSpringR: t.setSpringR += dir; break;
    case RDamp: t.setDamp += dir; break;
    case RDampR: t.setDampR += dir; break;
    case RArbF: t.setArbF += dir; break;
    case RArbR: t.setArbR += dir; break;
    case RPreload: t.setPreload += dir; break;
    default: t.setNos = std::clamp((t.setNos ? t.setNos : 100) + 5 * dir, 50, 100); if (t.setNos == 100) t.setNos = 0; break;
    }
    clampSetup(t);
    app_.saveCareer();
}

void SetupScreen::pointerDown(int, float x, float y) {
    const bool land = app_.settings.menuLandscape;
    const Rect& backBtn = land ? kBackSL : kBackSP;
    const Rect& resetBtn = land ? kResetL : kResetP;
    const Rect& refillBtn = land ? kRefillL : kRefillP;

    if (backBtn.hit(x, y)) { app_.goGarage(); return; }
    Career& c = app_.career;
    const Tune& t = c.car().tune;

    for (int i = 0; i < RCount; ++i) {
        if (*lockOf(t, i)) continue;
        const Rect& mR = land ? minusRL(i) : minusRP(i);
        const Rect& pR = land ? plusRL(i) : plusRP(i);
        if (mR.hit(x, y)) { change(i, -1); return; }
        if (pR.hit(x, y)) { change(i, +1); return; }
    }

    for (int k = 0; k < 3; ++k) {
        const Rect& lR = land ? loadRL(k) : loadRP(k);
        const Rect& sR = land ? saveRL(k) : saveRP(k);
        if (lR.hit(x, y)) {
            if (c.loadProfile(k)) { app_.saveCareer(); msg_ = std::string(kProfName[k]) + " PROFILI YUKLENDI"; }
            else msg_ = "BU PROFIL BOS";
            msgT_ = 1.4; return;
        }
        if (sR.hit(x, y)) {
            c.saveProfile(k); app_.saveCareer();
            msg_ = std::string(kProfName[k]) + " PROFILI KAYDEDILDI";
            msgT_ = 1.4; return;
        }
    }

    if (resetBtn.hit(x, y)) {
        Tune& curT = c.cars[c.current].tune;
        curT.psi = 0; curT.setRide = curT.setRideR = curT.setSpring = curT.setSpringR = curT.setDamp = curT.setDampR = curT.setArbF = curT.setArbR = curT.setPreload = curT.setNos = 0;
        app_.saveCareer();
        msg_ = "KURULUM SIFIRLANDI"; msgT_ = 1.4; return;
    }

    if (t.nitrous > 0 && refillBtn.hit(x, y)) {
        std::string why;
        if (c.refillNos(&why)) { app_.saveCareer(); msg_ = "NOS DOLDURULDU"; }
        else msg_ = why;
        msgT_ = 1.6;
    }
}

void SetupScreen::key(Key k, bool down) { if (down && k == Key::Back) app_.goGarage(); }

} // namespace zk

