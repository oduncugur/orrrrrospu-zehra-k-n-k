// ZEHRA KINIK - Bolge haritasi: kariyer ligleri yollarla bagli bolgeler; altta haftalik turnuva.
#include "Screens.h"
#include "Ui.h"
#include "app/Hints.h"

#include <cmath>
#include <cstdio>

namespace zk {

namespace {
struct Node { float x, y; Color c; };
const Color kLeagueCol[kLeagues] = {{0.85f, 0.55f, 0.15f}, {0.25f, 0.55f, 0.85f}, {0.55f, 0.35f, 0.75f}, {0.30f, 0.65f, 0.35f}, {0.85f, 0.25f, 0.25f}};

// --- Dikey (Portrait: 360x640) ---
const Node kNodesP[Career::kCities] = {
    {62, 420, {}}, {150, 404, {}},     // ISTANBUL, IZMIT (MAHALLE)
    {244, 380, {}}, {300, 318, {}},    // BURSA, ESKISEHIR (SEHIR)
    {222, 272, {}}, {116, 258, {}},    // ANKARA, KONYA (SEHIRLERARASI)
    {66, 196, {}}, {160, 168, {}},     // KAPADOKYA, NIGDE (DAG)
    {270, 140, {}}, {184, 92, {}},     // MERSIN, ANTALYA (PIST)
};
constexpr float kRP = 21.0f;
const Rect kTourP{8, 528, 352, 566}, kBackMP{8, 596, 176, 634}, kStreetMP{184, 596, 352, 634};
const Rect kRunTP{32, 254, 328, 294}, kSoloTP{32, 312, 328, 346}, kFastTP{32, 352, 328, 386}, kCancelTP{32, 392, 328, 428};

// --- Yatay (Landscape: 640x360) ---
// Harita sol tarafta (0..424, 44..360), turnuva ve butonlar sag panelde (428..632, 48..352)
const Node kNodesL[Career::kCities] = {
    { 44, 110, {}}, {110, 116, {}},     // ISTANBUL, IZMIT (MAHALLE)
    { 76, 214, {}}, {152, 196, {}},     // BURSA, ESKISEHIR (SEHIR)
    {224, 122, {}}, {204, 246, {}},     // ANKARA, KONYA (SEHIRLERARASI)
    {292, 154, {}}, {282, 252, {}},     // KAPADOKYA, NIGDE (DAG)
    {358, 260, {}}, {368, 134, {}},     // MERSIN, ANTALYA (PIST)
};
constexpr float kRLand = 18.0f;
const Rect kTourL{436, 122, 624, 188};
const Rect kBackML{436, 214, 624, 274}, kStreetML{436, 284, 624, 344};
const Rect kRunTL{120, 94, 520, 142}, kSoloTL{120, 162, 520, 204}, kFastTL{120, 212, 520, 254}, kCancelTL{120, 266, 520, 318};

bool bossBeaten(const Career& c, int league) {
    const auto& ev = leagueEvents();
    for (int i = 0; i < (int)ev.size(); ++i)
        if (ev[i].league == league && ev[i].rival >= 0 && rivals()[ev[i].rival].boss && c.eventWon(i)) return true;
    return false;
}
} // namespace

RegionMapScreen::RegionMapScreen(App& app) : app_(app) {
    app_.career.tourRefresh();
    app_.hint(HintMap, hintTexts()[HintMap]);
    if (!app_.eventNote.empty()) { msg_ = app_.eventNote; msgT_ = 3.5; app_.eventNote.clear(); }
}

void RegionMapScreen::render(Renderer& r) {
    const Career& c = app_.career;
    const bool land = landscape();
    const float W = land ? 640.0f : 360.0f;
    const float H = land ? 360.0f : 640.0f;
    r.begin((int)W, (int)H, kUiBg);

    const Node* nodes = land ? kNodesL : kNodesP;
    const float nodeR = land ? kRLand : kRP;

    // --- Arka plan ve Arazi ---
    if (land) {
        // Yatay mod: Harita sol bolgede (0..424, 44..360)
        r.gradientV(0, 44, 424, 360, {0.18f, 0.26f, 0.18f}, {0.13f, 0.20f, 0.14f});
        // Doga ve agac benekleri
        for (int i = 0; i < 20; ++i) {
            const float x = 14.0f + (i * 97) % 390, y = 52.0f + (i * 53) % 290;
            r.circle(x, y, 9.0f + (i * 7) % 13, 10, {0.10f, 0.16f, 0.10f, 0.65f});
        }
        // Daglar (Toroslar / Erciyes)
        r.tri(220, 220, 310, 75, 400, 220, {0.30f, 0.28f, 0.24f});
        r.tri(260, 220, 350, 95, 424, 210, {0.38f, 0.36f, 0.31f});
        r.tri(310, 75, 322, 85, 298, 85, {0.92f, 0.92f, 0.96f});          // kar zirvesi
        // Sahil (Marmara ve Akdeniz)
        r.gradientV(0, 44, 90, 150, {0.10f, 0.25f, 0.45f, 0.75f}, {0.10f, 0.25f, 0.45f, 0.0f});
        r.gradientV(0, 310, 424, 360, {0.10f, 0.25f, 0.45f, 0.0f}, {0.10f, 0.25f, 0.45f, 0.85f});
        // Harita ile sag panel arasi cizgi
        r.rect(424, 44, 426, 360, {0.16f, 0.18f, 0.24f});
    } else {
        // Dikey mod
        r.gradientV(0, 58, 360, 470, {0.20f, 0.30f, 0.20f}, {0.16f, 0.24f, 0.17f});
        for (int i = 0; i < 18; ++i) {
            const float x = 18.0f + (i * 97) % 330, y = 80.0f + (i * 53) % 370;
            r.circle(x, y, 10.0f + (i * 7) % 14, 10, {0.13f, 0.20f, 0.13f, 0.6f});
        }
        r.tri(200, 230, 290, 120, 360, 230, {0.32f, 0.30f, 0.26f});        // dag
        r.tri(240, 230, 320, 140, 360, 210, {0.40f, 0.38f, 0.33f});
        r.tri(290, 120, 300, 128, 280, 128, {0.9f, 0.9f, 0.95f});          // kar
        r.gradientV(0, 430, 360, 470, {0.10f, 0.25f, 0.45f, 0.0f}, {0.10f, 0.25f, 0.45f, 0.9f});   // sahil
    }

    // --- Yollar: sehirleri sirayla baglar (kilitli kisim kesik kesik) ---
    const int open = c.leagueUnlocked();
    for (int l = 0; l + 1 < Career::kCities; ++l) {
        const Node &a = nodes[l], &b = nodes[l + 1];
        const float dx = b.x - a.x, dy = b.y - a.y, len = std::sqrt(dx * dx + dy * dy);
        if (len < 1.0f) continue;
        const float nx = -dy / len * 4, ny = dx / len * 4;
        const bool unlocked = Career::cityLeague(l + 1) <= open;
        const int seg = 14;
        for (int k = 0; k < seg; ++k) {
            if (!unlocked && (k & 1)) continue;
            const float t0 = (float)k / seg, t1 = (float)(k + 1) / seg;
            const float x0 = a.x + dx * t0, y0 = a.y + dy * t0, x1 = a.x + dx * t1, y1 = a.y + dy * t1;
            const Color rc = unlocked ? Color{0.25f, 0.25f, 0.27f} : Color{0.35f, 0.35f, 0.37f, 0.6f};
            r.tri(x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, rc);
            r.tri(x0 + nx, y0 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny, rc);
            if (unlocked && (k & 1)) r.tri(x0, y0, x1, y1, x1 + nx * 0.15f, y1 + ny * 0.15f, {0.95f, 0.85f, 0.3f, 0.8f});
        }
    }

    // --- Bolgeler / Sehir Noktalari ---
    const auto& ev = leagueEvents();
    char b[64];
    for (int ci = 0; ci < Career::kCities; ++ci) {
        const int l = Career::cityLeague(ci);
        const Node& n = nodes[ci];
        const bool locked = l > open;
        int total = 0, won = 0;
        for (int i = 0; i < (int)ev.size(); ++i) if (eventCity(i) == ci) { ++total; won += c.eventWon(i) ? 1 : 0; }
        const float pulse = (l == open && !locked && ci % 2 == 0) ? 2.0f + 2.0f * std::sin((float)t_ * 3.0f) : 0.0f;
        r.circle(n.x, n.y + 3, nodeR + 2, 24, {0, 0, 0, 0.35f});
        r.circle(n.x, n.y, nodeR + 3 + pulse, 24, locked ? Color{0.25f, 0.25f, 0.27f} : bossBeaten(c, l) ? kUiGold : Color{0.95f, 0.95f, 0.95f});
        r.circle(n.x, n.y, nodeR, 24, locked ? Color{0.30f, 0.30f, 0.32f} : kLeagueCol[l]);
        std::snprintf(b, sizeof b, "%d", l + 1);
        r.textCentered(n.x, n.y - 7, b, 2, locked ? kUiDim : Color{1, 1, 1});
        r.textFit(n.x, n.y + nodeR + 4, Career::cityName(ci), 1, land ? 90 : 110, locked ? kUiDim : Color{1, 1, 1}, true);
        if (ci % 2 == 0) r.textFit(n.x, n.y - nodeR - 13, leagueName(l), 1, land ? 96 : 110, locked ? kUiDim : kUiGold, true);
        if (ci == c.city) {
            r.circle(n.x + nodeR * 0.75f, n.y - nodeR * 0.75f, land ? 10 : 11, 16, {0.1f, 0.6f, 0.95f});
            icon(r, IconGallery, n.x + nodeR * 0.75f, n.y - nodeR * 0.75f, land ? 6 : 7, {1, 1, 1});
        }
        std::snprintf(b, sizeof b, locked ? "KILITLI" : "%d / %d", won, total);
        r.textCentered(n.x, n.y + nodeR + 15, b, 1, locked ? kUiDim : kUiGold);
    }

    // --- Ust Serit ---
    const std::string m = money(c.money);
    if (land) {
        r.rect(0, 0, 640, 44, kUiPanel);
        r.rect(0, 42, 640, 44, {0.22f, 0.24f, 0.30f});
        r.text(12, 10, "HARITA", 2, kUiGold);
        std::snprintf(b, sizeof b, "UN %d   GALIBIYET %d", c.rep, c.wins);
        r.text(110, 14, b, 1, kUiText);
        std::snprintf(b, sizeof b, "KONUM: %s", Career::cityName(c.city));
        r.text(260, 14, b, 1, kUiDim);
        r.text(628 - r.textWidth(m, 2), 10, m, 2, {0.4f, 1.0f, 0.5f});
    } else {
        r.rect(0, 0, 360, 58, kUiPanel);
        r.text(8, 8, "HARITA", 2, kUiGold);
        std::snprintf(b, sizeof b, "UN %d   GALIBIYET %d", c.rep, c.wins);
        r.text(8, 34, b, 1, kUiText);
        r.text(352 - r.textWidth(m, 2), 8, m, 2, {0.4f, 1.0f, 0.5f});
    }

    // --- Turnuva ve Butonlar ---
    const int daysLeft = 7 - todayIndex() % 7;
    std::string why;
    const bool can = c.tourAvailable(&why);
    char tb[48]; std::snprintf(tb, sizeof tb, "TUR %d: DRAG ELEME >", c.tourRound + 1);

    if (land) {
        // HAFTALIK TURNUVA KARTI (Sag ust)
        r.rect(428, 48, 632, 198, {0.08f, 0.09f, 0.13f});
        r.rect(428, 48, 632, 50, kUiGold);
        r.text(436, 56, "HAFTALIK TURNUVA", 1, kUiGold);
        std::snprintf(b, sizeof b, "YENILENME %d GUN", daysLeft);
        r.text(624 - r.textWidth(b, 1), 56, b, 1, kUiDim);

        if (c.tourOut) {
            r.text(436, 74, "BU HAFTA ELENDIN", 1, {1.0f, 0.45f, 0.35f});
            std::snprintf(b, sizeof b, "TUR %d'DE KALDIN", c.tourRound + 1);
            r.text(436, 88, b, 1, kUiDim);
        } else if (c.tourRound >= Career::kTourRounds) {
            r.text(436, 74, "BU HAFTA SAMPIYONSUN!", 1, kUiGold);
            r.text(436, 88, "YENI TURNUVAYI BEKLE", 1, kUiDim);
        } else {
            std::snprintf(b, sizeof b, "TUR %d / %d (ELEME)", c.tourRound + 1, Career::kTourRounds);
            r.text(436, 72, b, 1, kUiText);
            std::snprintf(b, sizeof b, "GIRIS: %s", c.tourRound == 0 ? money(c.tourEntry()).c_str() : "UCRETSIZ");
            r.text(436, 86, b, 1, kUiDim);
            std::snprintf(b, sizeof b, "ODUL: %s", money(c.tourPrize()).c_str());
            r.text(530, 86, b, 1, {0.4f, 1.0f, 0.5f});
        }
        for (int k = 0; k < Career::kTourRounds; ++k)
            r.circle(446.0f + k * 18.0f, 106, 5, 10,
                     k < c.tourRound ? Color{0.4f, 1.0f, 0.5f} :
                     c.tourOut && k == c.tourRound ? kUiRed : Color{0.3f, 0.3f, 0.35f});

        button(r, kTourL, can ? std::string(tb) : why, can ? kUiOrange : Color{0.25f, 0.25f, 0.28f}, can ? 2 : 1);

        // NAVIGASYON KARTI (Sag alt)
        r.rect(428, 204, 632, 354, {0.08f, 0.09f, 0.13f});
        r.rect(428, 204, 632, 206, {0.25f, 0.45f, 0.65f});
        button(r, kBackML, "< GARAJ", kUiBtn, 2);
        button(r, kStreetML, "SOKAK >", {0.35f, 0.15f, 0.45f}, 2);
    } else {
        r.rect(8, 476, 352, 524, {0.08f, 0.09f, 0.13f});
        r.text(16, 482, "HAFTALIK TURNUVA", 1, kUiGold);
        std::snprintf(b, sizeof b, "YENILENME %d GUN", daysLeft);
        r.text(344 - r.textWidth(b, 1), 482, b, 1, kUiDim);
        if (c.tourOut) std::snprintf(b, sizeof b, "BU HAFTA ELENDIN (TUR %d)", c.tourRound + 1);
        else if (c.tourRound >= Career::kTourRounds) std::snprintf(b, sizeof b, "BU HAFTA SAMPIYONSUN!");
        else std::snprintf(b, sizeof b, "TUR %d / %d   GIRIS %s   ODUL %s", c.tourRound + 1, Career::kTourRounds,
                           c.tourRound == 0 ? money(c.tourEntry()).c_str() : "-", money(c.tourPrize()).c_str());
        r.textFit(16, 498, b, 1, 328, c.tourOut ? Color{1.0f, 0.45f, 0.35f} : kUiText);
        for (int k = 0; k < Career::kTourRounds; ++k)
            r.circle(24.0f + k * 18.0f, 515, 5, 10, k < c.tourRound ? Color{0.4f, 1.0f, 0.5f} : c.tourOut && k == c.tourRound ? kUiRed : Color{0.3f, 0.3f, 0.35f});
        button(r, kTourP, can ? std::string(tb) : why, can ? kUiOrange : Color{0.25f, 0.25f, 0.28f}, 2);
        button(r, kBackMP, "< GARAJ", kUiBtn, 2);
        button(r, kStreetMP, "SOKAK >", {0.35f, 0.15f, 0.45f}, 2);
    }

    // --- Seyahat Modali ---
    if (travelTo_ >= 0) {
        const int legs = std::abs(travelTo_ - c.city);
        int fieldN = 0;
        for (int k = 0; k < legs; ++k) fieldN = std::max(fieldN, Career::legField(travelTo_ > c.city ? c.city + k : c.city - k - 1));
        if (land) {
            r.rect(0, 0, 640, 360, {0, 0, 0, 0.72f});
            r.rect(100, 20, 540, 340, kUiPanel);
            r.rect(100, 20, 540, 23, kUiGold);
            std::snprintf(b, sizeof b, "%s -> %s", Career::cityName(c.city), Career::cityName(travelTo_));
            r.textFit(320, 34, b, 2, 400, kUiGold, true);
            std::snprintf(b, sizeof b, "%.0f KM, TEK ETAP (%d SEHIR)", c.travelKm(travelTo_), legs);
            r.textCentered(320, 60, b, 1, kUiText);
            std::snprintf(b, sizeof b, "THE RUN: %d ARAC", fieldN);
            r.textFit(320, 76, b, 1, 400, kUiDim, true);
            button(r, kRunTL, "THE RUN ILE GIT >", kUiOrange, 2);
            r.textCentered(320, 148, "ODUL + UN; YAKIT VE SIRA ONEMLI", 1, kUiDim);
            button(r, kSoloTL, "TEK BASINA SUR (YARISSIZ)", Color{0.2f, 0.4f, 0.3f}, 1);
            std::snprintf(b, sizeof b, "HIZLI GECIS %s", money(c.fastTravelPrice(travelTo_)).c_str());
            button(r, kFastTL, b, c.money >= c.fastTravelPrice(travelTo_) ? Color{0.15f, 0.45f, 0.7f} : Color{0.25f, 0.25f, 0.28f}, 1);
            button(r, kCancelTL, "VAZGEC", kUiBtn, 2);
        } else {
            r.rect(0, 0, 360, 640, {0, 0, 0, 0.6f});
            r.rect(16, 180, 344, 440, kUiPanel);
            r.rect(16, 180, 344, 183, kUiGold);
            std::snprintf(b, sizeof b, "%s -> %s", Career::cityName(c.city), Career::cityName(travelTo_));
            r.textFit(180, 194, b, 2, 320, kUiGold, true);
            std::snprintf(b, sizeof b, "%.0f KM, TEK ETAP (%d SEHIR)", c.travelKm(travelTo_), legs);
            r.textCentered(180, 220, b, 1, kUiText);
            std::snprintf(b, sizeof b, "THE RUN: %d ARAC", fieldN);
            r.textFit(180, 236, b, 1, 320, kUiDim, true);
            button(r, kRunTP, "THE RUN ILE GIT >", kUiOrange, 2);
            r.textCentered(180, kRunTP.y1 + 4, "ODUL + UN; YAKIT VE SIRA ONEMLI", 1, kUiDim);
            button(r, kSoloTP, "TEK BASINA SUR (YARISSIZ)", Color{0.2f, 0.4f, 0.3f}, 2);
            std::snprintf(b, sizeof b, "HIZLI GECIS %s", money(c.fastTravelPrice(travelTo_)).c_str());
            button(r, kFastTP, b, c.money >= c.fastTravelPrice(travelTo_) ? Color{0.15f, 0.45f, 0.7f} : Color{0.25f, 0.25f, 0.28f}, 2);
            button(r, kCancelTP, "VAZGEC", kUiBtn, 2);
        }
    }

    // --- Bildirim Mesaji ---
    if (msgT_ > 0) {
        if (land) {
            r.rect(8, 314, 416, 350, {0.02f, 0.02f, 0.04f, 0.94f});
            r.rect(8, 314, 416, 316, kUiGold);
            r.textFit(212, 324, msg_, 2, 396, kUiGold, true);
        } else {
            r.rect(0, 440, 360, 470, {0.02f, 0.02f, 0.04f, 0.92f});
            r.textFit(180, 448, msg_, 2, 344, kUiGold, true);
        }
    }
}

void RegionMapScreen::pointerDown(int, float x, float y) {
    const bool land = landscape();
    const Rect& tourBtn   = land ? kTourL    : kTourP;
    const Rect& backBtn   = land ? kBackML   : kBackMP;
    const Rect& streetBtn = land ? kStreetML : kStreetMP;
    const Rect& runBtn    = land ? kRunTL    : kRunTP;
    const Rect& soloBtn   = land ? kSoloTL   : kSoloTP;
    const Rect& fastBtn   = land ? kFastTL   : kFastTP;
    const Rect& cancelBtn = land ? kCancelTL : kCancelTP;
    const Node* nodes     = land ? kNodesL   : kNodesP;
    const float rHit      = land ? kRLand + 8.0f : kRP + 10.0f;

    if (travelTo_ >= 0) {
        std::string why;
        if (runBtn.hit(x, y)) { const int t = travelTo_; travelTo_ = -1; app_.startTravel(t); }
        else if (soloBtn.hit(x, y)) { const int t = travelTo_; travelTo_ = -1; app_.startTravel(t, true); }
        else if (fastBtn.hit(x, y)) {
            if (app_.career.fastTravel(travelTo_, &why)) { app_.saveCareer(); msg_ = std::string(Career::cityName(app_.career.city)) + "'A GELDIN"; }
            else msg_ = why;
            msgT_ = 2.0; travelTo_ = -1;
        } else if (cancelBtn.hit(x, y)) travelTo_ = -1;
        return;
    }
    if (backBtn.hit(x, y)) { app_.goGarage(); return; }
    if (streetBtn.hit(x, y)) { app_.goStreet(); return; }
    if (tourBtn.hit(x, y)) {
        std::string why;
        if (!app_.career.tourAvailable(&why)) { msg_ = why; msgT_ = 2.0; return; }
        app_.startTour();
        return;
    }
    for (int ci = 0; ci < Career::kCities; ++ci) {
        const float dx = x - nodes[ci].x, dy = y - nodes[ci].y;
        if (dx * dx + dy * dy > rHit * rHit) continue;
        if (Career::cityLeague(ci) > app_.career.leagueUnlocked()) { msg_ = "KILITLI: ONCEKI PATRONU YEN"; msgT_ = 2.0; return; }
        if (ci != app_.career.city) { travelTo_ = ci; return; }
        app_.goLeague(Career::cityLeague(ci));
        return;
    }
}

void RegionMapScreen::key(Key k, bool down) {
    if (!down) return;
    if (k == Key::Back) app_.goGarage();
    else if (k == Key::Enter) app_.goLeague(Career::cityLeague(app_.career.city));
}

} // namespace zk