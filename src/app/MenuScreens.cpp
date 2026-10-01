// ZEHRA KINIK - Menu ekranlari: parca dukkani, galeri, dyno (dikey 360x640)
#include "Screens.h"
#include "Ui.h"
#include "garage/VehicleCatalog.h"
#include "sim/VehicleSim.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace zk {

namespace {
constexpr double kHpK = 2.0 * 3.14159265358979 / 60.0 / 745.7;   // Nm*rpm -> HP
const Rect kBack{8, 596, 352, 634};

double curveAt(const std::vector<std::pair<double, double>>& c, double rpm) {
    if (c.empty()) return 0;
    if (rpm <= c.front().first) return c.front().second;
    for (size_t i = 1; i < c.size(); ++i)
        if (rpm <= c[i].first) {
            const double t = (rpm - c[i - 1].first) / (c[i].first - c[i - 1].first);
            return c[i - 1].second + t * (c[i].second - c[i - 1].second);
        }
    return c.back().second;
}
// Etkin egri: kam gecis devrinin altinda dusuk kam, ustunde yuksek kam
std::vector<std::pair<double, double>> effectiveCurve(const EngineSpec& e) {
    std::vector<std::pair<double, double>> out;
    for (size_t i = 0; i < e.lowCam.size(); ++i) {
        const double rpm = e.lowCam[i].first;
        out.push_back({rpm, rpm >= e.vtecRpm ? e.highCam[i].second : e.lowCam[i].second});
    }
    return out;
}
void header(Renderer& r, const App& app, const char* title) {
    r.rect(0, 0, 360, 58, kUiPanel);
    r.text(8, 8, title, 2, kUiGold);
    const VehicleDef& v = *findVehicle(app.career.car().carId);
    r.text(8, 32, upper(v.fullName()).substr(0, 28), 1, kUiText);
    const std::string m = money(app.career.money);
    r.text(352 - r.textWidth(m, 2), 8, m, 2, {0.4f, 1.0f, 0.5f});
}
} // namespace

// ================================================================== GALERI
namespace {
const Rect kGNav[4] = {{8, 468, 88, 506}, {94, 468, 174, 506}, {186, 468, 266, 506}, {272, 468, 352, 506}};
const int kGStep[4] = {-10, -1, +1, +10};
const Rect kBuy{8, 512, 352, 552}, kSell{8, 558, 352, 590};
} // namespace

GalleryScreen::GalleryScreen(App& app) : app_(app), carId_(app.career.car().carId) {
    if (const char* c = std::getenv("ZK_GALLERY_CAR")) if (findVehicle(std::atoi(c))) carId_ = std::atoi(c);   // test: model goruntusu
}

void GalleryScreen::render(Renderer& r) {
    if (std::getenv("ZK_SHEET")) {                    // test: 12 aracin model tablosu (yandan + 3/4 on)
        r.begin(360, 640, {0.16f, 0.17f, 0.21f});
        const bool side = std::atoi(std::getenv("ZK_SHEET")) == 2;
        for (int k = 0; k < 6; ++k) {
            const VehicleDef* c = findVehicle(carId_ + k);
            if (!c) break;
            const float cx = (k % 2) * 180.0f, cy = (k / 2) * 213.0f;
            const float L = (float)c->lengthM;
            const Mat4 view = side ? matLookAt(0.0f, 0.7f, 2.9f * L, 0.0f, 0.62f, 0.0f)
                                   : matLookAt(1.25f * L, 0.9f + 0.18f * L, 1.55f * L, 0.0f, 0.55f, 0.0f);
            r.drawCar(c->id, cx, cy + 14, 180, 195, matPerspective(0.5f, 180.0f / 195.0f, 0.1f, 50.0f), view, matRotY(0.0f));
            r.text(cx + 3, cy + 3, std::to_string(c->id) + " " + upper(c->model).substr(0, 24), 1, {1, 1, 1});
        }
        return;
    }
    const VehicleDef& v = *findVehicle(carId_);
    const EngineDef& e = engineTable()[v.engine];
    r.begin(360, 640, kUiBg);
    r.rect(0, 0, 360, 58, kUiPanel);
    r.text(8, 8, "GALERI", 2, kUiGold);
    char b[128];
    std::snprintf(b, sizeof b, "#%d %s", v.id, upper(v.fullName()).c_str());
    r.text(8, 32, std::string(b).substr(0, 42), 1, kUiText);
    const std::string m = money(app_.career.money);
    r.text(352 - r.textWidth(m, 2), 8, m, 2, {0.4f, 1.0f, 0.5f});
    studio(r, 60, 290, 190);
    const float L = (float)v.lengthM;
    r.drawCar(v.id, 0, 64, 360, 222, matPerspective(0.75f, 360.0f / 222.0f, 0.1f, 50.0f),
              matLookAt(0.0f, 1.1f + 0.2f * L, 1.35f * L + 1.2f, 0.0f, 0.55f, 0.0f), matRotY(spin_));
    std::snprintf(b, sizeof b, "%d %s %s %.0fKG", v.year, bodyName(v.body), driveName(v.drive), v.massKg);
    r.text(8, 296, upper(b), 2, kUiText);
    std::snprintf(b, sizeof b, "%s %.1fL %s", layoutName(e.layout), e.displacementL, inductionName(e.induction));
    r.text(8, 316, upper(b), 2, kUiText);
    std::snprintf(b, sizeof b, "%.0f HP  %.0f NM", e.powerHp, e.torqueNm);
    r.text(8, 336, b, 2, kUiText);
    std::snprintf(b, sizeof b, "%s %zuV", upper(gearboxName(gearboxTable()[v.gearbox].type)).c_str(), gearboxTable()[v.gearbox].ratios.size());
    r.text(8, 356, b, 2, kUiText);
    std::snprintf(b, sizeof b, "ENDEKS %.0f", performanceIndex(v, Tune{}));
    r.text(8, 376, b, 1, kUiDim);
    if (!v.streetLegal) r.text(8, 390, "YARIS ARACI (SOKAKTA SURULEMEZ)", 1, {1.0f, 0.35f, 0.3f});
    const int price = carPrice(v);
    const std::string ps = money(price);
    r.text(352 - r.textWidth(ps, 4), 408, ps, 4, kUiGold);

    for (int i = 0; i < 4; ++i) button(r, kGNav[i], i == 0 ? "<<" : i == 1 ? "<" : i == 2 ? ">" : ">>", kUiBtn, 3);
    bool owned = false;
    for (const OwnedCar& oc : app_.career.cars) owned |= oc.carId == carId_;
    if (owned) button(r, kBuy, "GARAJINDA", {0.12f, 0.3f, 0.16f}, 2);
    else button(r, kBuy, "SATIN AL " + ps, app_.career.money >= price ? kUiGreen : Color{0.25f, 0.25f, 0.28f}, 2);
    const OwnedCar& cur = app_.career.car();
    if (app_.career.cars.size() > 1) {
        std::snprintf(b, sizeof b, "SAT: %s %s", upper(findVehicle(cur.carId)->model).substr(0, 14).c_str(), money(sellPrice(cur)).c_str());
        button(r, kSell, b, kUiRed, 1);
    } else {
        r.rect(kSell.x0, kSell.y0, kSell.x1, kSell.y1, {0.15f, 0.15f, 0.17f});
        r.textCentered(kSell.cx(), kSell.cy() - 3, "TEK ARACIN SATILAMAZ", 1, kUiDim);
    }
    button(r, kBack, "< GARAJ", kUiBtn, 2);
    if (msgT_ > 0) { r.rect(0, 250, 360, 280, {0.02f, 0.02f, 0.04f, 0.9f}); r.textCentered(180, 258, msg_, 2, kUiGold); }

    // Onay penceresi (satin alma / satis): tek dokunusla para harcanmaz
    if (confirm_ != Confirm::None) {
        if (confirm_ == Confirm::Buy) {
            r.rect(0, 0, 360, 640, {0, 0, 0, 0.6f});
            r.rect(20, 150, 340, 470, {0.10f, 0.11f, 0.16f});
            r.rect(20, 150, 340, 154, kUiGreen);
            r.textCentered(180, 172, "SATIN ALINSIN MI?", 2, {1, 1, 1});
            r.textCentered(180, 204, upper(v.fullName()).substr(0, 40), 1, kUiText);
            r.textCentered(180, 236, money(price), 4, kUiGold);
            std::snprintf(b, sizeof b, "KALAN PARA: %s", money(app_.career.money - price).c_str());
            r.textCentered(180, 290, b, 2, kUiText);
            button(r, kDlgYes, "AL", kUiGreen, 2);
            button(r, kDlgNo, "VAZGEC", kUiBtn, 2);
        } else drawSaleDialog(r, cur);
    }
}

// Satis onay penceresi (galeri + garaj): fiyat dokumu arac + parca - hasar
void drawSaleDialog(Renderer& r, const OwnedCar& cur) {
    char b[96];
    r.rect(0, 0, 360, 640, {0, 0, 0, 0.6f});
    r.rect(20, 150, 340, 470, {0.10f, 0.11f, 0.16f});
    r.rect(20, 150, 340, 154, kUiRed);
    const VehicleDef& sv = *findVehicle(cur.carId);
    const SaleQuote q = saleQuote(cur);
    r.textCentered(180, 172, "ARAC SATILSIN MI?", 2, {1, 1, 1});
    r.textCentered(180, 196, upper(sv.fullName()).substr(0, 40), 1, kUiText);
    auto row = [&](float y, const char* name, const std::string& val, Color c) {
        r.text(36, y, name, 2, kUiText);
        r.text(324 - r.textWidth(val, 2), y, val, 2, c);
    };
    row(222, "ARAC (%65)", money(q.car), kUiText);
    row(248, "PARCALAR (%40)", money(q.parts), kUiText);
    row(274, "HASAR", q.damage > 0 ? "-" + money(q.damage) : "YOK", q.damage > 0 ? Color{1.0f, 0.4f, 0.3f} : kUiDim);
    r.rect(36, 302, 324, 304, kUiDim);
    row(314, "TOPLAM", money(q.total), {0.4f, 1.0f, 0.5f});
    if (q.total > q.car + q.parts - q.damage) r.textCentered(180, 342, "HURDA ALT SINIRI UYGULANDI", 1, kUiGold);
    std::snprintf(b, sizeof b, "YARIS %d  GALIBIYET %d", cur.races, cur.wins);
    r.textCentered(180, 362, b, 1, kUiDim);
    button(r, kDlgYes, "SAT", kUiRed, 2);
    button(r, kDlgNo, "VAZGEC", kUiBtn, 2);
}

void GalleryScreen::confirmAction() {
    std::string why;
    if (confirm_ == Confirm::Buy) {
        if (app_.career.buyCar(carId_, &why)) { msg_ = "HAYIRLI OLSUN!"; msgT_ = 2.0; app_.saveCareer(); }
        else { msg_ = why; msgT_ = 1.8; }
    } else if (confirm_ == Confirm::Sell) {
        const int got = sellPrice(app_.career.car());
        if (app_.career.sellCurrent(&why)) { msg_ = "SATILDI +" + money(got); msgT_ = 2.0; app_.saveCareer(); }
        else { msg_ = why; msgT_ = 1.8; }
    }
    confirm_ = Confirm::None;
}

void GalleryScreen::pointerDown(int, float x, float y) {
    if (confirm_ != Confirm::None) {                                   // modal: yalniz iki dugme
        if (kDlgYes.hit(x, y)) confirmAction();
        else if (kDlgNo.hit(x, y)) confirm_ = Confirm::None;
        return;
    }
    const int n = (int)vehicleCatalog().size();
    for (int i = 0; i < 4; ++i)
        if (kGNav[i].hit(x, y)) { carId_ = ((carId_ - 1 + kGStep[i]) % n + n) % n + 1; return; }
    if (kBuy.hit(x, y)) {
        bool owned = false;
        for (const OwnedCar& oc : app_.career.cars) owned |= oc.carId == carId_;
        if (owned) return;
        if (app_.career.money < carPrice(*findVehicle(carId_))) { msg_ = "PARA YETMIYOR"; msgT_ = 1.8; return; }
        confirm_ = Confirm::Buy;
        return;
    }
    if (kSell.hit(x, y)) {
        if (app_.career.cars.size() <= 1) { msg_ = "SON ARACINI SATAMAZSIN"; msgT_ = 1.8; return; }
        confirm_ = Confirm::Sell;
        return;
    }
    if (kBack.hit(x, y)) app_.goGarage();
}

void GalleryScreen::key(Key k, bool down) {
    if (!down) return;
    if (confirm_ != Confirm::None) {
        if (k == Key::Enter) confirmAction();
        else if (k == Key::Back) confirm_ = Confirm::None;
        return;
    }
    const int n = (int)vehicleCatalog().size();
    if (k == Key::Left) carId_ = ((carId_ - 2) % n + n) % n + 1;
    else if (k == Key::Right) carId_ = carId_ % n + 1;
    else if (k == Key::Back) app_.goGarage();
}

// ================================================================== DYNO
namespace {
const Rect kPull{8, 540, 352, 588};
constexpr float kGx0 = 44, kGx1 = 344, kGy0 = 86, kGy1 = 420;
} // namespace

DynoScreen::DynoScreen(App& app) : app_(app) {
    const OwnedCar& oc = app_.career.car();
    const VehicleDef& v = *findVehicle(oc.carId);
    VehicleSimConfig cfg; cfg.car = &v; cfg.tune = &oc.tune;
    const VehicleSim sim(cfg);
    tuned_ = effectiveCurve(sim.engineSpec());
    stock_ = effectiveCurve(buildEngineSpec(v));
    redline_ = sim.engineSpec().redlineRpm;
    idle_ = sim.engineSpec().idleRpm;
    for (auto* c : {&tuned_, &stock_})
        for (auto& p : *c) if (p.first <= redline_) { maxNm_ = std::max(maxNm_, p.second); maxHp_ = std::max(maxHp_, p.second * p.first * kHpK); }
    for (auto& p : tuned_) if (p.first <= redline_) {
        const double hp = p.second * p.first * kHpK;
        if (hp > peakHp_) { peakHp_ = hp; peakHpRpm_ = p.first; }
        if (p.second > peakNm_) { peakNm_ = p.second; peakNmRpm_ = p.first; }
    }
    for (auto& p : stock_) if (p.first <= redline_) stockPeakHp_ = std::max(stockPeakHp_, p.second * p.first * kHpK);
    app_.setVoice(1, nullptr);
}

void DynoScreen::update(double dt) {
    if (pullRpm_ >= 0) {
        pullRpm_ += (redline_ - idle_) / 5.0 * dt;                 // 5 s'lik cekis
        if (pullRpm_ >= redline_) pullRpm_ = -1;
    }
    const bool pulling = pullRpm_ >= 0;
    app_.voice(0, pulling ? pullRpm_ : idle_, pulling ? 1.0 : 0.0, false, pulling, 1.0f);
    app_.tire(0, 0.0);
}

void DynoScreen::render(Renderer& r) {
    r.begin(360, 640, kUiBg);
    header(r, app_, "DYNO");
    // Eksenler + izgara
    r.rect(kGx0, kGy0, kGx1, kGy1, {0.1f, 0.1f, 0.13f});
    const double rpmMax = std::ceil((redline_ + 500) / 1000.0) * 1000.0;
    char b[64];
    for (int k = 0; k <= (int)(rpmMax / 1000); ++k) {
        const float x = kGx0 + (float)(k * 1000 / rpmMax) * (kGx1 - kGx0);
        r.rect(x, kGy0, x + 1, kGy1, {0.18f, 0.18f, 0.22f});
        if (k % 2 == 0) { std::snprintf(b, sizeof b, "%d", k); r.textCentered(x, kGy1 + 4, b, 1, kUiDim); }
    }
    r.textCentered((kGx0 + kGx1) / 2, kGy1 + 14, "X1000 RPM", 1, kUiDim);
    const double yMax = std::max(maxNm_, maxHp_) * 1.1;
    for (int k = 1; k <= 4; ++k) {
        const float y = kGy1 - (float)k / 4 * (kGy1 - kGy0);
        r.rect(kGx0, y, kGx1, y + 1, {0.18f, 0.18f, 0.22f});
        std::snprintf(b, sizeof b, "%.0f", yMax * k / 4);
        r.text(4, y - 3, b, 1, kUiDim);
    }
    auto X = [&](double rpm) { return kGx0 + (float)(rpm / rpmMax) * (kGx1 - kGx0); };
    auto Y = [&](double val) { return kGy1 - (float)(val / yMax) * (kGy1 - kGy0); };
    auto plot = [&](const std::vector<std::pair<double, double>>& c, bool power, Color col, float w) {
        float px = -1, py = 0;
        for (double rpm = idle_; rpm <= redline_; rpm += 50) {
            const double nm = curveAt(c, rpm), val = power ? nm * rpm * kHpK : nm;
            const float x = X(rpm), y = Y(val);
            if (px >= 0) {
                const float dx = x - px, dy = y - py, l = std::sqrt(dx * dx + dy * dy) + 1e-3f, nx = -dy / l * w, ny = dx / l * w;
                r.tri(px + nx, py + ny, x + nx, y + ny, x - nx, y - ny, col);
                r.tri(px + nx, py + ny, x - nx, y - ny, px - nx, py - ny, col);
            }
            px = x; py = y;
        }
    };
    plot(stock_, false, {0.5f, 0.35f, 0.15f, 0.8f}, 0.6f);
    plot(stock_, true, {0.2f, 0.3f, 0.5f, 0.8f}, 0.6f);
    plot(tuned_, false, {1.0f, 0.6f, 0.15f}, 1.2f);
    plot(tuned_, true, {0.35f, 0.65f, 1.0f}, 1.2f);
    r.rect(kGx0 + 6, kGy0 + 6, kGx0 + 16, kGy0 + 10, {1.0f, 0.6f, 0.15f}); r.text(kGx0 + 20, kGy0 + 5, "TORK NM", 1, kUiText);
    r.rect(kGx0 + 86, kGy0 + 6, kGx0 + 96, kGy0 + 10, {0.35f, 0.65f, 1.0f}); r.text(kGx0 + 100, kGy0 + 5, "GUC HP", 1, kUiText);
    r.text(kGx0 + 160, kGy0 + 5, "SOLUK: STOK", 1, kUiDim);

    std::snprintf(b, sizeof b, "TEPE %.0f HP @ %.0f", peakHp_, peakHpRpm_);
    r.text(8, 446, b, 2, {0.55f, 0.8f, 1.0f});
    std::snprintf(b, sizeof b, "TEPE %.0f NM @ %.0f", peakNm_, peakNmRpm_);
    r.text(8, 468, b, 2, {1.0f, 0.7f, 0.3f});
    std::snprintf(b, sizeof b, "STOK %.0f HP  (%+.0f HP)", stockPeakHp_, peakHp_ - stockPeakHp_);
    r.text(8, 490, b, 2, kUiText);
    if (pullRpm_ >= 0) {
        const float x = X(pullRpm_);
        r.rect(x, kGy0, x + 2, kGy1, {1, 1, 1});
        const double nm = curveAt(tuned_, pullRpm_);
        std::snprintf(b, sizeof b, "%5.0f RPM %4.0f HP %4.0f NM", pullRpm_, nm * pullRpm_ * kHpK, nm);
        r.text(8, 514, b, 2, {1, 1, 1});
    }
    button(r, kPull, pullRpm_ >= 0 ? "CEKILIYOR..." : "DYNO CEKISI", pullRpm_ >= 0 ? kUiBtn : kUiOrange, 3);
    button(r, kBack, "< GARAJ", kUiBtn, 2);
}

void DynoScreen::pointerDown(int, float x, float y) {
    if (kPull.hit(x, y) && pullRpm_ < 0) pullRpm_ = idle_;
    else if (kBack.hit(x, y)) app_.goGarage();
}

void DynoScreen::key(Key k, bool down) {
    if (!down) return;
    if (k == Key::Enter && pullRpm_ < 0) pullRpm_ = idle_;
    else if (k == Key::Back) app_.goGarage();
}

} // namespace zk
