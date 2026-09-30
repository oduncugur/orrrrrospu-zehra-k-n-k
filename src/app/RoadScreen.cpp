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
const Rect kFree{40, 220, 320, 280}, kRace{40, 296, 320, 356}, kTouge{40, 372, 320, 432};
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

RoadScreen::RoadScreen(App& app, int carId, const Tune* tune) : app_(app), carId_(carId) {
    if (tune) tune_ = *tune;
    app_.setVoice(0, findVehicle(carId), tune_.turbo > 0);
    app_.setVoice(1, nullptr);
    autopilot_ = std::getenv("ZK_AUTOPILOT") != nullptr;
    if (const char* m = std::getenv("ZK_ROAD_MODE")) {
        const std::string n = m;
        start(n == "free" ? RoadSession::Mode::Free : RoadSession::Mode::Race, n == "touge" ? RoadSession::Kind::Touge : RoadSession::Kind::Highway);
    }
    else if (autopilot_) start(RoadSession::Mode::Free);
}

void RoadScreen::start(RoadSession::Mode m, RoadSession::Kind kind) {
    int rival = 0; Tune rt;
    if (m == RoadSession::Mode::Race) {
        const Opponent o = pickOpponent(carId_, tune_, (uint32_t)(app_.career.races * 7919 + 17));
        rival = o.carId; rt = o.tune;
        app_.setVoice(1, findVehicle(rival), rt.turbo > 0);
    }
    ses_ = std::make_unique<RoadSession>(m, carId_, &tune_, rival, &rt, (uint32_t)(app_.career.races + 1) * 2654435761u, kind);
    camPsi_ = ses_->player().sim().heading();
    menu_ = false; rewarded_ = false; prize_ = 0; finT_ = 0;
    flash(m == RoadSession::Mode::Free ? "SERBEST SURUS - ARA TASLAK" : kind == RoadSession::Kind::Touge ? "DAG YOLU 3 KM - ARA TASLAK" : "YOL YARISI 4 KM - ARA TASLAK", 2.5);
}

void RoadScreen::finishRace() {
    rewarded_ = true;
    if (autopilot_ || !ses_->rival()) return;
    app_.career.recordRace(*findVehicle(ses_->rivalCarId()), ses_->playerWon(), 0.0, &prize_);
    const VehicleSim& ps = ses_->player().sim();
    app_.career.recordDamage(false, ps.failure().bearingDamage(), ps.failure().bearingSpun());
    app_.saveCareer();
}

void RoadScreen::update(double dt) {
    msgT_ -= dt;
    if (menu_ || !ses_) { app_.voice(0, 900, 0, false, false, 0.6f); app_.tire(0, 0); app_.tire(1, 0); return; }
    RoadCar& P = ses_->player();
    const double v = P.sim().speed();
    // Direksiyon: hiza gore sinirli (kinematik yanal ivme ~1.1 g), rampali; klavye veya dokunma
    const bool L = kL_ || tL_ >= 0, Rr = kR_ || tR_ >= 0;
    const double maxSteer = std::clamp(P.sim().vehicleLoad().wheelbase * 1.1 * 9.81 / std::max(v * v, 1.0), 0.035, 0.50);
    double target = (L ? maxSteer : 0.0) - (Rr ? maxSteer : 0.0);
    if (!L && !Rr && app_.tiltAvailable && app_.tiltSteer) {   // telefon egimi (olu bolge %6)
        const double t = app_.tilt(), dz = 0.06;
        const double u = std::fabs(t) < dz ? 0.0 : (t - std::copysign(dz, t)) / (1.0 - dz);
        target = u * maxSteer;
    }
    const double rate = (std::fabs(target) > std::fabs(steer_) ? 1.0 : 2.5) * dt;
    steer_ += std::clamp(target - steer_, -rate, rate);
    thr_ = (kT_ || tG_ >= 0) ? std::min(1.0, thr_ + dt / 0.15) : std::max(0.0, thr_ - dt / 0.10);
    brake_ = (kB_ || tB_ >= 0) ? std::min(1.0, brake_ + dt / 0.12) : 0.0;
    RoadControls c{steer_, thr_, brake_};
    if (autopilot_) {
        double cap = 1e9;
        for (const TrafficCar& t : ses_->traffic())
            if (!t.oncoming && t.s > P.s() && t.s - P.s() < 40.0) cap = std::min(cap, t.v);
        c = P.aiControls(-ses_->lane(), 0.55, cap);
    }
    ses_->update(dt, c);
    for (auto& m : ses_->drainMessages()) flash(m);
    if (ses_->takeCrash()) app_.haptic(220, 255);
    if (ses_->mode() == RoadSession::Mode::Race && ses_->phase() == RoadSession::Phase::Finished) {
        finT_ += dt;
        if (!rewarded_) finishRace();
    }
    camPsi_ += std::remainder(P.sim().heading() - camPsi_, 6.283185307179586) * std::min(1.0, dt * 4.0);

    PowertrainCore& pt = P.sim().powertrain();
    app_.voice(0, pt.rpm(), pt.throttleEffective(), pt.limiterHit(), pt.gear() > 0, 1.0f);
    app_.tire(0, P.tireSlipSpeed());
    if (RoadCar* rv = ses_->rival()) {
        // Rakip sesi mesafeye gore kisilir
        const double d = std::hypot(rv->sim().posX() - P.sim().posX(), rv->sim().posY() - P.sim().posY());
        PowertrainCore& rp = rv->sim().powertrain();
        app_.voice(1, rp.rpm(), rp.throttleEffective(), rp.limiterHit(), rp.gear() > 0, (float)std::clamp(8.0 / (d + 8.0), 0.0, 0.8));
        app_.tire(1, 0.0);
    } else app_.tire(1, 0.0);
    if (std::getenv("ZK_ROAD_LOG")) { static double t = 0, nx = 0; t += dt; if (t >= nx) { nx += 0.5;
        std::printf("t=%.1f v=%.1f g%d s=%.1f lat=%.2f gap=%.1f phase=%d\n", t, v, pt.gear(), P.s(), P.lateral(), ses_->gapMeters(), (int)ses_->phase()); } }
}

void RoadScreen::render(Renderer& r) {
    const int W = 360, H = 640;
    r.begin(W, H, {0.36f, 0.55f, 0.28f});
    char b[80];
    if (menu_ || !ses_) {
        r.gradientV(0, 0, W, H, {0.10f, 0.12f, 0.2f}, {0.05f, 0.05f, 0.07f});
        r.textCentered(W / 2.0f, 120, "ACIK YOL", 4, {1.0f, 0.62f, 0.05f});
        r.textCentered(W / 2.0f, 170, "ARA TASLAK", 1, {0.6f, 0.6f, 0.65f});
        button(r, kFree, "SERBEST SURUS", Color{0.15f, 0.45f, 0.7f}, 2);
        button(r, kRace, "YOL YARISI 4 KM", kUiOrange, 2);
        button(r, kTouge, "DAG YOLU 3 KM", Color{0.55f, 0.2f, 0.6f}, 2);
        r.textCentered(W / 2.0f, 448, "YARISLAR: RAKIP + TRAFIK, ODULLU", 1, {0.7f, 0.7f, 0.75f});
#ifndef __ANDROID__
        r.textCentered(W / 2.0f, 470, "BOSLUK SERBEST ENTER YARIS PGUP DAG ESC", 1, {0.55f, 0.75f, 1.0f});
#endif
        r.flush2D();
        return;
    }
    const RoadPath& R = ses_->road();
    RoadCar& Pc = ses_->player();
    const VehicleSim& sim = Pc.sim();
    const double ps = Pc.s();
    const double X = sim.posX(), Y = sim.posY();
    const double cp = std::cos(camPsi_), sp = std::sin(camPsi_);
    // Kamera: arabanin 7.5 m arkasi, 2.7 m yukari; 5 m ilerisine bakar
    const Mat4 proj = matPerspective(1.05f, (float)W / H, 0.3f, 900.0f);
    const Mat4 view = matLookAt((float)(X - 7.5 * cp), 2.7f, (float)-(Y - 7.5 * sp), (float)(X + 5 * cp), 0.8f, (float)-(Y + 5 * sp));
    const Mat4 vp = matMul(proj, view);
    const Proj hz = project(vp, X + 3000 * cp, Y + 3000 * sp, 0.0, W, H);
    const float horizon = hz.ok ? std::clamp(hz.y, 0.0f, (float)H) : H * 0.35f;
    const bool mtn = ses_->kind() == RoadSession::Kind::Touge;
    r.rect(0, horizon, W, H, mtn ? Color{0.2f, 0.36f, 0.2f} : Color{0.36f, 0.55f, 0.28f});
    r.gradientV(0, 0, W, horizon, mtn ? Color{0.18f, 0.2f, 0.42f} : Color{0.30f, 0.45f, 0.85f}, mtn ? Color{0.95f, 0.55f, 0.35f} : Color{0.85f, 0.70f, 0.55f});
    if (mtn)   // uzak daglar (siluet)
        for (int k = 0; k < 7; ++k) { const float x0 = k * 60.0f - 20.0f, hh = 40.0f + 30.0f * hashf(k + 3); r.tri(x0, horizon, x0 + 90, horizon, x0 + 45, horizon - hh, {0.25f, 0.24f, 0.36f}); }
    r.rect(0, horizon, W, horizon + 3, {0.35f, 0.42f, 0.38f});

    // Yol seritleri: uzaktan yakina (ressam algoritmasi)
    const double hw = R.halfWidth();
    const bool touge = ses_->kind() == RoadSession::Kind::Touge;
    const int i0 = std::max(0, (int)((ps - 8.0) / RoadPath::kStep)), n = (int)R.points().size();
    const int i1 = std::min(n - 2, i0 + 160);
    const auto& P = R.points();
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
        const Color curb = touge ? (band ? Color{0.62f, 0.64f, 0.66f} : Color{0.8f, 0.8f, 0.78f})      // dag: celik bariyer
                                 : (band ? Color{0.85f, 0.15f, 0.12f} : Color{0.92f, 0.92f, 0.9f});
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
                if (h < (touge ? 0.08f : 0.35f)) continue;             // dag yolunda sik orman
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
    // Bitis cizgisi (yarista)
    if (ses_->mode() == RoadSession::Mode::Race) {
        const double fs = RoadSession::kStartS + ses_->raceLength();
        if (fs > ps - 5 && fs < ps + 300) {
            const RoadPoint q = R.at(fs), q2 = R.at(fs + 1.5);
            for (int k = 0; k < 10; ++k) {
                const double o0 = -hw + k * (2 * hw / 10), o1 = o0 + 2 * hw / 10;
                const Proj a = project(vp, q.x - o0 * std::sin(q.heading), q.y + o0 * std::cos(q.heading), 0.02, W, H);
                const Proj bq = project(vp, q.x - o1 * std::sin(q.heading), q.y + o1 * std::cos(q.heading), 0.02, W, H);
                const Proj c2 = project(vp, q2.x - o1 * std::sin(q2.heading), q2.y + o1 * std::cos(q2.heading), 0.02, W, H);
                const Proj d2 = project(vp, q2.x - o0 * std::sin(q2.heading), q2.y + o0 * std::cos(q2.heading), 0.02, W, H);
                if (!a.ok || !bq.ok || !c2.ok || !d2.ok) continue;
                const Color cc = (k & 1) ? Color{1, 1, 1} : Color{0.05f, 0.05f, 0.05f};
                r.tri(a.x, a.y, bq.x, bq.y, c2.x, c2.y, cc); r.tri(a.x, a.y, c2.x, c2.y, d2.x, d2.y, cc);
            }
        }
    }
    r.flush2D();
    // Diger araclar (uzaktan yakina), sonra oyuncu
    struct Obj { double d, x, y, psi, heave; int id; };
    std::vector<Obj> objs;
    for (const TrafficCar& t : ses_->traffic()) {
        if (t.s < ps - 12 || t.s > ps + 320) continue;
        Obj o{}; ses_->trafficPose(t, o.x, o.y, o.psi); o.id = t.carId; o.heave = 0;
        o.d = (o.x - X) * cp + (o.y - Y) * sp; objs.push_back(o);
    }
    if (RoadCar* rv = ses_->rival()) {
        const VehicleSim& rs = rv->sim();
        Obj o{0, rs.posX(), rs.posY(), rs.heading(), rs.suspension().heave(), ses_->rivalCarId()};
        o.d = (o.x - X) * cp + (o.y - Y) * sp;
        if (o.d > -8 && o.d < 320) objs.push_back(o);
    }
    std::sort(objs.begin(), objs.end(), [](const Obj& a, const Obj& c) { return a.d > c.d; });
    for (const Obj& o : objs) {
        if (o.d < -3.0) continue;                                  // kameraya cok yakin (goruntuyu kaplar)
        r.drawCar(o.id, 0, 0, W, H, proj, view, matMul(matTranslate((float)o.x, (float)o.heave, (float)-o.y), matRotY((float)o.psi)));
    }
    r.drawCar(carId_, 0, 0, W, H, proj, view, matMul(matTranslate((float)X, (float)sim.suspension().heave(), (float)-Y), matRotY((float)sim.heading())));

    // HUD
    const PowertrainCore& pt = const_cast<VehicleSim&>(sim).powertrain();
    r.rect(0, 0, W, 58, {0.02f, 0.02f, 0.04f, 0.75f});
    std::snprintf(b, sizeof b, "%3.0f", sim.speed() * 3.6);
    r.text(8, 8, b, 5, {1, 1, 1});
    r.text(104, 30, "KM/H", 1, {0.7f, 0.7f, 0.75f});
    std::snprintf(b, sizeof b, "%d", pt.gear());
    r.text(150, 8, b, 5, Pc.manual ? Color{1.0f, 0.62f, 0.05f} : Color{0.6f, 0.9f, 1.0f});
    r.text(182, 30, Pc.manual ? "MANUEL" : "OTO", 1, {0.7f, 0.7f, 0.75f});
    const float red = (float)sim.engineSpec().redlineRpm, fill = std::clamp((float)pt.rpm() / (red * 1.05f), 0.0f, 1.0f);
    r.rect(230, 12, 352, 24, {0.15f, 0.15f, 0.18f});
    r.rect(230, 12, 230 + 122 * fill, 24, pt.rpm() > red * 0.9 ? Color{0.95f, 0.2f, 0.3f} : Color{0.2f, 0.85f, 0.3f});
    std::snprintf(b, sizeof b, "%5.0f RPM", pt.rpm());
    r.text(236, 30, b, 1, {1, 1, 1});
    r.text(236, 46, Pc.assist ? "YARDIM ACIK" : "YARDIM KAPALI", 1, Pc.assist ? Color{0.4f, 0.9f, 0.5f} : Color{1.0f, 0.5f, 0.3f});
    if (app_.tiltAvailable) r.text(236, 66, app_.tiltSteer ? "EGIM ACIK" : "EGIM KAPALI", 1, app_.tiltSteer ? Color{0.4f, 0.9f, 0.5f} : Color{0.7f, 0.7f, 0.7f});
    if (ses_->mode() == RoadSession::Mode::Race) {
        const double left = std::max(0.0, RoadSession::kStartS + ses_->raceLength() - ps);
        const double gap = ses_->gapMeters();
        std::snprintf(b, sizeof b, "%s  KALAN %.2f KM  %+.0f M", gap >= 0 ? "1." : "2.", left / 1000.0, gap);
        r.text(8, 46, b, 1, gap >= 0 ? Color{0.4f, 1.0f, 0.5f} : Color{1.0f, 0.6f, 0.3f});
        std::snprintf(b, sizeof b, "%.1f S", ses_->raceTime());
        r.text(8, 64, b, 2, {1, 1, 1});
    } else {
        std::snprintf(b, sizeof b, "%.2f KM  %.2f G  KAYMA %2.0f", ps / 1000.0, std::fabs(sim.lateralAccel()) / 9.81, std::fabs(sim.bodySlipAngle()) * 57.3);
        r.text(8, 46, b, 1, {0.75f, 0.8f, 0.9f});
    }
    if (Pc.offRoad()) r.textCentered(W / 2.0f, 86, "YOL DISI", 2, {1.0f, 0.4f, 0.2f});
    if (msgT_ > 0) r.textCentered(W / 2.0f, 108, msg_, 1, {1.0f, 0.85f, 0.3f});
    if (ses_->phase() == RoadSession::Phase::Countdown) {
        std::snprintf(b, sizeof b, "%d", (int)std::ceil(ses_->countdown()));
        r.textCentered(W / 2.0f, 200, b, 10, {1.0f, 0.2f, 0.15f});
    }
    if (ses_->mode() == RoadSession::Mode::Race && ses_->phase() == RoadSession::Phase::Finished) {
        r.rect(20, 180, 340, 400, {0.03f, 0.03f, 0.05f, 0.92f});
        r.textCentered(W / 2.0f, 200, ses_->playerWon() ? "KAZANDIN" : "KAYBETTIN", 3, ses_->playerWon() ? Color{0.3f, 1.0f, 0.4f} : Color{1.0f, 0.3f, 0.2f});
        std::snprintf(b, sizeof b, "SEN   %s", ses_->playerTime() > 0 ? (std::to_string((int)ses_->playerTime()) + "." + std::to_string((int)(ses_->playerTime() * 10) % 10) + " S").c_str() : "BITIREMEDI");
        r.text(50, 250, b, 2, {1, 1, 1});
        std::snprintf(b, sizeof b, "RAKIP %s", ses_->rivalTime() > 0 ? (std::to_string((int)ses_->rivalTime()) + "." + std::to_string((int)(ses_->rivalTime() * 10) % 10) + " S").c_str() : "BITIREMEDI");
        r.text(50, 276, b, 2, {1, 1, 1});
        std::snprintf(b, sizeof b, "CARPISMA %d", ses_->collisions());
        r.text(50, 306, b, 1, {0.8f, 0.8f, 0.85f});
        std::snprintf(b, sizeof b, "ODUL $%ld", prize_);
        r.text(50, 324, b, 2, kUiGold);
        r.textCentered(W / 2.0f, 370, "DOKUN / ENTER: GARAJ", 1, {0.7f, 0.75f, 0.9f});
    }
#ifndef __ANDROID__
    r.text(8, 510, "<> DIREKSIYON W GAZ S FREN E/Q VITES PGDN YARDIM", 1, {0.55f, 0.75f, 1.0f});
#endif
    button(r, kSteerL, "<", tL_ >= 0 ? kUiOrange : Color{0.2f, 0.22f, 0.28f, 0.8f}, 4);
    button(r, kSteerR, ">", tR_ >= 0 ? kUiOrange : Color{0.2f, 0.22f, 0.28f, 0.8f}, 4);
    button(r, kBrakeB, "FREN", brake_ > 0 ? Color{0.8f, 0.15f, 0.15f} : Color{0.3f, 0.12f, 0.12f, 0.85f}, 2);
    button(r, kGas, "GAZ", thr_ > 0 ? Color{0.2f, 0.7f, 0.25f} : Color{0.12f, 0.3f, 0.14f, 0.85f}, 2);
    r.flush2D();
}

void RoadScreen::pointerDown(int id, float x, float y) {
    if (menu_ || !ses_) {
        if (kFree.hit(x, y)) start(RoadSession::Mode::Free);
        else if (kRace.hit(x, y)) start(RoadSession::Mode::Race);
        else if (kTouge.hit(x, y)) start(RoadSession::Mode::Race, RoadSession::Kind::Touge);
        return;
    }
    if (ses_->phase() == RoadSession::Phase::Finished && finT_ > 1.0) { app_.goGarage(); return; }
    RoadCar& P = ses_->player();
    if (kSteerL.hit(x, y)) tL_ = id;
    else if (kSteerR.hit(x, y)) tR_ = id;
    else if (kBrakeB.hit(x, y)) tB_ = id;
    else if (kGas.hit(x, y)) tG_ = id;
    else if (y >= 60 && y < 84 && x > 230 && app_.tiltAvailable) { app_.tiltSteer = !app_.tiltSteer; flash(app_.tiltSteer ? "EGIM DIREKSIYONU ACIK" : "EGIM DIREKSIYONU KAPALI", 1.5); }
    else if (y < 60 && x > 230) { P.assist = !P.assist; flash(P.assist ? "SURUS YARDIMI ACIK" : "SURUS YARDIMI KAPALI", 1.5); }
    else if (y < 60) { P.manual = !P.manual; flash(P.manual ? "MANUEL VITES" : "OTOMATIK VITES", 1.5); }
    else if (y > 380 && y < 520 && P.manual) P.requestShift(x > 180 ? +1 : -1);   // manuel: alt yari sol/sag vites
}
void RoadScreen::pointerMove(int, float, float) {}
void RoadScreen::pointerUp(int id) {
    if (id == tL_) tL_ = -1;
    if (id == tR_) tR_ = -1;
    if (id == tB_) tB_ = -1;
    if (id == tG_) tG_ = -1;
}

void RoadScreen::key(Key k, bool down) {
    if (menu_ || !ses_) {
        if (!down) return;
        if (k == Key::Clutch) start(RoadSession::Mode::Free);
        else if (k == Key::Enter) start(RoadSession::Mode::Race);
        else if (k == Key::PageUp) start(RoadSession::Mode::Race, RoadSession::Kind::Touge);
        else if (k == Key::Back) app_.goGarage();
        return;
    }
    RoadCar& P = ses_->player();
    switch (k) {
    case Key::Left: kL_ = down; break;
    case Key::Right: kR_ = down; break;
    case Key::Throttle: kT_ = down; break;
    case Key::Brake: kB_ = down; break;
    case Key::ShiftUp: case Key::ShiftDown:
        if (down) { P.manual = true; P.requestShift(k == Key::ShiftUp ? +1 : -1); }
        break;
    case Key::Enter:
        if (!down) break;
        if (ses_->phase() == RoadSession::Phase::Finished) app_.goGarage();
        else { P.manual = !P.manual; flash(P.manual ? "MANUEL VITES" : "OTOMATIK VITES", 1.5); }
        break;
    case Key::PageDown: if (down) { P.assist = !P.assist; flash(P.assist ? "SURUS YARDIMI ACIK" : "SURUS YARDIMI KAPALI", 1.5); } break;
    case Key::Back: if (down) app_.goGarage(); break;
    default: break;
    }
}

} // namespace zk
