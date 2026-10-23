// ZEHRA KINIK - Menu ekranlari: parca dukkani, galeri, dyno (dikey 360x640)
#include "Screens.h"
#include "Ui.h"
#include "sim/PartTables.h"
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
const Rect kJunk{252, 64, 352, 92};
const Rect kLTab[4] = {{8, 440, 88, 464}, {94, 440, 174, 464}, {186, 440, 266, 464}, {272, 440, 352, 464}};

// Yatay (640x360)
const Rect kGNavL[4] = {{12, 272, 82, 306}, {88, 272, 158, 306}, {166, 272, 236, 306}, {242, 272, 312, 306}};
const Rect kJunkL{12, 312, 158, 350};
const Rect kBackGalL{166, 312, 312, 350};
const Rect kLTabL[4] = {{334, 186, 404, 212}, {408, 186, 478, 212}, {482, 186, 552, 212}, {556, 186, 628, 212}};
const Rect kBuyL{334, 264, 628, 304};
const Rect kSellL{334, 310, 628, 350};
const Rect kDlgYesL{180, 280, 310, 322};
const Rect kDlgNoL{330, 280, 460, 322};
} // namespace

// Galeride aktif kadroda gezinme: |step| aktif arac ileri / geri (dongusel)
static int stepActive(int id, int step) {
    const int n = (int)vehicleCatalog().size(), d = step > 0 ? 1 : -1;
    for (int k = 0; k < std::abs(step); ++k) {
        int g = 0;
        do { id = ((id - 1 + d) % n + n) % n + 1; } while (!carActive(id) && ++g < n);
    }
    return id;
}

GalleryScreen::GalleryScreen(App& app) : app_(app), carId_(app.career.car().carId) {
    if (const char* c = std::getenv("ZK_GALLERY_CAR")) if (findVehicle(std::atoi(c))) carId_ = std::atoi(c);   // test: model goruntusu
    pickCar(carId_);
}

void GalleryScreen::render(Renderer& r) {
    const bool land = app_.settings.menuLandscape;
    const VehicleDef& v = *findVehicle(carId_);
    const EngineDef& e = engineTable()[v.engine];
    const long price = priceNow();
    const std::string ps = Career::isExclusive(carId_) ? std::string("?") : money(price);
    const float L = (float)v.lengthM;
    char b[128];
    const OwnedCar& cur = app_.career.car();
    const Career& cr = app_.career;
    bool owned = false;
    for (const OwnedCar& oc : app_.career.cars) owned |= oc.carId == carId_;

    r.begin(land ? 640 : 360, land ? 360 : 640, kUiBg);

    if (land) {
        // --- 640x360 YATAY DUKKAN ---
        r.rect(0, 0, 640, 44, kUiPanel);
        r.text(12, 10, "GALERI", 2, kUiGold);
        std::snprintf(b, sizeof b, "#%d %s", v.id, upper(v.fullName()).c_str());
        r.text(110, 12, upper(v.fullName()).substr(0, 32), 2, kUiText);
        const std::string m = money(app_.career.money);
        r.text(628 - r.textWidth(m, 2), 10, m, 2, {0.4f, 1.0f, 0.5f});

        // Sol Alan: 3B Donen Arac Studyosu
        studio(r, 48, 268, 195, 8, 320);
        r.drawCar(v.id, 8, 48, 312, 190, matPerspective(0.75f, 312.0f / 190.0f, 0.1f, 50.0f),
                  matLookAt(0.0f, 1.1f + 0.2f * L, 1.35f * L + 1.2f, 0.0f, 0.55f, 0.0f), matRotY(spin_));

        // Sol Alt Butonlar: Navigasyon, Hurdalik, Geri
        for (int i = 0; i < 4; ++i) button(r, kGNavL[i], i == 0 ? "<<" : i == 1 ? "<" : i == 2 ? ">" : ">>", kUiBtn, 2);
        button(r, kJunkL, "HURDALIK >", Color{0.45f, 0.30f, 0.12f, 0.95f}, 1);
        button(r, kBackGalL, "< GARAJ", kUiBtn, 2);

        // Sag Alan: Arac Bilgileri & Satin Alma
        r.rect(328, 48, 632, 258, {0.08f, 0.09f, 0.13f});
        std::snprintf(b, sizeof b, "%d %s %s %.0fKG", v.year, bodyName(v.body), driveName(v.drive), v.massKg);
        r.text(338, 54, upper(b), 1, kUiText);
        std::snprintf(b, sizeof b, "%s %.1fL %s   %.0f HP / %.0f NM", layoutName(e.layout), e.displacementL, inductionName(e.induction), e.powerHp, e.torqueNm);
        r.text(338, 70, upper(b), 1, kUiText);
        std::snprintf(b, sizeof b, "%s %zuV   ENDEKS %.0f", upper(gearboxName(gearboxTable()[v.gearbox].type)).c_str(), gearboxTable()[v.gearbox].ratios.size(), performanceIndex(v, Tune{}));
        r.text(338, 86, b, 1, kUiGold);
        if (!v.streetLegal) r.text(338, 102, "YARIS ARACI (SOKAKTA SURULEMEZ)", 1, {1.0f, 0.35f, 0.3f});

        // Sifir / 2. El Sekmeleri
        const bool nw = soldNew(v);
        for (int k = 0; k < 4; ++k) {
            const bool on = (k == 0 && listing_ < 0) || (k > 0 && listing_ == k - 1);
            const bool en = k > 0 || nw;
            r.rect(kLTabL[k].x0, kLTabL[k].y0, kLTabL[k].x1, kLTabL[k].y1, on ? Color{0.85f, 0.55f, 0.1f} : en ? Color{0.2f, 0.22f, 0.28f} : Color{0.12f, 0.12f, 0.14f});
            std::snprintf(b, sizeof b, k == 0 ? (nw ? "SIFIR" : "YOK") : "ILAN %d", k);
            r.textCentered(kLTabL[k].cx(), kLTabL[k].y0 + 7, b, 1, en ? Color{1, 1, 1} : kUiDim);
        }
        if (listing_ < 0) std::snprintf(b, sizeof b, "0 KM  FABRIKA CIKISLI");
        else {
            const UsedListing uL = listing();
            std::snprintf(b, sizeof b, "%.0f KM  KOND. %%%d%s%s%s", uL.km, uL.cond, uL.mods ? "  MODIFIYELI" : "",
                          uL.fault ? "  ARIZA: " : "", uL.fault ? faultName(uL.fault) : "");
        }
        r.text(338, 222, b, 1, listing_ >= 0 && listing().fault ? Color{1.0f, 0.55f, 0.35f} : Color{0.6f, 0.9f, 1.0f});

        // Fiyat ve Garaj Durumu
        std::snprintf(b, sizeof b, "GARAJ: %zu / %d", cr.cars.size(), cr.garageSlots);
        r.text(338, 240, b, 1, cr.garageFull() ? Color{1.0f, 0.6f, 0.3f} : kUiDim);
        r.text(624 - r.textWidth(ps, 2), 238, ps, 2, kUiGold);

        // Satin Alma & Satis Dugmeleri
        if (owned) button(r, kBuyL, "GARAJINDA", {0.12f, 0.3f, 0.16f}, 2);
        else if (Career::isExclusive(carId_)) button(r, kBuyL, "KOLEKSIYON: SADECE AHIRDA BULUNUR", {0.35f, 0.25f, 0.08f}, 1);
        else if (cr.garageFull()) {
            if (cr.garageSlots >= Career::kMaxSlots) button(r, kBuyL, "GARAJ DOLU (EN BUYUK)", {0.25f, 0.25f, 0.28f}, 2);
            else button(r, kBuyL, "GARAJ DOLU: YUVA AL " + money(cr.slotPrice()), cr.money >= cr.slotPrice() ? Color{0.15f, 0.35f, 0.6f} : Color{0.25f, 0.25f, 0.28f}, 2);
        } else button(r, kBuyL, "SATIN AL " + ps, app_.career.money >= price ? kUiGreen : Color{0.25f, 0.25f, 0.28f}, 2);

        if (app_.career.cars.size() > 1) {
            std::snprintf(b, sizeof b, "MEVCUT ARACI SAT: %s %s", upper(findVehicle(cur.carId)->model).substr(0, 14).c_str(), money(sellPrice(cur)).c_str());
            button(r, kSellL, b, kUiRed, 1);
        } else {
            r.rect(kSellL.x0, kSellL.y0, kSellL.x1, kSellL.y1, {0.15f, 0.15f, 0.17f});
            r.textCentered(kSellL.cx(), kSellL.cy() - 3, "TEK ARACIN SATILAMAZ", 1, kUiDim);
        }

        if (msgT_ > 0) { r.rect(160, 240, 480, 270, {0.02f, 0.02f, 0.04f, 0.9f}); r.textCentered(320, 248, msg_, 2, kUiGold); }

        // Yatay Onay Penceresi
        if (confirm_ != Confirm::None) {
            if (confirm_ == Confirm::Buy) {
                r.rect(0, 0, 640, 360, {0, 0, 0, 0.65f});
                r.rect(140, 30, 500, 330, {0.10f, 0.11f, 0.16f});
                r.rect(140, 30, 500, 34, kUiGreen);
                r.textCentered(320, 46, "SATIN ALINSIN MI?", 2, {1, 1, 1});
                r.textCentered(320, 72, upper(v.fullName()).substr(0, 40), 1, kUiText);
                r.textCentered(320, 100, money(price), 3, kUiGold);
                if (listing_ >= 0) {
                    const UsedListing uL = listing();
                    std::snprintf(b, sizeof b, "IKINCI EL  %.0f KM  KOND. %%%d", uL.km, uL.cond);
                    r.textCentered(320, 138, b, 1, kUiText);
                    if (uL.fault) { std::snprintf(b, sizeof b, "ARIZA: %s", faultName(uL.fault)); r.textCentered(320, 156, b, 1, {1.0f, 0.5f, 0.3f}); }
                } else r.textCentered(320, 138, "SIFIR  0 KM", 1, kUiText);
                std::snprintf(b, sizeof b, "KALAN PARA: %s", money(app_.career.money - price).c_str());
                r.textCentered(320, 185, b, 2, kUiText);
                button(r, kDlgYesL, "AL", kUiGreen, 2);
                button(r, kDlgNoL, "VAZGEC", kUiBtn, 2);
            } else {
                r.rect(0, 0, 640, 360, {0, 0, 0, 0.65f});
                r.rect(140, 20, 500, 340, {0.10f, 0.11f, 0.16f});
                r.rect(140, 20, 500, 24, kUiRed);
                const VehicleDef& sv = *findVehicle(cur.carId);
                const SaleQuote q = saleQuote(cur);
                r.textCentered(320, 36, "ARAC SATILSIN MI?", 2, {1, 1, 1});
                r.textCentered(320, 62, upper(sv.fullName()).substr(0, 40), 1, kUiText);
                auto row = [&](float y, const char* name, const std::string& val, Color c) {
                    r.text(160, y, name, 2, kUiText);
                    r.text(480 - r.textWidth(val, 2), y, val, 2, c);
                };
                std::snprintf(b, sizeof b, "ARAC %.0fK KM", cur.odo() / 1000.0);
                row(86, b, money(q.car), kUiText);
                row(112, "PARCALAR (%40)", money(q.parts), kUiText);
                row(138, "HASAR", q.damage > 0 ? "-" + money(q.damage) : "YOK", q.damage > 0 ? Color{1.0f, 0.4f, 0.3f} : kUiDim);
                r.rect(160, 162, 480, 164, kUiDim);
                row(172, "TOPLAM", money(q.total), {0.4f, 1.0f, 0.5f});
                button(r, kDlgYesL, "SAT", kUiRed, 2);
                button(r, kDlgNoL, "VAZGEC", kUiBtn, 2);
            }
        }
    } else {
        r.rect(0, 0, 360, 58, kUiPanel);
        r.text(8, 8, "GALERI", 2, kUiGold);
        std::snprintf(b, sizeof b, "#%d %s", v.id, upper(v.fullName()).c_str());
        r.text(8, 32, std::string(b).substr(0, 42), 1, kUiText);
        const std::string m = money(app_.career.money);
        r.text(352 - r.textWidth(m, 2), 8, m, 2, {0.4f, 1.0f, 0.5f});
        studio(r, 60, 290, 190);
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
        {
            const bool nw = soldNew(v);
            for (int k = 0; k < 4; ++k) {
                const bool on = (k == 0 && listing_ < 0) || (k > 0 && listing_ == k - 1);
                const bool en = k > 0 || nw;
                r.rect(kLTab[k].x0, kLTab[k].y0, kLTab[k].x1, kLTab[k].y1, on ? Color{0.85f, 0.55f, 0.1f} : en ? Color{0.2f, 0.22f, 0.28f} : Color{0.12f, 0.12f, 0.14f});
                std::snprintf(b, sizeof b, k == 0 ? (nw ? "SIFIR" : "SIFIRI YOK") : "ILAN %d", k);
                r.textCentered(kLTab[k].cx(), kLTab[k].y0 + 8, b, 1, en ? Color{1, 1, 1} : kUiDim);
            }
            if (listing_ < 0) std::snprintf(b, sizeof b, "0 KM  FABRIKA CIKISLI");
            else {
                const UsedListing L = listing();
                std::snprintf(b, sizeof b, "%.0f KM  KONDISYON %%%d%s%s%s", L.km, L.cond, L.mods ? "  MODIFIYELI" : "",
                              L.fault ? "  ARIZA: " : "", L.fault ? faultName(L.fault) : "");
            }
            r.text(8, 394, b, 1, listing_ >= 0 && listing().fault ? Color{1.0f, 0.55f, 0.35f} : Color{0.6f, 0.9f, 1.0f});
        }
        r.text(352 - r.textWidth(ps, 4), 408, ps, 4, kUiGold);

        for (int i = 0; i < 4; ++i) button(r, kGNav[i], i == 0 ? "<<" : i == 1 ? "<" : i == 2 ? ">" : ">>", kUiBtn, 3);
        if (owned) button(r, kBuy, "GARAJINDA", {0.12f, 0.3f, 0.16f}, 2);
        else if (Career::isExclusive(carId_)) button(r, kBuy, "KOLEKSIYON: SADECE GIZLI AHIRDA BULUNUR", {0.35f, 0.25f, 0.08f}, 1);
        else if (cr.garageFull()) {
            if (cr.garageSlots >= Career::kMaxSlots) button(r, kBuy, "GARAJ DOLU (EN BUYUK)", {0.25f, 0.25f, 0.28f}, 2);
            else button(r, kBuy, "GARAJ DOLU: YUVA AL " + money(cr.slotPrice()), cr.money >= cr.slotPrice() ? Color{0.15f, 0.35f, 0.6f} : Color{0.25f, 0.25f, 0.28f}, 2);
        } else button(r, kBuy, "SATIN AL " + ps, app_.career.money >= price ? kUiGreen : Color{0.25f, 0.25f, 0.28f}, 2);
        std::snprintf(b, sizeof b, "GARAJ %zu / %d", cr.cars.size(), cr.garageSlots);
        r.text(8, 408, b, 1, cr.garageFull() ? Color{1.0f, 0.6f, 0.3f} : kUiDim);
        if (app_.career.cars.size() > 1) {
            std::snprintf(b, sizeof b, "SAT: %s %s", upper(findVehicle(cur.carId)->model).substr(0, 14).c_str(), money(sellPrice(cur)).c_str());
            button(r, kSell, b, kUiRed, 1);
        } else {
            r.rect(kSell.x0, kSell.y0, kSell.x1, kSell.y1, {0.15f, 0.15f, 0.17f});
            r.textCentered(kSell.cx(), kSell.cy() - 3, "TEK ARACIN SATILAMAZ", 1, kUiDim);
        }
        button(r, kBack, "< GARAJ", kUiBtn, 2);
        button(r, kJunk, "HURDALIK >", Color{0.45f, 0.30f, 0.12f, 0.95f}, 1);
        if (msgT_ > 0) { r.rect(0, 250, 360, 280, {0.02f, 0.02f, 0.04f, 0.9f}); r.textCentered(180, 258, msg_, 2, kUiGold); }

        if (confirm_ != Confirm::None) {
            if (confirm_ == Confirm::Buy) {
                r.rect(0, 0, 360, 640, {0, 0, 0, 0.6f});
                r.rect(20, 150, 340, 470, {0.10f, 0.11f, 0.16f});
                r.rect(20, 150, 340, 154, kUiGreen);
                r.textCentered(180, 172, "SATIN ALINSIN MI?", 2, {1, 1, 1});
                r.textCentered(180, 204, upper(v.fullName()).substr(0, 40), 1, kUiText);
                r.textCentered(180, 236, money(price), 4, kUiGold);
                if (listing_ >= 0) {
                    const UsedListing L = listing();
                    std::snprintf(b, sizeof b, "IKINCI EL  %.0f KM  KOND. %%%d", L.km, L.cond);
                    r.textCentered(180, 270, b, 1, kUiText);
                    if (L.fault) { std::snprintf(b, sizeof b, "ARIZA: %s", faultName(L.fault)); r.textCentered(180, 320, b, 1, {1.0f, 0.5f, 0.3f}); }
                } else r.textCentered(180, 270, "SIFIR  0 KM", 1, kUiText);
                std::snprintf(b, sizeof b, "KALAN PARA: %s", money(app_.career.money - price).c_str());
                r.textCentered(180, 290, b, 2, kUiText);
                button(r, kDlgYes, "AL", kUiGreen, 2);
                button(r, kDlgNo, "VAZGEC", kUiBtn, 2);
            } else drawSaleDialog(r, cur);
        }
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
    std::snprintf(b, sizeof b, "ARAC %.0fK KM", cur.odo() / 1000.0);
    row(222, b, money(q.car), kUiText);
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

long GalleryScreen::priceNow() const { return listing_ < 0 ? carPrice(*findVehicle(carId_)) : listing().price; }

void GalleryScreen::confirmAction() {
    std::string why;
    if (confirm_ == Confirm::Buy) {
        if (listing_ >= 0 ? app_.career.buyUsed(listing(), &why) : app_.career.buyCar(carId_, &why)) { msg_ = "HAYIRLI OLSUN!"; msgT_ = 2.0; app_.saveCareer(); }
        else { msg_ = why; msgT_ = 1.8; }
    } else if (confirm_ == Confirm::Sell) {
        const int got = sellPrice(app_.career.car());
        if (app_.career.sellCurrent(&why)) { msg_ = "SATILDI +" + money(got); msgT_ = 2.0; app_.saveCareer(); }
        else { msg_ = why; msgT_ = 1.8; }
    }
    confirm_ = Confirm::None;
}

void GalleryScreen::pointerDown(int, float x, float y) {
    const bool land = app_.settings.menuLandscape;
    if (confirm_ != Confirm::None) {
        const Rect yesR = land ? kDlgYesL : kDlgYes;
        const Rect noR = land ? kDlgNoL : kDlgNo;
        if (yesR.hit(x, y)) confirmAction();
        else if (noR.hit(x, y)) confirm_ = Confirm::None;
        return;
    }
    const Rect junkR = land ? kJunkL : kJunk;
    if (junkR.hit(x, y)) { app_.goJunkyard(); return; }

    for (int i = 0; i < 4; ++i) {
        const Rect navR = land ? kGNavL[i] : kGNav[i];
        if (navR.hit(x, y)) { pickCar(stepActive(carId_, kGStep[i])); return; }
    }
    for (int k = 0; k < 4; ++k) {
        const Rect tabR = land ? kLTabL[k] : kLTab[k];
        if (tabR.hit(x, y)) { if (k > 0 || soldNew(*findVehicle(carId_))) listing_ = k - 1; return; }
    }
    const Rect buyR = land ? kBuyL : kBuy;
    if (buyR.hit(x, y)) {
        if (Career::isExclusive(carId_)) { msg_ = "SATILMAZ: ACIK DUNYADA BUL"; msgT_ = 2.0; return; }
        bool owned = false;
        for (const OwnedCar& oc : app_.career.cars) owned |= oc.carId == carId_;
        if (owned) return;
        if (app_.career.garageFull()) {
            std::string why;
            if (app_.career.buySlot(&why)) { msg_ = "YENI YUVA ACILDI"; app_.saveCareer(); } else msg_ = why;
            msgT_ = 1.8;
            return;
        }
        if (app_.career.money < priceNow()) { msg_ = "PARA YETMIYOR"; msgT_ = 1.8; return; }
        confirm_ = Confirm::Buy;
        return;
    }
    const Rect sellR = land ? kSellL : kSell;
    if (sellR.hit(x, y)) {
        if (app_.career.cars.size() <= 1) { msg_ = "SON ARACINI SATAMAZSIN"; msgT_ = 1.8; return; }
        confirm_ = Confirm::Sell;
        return;
    }
    const Rect backR = land ? kBackGalL : kBack;
    if (backR.hit(x, y)) app_.goGarage();
}

void GalleryScreen::key(Key k, bool down) {
    if (!down) return;
    if (confirm_ != Confirm::None) {
        if (k == Key::Enter) confirmAction();
        else if (k == Key::Back) confirm_ = Confirm::None;
        return;
    }
    if (k == Key::Left) pickCar(stepActive(carId_, -1));
    else if (k == Key::Right) pickCar(stepActive(carId_, 1));
    else if (k == Key::Back) app_.goGarage();
}

// ================================================================== DYNO
namespace {
const Rect kPull{8, 540, 352, 588};
const Rect kTuneT{240, 442, 352, 468};          // ECU AYARI / SONUCLAR
constexpr float kGx0P = 44, kGx1P = 344, kGy0P = 86, kGy1P = 420;
constexpr float kRowYP[3] = {472, 493, 514};
Rect tMinusP(int i) { return {232, kRowYP[i] - 1, 270, kRowYP[i] + 18}; }
Rect tPlusP(int i) { return {314, kRowYP[i] - 1, 352, kRowYP[i] + 18}; }

// Yatay (640x360)
const Rect kPullL{395, 245, 632, 295};
const Rect kBackL{395, 305, 632, 348};
const Rect kTuneTL{500, 50, 632, 82};
constexpr float kGx0L = 40, kGx1L = 380, kGy0L = 52, kGy1L = 295;
constexpr float kRowYL[3] = {112, 144, 176};
Rect tMinusL(int i) { return {520, kRowYL[i] - 2, 570, kRowYL[i] + 24}; }
Rect tPlusL(int i) { return {578, kRowYL[i] - 2, 628, kRowYL[i] + 24}; }
} // namespace

DynoScreen::DynoScreen(App& app) : app_(app) {
    recompute();
    app_.setVoice(1, nullptr);
}

void DynoScreen::recompute() {
    const OwnedCar& oc = app_.career.car();
    const VehicleDef& v = *findVehicle(oc.carId);
    VehicleSimConfig cfg; cfg.car = &v; cfg.tune = &oc.tune;
    const VehicleSim sim(cfg);
    octane_ = sim.octane(); octReq_ = sim.octaneRequired(); valveSafe_ = sim.valveSafeRpm();
    maxNm_ = maxHp_ = 1; peakHp_ = peakNm_ = 0; stockPeakHp_ = 0;
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
    const bool land = app_.settings.menuLandscape;
    r.begin(land ? 640 : 360, land ? 360 : 640, kUiBg);

    const float gx0 = land ? kGx0L : kGx0P;
    const float gx1 = land ? kGx1L : kGx1P;
    const float gy0 = land ? kGy0L : kGy0P;
    const float gy1 = land ? kGy1L : kGy1P;

    if (land) {
        r.rect(0, 0, 640, 42, kUiPanel);
        r.text(12, 10, "DYNO", 2, kUiGold);
        const VehicleDef& v = *findVehicle(app_.career.car().carId);
        r.text(90, 12, upper(v.fullName()).substr(0, 28), 1, kUiText);
        const std::string m = money(app_.career.money);
        r.text(628 - r.textWidth(m, 2), 10, m, 2, {0.4f, 1.0f, 0.5f});
    } else {
        header(r, app_, "DYNO");
    }

    // Eksenler + izgara
    r.rect(gx0, gy0, gx1, gy1, {0.1f, 0.1f, 0.13f});
    const double rpmMax = std::ceil((redline_ + 500) / 1000.0) * 1000.0;
    char b[64];
    for (int k = 0; k <= (int)(rpmMax / 1000); ++k) {
        const float x = gx0 + (float)(k * 1000 / rpmMax) * (gx1 - gx0);
        r.rect(x, gy0, x + 1, gy1, {0.18f, 0.18f, 0.22f});
        if (k % 2 == 0) { std::snprintf(b, sizeof b, "%d", k); r.textCentered(x, gy1 + 4, b, 1, kUiDim); }
    }
    r.textCentered((gx0 + gx1) / 2, gy1 + 14, "X1000 RPM", 1, kUiDim);
    const double yMax = std::max(maxNm_, maxHp_) * 1.1;
    for (int k = 1; k <= 4; ++k) {
        const float y = gy1 - (float)k / 4 * (gy1 - gy0);
        r.rect(gx0, y, gx1, y + 1, {0.18f, 0.18f, 0.22f});
        std::snprintf(b, sizeof b, "%.0f", yMax * k / 4);
        r.text(land ? 8 : 4, y - 3, b, 1, kUiDim);
    }
    auto X = [&](double rpm) { return gx0 + (float)(rpm / rpmMax) * (gx1 - gx0); };
    auto Y = [&](double val) { return gy1 - (float)(val / yMax) * (gy1 - gy0); };
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
    r.rect(gx0 + 6, gy0 + 6, gx0 + 16, gy0 + 10, {1.0f, 0.6f, 0.15f}); r.text(gx0 + 20, gy0 + 5, "TORK NM", 1, kUiText);
    r.rect(gx0 + 86, gy0 + 6, gx0 + 96, gy0 + 10, {0.35f, 0.65f, 1.0f}); r.text(gx0 + 100, gy0 + 5, "GUC HP", 1, kUiText);
    r.text(gx0 + 160, gy0 + 5, "SOLUK: STOK", 1, kUiDim);

    const Tune& t = app_.career.car().tune;
    if (land) {
        button(r, kTuneTL, tuneMode_ ? "SONUCLAR" : "ECU AYARI", tuneMode_ ? kUiBtn : Color{0.15f, 0.35f, 0.6f}, 1);
        if (!tuneMode_) {
            std::snprintf(b, sizeof b, "TEPE %.0f HP @ %.0f", peakHp_, peakHpRpm_);
            r.text(395, 96, b, 2, {0.55f, 0.8f, 1.0f});
            std::snprintf(b, sizeof b, "TEPE %.0f NM @ %.0f", peakNm_, peakNmRpm_);
            r.text(395, 126, b, 2, {1.0f, 0.7f, 0.3f});
            std::snprintf(b, sizeof b, "STOK %.0f HP  (%+.0f HP)", stockPeakHp_, peakHp_ - stockPeakHp_);
            r.text(395, 156, b, 2, kUiText);
        } else if (!ecuFineTuneAvailable(t)) {
            r.text(395, 100, "INCE AYAR ICIN", 2, kUiText);
            r.text(395, 126, "PLUG-IN ECU YA DA USTU GEREK", 1, {1.0f, 0.5f, 0.35f});
        } else {
            std::snprintf(b, sizeof b, "%.0f HP  OKTAN %.0f / %.0f", peakHp_, octane_, octReq_);
            r.text(395, 92, b, 1, octReq_ > octane_ ? Color{1.0f, 0.4f, 0.3f} : kUiText);
            const bool boosted = t.turbo > 0 || t.superch > 0 || engineTable()[effectiveEngine(*findVehicle(app_.career.car().carId), &t)].boostBar > 0;
            const char* names[3] = {"AVANS", "AFR", "BOOST"};
            for (int i = 0; i < 3; ++i) {
                if (i == 2 && !boosted) { r.text(395, kRowYL[i] + 4, "BOOST: ASIRI BESLEME YOK", 1, kUiDim); continue; }
                if (i == 0) std::snprintf(b, sizeof b, "%+d DERECE", t.ecuTiming);
                else if (i == 1) std::snprintf(b, sizeof b, "%.1f : 1", t.ecuAfr > 0 ? t.ecuAfr / 10.0 : 12.5);
                else std::snprintf(b, sizeof b, "%+.1f BAR", t.ecuBoost / 10.0);
                r.text(395, kRowYL[i] + 4, names[i], 2, kUiText);
                r.text(450, kRowYL[i] + 4, b, 1, kUiGold);
                button(r, tMinusL(i), "-", kUiBtn, 2);
                button(r, tPlusL(i), "+", kUiBtn, 2);
            }
        }
        if (pullRpm_ >= 0) {
            const float x = X(pullRpm_);
            r.rect(x, gy0, x + 2, gy1, {1, 1, 1});
            const double nm = curveAt(tuned_, pullRpm_);
            std::snprintf(b, sizeof b, "%5.0f RPM %4.0f HP %4.0f NM", pullRpm_, nm * pullRpm_ * kHpK, nm);
            r.text(395, 205, b, 2, {1, 1, 1});
        }
        button(r, kPullL, pullRpm_ >= 0 ? "CEKILIYOR..." : "DYNO CEKISI", pullRpm_ >= 0 ? kUiBtn : kUiOrange, 2);
        button(r, kBackL, "< GARAJ", kUiBtn, 2);
    } else {
        button(r, kTuneT, tuneMode_ ? "SONUCLAR" : "ECU AYARI", tuneMode_ ? kUiBtn : Color{0.15f, 0.35f, 0.6f}, 1);
        if (!tuneMode_) {
            std::snprintf(b, sizeof b, "TEPE %.0f HP @ %.0f", peakHp_, peakHpRpm_);
            r.text(8, 446, b, 2, {0.55f, 0.8f, 1.0f});
            std::snprintf(b, sizeof b, "TEPE %.0f NM @ %.0f", peakNm_, peakNmRpm_);
            r.text(8, 468, b, 2, {1.0f, 0.7f, 0.3f});
            std::snprintf(b, sizeof b, "STOK %.0f HP  (%+.0f HP)", stockPeakHp_, peakHp_ - stockPeakHp_);
            r.text(8, 490, b, 2, kUiText);
        } else if (!ecuFineTuneAvailable(t)) {
            r.text(8, 448, "INCE AYAR ICIN", 1, kUiText);
            r.text(8, 462, "PLUG-IN ECU YA DA USTU GEREK", 1, {1.0f, 0.5f, 0.35f});
        } else {
            std::snprintf(b, sizeof b, "%.0f HP  OKTAN %.0f / %.0f", peakHp_, octane_, octReq_);
            r.text(8, 448, b, 1, octReq_ > octane_ ? Color{1.0f, 0.4f, 0.3f} : kUiText);
            const bool boosted = t.turbo > 0 || t.superch > 0 || engineTable()[effectiveEngine(*findVehicle(app_.career.car().carId), &t)].boostBar > 0;
            const char* names[3] = {"AVANS", "AFR", "BOOST"};
            for (int i = 0; i < 3; ++i) {
                if (i == 2 && !boosted) { r.text(8, kRowYP[i] + 4, "BOOST: ASIRI BESLEME YOK", 1, kUiDim); continue; }
                if (i == 0) std::snprintf(b, sizeof b, "%+d DERECE", t.ecuTiming);
                else if (i == 1) std::snprintf(b, sizeof b, "%.1f : 1", t.ecuAfr > 0 ? t.ecuAfr / 10.0 : 12.5);
                else std::snprintf(b, sizeof b, "%+.1f BAR", t.ecuBoost / 10.0);
                r.text(8, kRowYP[i] + 2, names[i], 2, kUiText);
                r.text(96, kRowYP[i] + 2, b, 2, kUiGold);
                button(r, tMinusP(i), "-", kUiBtn, 2);
                button(r, tPlusP(i), "+", kUiBtn, 2);
            }
        }
        if (pullRpm_ >= 0) {
            const float x = X(pullRpm_);
            r.rect(x, gy0, x + 2, gy1, {1, 1, 1});
            const double nm = curveAt(tuned_, pullRpm_);
            std::snprintf(b, sizeof b, "%5.0f RPM %4.0f HP %4.0f NM", pullRpm_, nm * pullRpm_ * kHpK, nm);
            r.text(8, 514, b, 2, {1, 1, 1});
        }
        button(r, kPull, pullRpm_ >= 0 ? "CEKILIYOR..." : "DYNO CEKISI", pullRpm_ >= 0 ? kUiBtn : kUiOrange, 3);
        button(r, kBack, "< GARAJ", kUiBtn, 2);
    }
}

void DynoScreen::pointerDown(int, float x, float y) {
    const bool land = app_.settings.menuLandscape;
    const Rect& tuneBtn = land ? kTuneTL : kTuneT;
    const Rect& pullBtn = land ? kPullL : kPull;
    const Rect& backBtn = land ? kBackL : kBack;

    if (tuneBtn.hit(x, y)) { tuneMode_ = !tuneMode_; return; }
    if (tuneMode_ && ecuFineTuneAvailable(app_.career.car().tune)) {
        Tune& t = app_.career.cars[app_.career.current].tune;
        for (int i = 0; i < 3; ++i) {
            const Rect& mBtn = land ? tMinusL(i) : tMinusP(i);
            const Rect& pBtn = land ? tPlusL(i) : tPlusP(i);
            const int d = mBtn.hit(x, y) ? -1 : pBtn.hit(x, y) ? 1 : 0;
            if (!d) continue;
            if (i == 0) t.ecuTiming = std::clamp(t.ecuTiming + d, -4, 6);
            else if (i == 1) t.ecuAfr = std::clamp((t.ecuAfr > 0 ? t.ecuAfr : 125) + d, 115, 135);
            else t.ecuBoost = std::clamp(t.ecuBoost + d, -3, 5);
            app_.saveCareer();
            recompute();
            return;
        }
    }
    if (pullBtn.hit(x, y) && pullRpm_ < 0) pullRpm_ = idle_;
    else if (backBtn.hit(x, y)) app_.goGarage();
}

void DynoScreen::key(Key k, bool down) {
    if (!down) return;
    if (k == Key::Enter && pullRpm_ < 0) pullRpm_ = idle_;
    else if (k == Key::Back) app_.goGarage();
    else if (k == Key::Settings) tuneMode_ = !tuneMode_;
}

} // namespace zk
