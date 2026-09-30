#include "Screens.h"
#include "garage/VehicleCatalog.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace zk {

namespace {
// ---- yerlesim (sanal 640x360) ----
constexpr float kPx = 22.0f;                         // piksel / metre (yakin serit)
constexpr float kWorldTop = 30, kHorizon = 150, kWall0 = 180, kFar0 = 186, kFarGround = 211, kBarrier0 = 214,
                kNear0 = 218, kNearGround = 252, kWorldBottom = 262;
constexpr float kClutch[4] = {4, 36, 58, 356}, kThrottle[4] = {582, 36, 636, 356};
constexpr float kBrake[4] = {436, 268, 496, 354};
constexpr float kShift[4] = {500, 266, 578, 356};
constexpr float kColX[3] = {512, 538, 564};
constexpr float kRowTop = 274, kRowMid = 311, kRowBot = 348;
constexpr float kPadDn[4] = {500, 280, 536, 350}, kPadUp[4] = {542, 280, 578, 350};
constexpr float kStageBtn[4] = {250, 120, 390, 156};
constexpr float kAgain[4] = {170, 262, 310, 296}, kGarage[4] = {330, 262, 470, 296};
constexpr float kCamLead = 8.0f;                     // oyuncu arac merkezi, ekranin solundan 8 m sagda

bool in(const float* r, float x, float y) { return x >= r[0] && x <= r[2] && y >= r[1] && y <= r[3]; }
std::string up(const std::string& s) { std::string o = s; for (char& c : o) c = (char)std::toupper((unsigned char)c); return o; }
float sliderValue(const float* r, float y) { return std::clamp((r[3] - 6 - y) / (r[3] - r[1] - 12), 0.0f, 1.0f); }
float hash01(int i) { unsigned x = (unsigned)i * 2654435761u; x ^= x >> 13; x *= 0x5bd1e995u; x ^= x >> 15; return (x & 0xFFFF) / 65535.0f; }
std::string sec(double t) { if (t < 0) return "--.---"; char b[16]; std::snprintf(b, sizeof b, "%6.3f", t); return b; }

const Color kDim{0.18f, 0.16f, 0.12f}, kAmber{1.0f, 0.62f, 0.05f}, kGreen{0.1f, 1.0f, 0.25f}, kRed{1.0f, 0.1f, 0.1f};
} // namespace

DragScreen::DragScreen(App& app, int playerCar, int opponentCar) : app_(app), seed_(1234) {
    carIds_[0] = playerCar; carIds_[1] = opponentCar;
    app_.setVoice(0, findVehicle(playerCar));
    app_.setVoice(1, findVehicle(opponentCar));
    restart();
}

void DragScreen::restart() {
    seed_ = seed_ * 1103515245u + 12345u;
    race_ = std::make_unique<DragRace>(carIds_[0], carIds_[1], app_.treePro ? TreeType::Pro : TreeType::Sportsman, seed_, true);
    smoke_.clear(); ticker_.clear(); touches_.clear();
    clutchUi_ = throttleUi_ = 0; brakeBtn_ = false;
    knobX_ = kColX[0]; knobY_ = kRowTop; pendingGear_ = 1; pendingPaddle_ = 0;
    finishedT_ = 0; t_ = 0;
    hapGear_ = 1; hapFlat_ = 0; hapLeft_ = hapBroke_ = hapRed_ = false; hapLimiterT_ = 0;
    race_->setPlayerAutopilot(autopilot_);
}

DragScreen::Ctl DragScreen::hit(float x, float y) const {
    if (in(kClutch, x, y)) return Ctl::Clutch;
    if (in(kThrottle, x, y)) return Ctl::Throttle;
    if (in(kBrake, x, y)) return Ctl::Brake;
    const Gearbox box = race_->lane(0).sim->gearboxType();
    if (box == Gearbox::HPattern && in(kShift, x, y)) return Ctl::Shifter;
    if (box == Gearbox::Dogbox || box == Gearbox::DCT) {
        if (in(kPadUp, x, y)) return Ctl::PaddleUp;
        if (in(kPadDn, x, y)) return Ctl::PaddleDown;
    }
    return Ctl::None;
}

// H-desen: kol yalnizca bos sirasinda yana kayar; sutunda yukari/asagi. Olmayan vitese girmez.
void DragScreen::shifterFromPoint(float x, float y) {
    const int gears = race_->lane(0).sim->powertrain().gearCount();
    const bool inNeutralRow = std::fabs(knobY_ - kRowMid) < 5.0f;
    if (inNeutralRow) knobX_ = std::clamp(x, kColX[0], kColX[2]);
    int col = 0;
    for (int c = 1; c < 3; ++c) if (std::fabs(knobX_ - kColX[c]) < std::fabs(knobX_ - kColX[col])) col = c;
    const float ny = std::clamp(y, kRowTop, kRowBot);
    if (std::fabs(ny - kRowMid) >= 5.0f) {
        const int target = col * 2 + (ny < kRowMid ? 1 : 2);
        if (target > gears) { knobY_ = kRowMid; return; }       // bu sutunda vites yok
        knobX_ = kColX[col];
    }
    knobY_ = ny;
    if (knobY_ <= kRowTop + 6) pendingGear_ = col * 2 + 1;
    else if (knobY_ >= kRowBot - 6) pendingGear_ = col * 2 + 2;
    else if (std::fabs(knobY_ - kRowMid) < 12) pendingGear_ = 0;
}

void DragScreen::pointerDown(int id, float x, float y) {
    const RacePhase ph = race_->phase();
    if (ph == RacePhase::Finished && finishedT_ > 1.5) {
        if (in(kAgain, x, y)) { restart(); return; }
        if (in(kGarage, x, y)) { app_.goGarage(); return; }
    }
    if (ph == RacePhase::Burnout && in(kStageBtn, x, y)) { race_->skipBurnout(); return; }
    const Ctl c = hit(x, y);
    touches_.push_back({id, c});
    switch (c) {
    case Ctl::Clutch: clutchUi_ = sliderValue(kClutch, y); break;
    case Ctl::Throttle: throttleUi_ = sliderValue(kThrottle, y); break;
    case Ctl::Brake: brakeBtn_ = true; break;
    case Ctl::Shifter: shifterFromPoint(x, y); break;
    case Ctl::PaddleUp: pendingPaddle_ = +1; break;
    case Ctl::PaddleDown: pendingPaddle_ = -1; break;
    default: break;
    }
}

void DragScreen::pointerMove(int id, float x, float y) {
    for (const Touch& t : touches_) {
        if (t.id != id) continue;
        if (t.ctl == Ctl::Clutch) clutchUi_ = sliderValue(kClutch, y);
        else if (t.ctl == Ctl::Throttle) throttleUi_ = sliderValue(kThrottle, y);
        else if (t.ctl == Ctl::Shifter) shifterFromPoint(x, y);
    }
}

void DragScreen::pointerUp(int id) {
    for (size_t i = 0; i < touches_.size(); ++i) {
        if (touches_[i].id != id) continue;
        switch (touches_[i].ctl) {
        case Ctl::Clutch: clutchUi_ = 0.0f; break;      // ayak pedaldan kalkti
        case Ctl::Throttle: throttleUi_ = 0.0f; break;
        case Ctl::Brake: brakeBtn_ = false; break;
        default: break;
        }
        touches_.erase(touches_.begin() + i);
        return;
    }
}

void DragScreen::key(Key k, bool down) {
    switch (k) {
    case Key::Throttle: keyThr_ = down; break;
    case Key::Clutch: keyClutch_ = down; break;
    case Key::Brake: keyBrake_ = down; break;
    default: break;
    }
    if (!down) return;
    const Gearbox box = race_->lane(0).sim->gearboxType();
    const int gear = race_->lane(0).sim->powertrain().gear();
    auto setKnob = [&](int g) {
        pendingGear_ = g;
        if (g == 0) { knobY_ = kRowMid; return; }
        knobX_ = kColX[(g - 1) / 2]; knobY_ = (g % 2) ? kRowTop : kRowBot;
    };
    if (k >= Key::Gear0 && k <= Key::Gear6 && box == Gearbox::HPattern) {
        const int g = (int)k - (int)Key::Gear0;
        if (g <= race_->lane(0).sim->powertrain().gearCount()) setKnob(g);
    }
    if (k == Key::ShiftUp || k == Key::ShiftDown) {
        const int d = k == Key::ShiftUp ? 1 : -1;
        if (box == Gearbox::HPattern) setKnob(std::clamp(gear + d, 0, race_->lane(0).sim->powertrain().gearCount()));
        else pendingPaddle_ = d;
    }
    if (k == Key::Enter) {
        if (race_->phase() == RacePhase::Burnout) race_->skipBurnout();
        else if (race_->phase() == RacePhase::Finished && finishedT_ > 1.5) restart();
    }
    if (k == Key::Back) app_.goGarage();
}

void DragScreen::update(double dt) {
    t_ += dt;
    // Klavye analog rampalari: debriyaj yavas birakilir (kalkis icin), gaz hizli
    float& kc = keyClutchVal_; float& kt = keyThrVal_;
    kc = keyClutch_ ? std::min(1.0f, kc + (float)dt / 0.08f) : std::max(0.0f, kc - (float)dt / 0.35f);
    kt = keyThr_ ? std::min(1.0f, kt + (float)dt / 0.12f) : std::max(0.0f, kt - (float)dt / 0.08f);
    pc_.clutch = std::max(clutchUi_, kc);
    pc_.throttle = std::max(throttleUi_, kt);
    pc_.brake = (keyBrake_ || brakeBtn_) ? 1.0 : 0.0;
    const Gearbox box = race_->lane(0).sim->gearboxType();
    pc_.requestedGear = box == Gearbox::HPattern ? pendingGear_ : -1;
    pc_.paddle = pendingPaddle_;
    pendingPaddle_ = 0;
    race_->advance(dt, pc_);
    for (auto& e : race_->drainEvents()) {
        const bool mine = e.rfind("SEN:", 0) == 0;
        if (e == "YESIL!") { flash_ = "YESIL!"; flashColor_ = {0.2f, 1.0f, 0.3f}; flashT_ = 0.9; continue; }
        if (e == "KAZANDIN!" || e == "KAYBETTIN") { flash_ = e; flashColor_ = e == "KAZANDIN!" ? Color{0.2f, 1.0f, 0.3f} : Color{1.0f, 0.25f, 0.2f}; flashT_ = 1.4; continue; }
        if (mine && e.find("KIRMIZI") != std::string::npos) { flash_ = "KIRMIZI ISIK!"; flashColor_ = {1.0f, 0.15f, 0.1f}; flashT_ = 1.6; }
        if (mine && e.find("CITIRTISI") != std::string::npos) { flash_ = "DEBRIYAJ!"; flashColor_ = {1.0f, 0.6f, 0.1f}; flashT_ = 0.6; }
        ticker_.push_back(e); tickerT_ = 3.0;
        if (ticker_.size() > 2) ticker_.erase(ticker_.begin());
    }
    tickerT_ -= dt; flashT_ -= dt;
    if (tickerT_ <= 0 && !ticker_.empty()) { ticker_.erase(ticker_.begin()); tickerT_ = ticker_.empty() ? 0 : 2.0; }
    if (race_->phase() == RacePhase::Finished) finishedT_ += dt;

    // ---- duman: tahrikli tekerlek kayma hizi yuksekse ----
    for (int lane = 0; lane < 2; ++lane) {
        const VehicleSim& s = *race_->lane(lane).sim;
        const VehicleDef* v = race_->lane(lane).car;
        const double wb = v->wheelbaseM, L = v->lengthM;
        const double xFront = L * 0.5 - (L - wb) * (v->frontWeight > 0.5 ? 0.54 : 0.44), xRear = xFront - wb;
        for (int side = 0; side < 2; ++side) {
            const int wi = side ? s.drivenRight() : s.drivenLeft();
            const WheelSimulation& w = s.wheel(wi);
            const double slip = std::fabs(w.omega() * w.rEff() - s.speed());
            if (slip < 3.0 || std::fabs(w.Fx()) < 500) continue;
            const float n = (float)std::min(1.0, (slip - 3.0) / 10.0);
            if (hash01((int)(t_ * 997) + lane * 31 + side) > 0.35f + 0.6f * n) continue;
            const double wx = s.distance() + (wi < 2 ? xFront : xRear);
            smoke_.push_back({(float)wx, (float)lane, 0.25f, (float)(-0.3 * s.speed() - 1.0 + 2.0 * hash01((int)(t_ * 331))),
                              0.5f + 0.8f * hash01((int)(t_ * 713) + 5), 1.6f, 0.35f});
        }
    }
    for (Smoke& p : smoke_) { p.x += p.vx * (float)dt; p.y += p.vy * (float)dt; p.vy *= 0.98f; p.size += (float)dt * 1.1f; p.life -= (float)dt; }
    smoke_.erase(std::remove_if(smoke_.begin(), smoke_.end(), [](const Smoke& p) { return p.life <= 0; }), smoke_.end());
    if (smoke_.size() > 400) smoke_.erase(smoke_.begin(), smoke_.begin() + (smoke_.size() - 400));

    camX_ = (float)race_->lane(0).sim->distance() - kCamLead;

    // ---- haptik (oyuncu serdi) ----
    {
        const LaneState& P = race_->lane(0);
        const PowertrainCore& pt = P.sim->powertrain();
        if (P.left && !hapLeft_) app_.haptic(45, 220);                       // kalkis vurusu
        if (pt.gear() != hapGear_ && pt.gear() > 0) app_.haptic(22, 150);   // vites
        if (P.slip.broke && !hapBroke_) app_.haptic(320, 255);              // aks kirildi
        if (P.slip.redLight && !hapRed_) app_.haptic(180, 200);
        hapLimiterT_ -= dt;
        if ((pt.limiterHit() || P.cutIgnition) && hapLimiterT_ <= 0) { app_.haptic(10, 90); hapLimiterT_ = 0.06; }   // kesici tirtiklamasi
        int flat = 0;
        for (int i = 0; i < 4; ++i) flat += P.sim->wheel(i).hapticPulseCount();
        if (flat > hapFlat_) app_.haptic(14, (int)std::clamp(80 + 400 * P.sim->hapticIntensity(), 80.0, 255.0));   // flat-spot turu
        hapLeft_ = P.left; hapGear_ = pt.gear() > 0 ? pt.gear() : hapGear_; hapBroke_ = P.slip.broke; hapRed_ = P.slip.redLight; hapFlat_ = flat;
    }

    // ---- ses ----
    for (int lane = 0; lane < 2; ++lane) {
        const LaneState& L = race_->lane(lane);
        const PowertrainCore& pt = L.sim->powertrain();
        float gain = 1.0f;
        if (lane == 1) {
            const double gap = std::fabs(L.sim->distance() - race_->lane(0).sim->distance());
            gain = (float)(0.45 * std::clamp(1.0 - gap / 150.0, 0.15, 1.0));
        }
        app_.voice(lane, pt.rpm(), pt.throttleEffective(), pt.limiterHit() || L.cutIgnition, pt.gear() > 0, gain);
        double slip = 0;
        for (int i = 0; i < 4; ++i) {
            const WheelSimulation& w = L.sim->wheel(i);
            if (w.Fz() > 100) slip = std::max(slip, std::fabs(w.omega() * w.rEff() - L.sim->speed()));
        }
        app_.tire(lane, slip);
    }
}

// ------------------------------------------------------------------ cizim
void DragScreen::drawCarAt(Renderer& r, int lane, float sx, float groundY, float scale) {
    const LaneState& L = race_->lane(lane);
    const VehicleDef* v = L.car;
    const float px = kPx * scale;
    const float w = ((float)v->lengthM + 2.0f) * px, h = ((float)v->heightM + 1.6f) * px;
    const float bottom = -1.3f;                       // ortho alt siniri (m); zemin buradan ~0.41 m yukarida gorunur
    const float y = groundY - h + 0.41f * px;
    const Mat4 proj = matOrtho(-w / 2 / px, w / 2 / px, bottom, bottom + h / px, -20, 20);
    const Mat4 view = matLookAt(0, 2.2f, 10, 0, 0.9f, 0);
    const Suspension& su = L.sim->suspension();
    const Mat4 model = matMul(matTranslate(0, (float)su.heave(), 0), matRotZ((float)(su.pitchDeg() * 3.14159265 / 180.0)));
    r.drawCar(v->id, sx - w / 2, y, w, h, proj, view, model);

    // Egzoz alevi: devir kesici / dogbox atesleme kesme
    const PowertrainCore& pt = L.sim->powertrain();
    if ((pt.limiterHit() || L.cutIgnition) && pt.rpm() > 0.6 * L.sim->engineSpec().redlineRpm && hash01((int)(t_ * 60) + lane) > 0.3f) {
        const float ex = sx - (float)v->lengthM * 0.5f * px, ey = groundY - 0.28f * px;
        const float len = (10 + 14 * hash01((int)(t_ * 120) + 7 * lane)) * scale;
        r.tri(ex, ey - 3 * scale, ex, ey + 3 * scale, ex - len, ey, {1.0f, 0.55f, 0.1f, 0.9f});
        r.tri(ex, ey - 1.5f * scale, ex, ey + 1.5f * scale, ex - len * 0.6f, ey, {1.0f, 0.95f, 0.5f, 0.95f});
    }
}

void DragScreen::drawWorld(Renderer& r) {
    const float cam = camX_;
    auto sx = [&](float wx, float factor) { return 64.0f + (wx - cam * factor) * kPx; };
    // Gokyuzu, gunes
    r.gradientV(0, kWorldTop, 640, kHorizon, {0.16f, 0.22f, 0.48f}, {0.96f, 0.60f, 0.38f});
    r.circle(470 - cam * 0.2f * 0.02f * kPx, 118, 22, 16, {1.0f, 0.86f, 0.58f});
    // Daglar (paralaks 0.04)
    for (int layer = 0; layer < 2; ++layer) {
        const float f = layer ? 0.07f : 0.04f, period = layer ? 180.0f : 260.0f;
        const Color c = layer ? Color{0.26f, 0.17f, 0.30f} : Color{0.36f, 0.24f, 0.40f};
        const float off = std::fmod(cam * f * kPx, period);
        for (int i = -1; i < 640 / (int)period + 3; ++i) {
            const float x0 = i * period - off;
            const float hgt = 30 + 30 * hash01(i + (int)(cam * f * kPx / period) + layer * 100);
            r.tri(x0, kHorizon, x0 + period * 0.5f, kHorizon - hgt, x0 + period, kHorizon, c);
        }
    }
    // Sehir silueti (0.12)
    {
        const float f = 0.12f, period = 36.0f, off = std::fmod(cam * f * kPx, period);
        const int base = (int)(cam * f * kPx / period);
        for (int i = -1; i < 20; ++i) {
            const float x0 = i * period - off, hgt = 12 + 34 * hash01(base + i + 500);
            r.rect(x0, kHorizon - hgt, x0 + period - 4, kHorizon, {0.14f, 0.12f, 0.22f});
            for (int wy = 0; wy < (int)(hgt / 8); ++wy)
                if (hash01(base + i * 7 + wy) > 0.55f) r.rect(x0 + 6, kHorizon - hgt + 4 + wy * 8, x0 + 10, kHorizon - hgt + 7 + wy * 8, {0.95f, 0.8f, 0.4f});
        }
    }
    // Tribun (0.55) + seyirci + isik direkleri
    r.rect(0, kHorizon, 640, kWall0, {0.22f, 0.22f, 0.26f});
    {
        const float f = 0.55f, period = 8.0f, off = std::fmod(cam * f * kPx, period);
        const int base = (int)(cam * f * kPx / period);
        for (int row = 0; row < 4; ++row) {
            r.rect(0, kHorizon + 4 + row * 7, 640, kHorizon + 5 + row * 7, {0.3f, 0.3f, 0.34f});
            for (int i = -1; i < 82; ++i) {
                const float h = hash01((base + i) * 13 + row);
                if (h < 0.35f) continue;
                const Color cc = h > 0.85f ? Color{0.9f, 0.2f, 0.2f} : h > 0.7f ? Color{0.2f, 0.4f, 0.9f} : h > 0.55f ? Color{0.95f, 0.85f, 0.3f} : Color{0.85f, 0.85f, 0.85f};
                const float x = i * period - off + (row % 2) * 4;
                r.rect(x, kHorizon + row * 7, x + 3, kHorizon + row * 7 + 4, cc);
            }
        }
        const float pf = 0.55f, pp = 60.0f * kPx * pf;
        const float poff = std::fmod(cam * pf * kPx, pp);
        for (int i = -1; i < 4; ++i) {
            const float x = i * pp - poff + 100;
            r.rect(x, 60, x + 3, kWall0, {0.35f, 0.35f, 0.38f});
            r.rect(x - 10, 56, x + 13, 62, {0.95f, 0.95f, 0.8f});
        }
    }
    // Duvar + reklam panolari (1.0)
    r.rect(0, kWall0, 640, kFar0, {0.72f, 0.72f, 0.68f});
    for (int i = (int)(cam / 30.0f) - 1; i < (int)(cam / 30.0f) + 3; ++i) {
        const float x = sx(i * 30.0f + 12.0f, 1.0f);
        static const char* ads[] = {"100 OKTAN", "SLICK", "ZK MOTOR", "KINIK", "DYNO"};
        static const Color adc[] = {{0.8f, 0.1f, 0.1f}, {0.1f, 0.2f, 0.6f}, {0.95f, 0.7f, 0.1f}, {0.1f, 0.5f, 0.2f}, {0.2f, 0.2f, 0.2f}};
        const int k = ((i % 5) + 5) % 5;
        r.rect(x, kWall0 - 12, x + 88, kWall0, adc[k]);
        r.textCentered(x + 44, kWall0 - 10, ads[k], 1, {1, 1, 1});
    }
    // Seritler
    r.rect(0, kFar0, 640, kBarrier0, {0.20f, 0.20f, 0.22f});
    r.rect(0, kFar0 + 12, 640, kFar0 + 20, {0.15f, 0.15f, 0.16f});           // lastik izi
    r.rect(0, kBarrier0, 640, kNear0, {0.78f, 0.78f, 0.74f});
    r.rect(0, kNear0, 640, kWorldBottom - 4, {0.23f, 0.23f, 0.25f});
    r.rect(0, kNear0 + 18, 640, kNear0 + 30, {0.17f, 0.17f, 0.18f});
    r.rect(0, kWorldBottom - 6, 640, kWorldBottom - 4, {0.9f, 0.9f, 0.9f});
    r.rect(0, kWorldBottom - 4, 640, kWorldBottom, {0.16f, 0.32f, 0.14f});
    // 10 m isaretleri
    for (int m = (int)(cam / 10.0f) * 10 - 10; m < cam + 32; m += 10) {
        const float x = sx((float)m, 1.0f);
        r.rect(x, kBarrier0, x + 2, kNear0, {0.3f, 0.3f, 0.3f});
    }
    // Baslangic ve bitis cizgisi, tabelalar
    const float start = sx(0.0f, 1.0f);
    r.rect(start - 1, kFar0, start + 2, kWorldBottom - 4, {0.95f, 0.95f, 0.95f});
    const float fin = sx((float)DragRace::kQuarterMile, 1.0f);
    for (int i = 0; i < 16; ++i)
        for (int j = 0; j < 2; ++j)
            r.rect(fin + j * 5, kFar0 + i * 4.75f, fin + j * 5 + 5, kFar0 + (i + 1) * 4.75f, ((i + j) % 2) ? Color{0.05f, 0.05f, 0.05f} : Color{0.95f, 0.95f, 0.95f});
    struct Mark { double m; const char* label; };
    static const Mark marks[] = {{18.288, "60 FT"}, {100.584, "330 FT"}, {201.168, "1/8"}, {304.8, "1000 FT"}, {402.336, "BITIS"}};
    for (const Mark& mk : marks) {
        const float x = sx((float)mk.m, 1.0f);
        if (x < -80 || x > 720) continue;
        r.rect(x - 1, kWall0 - 34, x + 1, kWall0, {0.3f, 0.3f, 0.3f});
        r.rect(x - 34, kWall0 - 46, x + 34, kWall0 - 32, {0.1f, 0.1f, 0.1f});
        r.textCentered(x, kWall0 - 43, mk.label, 1, {1.0f, 0.85f, 0.2f});
    }
    // Pist agaci (baslangicta, bariyer uzerinde)
    {
        const float x = sx(-1.2f, 1.0f);
        const int lights = race_->treeLights();
        r.rect(x - 1, kBarrier0 - 44, x + 1, kBarrier0, {0.2f, 0.2f, 0.2f});
        r.rect(x - 6, kBarrier0 - 50, x + 6, kBarrier0 - 12, {0.12f, 0.12f, 0.12f});
        const Color c[5] = {(lights & 1) ? kAmber : kDim, (lights & 2) ? kAmber : kDim, (lights & 4) ? kAmber : kDim,
                            (lights & 8) ? kGreen : kDim, (lights & 16) ? kRed : kDim};
        for (int i = 0; i < 5; ++i) r.rect(x - 3, kBarrier0 - 47 + i * 7, x + 3, kBarrier0 - 42 + i * 7, c[i]);
    }
}

void DragScreen::drawHud(Renderer& r) {
    const LaneState& P = race_->lane(0);
    const LaneState& O = race_->lane(1);
    const PowertrainCore& pt = P.sim->powertrain();
    const EngineSpec& es = P.sim->engineSpec();
    const Gearbox box = P.sim->gearboxType();
    const RacePhase ph = race_->phase();
    char b[128];

    // ---- ust serit ----
    r.rect(0, 0, 640, kWorldTop, {0.07f, 0.07f, 0.09f, 0.95f});
    auto laneLine = [&](const LaneState& L, float x, const char* who, Color c) {
        const double et = L.left ? (L.slip.finished ? L.slip.quarter : race_->clock() - L.leaveTime) : -1;
        std::snprintf(b, sizeof b, "%s RT %s ET %s %3.0f KM/H", who, L.left ? sec(L.slip.reaction).c_str() : "--.---",
                      sec(et).c_str(), L.sim->speed() * 3.6);
        r.text(x, 4, b, 1, c);
        std::snprintf(b, sizeof b, "%s", up(L.car->model).substr(0, 22).c_str());
        r.text(x, 16, b, 1, {0.65f, 0.65f, 0.7f});
    };
    laneLine(P, 64, "SEN  ", {1, 1, 1});
    laneLine(O, 400, "RAKIP", {1.0f, 0.75f, 0.55f});
    std::snprintf(b, sizeof b, "%2.0fFPS %4.1fMS", app_.fps(), app_.updateMs());
    r.text(548, 16, b, 1, {0.45f, 0.5f, 0.45f});

    // ---- buyuk agac (orta ust) ----
    {
        const int lights = race_->treeLights();
        r.rect(298, 34, 342, 146, {0.08f, 0.08f, 0.09f, 0.92f});
        for (int lane = 0; lane < 2; ++lane) {
            const float x = lane ? 331 : 309;
            const bool staged = race_->lane(lane).staged;
            r.circle(x, 40, 3, 8, staged || ph != RacePhase::Burnout ? Color{1, 1, 0.9f} : kDim);
            r.circle(x, 49, 3, 8, staged ? Color{1, 1, 0.9f} : kDim);
            for (int a = 0; a < 3; ++a) r.circle(x, 63 + a * 16.0f, 6, 12, (lights & (1 << a)) ? kAmber : kDim);
            r.circle(x, 113, 6, 12, (lights & 8) && !(lights & (lane ? 32 : 16)) ? kGreen : kDim);
            r.circle(x, 131, 6, 12, (lights & (lane ? 32 : 16)) ? kRed : kDim);
        }
    }

    // ---- olay kutusu (agacin solu) + kritik an flasi ----
    if (!ticker_.empty() && !(ph == RacePhase::Finished && finishedT_ > 1.5)) {
        r.rect(66, 34, 292, 38 + 12.0f * ticker_.size(), {0.02f, 0.02f, 0.04f, 0.72f});
        for (size_t i = 0; i < ticker_.size(); ++i) r.text(70, 37 + i * 12.0f, ticker_[i].substr(0, 37), 1, {1.0f, 0.95f, 0.6f});
    }
    if (flashT_ > 0 && !(ph == RacePhase::Finished && finishedT_ > 1.5)) {
        const float w = r.textWidth(flash_, 4) + 24;
        r.rect(320 - w / 2, 160, 320 + w / 2, 196, {0.02f, 0.02f, 0.04f, 0.78f});
        r.textCentered(320, 164, flash_, 4, flashColor_);
    }
    if (ph == RacePhase::Burnout) {
        r.rect(kStageBtn[0], kStageBtn[1], kStageBtn[2], kStageBtn[3], {0.1f, 0.55f, 0.2f});
        r.textCentered(320, 131, "STAGE >", 2, {1, 1, 1});
    }

    // ---- kizaklar ----
    auto slider = [&](const float* rc, float v, const char* label, Color fillC) {
        r.rect(rc[0], rc[1], rc[2], rc[3], {0.10f, 0.10f, 0.12f, 0.72f});
        const float y = rc[3] - 6 - (rc[3] - rc[1] - 12) * v;
        r.rect(rc[0] + 4, y, rc[2] - 4, rc[3] - 4, fillC);
        r.rect(rc[0], y - 2, rc[2], y + 2, {1, 1, 1});
        for (size_t i = 0; label[i]; ++i) r.textCentered((rc[0] + rc[2]) / 2, rc[1] + 8 + i * 16.0f, std::string(1, label[i]), 2, {1, 1, 1});
    };
    if (box == Gearbox::HPattern || box == Gearbox::Dogbox) {
        slider(kClutch, (float)pc_.clutch, "DEBRIYAJ", {0.25f, 0.55f, 0.95f, 0.85f});
        // Isirma bolgesi (ClutchSpec: pedal 0.32 .. 0.62)
        const float y0 = kClutch[3] - 6 - (kClutch[3] - kClutch[1] - 12) * 0.62f, y1 = kClutch[3] - 6 - (kClutch[3] - kClutch[1] - 12) * 0.32f;
        r.rect(kClutch[2] - 4, y0, kClutch[2], y1, {1.0f, 0.8f, 0.1f});
    }
    slider(kThrottle, (float)pc_.throttle, "GAZ", {0.95f, 0.55f, 0.1f, 0.9f});

    // ---- gosterge paneli ----
    r.rect(62, kWorldBottom, 580, 360, {0.09f, 0.09f, 0.11f});
    const int gear = pt.gear();
    r.rect(66, 268, 128, 354, {0.14f, 0.14f, 0.17f});
    r.textCentered(97, 283, gear == 0 ? "N" : std::to_string(gear), 8, P.grind ? kRed : Color{1, 1, 1});
    // Devir seridi (30 segment) + vites isigi
    const float rpm = (float)pt.rpm(), red = (float)es.redlineRpm, shiftAt = (float)P.sim->shiftRpm();
    for (int i = 0; i < 30; ++i) {
        const float segRpm = red * 1.05f * (i + 1) / 30.0f;
        const bool lit = rpm >= segRpm - red * 1.05f / 30.0f;
        Color c = segRpm > red ? kRed : segRpm > shiftAt - 800 ? kAmber : kGreen;
        if (!lit) c = {c.r * 0.18f, c.g * 0.18f, c.b * 0.18f};
        r.rect(136 + i * 9.8f, 268, 136 + i * 9.8f + 8, 290, c);
    }
    if (rpm > shiftAt - 150 && std::fmod(t_, 0.12) < 0.06) r.rect(136, 264, 430, 267, {0.3f, 0.6f, 1.0f});
    std::snprintf(b, sizeof b, "%5.0f RPM", rpm);
    r.text(136, 296, b, 2, pt.limiterHit() ? kAmber : Color{1, 1, 1});
    if (pt.vtecActive()) r.text(268, 296, "VTEC", 2, kRed);
    std::snprintf(b, sizeof b, "%3.0f KM/H", P.sim->speed() * 3.6);
    r.text(320, 296, b, 2, {1, 1, 1});
    // Talimat
    const char* hint = "";
    const bool autoClutch = box == Gearbox::DCT || box == Gearbox::TorqueConverter;
    if (ph == RacePhase::Burnout) hint = autoClutch ? "BURNOUT: FRENE BAS + GAZ" : "BURNOUT: GAZ + DEBRIYAJI KAYDIR";
    else if ((ph == RacePhase::Staging || ph == RacePhase::Tree) && !P.armed) hint = autoClutch ? "FRENE BAS VE TUT" : "DEBRIYAJA BAS VE TUT";
    else if (ph == RacePhase::Staging || ph == RacePhase::Tree) hint = autoClutch ? "FREN + GAZ, YESILDE FRENI BIRAK" : "DEBRIYAJ + GAZ, YESILDE BIRAK";
    else if (ph == RacePhase::Run) hint = box == Gearbox::HPattern ? "VITES: DEBRIYAJ BAS + KOL" : box == Gearbox::TorqueConverter ? "OTOMATIK: SADECE GAZ" : "VITES: + / -";
    r.text(136, 322, hint, 1, {0.75f, 0.8f, 0.9f});
    std::snprintf(b, sizeof b, "YAG %.1f BAR  BALATA %.0fC  LASTIK %.0fC", pt.oilPressureBar(), pt.clutchTempC(),
                  P.sim->wheel(P.sim->drivenLeft()).tempC());
    r.text(136, 336, b, 1, {0.6f, 0.65f, 0.6f});
    // Fren
    r.rect(kBrake[0], kBrake[1], kBrake[2], kBrake[3], pc_.brake > 0 ? Color{0.8f, 0.15f, 0.15f} : Color{0.3f, 0.12f, 0.12f});
    r.textCentered((kBrake[0] + kBrake[2]) / 2, 304, "FREN", 2, {1, 1, 1});
    // Vites kolu / pedallar
    r.rect(kShift[0], kShift[1], kShift[2], kShift[3], {0.13f, 0.13f, 0.16f});
    if (box == Gearbox::HPattern) {
        const int gears = pt.gearCount();
        r.rect(kColX[0] - 2, kRowMid - 2, kColX[2] + 2, kRowMid + 2, {0.35f, 0.35f, 0.4f});
        for (int c = 0; c < 3; ++c) {
            r.rect(kColX[c] - 2, kRowTop, kColX[c] + 2, kRowBot, c * 2 + 1 <= gears ? Color{0.35f, 0.35f, 0.4f} : Color{0.2f, 0.2f, 0.22f});
            if (c * 2 + 1 <= gears) r.text(kColX[c] - 2, kRowTop - 8, std::to_string(c * 2 + 1), 1, {0.8f, 0.8f, 0.8f});
            if (c * 2 + 2 <= gears) r.text(kColX[c] - 2, kRowBot + 2, std::to_string(c * 2 + 2), 1, {0.8f, 0.8f, 0.8f});
        }
        r.circle(knobX_, knobY_, 8, 12, P.grind ? kRed : Color{0.9f, 0.9f, 0.92f});
    } else if (box == Gearbox::TorqueConverter) {
        r.textCentered(539, 300, "OTO", 2, {0.8f, 0.8f, 0.9f});
    } else {
        r.rect(kPadDn[0], kPadDn[1], kPadDn[2], kPadDn[3], {0.22f, 0.24f, 0.3f});
        r.rect(kPadUp[0], kPadUp[1], kPadUp[2], kPadUp[3], {0.22f, 0.24f, 0.3f});
        r.textCentered((kPadDn[0] + kPadDn[2]) / 2, 305, "-", 3, {1, 1, 1});
        r.textCentered((kPadUp[0] + kPadUp[2]) / 2, 305, "+", 3, {1, 1, 1});
    }
}

void DragScreen::drawResults(Renderer& r) {
    const LaneState& P = race_->lane(0);
    const LaneState& O = race_->lane(1);
    r.rect(110, 40, 530, 304, {0.03f, 0.03f, 0.05f, 0.96f});
    const bool won = race_->winner() == 0;
    r.textCentered(320, 50, won ? "KAZANDIN!" : "KAYBETTIN", 3, won ? kGreen : kRed);
    r.text(250, 80, "SEN", 2, {1, 1, 1});
    r.text(390, 80, "RAKIP", 2, {1.0f, 0.75f, 0.55f});
    auto row = [&](int i, const char* label, const std::string& a, const std::string& o) {
        const float y = 100 + i * 17.0f;
        r.text(126, y, label, 2, {0.7f, 0.7f, 0.75f});
        r.text(236, y, a, 2, {1, 1, 1});
        r.text(376, y, o, 2, {1, 1, 1});
    };
    auto kmh = [](double v) { char b[16]; std::snprintf(b, sizeof b, "%6.1f", v); return std::string(b); };
    row(0, "RT", P.slip.redLight ? "KIRMIZI" : sec(P.slip.reaction), O.slip.redLight ? "KIRMIZI" : sec(O.slip.reaction));
    row(1, "60 FT", sec(P.slip.sixtyFt), sec(O.slip.sixtyFt));
    row(2, "330 FT", sec(P.slip.t330), sec(O.slip.t330));
    row(3, "1/8", sec(P.slip.eighth), sec(O.slip.eighth));
    row(4, "1/8 KMH", kmh(P.slip.eighthKmh), kmh(O.slip.eighthKmh));
    row(5, "1000 FT", sec(P.slip.t1000), sec(O.slip.t1000));
    row(6, "1/4 ET", sec(P.slip.quarter), sec(O.slip.quarter));
    row(7, "TRAP", kmh(P.slip.trapKmh), kmh(O.slip.trapKmh));
    if (P.slip.broke || O.slip.broke || P.slip.stalled || O.slip.stalled) {
        std::string n = std::string(P.slip.broke ? "SEN: AKS KIRIK " : "") + (O.slip.broke ? "RAKIP: AKS KIRIK " : "") +
                        (P.slip.stalled ? "SEN: STOP " : "") + (O.slip.stalled ? "RAKIP: STOP" : "");
        r.textCentered(320, 240, n, 1, kRed);
    }
    r.rect(kAgain[0], kAgain[1], kAgain[2], kAgain[3], {0.1f, 0.55f, 0.2f});
    r.textCentered((kAgain[0] + kAgain[2]) / 2, kAgain[1] + 10, "TEKRAR", 2, {1, 1, 1});
    r.rect(kGarage[0], kGarage[1], kGarage[2], kGarage[3], {0.25f, 0.27f, 0.35f});
    r.textCentered((kGarage[0] + kGarage[2]) / 2, kGarage[1] + 10, "GARAJ", 2, {1, 1, 1});
}

void DragScreen::render(Renderer& r) {
    r.begin(640, 360, {0.05f, 0.05f, 0.06f});
    drawWorld(r);
    // Rakip (uzak serit, kucuk) sonra oyuncu (yakin serit)
    const float playerSx = 64.0f + kCamLead * kPx;
    const float oppSx = playerSx + (float)(race_->lane(1).sim->distance() - race_->lane(0).sim->distance()) * kPx;
    if (oppSx > -150 && oppSx < 790) drawCarAt(r, 1, oppSx, kFarGround, 0.8f);
    // Duman (uzak serit arabanin onunde, yakin seritten once)
    auto drawSmoke = [&](int lane) {
        for (const Smoke& p : smoke_) {
            if ((int)p.lane != lane) continue;
            const float scale = lane ? 0.8f : 1.0f;
            const float x = 64.0f + (p.x - camX_) * kPx * (lane ? 1.0f : 1.0f);
            const float y = (lane ? kFarGround : kNearGround) - p.y * kPx * scale;
            r.circle(x, y, p.size * kPx * 0.5f * scale, 10, {0.82f, 0.82f, 0.84f, 0.5f * p.life / 1.6f});
        }
    };
    drawSmoke(1);
    drawCarAt(r, 0, playerSx, kNearGround, 1.0f);
    drawSmoke(0);
    drawHud(r);
    if (race_->phase() == RacePhase::Finished && finishedT_ > 1.5) drawResults(r);
    r.flush2D();
}

} // namespace zk
