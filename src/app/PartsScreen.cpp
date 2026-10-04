// ZEHRA KINIK - Parca dukkani (5 sekme, 37 kategori, kaydirmali secenek listesi, onizleme + onay) ve
// ozel uretim atolyesi (turbo / kam / stroker / son disli / vites oranlari / kanat).
#include "Screens.h"
#include "app/Hints.h"
#include "Ui.h"
#include "garage/VehicleCatalog.h"
#include "sim/PartTables.h"
#include "sim/VehicleSim.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <thread>

namespace zk {

namespace {
constexpr double kHpK = 2.0 * 3.14159265358979 / 60.0 / 745.7;
const Rect kBackP{8, 596, 352, 634};
const Rect kSoftP{230, 100, 352, 126};        // ECU kategorisi: yazilim ekrani
// Kategori listesi satir yuksekligi: sekmedeki kategori sayisina gore (BESLEME 13 kategori)
float catRowH(int tab) {
    int n = 0;
    for (int i = 0; i < (int)PartCat::Count; ++i) n += partTab((PartCat)i) == tab;
    return std::min(40.0f, (588.0f - 132.0f) / std::max(1, n));
}
const Rect kYesP{8, 548, 176, 590}, kNoP{184, 548, 352, 590};
const Rect kNewP{8, 548, 124, 590}, kUsedP{128, 548, 244, 590}, kNo3P{248, 548, 352, 590};   // yeni / ikinci el / vazgec
constexpr float kListY0 = 132, kListY1 = 446, kRowH = 40;

std::vector<std::pair<double, double>> effCurve(const EngineSpec& e) {
    std::vector<std::pair<double, double>> out;
    for (size_t i = 0; i < e.lowCam.size(); ++i) {
        const double rpm = e.lowCam[i].first;
        out.push_back({rpm, rpm >= e.vtecRpm && i < e.highCam.size() ? e.highCam[i].second : e.lowCam[i].second});
    }
    return out;
}
void headerP(Renderer& r, const App& app, const char* title) {
    r.rect(0, 0, 360, 58, kUiPanel);
    r.text(8, 8, title, 2, kUiGold);
    const VehicleDef& v = *findVehicle(app.career.car().carId);
    r.text(8, 32, upper(v.fullName()).substr(0, 28), 1, kUiText);
    const std::string m = money(app.career.money);
    r.text(352 - r.textWidth(m, 2), 8, m, 2, {0.4f, 1.0f, 0.5f});
}
Color loadColor(double f) { return f < 0.85 ? Color{0.4f, 1.0f, 0.5f} : f < 1.0 ? kUiGold : Color{1.0f, 0.3f, 0.25f}; }
} // namespace

// Parca setinin ozeti: guc, tork, endeks, agirlik, guc egrisi ve yuk oranlari (tepe tork / dayanim, isi)
TuneStats tuneStats(const VehicleDef& v, const Tune& t) {
    VehicleSimConfig cfg; cfg.car = &v; cfg.tune = &t;
    const VehicleSim sim(cfg);
    TuneStats s;
    s.redline = sim.engineSpec().redlineRpm;
    for (auto& p : effCurve(sim.engineSpec()))
        if (p.first <= s.redline) {
            s.hp = std::max(s.hp, p.second * p.first * kHpK); s.nm = std::max(s.nm, p.second);
            s.hpCurve.push_back({p.first, p.second * p.first * kHpK});
        }
    const NosOpt& n = nosTable()[std::clamp(t.nitrous, 0, (int)nosTable().size() - 1)];
    s.idx = performanceIndex(v, t);
    s.mass = sim.baseMassKg();
    s.octane = sim.octane(); s.octaneReq = sim.octaneRequired();
    s.fuelCapped = sim.fuelCapped();
    s.valveSafe = sim.valveSafeRpm();
    const double peakT = s.nm + (n.hp > 0 ? n.hp * 7120.9 / 4000.0 : 0.0);   // nitro tepe tork (~4000 rpm)
    s.engineLoad = peakT / std::max(1.0, sim.engineRatingNm());
    s.gearboxLoad = peakT / std::max(1.0, sim.gearboxRatingNm());
    // Isi yuku: tam guc / sogutma kapasitesi (1.0 = 40 m/s'de 105 C dengesi)
    const double factoryHp = peakPowerHp(v);
    s.heatLoad = s.hp / std::max(1.0, factoryHp * 1.33 * coolingCapMul(t));   // stok ~%75 (radyator x fan x termostat x yag sogutucu)
    // Aks riski (tahmin): debriyajin aktarabilecegi tork / aksin kirilma torku
    double TmaxF = 0;
    const EngineSpec f = buildEngineSpec(v);
    for (auto* c : {&f.lowCam, &f.highCam}) for (auto& p : *c) TmaxF = std::max(TmaxF, p.second);
    const double clutchT = s.nm * 1.7 * clutchTable()[std::clamp(t.clutch, 0, (int)clutchTable().size() - 1)].mul;
    const AxleOpt& ax = axleTable()[std::clamp(t.axles, 0, (int)axleTable().size() - 1)];
    const double am = ax.spec == 2 ? 3.418 : ax.spec == 1 ? 2.107 : 1.0;
    s.axleRisk = clutchT / (1.7 * TmaxF / 0.75 * am * std::pow(ax.dia, 3.0));
    return s;
}

// ================================================================== PARCA DUKKANI
PartsScreen::PartsScreen(App& app, int cat) : app_(app), cat_(cat) {
    app_.hint(HintParts, hintTexts()[HintParts]);
    if (cat_ >= 0) tab_ = partTab((PartCat)cat_);
    recompute();
}

void PartsScreen::recompute() {
    const OwnedCar& oc = app_.career.car();
    now_ = tuneStats(*findVehicle(oc.carId), oc.tune);
    sel_ = -1; confirm_ = false;
}

void PartsScreen::select(int i) {
    const OwnedCar& oc = app_.career.car();
    Tune t = oc.tune;
    setPartLevel(t, (PartCat)cat_, i, *findVehicle(oc.carId));
    prev_ = tuneStats(*findVehicle(oc.carId), t);
    sel_ = i; confirm_ = false;
    et_ = std::make_shared<EtJob>();
    std::thread([job = et_, car = findVehicle(oc.carId), cur = oc.tune, nxt = t]() {
        job->cur = DragRace::estimateQuarter(car, &cur);
        job->nxt = DragRace::estimateQuarter(car, &nxt);
        job->done = true;
    }).detach();
}

void PartsScreen::drawStatsBar(Renderer& r) {
    char b[96];
    std::snprintf(b, sizeof b, "%.0f HP  %.0f NM  ENDEKS %.0f", now_.hp, now_.nm, now_.idx);
    r.text(8, 64, b, 2, {1, 1, 1});
    auto bar = [&](float x, const char* name, double f) {
        r.text(x, 86, name, 1, kUiDim);
        const float w = 66;
        r.rect(x + 42, 86, x + 42 + w, 93, {0.15f, 0.15f, 0.18f});
        r.rect(x + 42, 86, x + 42 + w * (float)std::min(1.0, f), 93, loadColor(f));
    };
    bar(8, "MOTOR", now_.engineLoad); bar(126, "SANZ.", now_.gearboxLoad); bar(244, "ISI", now_.heatLoad);
}

void PartsScreen::drawPreview(Renderer& r, float py) {
    const auto& opts = partOptions((PartCat)cat_);
    r.rect(8, py, 352, py + 94, {0.08f, 0.09f, 0.13f});
    r.text(16, py + 5, ("ONIZLEME: " + std::string(opts[sel_].name)).substr(0, prev_.fuelCapped ? 24 : 34), 1, kUiGold);
    if (prev_.fuelCapped) r.text(344 - r.textWidth("YAKIT SINIRI!", 1), py + 5, "YAKIT SINIRI!", 1, {1.0f, 0.35f, 0.3f});   // pompa / enjektor al
    const Color up{0.4f, 1.0f, 0.5f}, down{1.0f, 0.4f, 0.3f};
    auto line = [&](float y, float x, const char* name, double a, double bnew, const char* fmt, bool higherBetter) {
        char t[64]; std::snprintf(t, sizeof t, fmt, a, bnew);
        const double d = bnew - a;
        const bool same = std::fabs(d) < 1e-6 * std::max(1.0, std::fabs(a));
        r.text(x, y, name, 1, kUiText);
        r.text(x + 56, y, t, 1, same ? kUiDim : (d > 0) == higherBetter ? up : down);
    };
    line(py + 19, 16, "GUC", now_.hp, prev_.hp, "%.0f > %.0f", true);
    line(py + 31, 16, "TORK", now_.nm, prev_.nm, "%.0f > %.0f", true);
    line(py + 43, 16, "ENDEKS", now_.idx, prev_.idx, "%.0f > %.0f", true);
    line(py + 55, 16, "AGIRLIK", now_.mass, prev_.mass, "%.0f > %.0f", false);
    line(py + 19, 180, "MOTOR", now_.engineLoad * 100, prev_.engineLoad * 100, "%%%.0f > %%%.0f", false);
    line(py + 31, 180, "SANZIMAN", now_.gearboxLoad * 100, prev_.gearboxLoad * 100, "%%%.0f > %%%.0f", false);
    line(py + 43, 180, "ISI", now_.heatLoad * 100, prev_.heatLoad * 100, "%%%.0f > %%%.0f", false);
    line(py + 55, 180, "AKS", now_.axleRisk * 100, prev_.axleRisk * 100, "%%%.0f > %%%.0f", false);
    line(py + 67, 16, "DEVIR", now_.redline, prev_.redline, "%.0f > %.0f", true);         // devir siniri (kesici)
    {   // guvenli (max) devir: supap / kam / kafa ile artar; ECU kesiciyi bunun ustune acarsa supap zorlanir
        char t[32]; std::snprintf(t, sizeof t, "%.0f > %.0f", now_.valveSafe, prev_.valveSafe);
        r.text(180, py + 67, "MAX DEV", 1, kUiText);
        r.text(236, py + 67, t, 1, prev_.redline > prev_.valveSafe ? down : prev_.valveSafe > now_.valveSafe + 1 ? up : kUiDim);
    }
    r.text(16, py + 81, "1/4 MIL", 1, kUiText);
    if (!et_ || !et_->done) r.text(72, py + 81, "HESAPLANIYOR...", 1, kUiDim);
    else {
        auto str = [](const QuarterEstimate& e) {
            if (e.broke) return std::string("AKS!");
            if (e.quarter < 0) return std::string("--");
            char t[24]; std::snprintf(t, sizeof t, "%.2f", e.quarter); return std::string(t);
        };
        const bool ok = et_->cur.quarter > 0 && et_->nxt.quarter > 0;
        const double d = ok ? et_->nxt.quarter - et_->cur.quarter : 0.0;
        r.text(72, py + 81, str(et_->cur) + " > " + str(et_->nxt) + " S", 1, et_->nxt.broke ? down : !ok || std::fabs(d) < 0.005 ? kUiDim : d < 0 ? up : down);
    }
    if (prev_.engineLoad > 1.0) r.text(180, py + 81, "MOTOR DAYANMAZ!", 1, down);
    else if (prev_.gearboxLoad > 1.0) r.text(180, py + 81, "SANZIMAN DAYANMAZ!", 1, down);
    else if (prev_.octaneReq > prev_.octane) r.text(180, py + 81, "VURUNTU! OKTAN", 1, down);
    else if (prev_.heatLoad > 1.15) r.text(180, py + 81, "SOGUTMA YETMEZ", 1, kUiGold);
    else {   // oktan: yakit / istenen (sicak motorda istenen ~3 artar)
        char t[48]; std::snprintf(t, sizeof t, "OKTAN %.0f / %.0f", prev_.octane, prev_.octaneReq);
        r.text(180, py + 81, t, 1, prev_.octaneReq > prev_.octane - 3 ? kUiGold : kUiDim);
    }

}

void PartsScreen::render(Renderer& r) {
    const OwnedCar& oc = app_.career.car();
    const VehicleDef& v = *findVehicle(oc.carId);
    r.begin(360, 640, kUiBg);
    headerP(r, app_, "MODIFIYE");
    drawStatsBar(r);

    if (cat_ < 0) {
        // Sekmeler + kategori listesi
        for (int t = 0; t < kPartTabs; ++t) {
            const float tw = 352.0f / kPartTabs;
            const Rect tr{4.0f + t * tw, 102, 2.0f + (t + 1) * tw, 126};
            r.rect(tr.x0, tr.y0, tr.x1, tr.y1, t == tab_ ? kUiOrange : kUiPanel);
            r.textCentered(tr.cx(), tr.y0 + 9, partTabName(t), 1, {1, 1, 1});
        }
        int row = 0;
        for (int i = 0; i < (int)PartCat::Count; ++i) {
            const PartCat c = (PartCat)i;
            if (partTab(c) != tab_) continue;
            const float rh = catRowH(tab_);
            const Rect rr{8, 132.0f + row * rh, 352, 132.0f + row * rh + rh - 4};
            r.rect(rr.x0, rr.y0, rr.x1, rr.y1, kUiPanel);
            r.text(16, rr.y0 + (rh < 38 ? 3 : 6), partCatName(c), 2, {1, 1, 1});
            const int lvl = partLevel(oc.tune, c, v);
            std::string cur = partOptions(c)[lvl].name;
            if (c == PartCat::Electronics && lvl == 0) cur = v.abs ? (v.tc ? "FABRIKA ABS+TC" : "FABRIKA ABS") : "YOK";
            char cnt[16]; std::snprintf(cnt, sizeof cnt, "%zu SECENEK", partOptions(c).size());
            const float y2 = rr.y0 + (rh < 38 ? rh - 14 : 24);
            r.text(16, y2, cnt, 1, kUiDim);
            r.text(344 - r.textWidth(cur.substr(0, 26), 1), y2, cur.substr(0, 26), 1, lvl > 0 ? kUiGold : kUiDim);
            if (customOption(c) >= 0) r.text(344 - r.textWidth("ATOLYE", 1), rr.y0 + 6, "ATOLYE", 1, {0.55f, 0.75f, 1.0f});
            ++row;
        }
        button(r, kBackP, "< GARAJ", kUiBtn, 2);
    } else {
        const PartCat c = (PartCat)cat_;
        r.text(8, 104, partCatName(c), 2, kUiGold);
        const auto& opts = partOptions(c);
        char b[48];
        const int mk = (int)std::lround((marketMul(c, app_.career.marketWeek()) - 1.0) * 100.0);
        if (mk) std::snprintf(b, sizeof b, "PAZAR %+d%%  %zu SECENEK", mk, opts.size());   // haftalik fiyat
        else std::snprintf(b, sizeof b, "%zu SECENEK", opts.size());
        if (c == PartCat::Ecu) button(r, kSoftP, "YAZILIM >", {0.15f, 0.35f, 0.6f}, 1);
        else r.text(352 - r.textWidth(b, 1), 110, b, 1, kUiDim);
        const int cur = partLevel(oc.tune, c, v);
        const float listH = (sel_ >= 0 ? kListY1 : 540.0f) - kListY0;
        const float maxScroll = std::max(0.0f, opts.size() * kRowH - listH);
        scroll_ = std::clamp(scroll_, 0.0f, maxScroll);
        for (int i = 0; i < (int)opts.size(); ++i) {
            const float y0 = kListY0 + i * kRowH - scroll_;
            if (y0 + kRowH < kListY0 || y0 > kListY0 + listH) continue;
            if (y0 < kListY0 - 1 || y0 + kRowH - 4 > kListY0 + listH + 1) continue;   // kismen gorunen satir cizilmez
            std::string why;
            const bool avail = partAvailable(c, i, v, &why, &oc.tune);
            const int price = app_.career.shopPrice(c, i);
            const bool mine = i == cur, afford = app_.career.money >= price;
            r.rect(8, y0, 352, y0 + kRowH - 4, mine ? Color{0.12f, 0.3f, 0.16f} : i == sel_ ? Color{0.2f, 0.24f, 0.36f} : kUiPanel);
            if (i == sel_) r.rect(8, y0, 12, y0 + kRowH - 4, kUiOrange);
            r.text(16, y0 + 5, std::string(opts[i].name).substr(0, 30), 1, {1, 1, 1});
            std::string right = mine ? "TAKILI" : !avail ? why : price == 0 ? "UCRETSIZ" : money(price);
            if (i == customOption(c) && !mine) right = "ATOLYE: URET >";
            r.text(344 - r.textWidth(right, 2), y0 + 17, right, 2,
                   mine ? Color{0.4f, 1.0f, 0.5f} : i == customOption(c) ? Color{0.55f, 0.75f, 1.0f} : !avail ? Color{1.0f, 0.35f, 0.3f} : afford ? kUiGold : kUiDim);
        }
        if (maxScroll > 0) {                                                      // kaydirma cubugu
            const float th = std::max(24.0f, listH * listH / (listH + maxScroll)), ty = kListY0 + (listH - th) * scroll_ / maxScroll;
            r.rect(354, kListY0, 357, kListY0 + listH, {0.15f, 0.15f, 0.18f});
            r.rect(354, ty, 357, ty + th, kUiDim);
        }
        if (sel_ >= 0) drawPreview(r, 450);
        if (confirm_ && sel_ >= 0) {
            r.rect(0, 540, 360, 596, {0.03f, 0.03f, 0.05f, 0.95f});
            const int np = app_.career.shopPrice(c, sel_);
            if (usedAvailable(c, sel_) && np > 0) {                       // yeni / ikinci el
                button(r, kNewP, "YENI", kUiGreen, 2);
                r.textCentered(kNewP.cx(), kNewP.y1 - 11, money(np), 1, {0.85f, 1.0f, 0.85f});
                button(r, kUsedP, "2. EL", {0.55f, 0.40f, 0.12f}, 2);
                r.textCentered(kUsedP.cx(), kUsedP.y1 - 11, money(usedPrice(np)) + " YIPRANMIS", 1, {1.0f, 0.9f, 0.7f});
                button(r, kNo3P, "VAZGEC", kUiRed, 2);
            } else {
                button(r, kYesP, std::string("EVET ") + money(np), kUiGreen, 2);
                button(r, kNoP, "VAZGEC", kUiRed, 2);
            }
            const int old = partLevel(oc.tune, c, v);
            if (old > 0 && old != customOption(c)) {
                char t[64]; std::snprintf(t, sizeof t, "ESKI PARCA SATILIR: +%s", money((long)partPrice(c, old, v) * 35 / 100 / 10 * 10).c_str());
                r.textCentered(180, 532, t, 1, {0.6f, 0.85f, 0.65f});
            }
        } else if (sel_ >= 0) r.textCentered(180, 566, "TEKRAR DOKUN: SATIN AL", 1, {0.7f, 0.75f, 0.9f});
        button(r, kBackP, "< KATEGORILER", kUiBtn, 2);
    }
    if (msgT_ > 0) { r.rect(0, 500, 360, 528, {0.02f, 0.02f, 0.04f, 0.92f}); r.textCentered(180, 507, msg_, 2, kUiGold); }
}

void PartsScreen::buySelected(bool used) {
    const PartCat c = (PartCat)cat_;
    std::string why;
    long back = 0;
    if (app_.career.buyPart(c, sel_, &why, used, &back)) {
        msg_ = std::string(partOptions(c)[sel_].name).substr(0, 18) + (used ? " (2. EL)" : " TAKILDI");
        if (back > 0) msg_ += " +" + money(back);
        msgT_ = 2.0;
        app_.saveCareer();
        recompute();
    } else { msg_ = why; msgT_ = 1.8; confirm_ = false; }
}

void PartsScreen::tap(float x, float y) {
    if (cat_ >= 0 && confirm_ && sel_ >= 0) {
        const OwnedCar& o = app_.career.car();
        const bool usedOk = usedAvailable((PartCat)cat_, sel_) && partPrice((PartCat)cat_, sel_, *findVehicle(o.carId)) > 0;
        if (usedOk) {
            if (kNewP.hit(x, y)) { buySelected(false); return; }
            if (kUsedP.hit(x, y)) { buySelected(true); return; }
            if (kNo3P.hit(x, y)) { confirm_ = false; return; }
        } else {
            if (kYesP.hit(x, y)) { buySelected(false); return; }
            if (kNoP.hit(x, y)) { confirm_ = false; return; }
        }
    }
    if (kBackP.hit(x, y)) { if (cat_ >= 0) { cat_ = -1; sel_ = -1; confirm_ = false; } else app_.goGarage(); return; }
    if (cat_ == (int)PartCat::Ecu && kSoftP.hit(x, y)) { app_.goEcu(); return; }
    const OwnedCar& oc = app_.career.car();
    const VehicleDef& v = *findVehicle(oc.carId);
    if (cat_ < 0) {
        const float tw = 352.0f / kPartTabs;
        for (int t = 0; t < kPartTabs; ++t)
            if (Rect{4.0f + t * tw, 102, 2.0f + (t + 1) * tw, 126}.hit(x, y)) { tab_ = t; return; }
        int row = 0;
        for (int i = 0; i < (int)PartCat::Count; ++i) {
            if (partTab((PartCat)i) != tab_) continue;
            const float rh = catRowH(tab_);
            if (Rect{8, 132.0f + row * rh, 352, 132.0f + row * rh + rh - 4}.hit(x, y)) {
                cat_ = i; sel_ = -1; confirm_ = false; scroll_ = 0;
                const int cur = partLevel(oc.tune, (PartCat)i, v);                 // takili parcayi gorunur yap
                scroll_ = std::max(0.0f, cur * kRowH - 120.0f);
                return;
            }
            ++row;
        }
        return;
    }
    const PartCat c = (PartCat)cat_;
    const float listH = (sel_ >= 0 ? kListY1 : 540.0f) - kListY0;
    if (y < kListY0 || y > kListY0 + listH) return;
    const int i = (int)((y - kListY0 + scroll_) / kRowH);
    if (i < 0 || i >= (int)partOptions(c).size()) return;
    if (i == customOption(c) && i != partLevel(oc.tune, c, v)) { app_.goFabricate(cat_); return; }   // atolye
    if (i == partLevel(oc.tune, c, v)) { msg_ = "ZATEN TAKILI"; msgT_ = 1.2; return; }
    std::string why;
    if (!partAvailable(c, i, v, &why, &oc.tune)) { msg_ = why; msgT_ = 1.8; return; }
    if (sel_ != i) select(i);
    else {
        const int np = app_.career.shopPrice(c, i);
        if (app_.career.money < (usedAvailable(c, i) ? usedPrice(np) : np)) { msg_ = "PARA YETMIYOR"; msgT_ = 1.8; return; }
        confirm_ = true;
    }
}

void PartsScreen::pointerDown(int id, float x, float y) {
    if (dragId_ >= 0) return;
    dragId_ = id; downX_ = lastX_ = x; downY_ = lastY_ = y; dragging_ = false;
}
void PartsScreen::pointerMove(int id, float x, float y) {
    if (id != dragId_) return;
    if (!dragging_ && std::fabs(y - downY_) > 8 && cat_ >= 0 && downY_ >= kListY0 && downY_ <= 540) dragging_ = true;
    if (dragging_) scroll_ -= y - lastY_;
    lastX_ = x; lastY_ = y;
}
void PartsScreen::pointerUp(int id) {
    if (id != dragId_) return;
    dragId_ = -1;
    if (!dragging_) tap(downX_, downY_);
    dragging_ = false;
}

void PartsScreen::key(Key k, bool down) {
    if (!down) return;
    if (k == Key::Back) {
        if (confirm_) confirm_ = false;
        else if (cat_ >= 0) { cat_ = -1; sel_ = -1; }
        else app_.goGarage();
    }
    if (k == Key::Left && cat_ < 0) tab_ = (tab_ + kPartTabs - 1) % kPartTabs;
    if (k == Key::Right && cat_ < 0) tab_ = (tab_ + 1) % kPartTabs;
    if (k == Key::PageDown) scroll_ += kRowH * 4;
    if (k == Key::PageUp) scroll_ -= kRowH * 4;
    if (k == Key::Enter && cat_ >= 0 && sel_ >= 0) { if (confirm_) buySelected(); else confirm_ = true; }
}

// ================================================================== OZEL URETIM ATOLYESI
namespace {
const Rect kMake{8, 548, 352, 590};
struct SliderDef { const char* name; double lo, hi, step; const char* fmt; };
} // namespace

FabricateScreen::FabricateScreen(App& app, int cat) : app_(app), cat_(cat) {
    const OwnedCar& oc = app_.career.car();
    const VehicleDef& v = *findVehicle(oc.carId);
    t_ = oc.tune;
    // Varsayilan / mevcut degerler
    switch ((PartCat)cat_) {
    case PartCat::Turbo: vals_ = {t_.custTurboMm > 0 ? t_.custTurboMm : 60.0, t_.custTurboAr > 0 ? t_.custTurboAr : 0.82}; break;
    case PartCat::Cam: vals_ = {t_.custCamDeg > 0 ? t_.custCamDeg : 272.0}; break;
    case PartCat::Crank: vals_ = {t_.custDisp > 0 ? t_.custDisp * 100.0 : 12.0}; break;
    case PartCat::FinalDrive: vals_ = {t_.custFinal > 0 ? t_.custFinal : buildGearboxFor(effectiveGearbox(v, &t_)).finalDrive}; break;
    case PartCat::Aero: vals_ = {t_.custWingN > 0 ? t_.custWingN : 500.0}; break;
    case PartCat::Gearbox: {
        const GearboxSpec g = buildGearboxFor(effectiveGearbox(v, &t_));
        for (size_t i = 0; i < g.ratios.size() && i < 8; ++i) vals_.push_back(g.ratios[i] * (t_.custGear[i] > 0 ? t_.custGear[i] : 1.0));
        break;
    }
    default: break;
    }
    refresh();
}

std::vector<FabricateScreen::Slider> FabricateScreen::sliders() const {
    std::vector<Slider> s;
    switch ((PartCat)cat_) {
    case PartCat::Turbo: s = {{"KOMPRESOR CAPI (MM)", 38, 100, 1, "%.0f"}, {"TURBIN A/R", 0.48, 1.32, 0.02, "%.2f"}}; break;
    case PartCat::Cam: s = {{"KAM SURESI (DERECE)", 230, 330, 2, "%.0f"}}; break;
    case PartCat::Crank: s = {{"HACIM ARTISI (%)", 1, 40, 1, "%.0f"}}; break;
    case PartCat::FinalDrive: s = {{"SON DISLI ORANI", 2.0, 7.5, 0.01, "%.2f"}}; break;
    case PartCat::Aero: s = {{"BASKI KUVVETI (N, 100 KM/H)", 50, 1600, 10, "%.0f"}}; break;
    case PartCat::Gearbox: {
        const OwnedCar& oc = app_.career.car();
        const GearboxSpec g = buildGearboxFor(effectiveGearbox(*findVehicle(oc.carId), &oc.tune));
        static const char* names[8] = {"1. VITES", "2. VITES", "3. VITES", "4. VITES", "5. VITES", "6. VITES", "7. VITES", "8. VITES"};
        for (size_t i = 0; i < vals_.size(); ++i) s.push_back({names[i], g.ratios[i] * 0.6, g.ratios[i] * 1.5, 0.01, "%.3f"});
        break;
    }
    default: break;
    }
    return s;
}

Tune FabricateScreen::built() const {
    Tune t = t_;
    const OwnedCar& oc = app_.career.car();
    const VehicleDef& v = *findVehicle(oc.carId);
    switch ((PartCat)cat_) {
    case PartCat::Turbo: t.custTurboMm = vals_[0]; t.custTurboAr = vals_[1]; break;
    case PartCat::Cam: t.custCamDeg = vals_[0]; break;
    case PartCat::Crank: t.custDisp = vals_[0] / 100.0; break;
    case PartCat::FinalDrive: t.custFinal = vals_[0]; break;
    case PartCat::Aero: t.custWingN = vals_[0]; break;
    case PartCat::Gearbox: {
        Tune base = t_; base.gearSwap = 0;
        const GearboxSpec g = buildGearboxFor(effectiveGearbox(v, &base));   // ozel oranlar takili kutunun uzerine
        for (size_t i = 0; i < vals_.size(); ++i) t.custGear[i] = vals_[i] / g.ratios[i];
        break;
    }
    default: break;
    }
    setPartLevel(t, (PartCat)cat_, customOption((PartCat)cat_), v);
    return t;
}

int FabricateScreen::price() const {
    const OwnedCar& oc = app_.career.car();
    const double scale = std::clamp(0.6 + carPrice(*findVehicle(oc.carId)) / 40000.0, 0.6, 4.0);
    double p = 0;
    switch ((PartCat)cat_) {
    case PartCat::Turbo: p = 2500 + vals_[0] * 90 + std::fabs(vals_[1] - 0.82) * 1500; break;
    case PartCat::Cam: p = 900 + std::fabs(vals_[0] - 250) * 28; break;
    case PartCat::Crank: p = 2200 + vals_[0] * 260; break;
    case PartCat::FinalDrive: p = 1100; break;
    case PartCat::Aero: p = 900 + vals_[0] * 2.2; break;
    case PartCat::Gearbox: p = 2600 + vals_.size() * 450; break;
    default: break;
    }
    return (int)(std::round(p * scale / 10.0) * 10.0);
}

void FabricateScreen::refresh() {
    const OwnedCar& oc = app_.career.car();
    const VehicleDef& v = *findVehicle(oc.carId);
    now_ = tuneStats(v, oc.tune);
    next_ = tuneStats(v, built());
    confirm_ = false;
}

void FabricateScreen::update(double dt) {
    msgT_ -= dt;
    if (held_ >= 0) {                                                     // +/- basili tutma: hizli adim
        holdT_ += dt;
        if (holdT_ > 0.35) { holdT_ = 0.28; nudge(held_, heldDir_); }
    }
}

void FabricateScreen::nudge(int i, int dir) {
    const auto s = sliders();
    if (i < 0 || i >= (int)s.size()) return;
    vals_[i] = std::clamp(vals_[i] + dir * s[i].step, s[i].lo, s[i].hi);
    refresh();
}

void FabricateScreen::render(Renderer& r) {
    r.begin(360, 640, kUiBg);
    headerP(r, app_, "OZEL URETIM ATOLYESI");
    r.text(8, 64, partCatName((PartCat)cat_), 2, kUiGold);
    const auto s = sliders();
    char b[96];
    for (size_t i = 0; i < s.size(); ++i) {
        const float y = 92.0f + i * 50.0f;
        r.text(8, y, s[i].name, 1, kUiText);
        std::snprintf(b, sizeof b, s[i].fmt, vals_[i]);
        r.text(352 - r.textWidth(b, 2), y - 3, b, 2, {1, 1, 1});
        const Rect minus{8, y + 14, 48, y + 44}, plus{312, y + 14, 352, y + 44};
        button(r, minus, "-", kUiBtn, 3); button(r, plus, "+", kUiBtn, 3);
        const float f = (float)((vals_[i] - s[i].lo) / (s[i].hi - s[i].lo));
        r.rect(56, y + 26, 304, y + 32, {0.15f, 0.15f, 0.18f});
        r.rect(56, y + 26, 56 + 248 * f, y + 32, kUiOrange);
        r.rect(56 + 248 * f - 4, y + 18, 56 + 248 * f + 4, y + 40, {1, 1, 1});
    }
    // Sonuc paneli: simdiki > uretilen
    const float py = std::max(300.0f, 100.0f + s.size() * 50.0f);
    r.rect(8, py, 352, py + 130, {0.08f, 0.09f, 0.13f});
    r.text(16, py + 6, "SONUC (TAKILIRSA)", 1, kUiGold);
    const Color up{0.4f, 1.0f, 0.5f}, down{1.0f, 0.4f, 0.3f};
    auto line = [&](int k, const char* n, double a, double c, const char* fmt, bool hb) {
        std::snprintf(b, sizeof b, fmt, a, c);
        const double d = c - a;
        r.text(16, py + 22 + k * 15, n, 1, kUiText);
        r.text(110, py + 22 + k * 15, b, 1, std::fabs(d) < 1e-6 ? kUiDim : (d > 0) == hb ? up : down);
    };
    line(0, "GUC", now_.hp, next_.hp, "%.0f > %.0f HP", true);
    line(1, "TORK", now_.nm, next_.nm, "%.0f > %.0f NM", true);
    line(2, "ENDEKS", now_.idx, next_.idx, "%.0f > %.0f", true);
    line(3, "MOTOR YUKU", now_.engineLoad * 100, next_.engineLoad * 100, "%%%.0f > %%%.0f", false);
    line(4, "SANZIMAN YUKU", now_.gearboxLoad * 100, next_.gearboxLoad * 100, "%%%.0f > %%%.0f", false);
    line(5, "ISI YUKU", now_.heatLoad * 100, next_.heatLoad * 100, "%%%.0f > %%%.0f", false);
    line(6, "DEVIR SINIRI", now_.redline, next_.redline, "%.0f > %.0f", true);
    if ((PartCat)cat_ == PartCat::Turbo) {
        const TurboOpt T = customTurbo(built());
        std::snprintf(b, sizeof b, "BOOST +%.2f BAR, SPOOL %%%.0f-%%%.0f DEVIR", T.bar, T.spoolLo * 100, T.spoolHi * 100);
        r.text(16, py + 132, b, 1, {0.55f, 0.75f, 1.0f});
    }
    // Uret / onay / geri
    const int p = price();
    if (confirm_) {
        button(r, kYesP, "URET " + money(p), kUiGreen, 2);
        button(r, kNoP, "VAZGEC", kUiRed, 2);
    } else button(r, kMake, "URET VE TAK " + money(p), app_.career.money >= p ? kUiGreen : Color{0.25f, 0.25f, 0.28f}, 2);
    button(r, kBackP, "< GERI", kUiBtn, 2);
    if (msgT_ > 0) { r.rect(0, 500, 360, 528, {0.02f, 0.02f, 0.04f, 0.92f}); r.textCentered(180, 507, msg_, 2, kUiGold); }
}

void FabricateScreen::make() {
    const int p = price();
    Career& c = app_.career;
    if (c.money < p) { msg_ = "PARA YETMIYOR"; msgT_ = 1.8; confirm_ = false; return; }
    c.money -= p;
    c.car().paidParts += p;
    c.car().tune = built();
    app_.saveCareer();
    msg_ = "URETILDI VE TAKILDI"; msgT_ = 2.0;
    t_ = c.car().tune;
    refresh();
}

void FabricateScreen::pointerDown(int id, float x, float y) {
    if (confirm_) {
        if (kYesP.hit(x, y)) make();
        else if (kNoP.hit(x, y)) confirm_ = false;
        return;
    }
    if (kBackP.hit(x, y)) { app_.goParts(cat_); return; }
    if (kMake.hit(x, y)) { confirm_ = true; return; }
    const auto s = sliders();
    for (size_t i = 0; i < s.size(); ++i) {
        const float yy = 92.0f + i * 50.0f;
        if (Rect{8, yy + 14, 48, yy + 44}.hit(x, y)) { nudge((int)i, -1); held_ = (int)i; heldDir_ = -1; holdT_ = 0; ptr_ = id; return; }
        if (Rect{312, yy + 14, 352, yy + 44}.hit(x, y)) { nudge((int)i, +1); held_ = (int)i; heldDir_ = +1; holdT_ = 0; ptr_ = id; return; }
        if (Rect{52, yy + 12, 308, yy + 46}.hit(x, y)) { dragSlider_ = (int)i; ptr_ = id; pointerMove(id, x, y); return; }
    }
}
void FabricateScreen::pointerMove(int id, float x, float) {
    if (id != ptr_ || dragSlider_ < 0) return;
    const auto s = sliders();
    const SliderDef* unused = nullptr; (void)unused;
    const Slider& d = s[dragSlider_];
    const double f = std::clamp((x - 56.0) / 248.0, 0.0, 1.0);
    vals_[dragSlider_] = std::clamp(std::round((d.lo + f * (d.hi - d.lo)) / d.step) * d.step, d.lo, d.hi);
    refresh();
}
void FabricateScreen::pointerUp(int id) { if (id == ptr_) { ptr_ = -1; held_ = -1; dragSlider_ = -1; } }
void FabricateScreen::key(Key k, bool down) {
    if (!down) return;
    if (k == Key::Back) { if (confirm_) confirm_ = false; else app_.goParts(cat_); }
    if (k == Key::Left) nudge(0, -1);
    if (k == Key::Right) nudge(0, +1);
    if (k == Key::Enter) { if (confirm_) make(); else confirm_ = true; }
}

// ---------------------------------------------------------------- ECU yazilim
// Donanim: yuva sayisi + modul seviye siniri. Her modul satiri: seviye, etki, [-] dusur (iade yok), [+] bir seviye al.
namespace {
constexpr float kSwY0 = 130, kSwH = 52;
const Rect kBackE{8, 596, 352, 634};
Rect swMinus(int i) { return {214, kSwY0 + i * kSwH + 14, 254, kSwY0 + i * kSwH + 42}; }
Rect swPlus(int i) { return {258, kSwY0 + i * kSwH + 14, 352, kSwY0 + i * kSwH + 42}; }
}

EcuScreen::EcuScreen(App& app) : app_(app) {
    app_.hint(HintEcu, hintTexts()[HintEcu]);
    now_ = tuneStats(*findVehicle(app_.career.car().carId), app_.career.car().tune);
}

void EcuScreen::render(Renderer& r) {
    const OwnedCar& oc = app_.career.car();
    const VehicleDef& v = *findVehicle(oc.carId);
    const Tune& t = oc.tune;
    r.begin(360, 640, kUiBg);
    headerP(r, app_, "ECU YAZILIM");
    const EcuHwOpt& hw = ecuHwTable()[std::clamp(t.ecuHw, 0, 9)];
    char b[96];
    std::snprintf(b, sizeof b, "%s   YUVA %d / %d", hw.name, ecuSlotsUsed(t), hw.slots);
    r.text(8, 64, b, 2, kUiGold);
    {   // supap siniri: devir siniri bunu asarsa supap atar
        VehicleSimConfig cfg; cfg.car = &v; cfg.tune = &t;
        const VehicleSim sim(cfg);
        const bool over = sim.engineSpec().redlineRpm > sim.valveSafeRpm();
        std::snprintf(b, sizeof b, "%.0f HP   KESICI %.0f   SUPAP %.0f", now_.hp, sim.engineSpec().redlineRpm, sim.valveSafeRpm());
        r.text(8, 88, b, 1, over ? Color{1.0f, 0.4f, 0.3f} : kUiText);
        if (over) r.text(8, 100, "SUPAP SINIRI ASILDI: SUPAP / KAM AL YA DA DEVRI DUSUR", 1, {1.0f, 0.4f, 0.3f});
        std::snprintf(b, sizeof b, "OKTAN %.0f / ISTENEN %.0f%s", now_.octane, now_.octaneReq, hw.knockBuiltin || t.swKnock ? "" : "   VURUNTU KORUMASI YOK");
        r.text(8, 112, b, 1, now_.octaneReq > now_.octane || !(hw.knockBuiltin || t.swKnock) ? Color{1.0f, 0.75f, 0.3f} : kUiDim);
    }
    for (int i = 0; i < SwCount; ++i) {
        const EcuSwDef& d = ecuSwDef(i);
        const int lv = ecuSwLevel(t, i), mx = ecuSwMax(t, i);
        const float y = kSwY0 + i * kSwH;
        r.rect(8, y, 352, y + kSwH - 4, lv > 0 ? Color{0.12f, 0.24f, 0.16f} : kUiPanel);
        r.text(16, y + 5, d.name, 1, mx > 0 ? Color{1, 1, 1} : kUiDim);
        if (i == SwRev) std::snprintf(b, sizeof b, "+%d RPM", lv * 250);
        else std::snprintf(b, sizeof b, mx > 0 ? "SEVIYE %d / %d" : "ECU DESTEKLEMIYOR", lv, mx);
        r.text(16, y + 19, b, 1, mx > 0 ? (lv > 0 ? kUiGold : kUiText) : Color{0.6f, 0.4f, 0.35f});
        r.text(16, y + 31, std::string(d.desc).substr(0, 30), 1, kUiDim);
        if (mx <= 0) continue;
        if (lv > 0) button(r, swMinus(i), "-", kUiBtn, 2);
        if (lv < mx) {
            const int p = ecuSwPrice(i, lv + 1, v);
            button(r, swPlus(i), "+ " + money(p), app_.career.money >= p ? kUiGreen : Color{0.3f, 0.3f, 0.32f}, 1);
        } else r.textCentered(swPlus(i).cx(), swPlus(i).y0 + 10, "MAKS", 1, kUiGold);
    }
    button(r, kBackE, "< ECU", kUiBtn, 2);
    if (msgT_ > 0) { r.rect(0, 560, 360, 590, {0.02f, 0.02f, 0.04f, 0.92f}); r.textCentered(180, 568, msg_, 2, kUiGold); }
}

void EcuScreen::pointerDown(int, float x, float y) {
    if (kBackE.hit(x, y)) { app_.goParts((int)PartCat::Ecu); return; }
    for (int i = 0; i < SwCount; ++i) {
        std::string why;
        if (swPlus(i).hit(x, y)) {
            if (app_.career.buyEcuSoftware(i, &why)) { msg_ = std::string(ecuSwDef(i).name) + " YUKLENDI"; app_.saveCareer(); }
            else msg_ = why;
            msgT_ = 1.6;
        } else if (swMinus(i).hit(x, y) && ecuSwLevel(app_.career.car().tune, i) > 0) {
            app_.career.dropEcuSoftware(i); app_.saveCareer();
            msg_ = std::string(ecuSwDef(i).name) + " DUSURULDU"; msgT_ = 1.2;
        } else continue;
        now_ = tuneStats(*findVehicle(app_.career.car().carId), app_.career.car().tune);
        return;
    }
}

void EcuScreen::key(Key k, bool down) { if (down && k == Key::Back) app_.goParts((int)PartCat::Ecu); }

} // namespace zk
