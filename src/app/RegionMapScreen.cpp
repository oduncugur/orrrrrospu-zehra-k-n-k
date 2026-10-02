// ZEHRA KINIK - Bolge haritasi: kariyer ligleri yollarla bagli bolgeler; altta haftalik turnuva.
#include "Screens.h"
#include "Ui.h"
#include "app/Hints.h"

#include <cmath>
#include <cstdio>

namespace zk {

namespace {
struct Node { float x, y; Color c; };
const Node kNodes[kLeagues] = {
    {92, 410, {0.85f, 0.55f, 0.15f}},   // MAHALLE
    {232, 352, {0.25f, 0.55f, 0.85f}},  // SEHIR
    {104, 268, {0.55f, 0.35f, 0.75f}},  // SEHIRLERARASI
    {250, 180, {0.30f, 0.65f, 0.35f}},  // DAG YOLLARI
    {130, 110, {0.85f, 0.25f, 0.25f}},  // PIST
};
constexpr float kR = 30;
const Rect kTour{8, 528, 352, 566}, kBackM{8, 596, 176, 634}, kStreetM{184, 596, 352, 634};
const Rect kRunT{32, 260, 328, 304}, kFastT{32, 326, 328, 370}, kCancelT{32, 384, 328, 424};   // seyahat penceresi

bool bossBeaten(const Career& c, int league) {
    const auto& ev = leagueEvents();
    for (int i = 0; i < (int)ev.size(); ++i)
        if (ev[i].league == league && ev[i].rival >= 0 && rivals()[ev[i].rival].boss && c.eventWon(i)) return true;
    return false;
}
}

RegionMapScreen::RegionMapScreen(App& app) : app_(app) {
    app_.career.tourRefresh();
    app_.hint(HintMap, hintTexts()[HintMap]);
    if (!app_.eventNote.empty()) { msg_ = app_.eventNote; msgT_ = 3.5; app_.eventNote.clear(); }
}

void RegionMapScreen::render(Renderer& r) {
    const Career& c = app_.career;
    r.begin(360, 640, kUiBg);
    // Arazi: deniz (sol alt), kara, dag golgeleri
    r.gradientV(0, 58, 360, 470, {0.20f, 0.30f, 0.20f}, {0.16f, 0.24f, 0.17f});
    for (int i = 0; i < 18; ++i) {
        const float x = 18.0f + (i * 97) % 330, y = 80.0f + (i * 53) % 370;
        r.circle(x, y, 10.0f + (i * 7) % 14, 10, {0.13f, 0.20f, 0.13f, 0.6f});
    }
    r.tri(200, 230, 290, 120, 360, 230, {0.32f, 0.30f, 0.26f});        // dag
    r.tri(240, 230, 320, 140, 360, 210, {0.40f, 0.38f, 0.33f});
    r.tri(290, 120, 300, 128, 280, 128, {0.9f, 0.9f, 0.95f});          // kar
    r.gradientV(0, 430, 360, 470, {0.10f, 0.25f, 0.45f, 0.0f}, {0.10f, 0.25f, 0.45f, 0.9f});   // sahil
    // Yollar: bolgeleri sirayla baglar (kilitli kisim kesik)
    const int open = c.leagueUnlocked();
    for (int l = 0; l + 1 < kLeagues; ++l) {
        const Node &a = kNodes[l], &b = kNodes[l + 1];
        const float dx = b.x - a.x, dy = b.y - a.y, len = std::sqrt(dx * dx + dy * dy), nx = -dy / len * 4, ny = dx / len * 4;
        const bool unlocked = l + 1 <= open;
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
    // Bolgeler
    const auto& ev = leagueEvents();
    char b[64];
    for (int l = 0; l < kLeagues; ++l) {
        const Node& n = kNodes[l];
        const bool locked = l > open;
        int total = 0;
        for (const EventDef& e : ev) total += e.league == l;
        const float pulse = (l == open && !locked) ? 2.0f + 2.0f * std::sin((float)t_ * 3.0f) : 0.0f;
        r.circle(n.x, n.y + 3, kR + 2, 24, {0, 0, 0, 0.35f});
        r.circle(n.x, n.y, kR + 3 + pulse, 24, locked ? Color{0.25f, 0.25f, 0.27f} : bossBeaten(c, l) ? kUiGold : Color{0.95f, 0.95f, 0.95f});
        r.circle(n.x, n.y, kR, 24, locked ? Color{0.30f, 0.30f, 0.32f} : n.c);
        std::snprintf(b, sizeof b, "%d", l + 1);
        r.textCentered(n.x, n.y - 10, b, 3, locked ? kUiDim : Color{1, 1, 1});
        std::string name = std::string(Career::cityName(l)) + ": " + leagueName(l);   // sehir + lig
        r.textFit(n.x, n.y + kR + 6, name, 1, 170, locked ? kUiDim : Color{1, 1, 1}, true);
        if (l == c.city) {                                                // buradasin: araba isareti
            r.circle(n.x + kR * 0.8f, n.y - kR * 0.8f, 11, 16, {0.1f, 0.6f, 0.95f});
            icon(r, IconGallery, n.x + kR * 0.8f, n.y - kR * 0.8f, 7, {1, 1, 1});
        }
        std::snprintf(b, sizeof b, locked ? "KILITLI" : "%d / %d", c.leagueWins(l), total);
        r.textCentered(n.x, n.y + kR + 17, b, 1, locked ? kUiDim : kUiGold);
    }
    // Ust serit
    r.rect(0, 0, 360, 58, kUiPanel);
    r.text(8, 8, "HARITA", 2, kUiGold);
    std::snprintf(b, sizeof b, "UN %d   GALIBIYET %d", c.rep, c.wins);
    r.text(8, 34, b, 1, kUiText);
    const std::string m = money(c.money);
    r.text(352 - r.textWidth(m, 2), 8, m, 2, {0.4f, 1.0f, 0.5f});
    // Haftalik turnuva
    r.rect(8, 476, 352, 524, {0.08f, 0.09f, 0.13f});
    r.text(16, 482, "HAFTALIK TURNUVA", 1, kUiGold);
    const int daysLeft = 7 - todayIndex() % 7;
    std::snprintf(b, sizeof b, "YENILENME %d GUN", daysLeft);
    r.text(344 - r.textWidth(b, 1), 482, b, 1, kUiDim);
    if (c.tourOut) std::snprintf(b, sizeof b, "BU HAFTA ELENDIN (TUR %d)", c.tourRound + 1);
    else if (c.tourRound >= Career::kTourRounds) std::snprintf(b, sizeof b, "BU HAFTA SAMPIYONSUN!");
    else std::snprintf(b, sizeof b, "TUR %d / %d   GIRIS %s   ODUL %s", c.tourRound + 1, Career::kTourRounds,
                       c.tourRound == 0 ? money(c.tourEntry()).c_str() : "-", money(c.tourPrize()).c_str());
    r.textFit(16, 498, b, 1, 328, c.tourOut ? Color{1.0f, 0.45f, 0.35f} : kUiText);
    for (int k = 0; k < Career::kTourRounds; ++k)                       // tur noktalari
        r.circle(24.0f + k * 18.0f, 515, 5, 10, k < c.tourRound ? Color{0.4f, 1.0f, 0.5f} : c.tourOut && k == c.tourRound ? kUiRed : Color{0.3f, 0.3f, 0.35f});
    std::string why;
    const bool can = c.tourAvailable(&why);
    char tb[48]; std::snprintf(tb, sizeof tb, "TUR %d: DRAG ELEME >", c.tourRound + 1);
    button(r, kTour, can ? std::string(tb) : why, can ? kUiOrange : Color{0.25f, 0.25f, 0.28f}, 2);
    button(r, kBackM, "< GARAJ", kUiBtn, 2);
    button(r, kStreetM, "SOKAK >", {0.35f, 0.15f, 0.45f}, 2);
    if (travelTo_ >= 0) {                                                // seyahat secimi
        r.rect(0, 0, 360, 640, {0, 0, 0, 0.6f});
        r.rect(16, 180, 344, 440, kUiPanel);
        r.rect(16, 180, 344, 183, kUiGold);
        const int legs = std::abs(travelTo_ - c.city);
        std::snprintf(b, sizeof b, "%s -> %s", Career::cityName(c.city), Career::cityName(travelTo_));
        r.textFit(180, 194, b, 2, 320, kUiGold, true);
        std::snprintf(b, sizeof b, "%.0f KM, %d ETAP", c.travelKm(travelTo_), legs);
        r.textCentered(180, 220, b, 1, kUiText);
        std::string rs = "THE RUN: ";
        for (int k = 0; k < legs; ++k) {
            const int from = travelTo_ > c.city ? c.city + k : c.city - k - 1;
            char e[32]; std::snprintf(e, sizeof e, "%s%d ARAC", k ? " + " : "", Career::legField(from)); rs += e;
        }
        r.textFit(180, 236, rs, 1, 320, kUiDim, true);
        button(r, kRunT, "THE RUN ILE GIT >", kUiOrange, 2);
        r.textCentered(180, kRunT.y1 + 4, "ODUL + UN; YAKIT VE SIRA ONEMLI", 1, kUiDim);
        std::snprintf(b, sizeof b, "HIZLI GECIS %s", money(c.fastTravelPrice(travelTo_)).c_str());
        button(r, kFastT, b, c.money >= c.fastTravelPrice(travelTo_) ? Color{0.15f, 0.45f, 0.7f} : Color{0.25f, 0.25f, 0.28f}, 2);
        button(r, kCancelT, "VAZGEC", kUiBtn, 2);
    }
    if (msgT_ > 0) { r.rect(0, 440, 360, 470, {0.02f, 0.02f, 0.04f, 0.92f}); r.textFit(180, 448, msg_, 2, 344, kUiGold, true); }
}

void RegionMapScreen::pointerDown(int, float x, float y) {
    if (travelTo_ >= 0) {                                                // seyahat penceresi (modal)
        std::string why;
        if (kRunT.hit(x, y)) { const int t = travelTo_; travelTo_ = -1; app_.startTravel(t); }
        else if (kFastT.hit(x, y)) {
            if (app_.career.fastTravel(travelTo_, &why)) { app_.saveCareer(); msg_ = std::string(Career::cityName(app_.career.city)) + "'A GELDIN"; }
            else msg_ = why;
            msgT_ = 2.0; travelTo_ = -1;
        } else if (kCancelT.hit(x, y)) travelTo_ = -1;
        return;
    }
    if (kBackM.hit(x, y)) { app_.goGarage(); return; }
    if (kStreetM.hit(x, y)) { app_.goStreet(); return; }
    if (kTour.hit(x, y)) {
        std::string why;
        if (!app_.career.tourAvailable(&why)) { msg_ = why; msgT_ = 2.0; return; }
        app_.startTour();
        return;
    }
    for (int l = 0; l < kLeagues; ++l) {
        const float dx = x - kNodes[l].x, dy = y - kNodes[l].y;
        if (dx * dx + dy * dy > (kR + 12) * (kR + 12)) continue;
        if (l > app_.career.leagueUnlocked()) { msg_ = "KILITLI: ONCEKI PATRONU YEN"; msgT_ = 2.0; return; }
        if (l != app_.career.city) { travelTo_ = l; return; }           // baska sehir: seyahat (The Run / hizli gecis)
        app_.goLeague(l);
        return;
    }
}

void RegionMapScreen::key(Key k, bool down) {
    if (!down) return;
    if (k == Key::Back) app_.goGarage();
    else if (k == Key::Enter) app_.goLeague(app_.career.city);
}

} // namespace zk
