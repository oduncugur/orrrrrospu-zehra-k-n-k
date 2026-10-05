// ZEHRA KINIK - Kariyer: ligler, etkinlikler (isimli rakipler, patron, pink slip), un puani, gunluk gorevler.
#include "Screens.h"
#include "app/Hints.h"
#include "Ui.h"
#include "garage/VehicleCatalog.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace zk {

namespace {
const Rect kBackL{8, 596, 352, 634};
const Rect kStartL{8, 548, 352, 590};
const Rect kYesL{8, 548, 176, 590}, kNoL{184, 548, 352, 590};
const Rect kWagerL{214, 514, 352, 542};
const Rect kSponsorL{8, 530, 352, 546};                                // sponsor satiri (etkinlik secili degilken)                                // bahis kademesi (onay adiminda)
const Rect kAchL{262, 30, 352, 54};
constexpr float kRowY0 = 114;
float rowH(size_t n) { return std::min(44.0f, (464.0f - kRowY0) / std::max<size_t>(1, n)); }   // lig 9 etkinlige kadar sigar
}

LeagueScreen::LeagueScreen(App& app, int tab) : app_(app) {
    app_.hint(HintLeague, hintTexts()[HintLeague]);
    app_.career.dailyRefresh();
    tab_ = tab >= 0 ? std::min(tab, kLeagues - 1) : app_.career.leagueUnlocked();
    if (!app_.eventNote.empty()) { msg_ = app_.eventNote; msgT_ = 3.5; app_.eventNote.clear(); }
}

std::vector<int> LeagueScreen::rows() const {
    std::vector<int> r;
    const auto& ev = leagueEvents();
    for (int i = 0; i < (int)ev.size(); ++i) if (ev[i].league == tab_ && eventCity(i) == viewCity()) r.push_back(i);
    return r;
}

void LeagueScreen::render(Renderer& r) {
    if (!app_.career.sponsorNote.empty()) { msg_ = app_.career.sponsorNote; msgT_ = 3.0; app_.career.sponsorNote.clear(); }   // sponsor olayi
    const Career& c = app_.career;
    r.begin(360, 640, kUiBg);
    r.rect(0, 0, 360, 58, kUiPanel);
    r.text(8, 8, "KARIYER", 2, kUiGold);
    char b[96];
    std::snprintf(b, sizeof b, "UN %d   YARIS %d  GALIBIYET %d", c.rep, c.races, c.wins);
    r.text(8, 32, b, 1, kUiText);
    const std::string m = money(c.money);
    r.text(352 - r.textWidth(m, 2), 8, m, 2, {0.4f, 1.0f, 0.5f});
    {
        int n = 0; for (uint32_t a = c.achieved; a; a >>= 1) n += a & 1u;
        std::snprintf(b, sizeof b, "BASARIM %d", n);
        button(r, kAchL, b, {0.45f, 0.35f, 0.08f}, 1);
    }
    // Lig sekmeleri
    const int open = c.leagueUnlocked();
    for (int l = 0; l < kLeagues; ++l) {
        const Rect t{4.0f + l * 70.8f, 64, 72.0f + l * 70.8f, 92};
        const bool locked = l > open;
        r.rect(t.x0, t.y0, t.x1, t.y1, l == tab_ ? kUiOrange : locked ? Color{0.12f, 0.12f, 0.14f} : kUiPanel);
        std::string n = leagueName(l);
        if (n.size() > 10) n = n.substr(0, 9) + ".";
        r.textCentered(t.cx(), t.y0 + 5, n, 1, locked ? kUiDim : Color{1, 1, 1});
        std::snprintf(b, sizeof b, "%d/%d", c.leagueWins(l), (int)std::count_if(leagueEvents().begin(), leagueEvents().end(), [&](const EventDef& e) { return e.league == l; }));
        r.textCentered(t.cx(), t.y0 + 16, locked ? "KILITLI" : b, 1, locked ? kUiDim : kUiGold);
    }
    const double cap = leagueIndexCap(tab_);
    std::snprintf(b, sizeof b, cap > 0 ? "SINIF: ENDEKS %.0f VE ALTI" : "SINIF: SINIRSIZ", cap);
    r.text(8, 99, b, 1, kUiDim);
    {   // sehir secici: ligin iki sehri (bulundugun sehir yesil)
        std::snprintf(b, sizeof b, "%s >", Career::cityName(viewCity()));
        const Rect cb{200, 95, 352, 111};
        r.rect(cb.x0, cb.y0, cb.x1, cb.y1, viewCity() == c.city ? Color{0.12f, 0.35f, 0.18f} : kUiPanel);
        r.textCentered(cb.cx(), cb.y0 + 4, b, 1, {1, 1, 1});
    }
    // Etkinlikler
    const auto& ev = leagueEvents();
    const auto list = rows();
    for (size_t k = 0; k < list.size(); ++k) {
        const int i = list[k];
        const EventDef& e = ev[i];
        const float kRowH = rowH(list.size());
        const float y = kRowY0 + k * kRowH;
        const bool won = c.eventWon(i);
        const bool boss = e.rival >= 0 && rivals()[e.rival].boss;
        std::string why;
        const bool avail = c.eventAvailable(i, &why);
        r.rect(8, y, 352, y + kRowH - 4, i == sel_ ? Color{0.2f, 0.24f, 0.36f} : won ? Color{0.10f, 0.22f, 0.13f} : kUiPanel);
        if (boss) r.rect(8, y, 12, y + kRowH - 4, kUiGold);
        else if (e.pink) r.rect(8, y, 12, y + kRowH - 4, {1.0f, 0.4f, 0.6f});
        r.text(18, y + 5, e.name, 1, boss ? kUiGold : Color{1, 1, 1});
        std::string sub = eventModeName(e.mode);
        if (e.rival >= 0) sub += std::string("  ") + rivals()[e.rival].name;
        else if (e.mode == EventMode::Flow) { std::snprintf(b, sizeof b, "  HEDEF %ld", e.flowTarget); sub += b; }
        else if (e.mode == EventMode::Chase) sub += "  400 M ACIL";
        else if (e.mode == EventMode::Marathon) sub += "  18 KM, BENZINLIK";
        else sub += "  DENGI RAKIP";
        r.text(18, y + kRowH - 22, sub.substr(0, 40), 1, kUiDim);
        std::string right = won ? "KAZANILDI" : e.pink ? "ARABA" : money(c.eventPrize(i, true));
        if (!avail && !won) right = "KILITLI";
        r.text(344 - r.textWidth(right, 1), y + 5, right, 1, won ? Color{0.4f, 1.0f, 0.5f} : !avail ? kUiDim : kUiGold);
        std::snprintf(b, sizeof b, "+%d UN", e.rep);
        r.text(344 - r.textWidth(b, 1), y + kRowH - 22, b, 1, {0.55f, 0.75f, 1.0f});
    }
    // Alt panel: secili etkinlik ayrintisi ya da gunluk gorevler
    const float py = 470;
    r.rect(8, py, 352, py + 74, {0.08f, 0.09f, 0.13f});
    if (sel_ >= 0) {
        const EventDef& e = ev[sel_];
        std::string why;
        const bool avail = c.eventAvailable(sel_, &why);
        if (e.rival >= 0) {
            const RivalDef& rv = rivals()[e.rival];
            r.text(16, py + 6, std::string("RAKIP: ") + rv.name + (rv.boss ? " (PATRON)" : ""), 1, rv.boss ? kUiGold : Color{1, 1, 1});
            r.text(16, py + 20, upper(findVehicle(rv.carId)->fullName()).substr(0, 40), 1, kUiText);
            static const char* pre[3] = {"STOK", "SOKAK PAKETI", "DRAG PAKETI"};
            std::snprintf(b, sizeof b, "PARCA: %s   TAHMINI 1/4: %.2f S", pre[std::clamp(rv.preset, 0, 2)], tableEt(rv.carId, rv.preset));
            r.text(16, py + 34, b, 1, kUiDim);
            if (rv.boss) r.textFit(16, py - 14, std::string("\"") + bossLine(e.rival, 0) + "\"", 1, 336, {1.0f, 0.85f, 0.45f});   // meydan okuma
        } else r.text(16, py + 6, e.mode == EventMode::Flow ? "SKOR HEDEFINI GEC" : e.mode == EventMode::Chase ? "POLISTEN KAC: 400 M ACIL VE 4 S TUT" : e.mode == EventMode::Marathon ? "18 KM: YAKIT BITMEDEN BENZINLIGE GIR"
                                                                                                              : "SENIN SEVIYENDE BIR RAKIP", 1, {1, 1, 1});
        if (e.pink) r.text(16, py + 50, "PINK SLIP: KAYBEDERSEN ARABAN GIDER!", 1, {1.0f, 0.4f, 0.5f});
        else if (!avail) r.text(16, py + 50, why, 1, {1.0f, 0.4f, 0.3f});
        else {
            std::snprintf(b, sizeof b, "ODUL %s (TEKRAR %s)", money(c.eventPrize(sel_, true)).c_str(), money(c.eventPrize(sel_, false)).c_str());
            r.text(16, py + 50, b, 1, kUiGold);
            if (confirm_) {                                               // bahis: kazanirsan +, kaybedersen - ayni miktar
                const long w = c.wagerFor(sel_, wagerStep_);
                std::snprintf(b, sizeof b, w > 0 ? "BAHIS %s" : "BAHIS YOK", money(w).c_str());
                button(r, kWagerL, b, w > 0 ? Color{0.55f, 0.35f, 0.05f} : kUiBtn, 1);
            }
        }
        if (confirm_) {
            button(r, kYesL, "YARIS!", {0.75f, 0.15f, 0.2f}, 2);
            button(r, kNoL, "VAZGEC", kUiBtn, 2);
        } else button(r, kStartL, avail ? "YARISA GIR >" : "KILITLI", avail ? kUiOrange : Color{0.25f, 0.25f, 0.28f}, 2);
    } else {
        r.text(16, py + 6, "GUNLUK GOREVLER", 1, kUiGold);
        const auto tasks = dailyTasks(c.dailyDay, estimatedEt(*findVehicle(c.car().carId), c.car().tune));
        for (int i = 0; i < 3; ++i) {
            const bool done = (c.dailyDone >> i) & 1;
            r.text(16, py + 20 + i * 16, taskText(tasks[i]), 1, done ? Color{0.4f, 1.0f, 0.5f} : kUiText);
            std::snprintf(b, sizeof b, done ? "TAMAM" : "%s", money(tasks[i].reward).c_str());
            r.text(344 - r.textWidth(b, 1), py + 20 + i * 16, b, 1, done ? Color{0.4f, 1.0f, 0.5f} : kUiGold);
        }
        {   // sponsor: suren sozlesme ya da teklif (dokun: kabul)
            if (c.sponsor >= 0) {
                const SponsorDef& d = sponsors()[c.sponsor];
                std::snprintf(b, sizeof b, "%s: %d/%d GALIBIYET  %d/%d YARIS", d.name, c.spWins, d.wins, c.spRaces, d.races);
                r.textFit(16, 534, b, 1, 336, {0.55f, 0.85f, 1.0f});
            } else if (const int o = c.sponsorOffer(); o >= 0) {
                const SponsorDef& d = sponsors()[o];
                std::snprintf(b, sizeof b, "SPONSOR TEKLIFI %s: %d YARISTA %d GAL, BONUS %s  [KABUL]", d.name, d.races, d.wins, money(d.bonus).c_str());
                r.textFit(16, 534, b, 1, 336, kUiGold);
            }
        }
        r.textCentered(180, 562, "BIR ETKINLIK SEC", 1, kUiDim);
    }
    button(r, kBackL, "< HARITA", kUiBtn, 2);
    if (msgT_ > 0) { r.rect(0, 430, 360, 462, {0.02f, 0.02f, 0.04f, 0.92f}); r.textFit(180, 440, msg_, 2, 344, kUiGold, true); }
}

void LeagueScreen::pointerDown(int, float x, float y) {
    if (confirm_ && sel_ >= 0) {
        if (kYesL.hit(x, y)) { app_.career.wager = leagueEvents()[sel_].pink ? 0 : app_.career.wagerFor(sel_, wagerStep_); app_.startEvent(sel_); return; }
        if (kNoL.hit(x, y)) { confirm_ = false; return; }
        if (!leagueEvents()[sel_].pink && kWagerL.hit(x, y)) { wagerStep_ = (wagerStep_ + 1) % Career::kWagerSteps; return; }
    }
    if (sel_ < 0 && kSponsorL.hit(x, y)) {
        Career& c = app_.career;
        if (c.signSponsor(c.sponsorOffer())) {
            const SponsorDef& d = sponsors()[c.sponsor];
            char b[96]; std::snprintf(b, sizeof b, "SOZLESME: GALIBIYET BASI %s, BOZULURSA -%s", money(d.perWin).c_str(), money(d.penalty).c_str());
            msg_ = b; msgT_ = 2.6;
        }
        return;
    }
    if (kBackL.hit(x, y)) { app_.goMap(); return; }
    if (kAchL.hit(x, y)) { app_.goAchievements(); return; }
    for (int l = 0; l < kLeagues; ++l)
        if (Rect{4.0f + l * 70.8f, 64, 72.0f + l * 70.8f, 92}.hit(x, y)) { tab_ = l; side_ = -1; sel_ = -1; confirm_ = false; return; }
    if (Rect{200, 95, 352, 111}.hit(x, y)) { side_ = viewCity() % 2 ? 0 : 1; sel_ = -1; confirm_ = false; return; }
    if (sel_ >= 0 && kStartL.hit(x, y)) {
        std::string why;
        if (!app_.career.eventAvailable(sel_, &why)) { msg_ = why; msgT_ = 2.0; return; }
        if (eventCity(sel_) != app_.career.city) {                        // etkinlik baska sehirde: haritadan seyahat
            msg_ = std::string("ONCE ") + Career::cityName(eventCity(sel_)) + " (HARITA)"; msgT_ = 2.2; return;
        }
        confirm_ = true;                                                  // onay (pink slip uyarisi panelde)
        return;
    }
    const auto list = rows();
    for (size_t k = 0; k < list.size(); ++k)
        if (Rect{8, kRowY0 + k * rowH(list.size()), 352, kRowY0 + (k + 1) * rowH(list.size()) - 4}.hit(x, y)) { sel_ = list[k]; confirm_ = false; return; }
}

void LeagueScreen::key(Key k, bool down) {
    if (!down) return;
    if (k == Key::Back) { if (confirm_) confirm_ = false; else if (sel_ >= 0) sel_ = -1; else app_.goMap(); }
    if (k == Key::Left) { tab_ = std::max(0, tab_ - 1); side_ = -1; sel_ = -1; }
    if (k == Key::Right) { tab_ = std::min(kLeagues - 1, tab_ + 1); side_ = -1; sel_ = -1; }
    if (k == Key::Enter && sel_ >= 0) { if (confirm_) { app_.career.wager = leagueEvents()[sel_].pink ? 0 : app_.career.wagerFor(sel_, wagerStep_); app_.startEvent(sel_); } else if (app_.career.eventAvailable(sel_)) confirm_ = true; }
}

} // namespace zk
