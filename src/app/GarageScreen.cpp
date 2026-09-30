#include "Screens.h"
#include "garage/VehicleCatalog.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace zk {

namespace {
struct Btn { float x0, y0, x1, y1; int delta; const char* label; };
const Btn kBtns[] = {{8, 566, 68, 632, -10, "<<"}, {74, 566, 134, 632, -1, "<"}, {140, 566, 200, 632, +1, ">"}, {206, 566, 266, 632, +10, ">>"}};
const float kSl[4] = {292, 380, 352, 632};
const float kRace[4] = {8, 492, 190, 544};
const float kTree[4] = {196, 492, 266, 544};
std::string up(const std::string& s) { std::string o = s; for (char& c : o) c = (char)std::toupper((unsigned char)c); return o; }
bool in(const float* r, float x, float y) { return x >= r[0] && x <= r[2] && y >= r[1] && y <= r[3]; }

// Benzer guc/agirlik oraninda rakip (oyuncunun kendisi haric), deterministik tohumla
int pickOpponent(int playerId, unsigned seed) {
    const auto& cat = vehicleCatalog();
    const VehicleDef* p = findVehicle(playerId);
    const double pw = peakPowerHp(*p) / p->massKg;
    std::vector<int> pool;
    for (const auto& v : cat) {
        if (v.id == playerId || !v.streetLegal) continue;
        const double r = peakPowerHp(v) / v.massKg / pw;
        if (r > 0.85 && r < 1.18) pool.push_back(v.id);
    }
    if (pool.empty()) return playerId;
    return pool[seed % pool.size()];
}
} // namespace

GarageScreen::GarageScreen(App& app) : app_(app) {
    app_.setVoice(1, nullptr);
    select(app_.selectedCar);
}

void GarageScreen::select(int id) {
    const int n = (int)vehicleCatalog().size();
    id = ((id - 1) % n + n) % n + 1;
    app_.selectedCar = id;
    const VehicleDef& v = *findVehicle(id);
    pt_ = std::make_unique<PowertrainCore>(buildEngineSpec(v), ClutchSpec{}, DiffSpec{}, buildGearbox(v));
    pt_->setGear(0);
    pt_->setClutchPedal(1.0);
    app_.setVoice(0, &v);
}

void GarageScreen::update(double dt) {
    if (throttleKey_) throttle_ = std::min(1.0f, throttle_ + (float)dt / 0.12f);
    else if (throttlePtr_ < 0) throttle_ = std::max(0.0f, throttle_ - (float)dt / 0.08f);
    pt_->setThrottle(throttle_);
    acc_ += dt;
    while (acc_ >= 0.001) { pt_->step(0.001, 0.0, 0.0, 1.0); acc_ -= 0.001; }   // bosta, 1 ms
    pt_->drainEvents();
    app_.voice(0, pt_->rpm(), throttle_, pt_->limiterHit(), false, 1.0f);
    app_.tire(0, 0.0); app_.tire(1, 0.0);
    hapT_ -= dt;
    if (pt_->limiterHit() && hapT_ <= 0) { app_.haptic(10, 80); hapT_ = 0.06; }   // kesici tirtiklamasi
    spin_ += (float)dt * 0.6f;
}

void GarageScreen::render(Renderer& r) {
    const VehicleDef& v = *findVehicle(app_.selectedCar);
    const EngineDef& e = engineTable()[v.engine];
    r.begin(360, 640, {0.07f, 0.08f, 0.10f});
    r.gradientV(0, 60, 360, 300, {0.09f, 0.10f, 0.14f}, {0.16f, 0.17f, 0.21f});
    r.rect(0, 0, 360, 58, {0.12f, 0.13f, 0.16f});
    char b[128];
    std::snprintf(b, sizeof b, "#%d  %s", v.id, up(v.brand).c_str());
    r.text(8, 8, b, 2, {0.95f, 0.75f, 0.2f});
    r.text(8, 30, up(v.model).substr(0, 29), 2, {1, 1, 1});

    const float L = (float)v.lengthM;
    const Mat4 proj = matPerspective(0.75f, 360.0f / 230.0f, 0.1f, 50.0f);
    const Mat4 view = matLookAt(0.0f, 1.1f + 0.2f * L, 1.35f * L + 1.2f, 0.0f, 0.55f, 0.0f);
    r.drawCar(v.id, 0, 64, 360, 230, proj, view, matRotY(spin_));

    std::snprintf(b, sizeof b, "%d %s %s %.0fKG", v.year, bodyName(v.body), driveName(v.drive), v.massKg);
    r.text(8, 300, up(b), 2, {0.8f, 0.8f, 0.85f});
    std::snprintf(b, sizeof b, "%s %s %.1fL", e.code, layoutName(e.layout), e.displacementL);
    r.text(8, 320, up(b), 2, {0.8f, 0.8f, 0.85f});
    std::snprintf(b, sizeof b, "%s %.0fHP %.0fNM", inductionName(e.induction), e.powerHp, e.torqueNm);
    r.text(8, 340, up(b), 2, {0.8f, 0.8f, 0.85f});
    std::snprintf(b, sizeof b, "SANZIMAN: %s %zuV", up(gearboxName(gearboxTable()[v.gearbox].type)).c_str(),
                  gearboxTable()[v.gearbox].ratios.size());
    r.text(8, 360, b, 2, {0.8f, 0.8f, 0.85f});
    if (!v.streetLegal) r.text(8, 280, "YARIS ARACI - ROMORK", 2, {1.0f, 0.3f, 0.3f});

    const float rpm = (float)pt_->rpm(), red = (float)e.redline;
    r.rect(8, 390, 280, 426, {0.15f, 0.15f, 0.18f});
    const float fill = std::clamp(rpm / (red * 1.08f), 0.0f, 1.0f);
    const bool hot = rpm > red * 0.9f;
    r.rect(8, 390, 8 + 272 * fill, 426, hot ? Color{0.95f, 0.2f, 0.3f} : Color{0.2f, 0.85f, 0.3f});
    r.rect(8 + 272 / 1.08f, 386, 10 + 272 / 1.08f, 430, {1, 0.2f, 0.2f});
    std::snprintf(b, sizeof b, "%5.0f RPM", rpm);
    r.text(8, 436, b, 3, pt_->limiterHit() ? Color{1.0f, 0.5f, 0.1f} : Color{1, 1, 1});
    if (pt_->vtecActive()) r.text(196, 436, "VTEC", 3, {1.0f, 0.2f, 0.2f});
    std::snprintf(b, sizeof b, "YAG %.1f BAR  YAKIT %.1fG", pt_->oilPressureBar(), pt_->fuelGrams());
    r.text(8, 466, b, 2, {0.7f, 0.8f, 0.7f});

    r.rect(kRace[0], kRace[1], kRace[2], kRace[3], {0.85f, 0.35f, 0.08f});
    r.textCentered((kRace[0] + kRace[2]) / 2, kRace[1] + 19, "DRAG YARISI >", 2, {1, 1, 1});
    r.rect(kTree[0], kTree[1], kTree[2], kTree[3], {0.2f, 0.22f, 0.28f});
    r.textCentered((kTree[0] + kTree[2]) / 2, kTree[1] + 10, "AGAC", 1, {0.7f, 0.7f, 0.75f});
    r.textCentered((kTree[0] + kTree[2]) / 2, kTree[1] + 24, app_.treePro ? "PRO .4" : "SPT .5", 2, {1, 0.85f, 0.3f});
    r.text(8, 550, "ARAC SEC", 2, {0.6f, 0.6f, 0.65f});
    std::snprintf(b, sizeof b, "%2.0f FPS", app_.fps());
    r.text(300, 8, b, 1, {0.45f, 0.5f, 0.45f});
    for (const Btn& bt : kBtns) {
        r.rect(bt.x0, bt.y0, bt.x1, bt.y1, {0.2f, 0.22f, 0.28f});
        r.textCentered((bt.x0 + bt.x1) / 2, bt.y0 + 22, bt.label, 3, {1, 1, 1});
    }
    r.rect(kSl[0], kSl[1], kSl[2], kSl[3], {0.18f, 0.18f, 0.22f});
    r.rect(kSl[0], kSl[3] - (kSl[3] - kSl[1]) * throttle_, kSl[2], kSl[3], {0.9f, 0.55f, 0.1f});
    r.textCentered((kSl[0] + kSl[2]) / 2, kSl[1] - 16, "GAZ", 2, {1, 1, 1});
}

void GarageScreen::pointerDown(int id, float x, float y) {
    if (x >= kSl[0] - 10 && y >= kSl[1] - 20) {
        throttlePtr_ = id;
        throttle_ = std::clamp((kSl[3] - y) / (kSl[3] - kSl[1]), 0.0f, 1.0f);
        return;
    }
    if (in(kTree, x, y)) { app_.treePro = !app_.treePro; return; }
    if (in(kRace, x, y)) { app_.goDrag(app_.selectedCar, pickOpponent(app_.selectedCar, (unsigned)(spin_ * 1000))); return; }
    for (const Btn& b : kBtns)
        if (x >= b.x0 && x <= b.x1 && y >= b.y0 && y <= b.y1) select(app_.selectedCar + b.delta);
}
void GarageScreen::pointerMove(int id, float, float y) {
    if (id == throttlePtr_) throttle_ = std::clamp((kSl[3] - y) / (kSl[3] - kSl[1]), 0.0f, 1.0f);
}
void GarageScreen::pointerUp(int id) { if (id == throttlePtr_) throttlePtr_ = -1; }

void GarageScreen::key(Key k, bool down) {
    if (k == Key::Throttle) throttleKey_ = down;
    if (!down) return;
    if (k == Key::Left) select(app_.selectedCar - 1);
    else if (k == Key::Right) select(app_.selectedCar + 1);
    else if (k == Key::PageUp) select(app_.selectedCar - 10);
    else if (k == Key::PageDown) select(app_.selectedCar + 10);
    else if (k == Key::Enter) app_.goDrag(app_.selectedCar, pickOpponent(app_.selectedCar, (unsigned)(spin_ * 1000)));
}

} // namespace zk
