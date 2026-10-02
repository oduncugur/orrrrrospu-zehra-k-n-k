// ZEHRA KINIK - Sokak: gece bulusmasi (bahisli drag, polis baskini riski), haftalik dyno yarismasi, musteri isleri.
// Kariyer mantigi Career (meet* / dyno* / job*); bu ekran yalniz gosterir ve baslatir.
#include "Screens.h"
#include "Ui.h"

#include <algorithm>
#include <cstdio>
#include <ctime>

namespace zk {

namespace {
const char* const kTabName[3] = {"BULUSMA", "DYNO", "MUSTERI"};
Rect tabR(int i) { return {8 + i * 116.0f, 64, 120 + i * 116.0f, 96}; }
const Rect kGo{8, 300, 352, 344}, kBackSt{8, 596, 352, 634};
Rect jobR(int k) { return {8, 110 + k * 120.0f, 352, 222 + k * 120.0f}; }
Rect jobBtn(int k) { return {232, 176 + k * 120.0f, 344, 214 + k * 120.0f}; }
}

StreetScreen::StreetScreen(App& app) : app_(app) {
    if (!app_.eventNote.empty()) { msg_ = app_.eventNote; msgT_ = 4.0; app_.eventNote.clear(); }
}

void StreetScreen::render(Renderer& r) {
    const Career& c = app_.career;
    r.begin(360, 640, kUiBg);
    r.rect(0, 0, 360, 58, kUiPanel);
    r.text(8, 8, "SOKAK", 2, kUiGold);
    r.text(8, 34, "GECE BULUSMASI, DYNO YARISMASI, MUSTERI ISLERI", 1, kUiDim);
    const std::string m = money(c.money);
    r.text(352 - r.textWidth(m, 2), 8, m, 2, {0.4f, 1.0f, 0.5f});
    for (int i = 0; i < 3; ++i) button(r, tabR(i), kTabName[i], i == tab_ ? kUiOrange : kUiBtn, 1);
    char b[96];
    std::string why;
    if (tab_ == 0) {                                                   // gece bulusmasi
        const Opponent o = c.meetOpponent();
        r.rect(8, 110, 352, 290, {0.06f, 0.06f, 0.12f});
        for (int i = 0; i < 24; ++i) r.circle(20.0f + (i * 67) % 330, 118.0f + (i * 29) % 60, 1.0f, 4, {1, 1, 0.8f, 0.6f});   // yildizlar
        r.text(16, 120, "BU GECENIN RAKIBI", 1, kUiDim);
        r.textFit(16, 136, upper(findVehicle(o.carId)->fullName()), 2, 328, {1, 1, 1});
        std::snprintf(b, sizeof b, "TAHMINI 1/4: %.2f S", o.estEt);
        r.text(16, 162, b, 1, kUiText);
        std::snprintf(b, sizeof b, "BAHIS %s   KAZANIRSAN %s", money(c.meetStake()).c_str(), money(2 * c.meetStake()).c_str());
        r.text(16, 186, b, 1, kUiGold);
        std::snprintf(b, sizeof b, "POLIS BASKINI RISKI %%%d (CEZA: BAHSIN YARISI)", (int)(Career::kRaidChance * 100));
        r.text(16, 204, b, 1, {1.0f, 0.5f, 0.4f});
        const long left = 3 * 3600 - (long)(std::time(nullptr) % (3 * 3600));
        std::snprintf(b, sizeof b, "YENI BULUSMA: %ld SA %ld DK", left / 3600, left % 3600 / 60);
        r.text(16, 268, b, 1, kUiDim);
        const bool can = c.meetAvailable(&why);
        button(r, kGo, can ? "BULUSMAYA GIT >" : why, can ? kUiOrange : Color{0.25f, 0.25f, 0.28f}, 2);
    } else if (tab_ == 1) {                                            // dyno yarismasi
        r.rect(8, 110, 352, 290, kUiPanel);
        r.text(16, 120, "HAFTALIK DYNO YARISMASI", 1, kUiGold);
        const double hp = peakHpOf(*findVehicle(c.car().carId), c.car().tune);
        std::snprintf(b, sizeof b, "SENIN GUCUN: %.0f HP", hp);
        r.text(16, 140, b, 2, {1, 1, 1});
        std::snprintf(b, sizeof b, "GIRIS %s", money(c.dynoEntryFee()).c_str());
        r.text(16, 170, b, 1, kUiText);
        const long fee = c.dynoEntryFee();
        std::snprintf(b, sizeof b, "1. %s   2. %s   3. %s", money(fee * 5).c_str(), money(fee * 5 / 2).c_str(), money(fee * 6 / 5).c_str());
        r.text(16, 186, b, 1, kUiGold);
        r.text(16, 210, "10 ARAC DYNODA: EN YUKSEK BEYGIR KAZANIR", 1, kUiDim);
        r.text(16, 224, "RAKIPLER SENIN SEVIYENDE, HER HAFTA DEGISIR", 1, kUiDim);
        const bool can = c.dynoAvailable(&why);
        button(r, kGo, can ? "DYNOYA CIK >" : why, can ? kUiOrange : Color{0.25f, 0.25f, 0.28f}, 2);
        if (!board_.empty()) {                                         // son sonuc tablosu
            r.text(16, 356, "SONUC", 1, kUiGold);
            for (size_t i = 0; i < board_.size(); ++i) {
                const auto& e = board_[i];
                const float y = 372 + i * 20.0f;
                r.rect(8, y - 2, 352, y + 16, e.player ? Color{0.15f, 0.30f, 0.18f} : (i % 2 ? kUiPanel : Color{0.09f, 0.09f, 0.12f}));
                std::snprintf(b, sizeof b, "%2zu. %s", i + 1, e.name);
                r.text(14, y + 3, b, 1, e.player ? kUiGold : Color{1, 1, 1});
                r.textFit(150, y + 3, upper(findVehicle(e.carId)->model), 1, 130, kUiDim);
                std::snprintf(b, sizeof b, "%.0f HP", e.hp);
                r.text(344 - r.textWidth(b, 1), y + 3, b, 1, e.player ? kUiGold : kUiText);
            }
        }
    } else {                                                           // musteri isleri
        for (int k = 0; k < 3; ++k) {
            const Career::JobOffer o = c.jobOffer(k);
            const Rect R = jobR(k);
            const bool taken = c.jobTaken(k);
            r.rect(R.x0, R.y0, R.x1, R.y1, taken ? Color{0.10f, 0.14f, 0.10f} : kUiPanel);
            r.textFit(16, R.y0 + 8, upper(findVehicle(o.carId)->fullName()), 1, 328, {1, 1, 1});
            std::snprintf(b, sizeof b, "FABRIKA %d HP  ->  HEDEF %d HP", o.stockHp, o.targetHp);
            r.textFit(16, R.y0 + 26, b, 2, 328, kUiText);
            std::snprintf(b, sizeof b, "ODEME %s (PARCALAR SENDEN)", money(o.reward).c_str());
            r.text(16, R.y0 + 52, b, 1, kUiGold);
            r.text(16, R.y0 + 72, "ARAC GARAJINA GELIR,", 1, kUiDim);
            r.text(16, R.y0 + 86, "HEDEFE ULASINCA TESLIM ET", 1, kUiDim);
            if (taken) r.textCentered(jobBtn(k).cx(), jobBtn(k).y0 + 14, "ALINDI", 1, {0.4f, 1.0f, 0.5f});
            else button(r, jobBtn(k), "ISI AL", c.garageFull() ? Color{0.25f, 0.25f, 0.28f} : kUiGreen, 2);
        }
    }
    button(r, kBackSt, "< HARITA", kUiBtn, 2);
    if (msgT_ > 0) { r.rect(0, 556, 360, 588, {0.02f, 0.02f, 0.04f, 0.92f}); r.textFit(180, 566, msg_, 2, 344, kUiGold, true); }
}

void StreetScreen::pointerDown(int, float x, float y) {
    if (kBackSt.hit(x, y)) { app_.goMap(); return; }
    for (int i = 0; i < 3; ++i) if (tabR(i).hit(x, y)) { tab_ = i; return; }
    Career& c = app_.career;
    std::string why;
    if (tab_ == 0 && kGo.hit(x, y)) {
        if (!c.meetAvailable(&why)) { msg_ = why; msgT_ = 2.0; return; }
        app_.startMeet();
    } else if (tab_ == 1 && kGo.hit(x, y)) {
        int place = 0; long prize = 0;
        if (!c.dynoEnter(&place, &prize, &board_, &why)) { msg_ = why; msgT_ = 2.0; return; }
        app_.saveCareer();
        char b[64];
        if (prize > 0) std::snprintf(b, sizeof b, "%d. OLDUN! +%s", place, money(prize).c_str());
        else std::snprintf(b, sizeof b, "%d. OLDUN", place);
        msg_ = b; msgT_ = 3.0;
    } else if (tab_ == 2) {
        for (int k = 0; k < 3; ++k) {
            if (!jobBtn(k).hit(x, y) || c.jobTaken(k)) continue;
            if (c.acceptJob(k, &why)) { app_.saveCareer(); msg_ = "ARAC GARAJINDA: HEDEFE CIKAR"; }
            else msg_ = why;
            msgT_ = 2.2;
            return;
        }
    }
}

void StreetScreen::key(Key k, bool down) {
    if (!down) return;
    if (k == Key::Back) app_.goMap();
    else if (k == Key::Left) tab_ = (tab_ + 2) % 3;
    else if (k == Key::Right) tab_ = (tab_ + 1) % 3;
}

} // namespace zk
