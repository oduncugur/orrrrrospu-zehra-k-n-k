// ZEHRA KINIK - Acik yol ekrani (Faz 5 ara taslak): prosedurel yol, arkadan kamera, duzlemsel fizik.
#include "Screens.h"
#include "Ui.h"
#include "garage/VehicleCatalog.h"
#include "sim/VehicleSim.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace zk {

namespace {
constexpr double kStep = 5e-5;                         // fizik adimi (drag ile ayni)
const Rect kSteerL{4, 524, 84, 636}, kSteerR{88, 524, 168, 636}, kBrakeB{188, 548, 262, 636}, kGas{268, 524, 356, 636};

struct Proj { float x, y, w; bool ok; };
// Dunya: sim (X ileri, Y sol) -> 3B (x = X, y = yukari, z = -Y)
Proj project(const Mat4& vp, double X, double Y, double h, int vw, int vh) {
    const float x = (float)X, y = (float)h, z = (float)-Y;
    const float cx = vp.m[0] * x + vp.m[4] * y + vp.m[8] * z + vp.m[12];
    const float cy = vp.m[1] * x + vp.m[5] * y + vp.m[9] * z + vp.m[13];
    const float cw = vp.m[3] * x + vp.m[7] * y + vp.m[11] * z + vp.m[15];
    if (cw < 0.4f) return {0, 0, cw, false};
    return {(cx / cw * 0.5f + 0.5f) * vw, (1.0f - (cy / cw * 0.5f + 0.5f)) * vh, cw, true};
}
float hashf(int i) { unsigned x = (unsigned)i * 2654435761u; x ^= x >> 13; x *= 0x5bd1e995u; x ^= x >> 15; return (x & 0xFFFF) / 65535.0f; }
} // namespace

RoadScreen::RoadScreen(App& app, int carId, const Tune* tune)
    : app_(app), carId_(carId), road_(20250930u, 20000.0, 90.0) {
    if (tune) { tune_ = *tune; hasTune_ = true; }
    VehicleSimConfig c;
    c.car = findVehicle(carId); c.tune = hasTune_ ? &tune_ : nullptr; c.planar = true; c.road = "otoban"; c.laneAsymmetry = false;
    sim_ = std::make_unique<VehicleSim>(c);
    sim_->resetPose(0.0, -1.8, 0.0);                   // sag serit
    sim_->powertrain().setGear(1);
    sim_->powertrain().setClutchPedal(1.0);
    app_.setVoice(0, c.car, hasTune_ && tune_.turbo > 0);
    app_.setVoice(1, nullptr);
    msg_ = "SERBEST SURUS - ARA TASLAK"; msgT_ = 3.0;
    autopilot_ = std::getenv("ZK_AUTOPILOT") != nullptr;
}

void RoadScreen::recover() {
    const RoadPoint p = road_.at(std::max(0.0, s_ - 20.0));
    const double c = std::cos(p.heading), s = std::sin(p.heading);
    sim_->resetPose(p.x + 1.8 * s, p.y - 1.8 * c, p.heading);   // sag serit ortasi
    sim_->powertrain().setGear(1); sim_->powertrain().restart();
    launching_ = true; launchPedal_ = 1.0; shiftT_ = -1; camPsi_ = p.heading;
    hint_ = std::max(0, (int)(p.s / RoadPath::kStep));
    msg_ = "ARAC YOLA ALINDI"; msgT_ = 2.0;
}

// Otomatik debriyaj/vites: kalkista devir 1800'de tutulur, vites degisiminde gaz kesilip debriyaj basilir
void RoadScreen::driverAssist(double dt) {
    PowertrainCore& pt = sim_->powertrain();
    const double v = sim_->speed();
    double clutch = 0.0, thr = thr_;
    if (pt.stalled()) { pt.restart(); pt.setGear(1); launching_ = true; launchPedal_ = 1.0; msg_ = "MOTOR STOP ETTI"; msgT_ = 1.5; }
    if (shiftT_ >= 0.0) {
        shiftT_ += dt;
        clutch = shiftT_ < 0.12 ? 1.0 : std::max(0.0, 1.0 - (shiftT_ - 0.12) / 0.14);
        if (shiftT_ < 0.12) thr = 0.0;
        if (shiftT_ > 0.08 && pt.gear() != target_) pt.setGear(target_);
        if (shiftT_ > 0.26) shiftT_ = -1.0;
    } else if (pt.gear() == 1 && v < 3.0 && (launching_ || thr_ < 0.05)) {
        if (thr_ < 0.05 && v < 1.0) launchPedal_ = 1.0;                       // durus: debriyaj basili (stop etmez)
        else {   // kalkis: devir ~2500'de tutulur; pedal hizla isirma noktasina, sonra devre gore birakilir
            const double hold = 1500.0 + 1200.0 * thr_;
            launchPedal_ = std::min(launchPedal_, 0.65);
            launchPedal_ = std::clamp(launchPedal_ + (pt.rpm() < hold ? 0.8 : -1.6) * dt, 0.0, 1.0);
        }
        launching_ = launchPedal_ > 0.0;
        clutch = launchPedal_;
    } else {
        launching_ = false;
        const int gear = pt.gear();
        sinceShift_ += dt;
        if (!manual_ && sinceShift_ > 0.8) {
            // Tekerlek devrinden motor devri (debriyaj kaymasindan bagimsiz): vites secimi bununla
            const double wheelRpm = pt.rpm();
            if (wheelRpm > sim_->shiftRpm() - 150 && gear < pt.gearCount()) { shiftT_ = 0; target_ = gear + 1; sinceShift_ = 0; }
            else if (wheelRpm < 0.36 * sim_->engineSpec().redlineRpm && gear > 1 && sinceShift_ > 1.5) { shiftT_ = 0; target_ = gear - 1; sinceShift_ = 0; }
        }
        if (gear > 1 && v < 2.0) { shiftT_ = 0; target_ = 1; launching_ = true; launchPedal_ = 1.0; }
        if (pt.rpm() < sim_->engineSpec().idleRpm * 0.9 && gear == 1) { launching_ = true; launchPedal_ = 0.6; }
    }
    pt.setClutchPedal(clutch);
    pt.setThrottle(std::clamp(thr, 0.0, 1.0));
}

void RoadScreen::update(double dt) {
    dt = std::min(dt, 0.05);
    // Girdi: klavye veya dokunma. Direksiyon hiza gore sinirlanir, rampali
    const bool L = kL_ || tL_ >= 0, R = kR_ || tR_ >= 0;
    const double v = sim_->speed();
    // Hizla azalan direksiyon: kinematik yanal ivme ~1.1 g ile sinirli (klavye/dokunmatik tam kilit verir)
    const double L0 = sim_->vehicleLoad().wheelbase;
    const double maxSteer = std::clamp(L0 * 1.1 * 9.81 / std::max(v * v, 1.0), 0.035, 0.50);
    const double target = (L ? maxSteer : 0.0) - (R ? maxSteer : 0.0);
    const double rate = (std::fabs(target) > std::fabs(steer_) ? 1.0 : 2.5) * dt;
    steer_ += std::clamp(target - steer_, -rate, rate);
    if (autopilot_) {   // test/demo: saf takip (pure pursuit), sag serit
        const RoadPoint q = road_.at(s_ + 6.0 + 0.35 * v);
        const double tx = q.x + 1.8 * std::sin(q.heading), ty = q.y - 1.8 * std::cos(q.heading);
        const double dx = tx - sim_->posX(), dy = ty - sim_->posY(), ld = std::hypot(dx, dy);
        const double alpha = std::atan2(dy, dx) - sim_->heading();
        const double Lw = sim_->vehicleLoad().wheelbase;
        steer_ = std::clamp(std::atan2(2.0 * Lw * std::sin(alpha), ld) + 0.5 * Lw * road_.at(s_ + 0.2 * v).curvature
                            - 0.02 * (lat_ + 1.8), -0.5, 0.5);   // + egrilik on beslemesi + serit hatasi
        double vMax = 60.0;                                   // ileriye bak: fren mesafesi icindeki en dar yer
        for (double d = 0; d < v * v / (2 * 5.0) + 30.0; d += 8.0) {
            const double k = std::fabs(road_.at(s_ + d).curvature);
            vMax = std::min(vMax, std::sqrt(std::sqrt(0.7 * 9.81 / std::max(k, 1e-4)) * std::sqrt(0.7 * 9.81 / std::max(k, 1e-4)) + 2 * 5.0 * d));
        }
        kT_ = v < vMax; kB_ = v > vMax + 2.0;
    }
    thr_ = (kT_ || tG_ >= 0) ? std::min(1.0, thr_ + dt / 0.15) : std::max(0.0, thr_ - dt / 0.10);
    brake_ = (kB_ || tB_ >= 0) ? std::min(1.0, brake_ + dt / 0.12) : 0.0;

    driverAssist(dt);
    // Zemin: asfalt disi (cim/toprak) daha az tutar
    const bool off = std::fabs(lat_) > road_.halfWidth() + 0.6;
    sim_->setSurfaceMu(off ? 0.55 : 1.0);
    VehicleInputs in; in.steer = steer_; in.brake = brake_;
    if (assist_ && v > 3.0) {
        // Surus yardimi (ESP benzeri): arka kayarsa otomatik karsi direksiyon + gaz kesme
        const double beta = sim_->bodySlipAngle();
        in.steer = std::clamp(steer_ + 0.8 * beta, -0.5, 0.5);
        const double over = std::fabs(beta) - 0.10;
        if (over > 0) sim_->powertrain().setThrottle(sim_->powertrain().throttle() * std::max(0.15, 1.0 - over * 5.0));
    }
    acc_ += dt;
    while (acc_ >= kStep) { sim_->step(kStep, in); acc_ -= kStep; }
    sim_->drainFailureEvents();
    road_.project(sim_->posX(), sim_->posY(), hint_, s_, lat_);
    topSpeed_ = std::max(topSpeed_, sim_->speed());
    offT_ = std::fabs(lat_) > road_.halfWidth() + 12.0 ? offT_ + dt : 0.0;
    if (offT_ > 1.5 || s_ > road_.length() - 60.0) recover();
    camPsi_ += (sim_->heading() - camPsi_) * std::min(1.0, dt * 4.0);   // kamera yumusak takip

    PowertrainCore& pt = sim_->powertrain();
    app_.voice(0, pt.rpm(), pt.throttleEffective(), pt.limiterHit(), pt.gear() > 0, 1.0f);
    double slip = 0;
    for (int i = 0; i < 4; ++i) {
        const WheelSimulation& w = sim_->wheel(i);
        if (w.Fz() < 100) continue;
        slip = std::max({slip, std::fabs(w.omega() * w.rEff() - v) * (off ? 0.2 : 1.0), std::fabs(w.slipAngle()) * v * (off ? 0.2 : 0.9)});
    }
    app_.tire(0, slip); app_.tire(1, 0.0);
    msgT_ -= dt;
    if (std::getenv("ZK_ROAD_LOG")) { static double t = 0, nx = 0; t += dt; if (t >= nx) { nx += 0.5;
        std::printf("t=%.1f v=%.1f g%d rpm=%.0f cl=%.2f thr=%.2f s=%.1f lat=%.2f psi=%.3f launch=%d st=%d steer=%.3f ay=%.2f beta=%.3f k=%.4f\n", t, v, pt.gear(), pt.rpm(), launchPedal_, thr_, s_, lat_, sim_->heading(), (int)launching_, (int)pt.stalled(), steer_, sim_->lateralAccel(), sim_->bodySlipAngle(), road_.at(s_).curvature); } }
}

void RoadScreen::render(Renderer& r) {
    const int W = 360, H = 640;
    r.begin(W, H, {0.36f, 0.55f, 0.28f});
    const double X = sim_->posX(), Y = sim_->posY();
    const double cp = std::cos(camPsi_), sp = std::sin(camPsi_);
    // Kamera: arabanin 7.5 m arkasi, 2.7 m yukari; 5 m ilerisine bakar
    const Mat4 proj = matPerspective(1.05f, (float)W / H, 0.3f, 900.0f);
    const Mat4 view = matLookAt((float)(X - 7.5 * cp), 2.7f, (float)-(Y - 7.5 * sp), (float)(X + 5 * cp), 0.8f, (float)-(Y + 5 * sp));
    const Mat4 vp = matMul(proj, view);
    // Gokyuzu: ufuk cizgisine kadar
    const Proj hz = project(vp, X + 3000 * cp, Y + 3000 * sp, 0.0, W, H);
    const float horizon = hz.ok ? std::clamp(hz.y, 0.0f, (float)H) : H * 0.35f;
    r.gradientV(0, 0, W, horizon, {0.30f, 0.45f, 0.85f}, {0.85f, 0.70f, 0.55f});
    r.rect(0, horizon, W, horizon + 3, {0.35f, 0.42f, 0.38f});   // uzak dag/sis seridi

    // Yol seritleri: uzaktan yakina (ressam algoritmasi)
    const double hw = road_.halfWidth();
    const int i0 = std::max(0, (int)((s_ - 8.0) / RoadPath::kStep)), n = (int)road_.points().size();
    const int i1 = std::min(n - 2, i0 + 160);
    const auto& P = road_.points();
    auto edge = [&](int i, double off) {
        const RoadPoint& p = P[i];
        return project(vp, p.x - off * std::sin(p.heading), p.y + off * std::cos(p.heading), 0.0, W, H);
    };
    for (int i = i1; i >= i0; --i) {
        const int j = i + 1;
        const Proj aL = edge(i, hw + 1.0), aR = edge(i, -hw - 1.0), bL = edge(j, hw + 1.0), bR = edge(j, -hw - 1.0);
        if (!aL.ok || !aR.ok || !bL.ok || !bR.ok) continue;
        const bool band = ((i / 3) & 1) != 0;
        // banket (kirmizi/beyaz kaldirim)
        const Color curb = band ? Color{0.85f, 0.15f, 0.12f} : Color{0.92f, 0.92f, 0.9f};
        r.tri(aL.x, aL.y, aR.x, aR.y, bR.x, bR.y, curb); r.tri(aL.x, aL.y, bR.x, bR.y, bL.x, bL.y, curb);
        const Proj cL = edge(i, hw), cR = edge(i, -hw), dL = edge(j, hw), dR = edge(j, -hw);
        const Color asp = band ? Color{0.30f, 0.30f, 0.32f} : Color{0.27f, 0.27f, 0.29f};
        r.tri(cL.x, cL.y, cR.x, cR.y, dR.x, dR.y, asp); r.tri(cL.x, cL.y, dR.x, dR.y, dL.x, dL.y, asp);
        if ((i % 5) < 2) {                                         // orta kesik cizgi (4 m cizgi, 6 m bosluk)
            const Proj m0 = edge(i, 0.08), m1 = edge(i, -0.08), m2 = edge(j, 0.08), m3 = edge(j, -0.08);
            r.tri(m0.x, m0.y, m1.x, m1.y, m3.x, m3.y, {0.95f, 0.9f, 0.6f}); r.tri(m0.x, m0.y, m3.x, m3.y, m2.x, m2.y, {0.95f, 0.9f, 0.6f});
        }
        // Kenar agaclari / direkleri (her 16 m, yol kenarindan 5-25 m)
        if (i % 8 == 0) {
            for (int side = -1; side <= 1; side += 2) {
                const float h = hashf(i * 2 + (side > 0));
                if (h < 0.35f) continue;
                const Proj b = edge(i, side * (hw + 5.0 + 20.0 * hashf(i * 7 + side)));
                if (!b.ok) continue;
                const float sc = 360.0f / b.w;                     // metre -> piksel (yaklasik)
                const float th = (5.0f + 4.0f * h) * sc * 0.55f, tw = (1.6f + h) * sc * 0.55f;
                if (h > 0.8f) {                                    // elektrik diregi
                    r.rect(b.x - 0.12f * sc, b.y - 7.5f * sc * 0.55f, b.x + 0.12f * sc, b.y, {0.35f, 0.28f, 0.2f});
                } else {
                    r.rect(b.x - tw * 0.12f, b.y - th * 0.35f, b.x + tw * 0.12f, b.y, {0.35f, 0.24f, 0.14f});
                    r.tri(b.x - tw, b.y - th * 0.3f, b.x + tw, b.y - th * 0.3f, b.x, b.y - th, {0.12f, 0.38f + 0.1f * h, 0.16f});
                }
            }
        }
    }
    r.flush2D();
    // Arac
    const auto& su = sim_->suspension();
    const Mat4 model = matMul(matTranslate((float)X, (float)su.heave(), (float)-Y), matRotY((float)sim_->heading()));
    r.drawCar(carId_, 0, 0, W, H, proj, view, model);

    // HUD
    PowertrainCore& pt = sim_->powertrain();
    char b[64];
    r.rect(0, 0, W, 58, {0.02f, 0.02f, 0.04f, 0.75f});
    std::snprintf(b, sizeof b, "%3.0f", sim_->speed() * 3.6);
    r.text(8, 8, b, 5, {1, 1, 1});
    r.text(104, 30, "KM/H", 1, {0.7f, 0.7f, 0.75f});
    std::snprintf(b, sizeof b, "%d", pt.gear());
    r.text(150, 8, b, 5, manual_ ? Color{1.0f, 0.62f, 0.05f} : Color{0.6f, 0.9f, 1.0f});
    r.text(182, 30, manual_ ? "MANUEL" : "OTO", 1, {0.7f, 0.7f, 0.75f});
    const float red = (float)sim_->engineSpec().redlineRpm, fill = std::clamp((float)pt.rpm() / (red * 1.05f), 0.0f, 1.0f);
    r.rect(230, 12, 352, 24, {0.15f, 0.15f, 0.18f});
    r.rect(230, 12, 230 + 122 * fill, 24, pt.rpm() > red * 0.9 ? Color{0.95f, 0.2f, 0.3f} : Color{0.2f, 0.85f, 0.3f});
    std::snprintf(b, sizeof b, "%5.0f RPM", pt.rpm());
    r.text(236, 30, b, 1, {1, 1, 1});
    std::snprintf(b, sizeof b, "%.2f KM  %.2f G  KAYMA %2.0f", s_ / 1000.0, std::fabs(sim_->lateralAccel()) / 9.81, std::fabs(sim_->bodySlipAngle()) * 57.3);
    r.text(8, 46, b, 1, {0.75f, 0.8f, 0.9f});
    if (std::fabs(lat_) > road_.halfWidth() + 0.6) r.textCentered(W / 2.0f, 70, "YOL DISI", 2, {1.0f, 0.4f, 0.2f});
    if (msgT_ > 0) r.textCentered(W / 2.0f, 92, msg_, 1, {1.0f, 0.85f, 0.3f});
#ifndef __ANDROID__
    r.text(8, 510, "</> DIREKSIYON  W GAZ  S FREN  E/Q VITES  ESC", 1, {0.55f, 0.75f, 1.0f});
#endif
    // Dokunmatik kontroller
    button(r, kSteerL, "<", tL_ >= 0 ? kUiOrange : Color{0.2f, 0.22f, 0.28f, 0.8f}, 4);
    button(r, kSteerR, ">", tR_ >= 0 ? kUiOrange : Color{0.2f, 0.22f, 0.28f, 0.8f}, 4);
    button(r, kBrakeB, "FREN", brake_ > 0 ? Color{0.8f, 0.15f, 0.15f} : Color{0.3f, 0.12f, 0.12f, 0.85f}, 2);
    button(r, kGas, "GAZ", thr_ > 0 ? Color{0.2f, 0.7f, 0.25f} : Color{0.12f, 0.3f, 0.14f, 0.85f}, 2);
    r.flush2D();
}

void RoadScreen::pointerDown(int id, float x, float y) {
    if (kSteerL.hit(x, y)) tL_ = id;
    else if (kSteerR.hit(x, y)) tR_ = id;
    else if (kBrakeB.hit(x, y)) tB_ = id;
    else if (kGas.hit(x, y)) tG_ = id;
    else if (y < 60) {                                      // ustteki gosterge: manuel/otomatik degistir
        manual_ = !manual_; msg_ = manual_ ? "MANUEL VITES (E/Q)" : "OTOMATIK VITES"; msgT_ = 1.5;
    } else if (y > 380 && y < 520) {                        // manuel: ekranin sol/sag alt yarisi vites
        if (manual_ && shiftT_ < 0) {
            PowertrainCore& pt = sim_->powertrain();
            target_ = std::clamp(pt.gear() + (x > 180 ? 1 : -1), 1, pt.gearCount());
            if (target_ != pt.gear()) shiftT_ = 0;
        }
    }
}
void RoadScreen::pointerMove(int, float, float) {}
void RoadScreen::pointerUp(int id) {
    if (id == tL_) tL_ = -1;
    if (id == tR_) tR_ = -1;
    if (id == tB_) tB_ = -1;
    if (id == tG_) tG_ = -1;
}

void RoadScreen::key(Key k, bool down) {
    switch (k) {
    case Key::Left: kL_ = down; break;
    case Key::Right: kR_ = down; break;
    case Key::Throttle: kT_ = down; break;
    case Key::Brake: kB_ = down; break;
    case Key::ShiftUp: case Key::ShiftDown:
        if (down && shiftT_ < 0) {
            PowertrainCore& pt = sim_->powertrain();
            manual_ = true;
            target_ = std::clamp(pt.gear() + (k == Key::ShiftUp ? 1 : -1), 1, pt.gearCount());
            if (target_ != pt.gear()) shiftT_ = 0;
        }
        break;
    case Key::Enter: if (down) { manual_ = !manual_; msg_ = manual_ ? "MANUEL VITES" : "OTOMATIK VITES"; msgT_ = 1.5; } break;
    case Key::Back: if (down) app_.goGarage(); break;
    default: break;
    }
}

} // namespace zk
