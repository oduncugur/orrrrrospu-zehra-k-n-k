// ZEHRA KINIK - Acik yol ekrani (yatay 640x360): prosedurel yol, arkadan kamera, duzlemsel fizik.
// Kontroller (Cockpit): analog gaz / fren (/ debriyaj), sanzimana gore vites kolu. Direksiyon: telefonda egim,
// masaustunde klavye (ekranda sag/sol tusu yok).
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
constexpr double kCurveK = 1.0 / 350.0;          // bu egrilikten dar yer "viraj" (otomatik debriyajda vites kilidi)

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
std::string secStr(double t) { char b[16]; std::snprintf(b, sizeof b, "%.1f S", t); return t > 0 ? b : "BITIREMEDI"; }
} // namespace

// Ekran yonu ayardan (yatay 640x360 / dikey 360x640): menu dugmeleri, HUD dugmeleri, kokpit yerlesimi
void RoadScreen::setupLayout() {
    land_ = !app_.settings.roadPortrait;
    W = land_ ? 640 : 360; H = land_ ? 360 : 640;
    if (land_) {
        free_ = {90, 100, 314, 146}; flow_ = {326, 100, 550, 146}; race_ = {90, 156, 314, 202}; touge_ = {326, 156, 550, 202};
        karma_ = {90, 212, 550, 258};
        assistBtn_ = {470, 2, 576, 34}; tiltBtn_ = {362, 2, 466, 34};
    } else {
        free_ = {40, 186, 320, 234}; flow_ = {40, 244, 320, 292}; race_ = {40, 302, 320, 350}; touge_ = {40, 360, 320, 408};
        karma_ = {40, 418, 320, 466};
        assistBtn_ = {252, 4, 356, 26}; tiltBtn_ = {252, 30, 356, 52};
    }
    cockpit_.setPortrait(!land_);
}

RoadScreen::RoadScreen(App& app, int carId, const Tune* tune) : app_(app), carId_(carId) {
    setupLayout();
    if (tune) tune_ = *tune;
    app_.setVoice(0, findVehicle(carId), tune_.turbo > 0);
    app_.setVoice(1, nullptr);
    autopilot_ = std::getenv("ZK_AUTOPILOT") != nullptr;
    if (const char* m = std::getenv("ZK_ROAD_MODE")) {
        const std::string n = m;
        start(n == "free" ? RoadSession::Mode::Free : n == "flow" ? RoadSession::Mode::Flow : n == "karma" ? RoadSession::Mode::Karma : RoadSession::Mode::Race,
              n == "touge" ? RoadSession::Kind::Touge : RoadSession::Kind::Highway);
    }
    else if (autopilot_) start(RoadSession::Mode::Free);
}

void RoadScreen::start(RoadSession::Mode m, RoadSession::Kind kind) {
    int rival = 0; Tune rt;
    if (m == RoadSession::Mode::Race || m == RoadSession::Mode::Karma) {
        const Opponent o = pickOpponent(carId_, tune_, (uint32_t)(app_.career.races * 7919 + 17));
        rival = o.carId; rt = o.tune;
        app_.setVoice(1, findVehicle(rival), rt.turbo > 0);
    }
    static uint32_t runs = 0;                                      // ayni oturumda her surus farkli yol/trafik
    const uint32_t seed = (uint32_t)(app_.career.races + 1 + (m == RoadSession::Mode::Flow ? runs++ : 0)) * 2654435761u;
    ses_ = std::make_unique<RoadSession>(m, carId_, &tune_, rival, &rt, seed, kind);
    camPsi_ = ses_->player().sim().heading();
    RoadCar& P = ses_->player();
    P.assist = app_.settings.assist;
    // Vites kolu sanziman tipinden: H-desen (oyuncu ya da otomatik debriyaj), otomatik P-N-D, sirali +/-
    const Gearbox box = P.sim().gearboxType();
    const int gears = P.sim().powertrain().gearCount();
    if (box == Gearbox::HPattern) {
        cockpit_.configure(Cockpit::Lever::HPattern, gears, !app_.settings.autoClutch);
        P.manual = true; P.slowClutch = app_.settings.autoClutch;
    } else if (box == Gearbox::TorqueConverter) {
        cockpit_.configure(Cockpit::Lever::Automatic, gears, false);
        P.manual = false;
    } else {
        cockpit_.configure(Cockpit::Lever::Sequential, gears, false);
        P.manual = true;
    }
    cockpit_.setPortrait(!land_);
    cockpit_.setKnobGear(1);
    menu_ = false; rewarded_ = false; record_ = false; prize_ = 0; finT_ = 0;
    flash(m == RoadSession::Mode::Free ? "SERBEST SURUS"
          : m == RoadSession::Mode::Flow ? "OTOBAN AKISI: YAKIN GEC, HIZLI GIT"
          : m == RoadSession::Mode::Karma ? "KARMA: DUZDE DRAG, VIRAJDA SURUS"
          : kind == RoadSession::Kind::Touge ? "DAG YOLU 3 KM" : "YOL YARISI 4 KM", 2.5);
}

bool RoadScreen::autoClutchPenalty() const {
    return cockpit_.lever() == Cockpit::Lever::HPattern && !cockpit_.clutchPedal();
}

void RoadScreen::finishRace() {
    rewarded_ = true;
    if (autopilot_) return;
    if (ses_->mode() == RoadSession::Mode::Flow) prize_ = app_.career.recordFlow(ses_->flow()->score(), &record_);
    else if (ses_->rival()) app_.career.recordRace(*findVehicle(ses_->rivalCarId()), ses_->playerWon(), 0.0, &prize_);
    else return;
    // Otomatik debriyaj (H-desen) odul cezasi: kazanilan paranin %25'i geri alinir
    if (autoClutchPenalty() && prize_ > 0) {
        const long cut = prize_ - (long)(prize_ * Settings::kAutoClutchPrize);
        app_.career.money -= cut; app_.career.earnings -= cut; prize_ -= cut;
    }
    const VehicleSim& ps = ses_->player().sim();
    app_.career.recordDamage(false, ps.failure().bearingDamage(), ps.failure().bearingSpun());
    app_.saveCareer();
}

void RoadScreen::update(double dt) {
    msgT_ -= dt;
    if (menu_ || !ses_) { app_.voice(0, 900, 0, false, false, 0.6f); app_.tire(0, 0); app_.tire(1, 0); return; }
    RoadCar& P = ses_->player();
    const double v = P.sim().speed();
    cockpit_.update(dt);
    // Direksiyon: hiza gore sinirli (kinematik yanal ivme ~1.1 g), rampali; klavye ya da telefon egimi
    const double maxSteer = std::clamp(P.sim().vehicleLoad().wheelbase * 1.1 * 9.81 / std::max(v * v, 1.0), 0.035, 0.50);
    double target = (kL_ ? maxSteer : 0.0) - (kR_ ? maxSteer : 0.0);
    if (!kL_ && !kR_ && app_.tiltAvailable && app_.settings.tiltSteer) {   // olu bolge %6
        const double t = std::clamp(app_.tilt() * app_.settings.tiltSens / 100.0, -1.0, 1.0), dz = 0.06;
        const double u = std::fabs(t) < dz ? 0.0 : (t - std::copysign(dz, t)) / (1.0 - dz);
        target = u * maxSteer;
    }
    const double rate = (std::fabs(target) > std::fabs(steer_) ? 1.0 : 2.5) * dt;
    steer_ += std::clamp(target - steer_, -rate, rate);
    // Karma: duz bolumde drag gorunumu (yandan kamera) ve arac seridi kendi tutar; virajli bolumde 3B surus
    const bool karma = ses_->mode() == RoadSession::Mode::Karma;
    const bool curvy = !karma || ses_->road().curvyAt(P.s());
    camBlend_ += std::clamp((curvy ? 1.0 : 0.0) - camBlend_, -dt / 0.9, dt / 0.9);
    if (karma && !curvy) steer_ = P.aiControls(-ses_->lane(), 0.6).steer;

    RoadControls c;
    c.steer = steer_; c.throttle = cockpit_.throttle(); c.brake = cockpit_.brake();
    PowertrainCore& pt = P.sim().powertrain();
    switch (cockpit_.lever()) {
    case Cockpit::Lever::HPattern: {
        // Otomatik debriyajda virajda vites degismez (denge): kol mevcut vitese geri oturur
        const bool inCurve = karma ? curvy : std::fabs(ses_->road().at(P.s()).curvature) > kCurveK;
        if (autoClutchPenalty() && inCurve && cockpit_.knobGear() != pt.gear()) {
            cockpit_.setKnobGear(pt.gear());
            flash("VIRAJDA VITES YOK (OTOMATIK DEBRIYAJ)", 1.2);
        }
        c.gear = cockpit_.knobGear();
        if (cockpit_.clutchPedal()) c.clutch = cockpit_.clutch();
        break;
    }
    case Cockpit::Lever::Sequential: c.shift = cockpit_.takeShift(); break;
    case Cockpit::Lever::Automatic:
        c.neutral = cockpit_.autoPos() != Cockpit::AutoPos::D;
        if (cockpit_.autoPos() == Cockpit::AutoPos::P && v < 0.5) c.brake = std::max(c.brake, 0.6);   // park kilidi
        break;
    }
    if (autopilot_) {
        double cap = 1e9;
        for (const TrafficCar& t : ses_->traffic())
            if (!t.oncoming && t.s > P.s() && t.s - P.s() < 40.0) cap = std::min(cap, t.v);
        c = P.aiControls(-ses_->lane(), 0.55, cap);
        P.manual = false; P.slowClutch = false;
    }
    ses_->update(dt, c);
    if (P.grinding()) {
        // Debriyajsiz vites girmedi: kol gercek vitese geri seker (kol ile gercek vites hic ayrismasin; eskiden kol
        // 5'te kalip arac alt viteste gidiyor, debriyaja basinca 5 aniden giriyordu)
        cockpit_.setKnobGear(pt.gear());
        flash("DEBRIYAJ!", 0.6);
        app_.haptic(60, 200);
    }
    for (auto& m : ses_->drainMessages()) flash(m);
    if (ses_->takeCrash()) app_.haptic(220, 255);
    if (ses_->mode() != RoadSession::Mode::Free && ses_->phase() == RoadSession::Phase::Finished) {
        finT_ += dt;
        if (!rewarded_) finishRace();
    }
    camPsi_ += std::remainder(P.sim().heading() - camPsi_, 6.283185307179586) * std::min(1.0, dt * 4.0);

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

void RoadScreen::drawMenu(Renderer& r) {
    r.gradientV(0, 0, W, H, {0.10f, 0.12f, 0.2f}, {0.05f, 0.05f, 0.07f});
    const float ty = land_ ? 30 : 110, by = karma_.y1;                   // baslik / dugmelerin alti
    r.textCentered(W / 2.0f, ty, "ACIK YOL", 4, {1.0f, 0.62f, 0.05f});
    r.textCentered(W / 2.0f, ty + 40, "ARA TASLAK", 1, {0.6f, 0.6f, 0.65f});
    button(r, free_, "SERBEST SURUS", Color{0.15f, 0.45f, 0.7f}, 2);
    button(r, flow_, "OTOBAN AKISI 2 DK", Color{0.1f, 0.5f, 0.35f}, 2);
    button(r, race_, "YOL YARISI 4 KM", kUiOrange, 2);
    button(r, touge_, "DAG YOLU 3 KM", Color{0.55f, 0.2f, 0.6f}, 2);
    button(r, karma_, "KARMA: DRAG + VIRAJ", Color{0.7f, 0.15f, 0.15f}, 2);
    r.textCentered(W / 2.0f, by + 18, "AKIS: YAKIN GECIS + HIZ + VIRAJ = SKOR", 1, {0.7f, 0.7f, 0.75f});
    r.textCentered(W / 2.0f, by + 30, "YARISLAR: RAKIP + TRAFIK, ODULLU", 1, {0.7f, 0.7f, 0.75f});
    if (app_.career.bestFlow > 0)
        r.textCentered(W / 2.0f, by + 46, "AKIS REKORU " + money(app_.career.bestFlow).substr(1), 2, kUiGold);
#ifndef __ANDROID__
    r.textCentered(W / 2.0f, by + 74, land_ ? "BOSLUK SERBEST  PGDN AKIS  ENTER YARIS  PGUP DAG  1 KARMA"
                                            : "BOSLUK SERBEST PGDN AKIS ENTER YARIS PGUP DAG 1 KARMA", 1, {0.55f, 0.75f, 1.0f});
#endif
}

// 3B sahne: gokyuzu, arazi, yol, agaclar, bitis cizgisi, araclar (uzaktan yakina)
void RoadScreen::drawWorld(Renderer& r) {
    const RoadPath& R = ses_->road();
    RoadCar& Pc = ses_->player();
    const VehicleSim& sim = Pc.sim();
    const double ps = Pc.s();
    const double X = sim.posX(), Y = sim.posY();
    const double cp = std::cos(camPsi_), sp = std::sin(camPsi_);
    // Kamera: arabanin 7.5 m arkasi, 2.4 m yukari; 6 m ilerisine bakar (yol yuksekligini izler)
    const double zCar = Pc.elevation(), zBack = R.at(ps - 7.5).z, zFront = R.at(ps + 6.0).z;
    // Takip kamerasi. Dikeyde alt ceyrek kontrollerle dolu: kamera yukseltilip asagi bakar, arac ortaya cikar
    const double camUp = land_ ? 2.4 : 3.0, lookAhead = land_ ? 6.0 : 4.0, lookUp = land_ ? 0.9 : 0.1;
    double ex = X - 7.5 * cp, ey = Y - 7.5 * sp, ez = std::max(zBack, zCar) + camUp;
    double tx = X + lookAhead * cp, ty = Y + lookAhead * sp, tz = zFront + lookUp;
    double camFov = fov();
    const bool sideCam = camBlend_ < 0.999;
    if (sideCam) {
        // Drag gorunumu: yolun sagindan 26 m, alcak, dar acili kamera (yandan profil; egim gorunur). Takip
        // kamerasina yumusak gecis (smoothstep) -> karma yarista virajli bolume girerken kamera arkaya ucar.
        const RoadPoint q = R.at(ps);
        const double fx = std::cos(q.heading), fy = std::sin(q.heading), rxn = std::sin(q.heading), ryn = -std::cos(q.heading);
        const double sex = X + 26.0 * rxn + 1.5 * fx, sey = Y + 26.0 * ryn + 1.5 * fy, sez = zCar + 5.5;
        const double stx = X + 1.5 * fx - 1.8 * rxn, sty = Y + 1.5 * fy - 1.8 * ryn, stz = zCar + 3.2;
        const double b = camBlend_ * camBlend_ * (3.0 - 2.0 * camBlend_);
        ex = sex + (ex - sex) * b; ey = sey + (ey - sey) * b; ez = sez + (ez - sez) * b;
        tx = stx + (tx - stx) * b; ty = sty + (ty - sty) * b; tz = stz + (tz - stz) * b;
        camFov = (land_ ? 0.55 : 0.75) + (fov() - (land_ ? 0.55 : 0.75)) * b;
    }
    const Mat4 proj = matPerspective((float)camFov, (float)W / H, 0.3f, 900.0f);
    const Mat4 view = matLookAt((float)ex, (float)ez, (float)-ey, (float)tx, (float)tz, (float)-ty);
    const Mat4 vp = matMul(proj, view);
    const float pxPerM = H * 0.5f / std::tan((float)camFov * 0.5f);      // derinlik 1 m'de metre basina piksel
    double dx = tx - ex, dy = ty - ey;
    { const double l = std::max(1e-6, std::hypot(dx, dy)); dx /= l; dy /= l; }
    const Proj hz = project(vp, ex + 3000 * dx, ey + 3000 * dy, zCar, W, H);
    const float horizon = hz.ok ? std::clamp(hz.y, 0.0f, (float)H) : H * 0.4f;
    const bool mtn = ses_->kind() == RoadSession::Kind::Touge;
    const Color grass = mtn ? Color{0.2f, 0.36f, 0.2f} : Color{0.36f, 0.55f, 0.28f};
    r.rect(0, horizon, W, H, grass);
    r.gradientV(0, 0, W, horizon, mtn ? Color{0.18f, 0.2f, 0.42f} : Color{0.30f, 0.45f, 0.85f}, mtn ? Color{0.95f, 0.55f, 0.35f} : Color{0.85f, 0.70f, 0.55f});
    if (mtn)   // uzak daglar (siluet)
        for (int k = 0; k < 10; ++k) { const float x0 = k * 70.0f - 30.0f, hh = 30.0f + 25.0f * hashf(k + 3); r.tri(x0, horizon, x0 + 100, horizon, x0 + 50, horizon - hh, {0.25f, 0.24f, 0.36f}); }
    r.rect(0, horizon, W, horizon + 2, {0.35f, 0.42f, 0.38f});

    const double hw = R.halfWidth();
    const int i0 = std::max(0, (int)((ps - (sideCam ? 70.0 : 8.0)) / RoadPath::kStep)), n = (int)R.points().size();
    const int i1 = std::min(n - 2, i0 + (sideCam ? 200 : 160));
    const auto& P = R.points();
    auto edge = [&](int i, double off) {
        const RoadPoint& p = P[i];
        return project(vp, p.x - off * std::sin(p.heading), p.y + off * std::cos(p.heading), p.z, W, H);
    };
    for (int i = i1; i >= i0; --i) {
        const int j = i + 1;
        const Proj aL = edge(i, hw + 1.0), aR = edge(i, -hw - 1.0), bL = edge(j, hw + 1.0), bR = edge(j, -hw - 1.0);
        if (!aL.ok || !aR.ok || !bL.ok || !bR.ok) continue;
        const bool band = ((i / 3) & 1) != 0;
        {   // Arazi: yol yuksekligini izleyen 60 m'lik cim seritleri (tepede yol havada kalmasin)
            const Proj gL0 = edge(i, hw + 60.0), gL1 = edge(j, hw + 60.0), gR0 = edge(i, -hw - 60.0), gR1 = edge(j, -hw - 60.0);
            if (gL0.ok && gL1.ok) { r.tri(gL0.x, gL0.y, aL.x, aL.y, bL.x, bL.y, grass); r.tri(gL0.x, gL0.y, bL.x, bL.y, gL1.x, gL1.y, grass); }
            if (gR0.ok && gR1.ok) { r.tri(aR.x, aR.y, gR0.x, gR0.y, gR1.x, gR1.y, grass); r.tri(aR.x, aR.y, gR1.x, gR1.y, bR.x, bR.y, grass); }
        }
        const Color curb = mtn ? (band ? Color{0.62f, 0.64f, 0.66f} : Color{0.8f, 0.8f, 0.78f})      // dag: celik bariyer
                               : (band ? Color{0.85f, 0.15f, 0.12f} : Color{0.92f, 0.92f, 0.9f});
        r.tri(aL.x, aL.y, aR.x, aR.y, bR.x, bR.y, curb); r.tri(aL.x, aL.y, bR.x, bR.y, bL.x, bL.y, curb);
        const Proj cL = edge(i, hw), cR = edge(i, -hw), dL = edge(j, hw), dR = edge(j, -hw);
        const Color asp = band ? Color{0.30f, 0.30f, 0.32f} : Color{0.27f, 0.27f, 0.29f};
        r.tri(cL.x, cL.y, cR.x, cR.y, dR.x, dR.y, asp); r.tri(cL.x, cL.y, dR.x, dR.y, dL.x, dL.y, asp);
        if ((i % 5) < 2) {                                         // orta kesik cizgi (4 m cizgi, 6 m bosluk)
            const Proj m0 = edge(i, 0.08), m1 = edge(i, -0.08), m2 = edge(j, 0.08), m3 = edge(j, -0.08);
            r.tri(m0.x, m0.y, m1.x, m1.y, m3.x, m3.y, {0.95f, 0.9f, 0.6f}); r.tri(m0.x, m0.y, m3.x, m3.y, m2.x, m2.y, {0.95f, 0.9f, 0.6f});
        }
        if (i % 8 == 0) {                                          // kenar agaclari / direkleri (her 16 m)
            for (int side = -1; side <= 1; side += 2) {
                if (camBlend_ < 0.5 && side < 0) continue;            // drag gorunumu: kamera tarafindaki agaclar gorusu kapatir
                const float h = hashf(i * 2 + (side > 0));
                if (h < (mtn ? 0.08f : 0.35f)) continue;
                const Proj b = edge(i, side * (hw + 5.0 + 20.0 * hashf(i * 7 + side)));
                if (!b.ok) continue;
                const float sc = pxPerM / b.w;
                const float th = (5.0f + 4.0f * h) * sc, tw = (1.6f + h) * sc;
                if (h > 0.8f) r.rect(b.x - 0.12f * sc, b.y - 7.5f * sc, b.x + 0.12f * sc, b.y, {0.35f, 0.28f, 0.2f});   // direk
                else {
                    r.rect(b.x - tw * 0.12f, b.y - th * 0.35f, b.x + tw * 0.12f, b.y, {0.35f, 0.24f, 0.14f});
                    r.tri(b.x - tw, b.y - th * 0.3f, b.x + tw, b.y - th * 0.3f, b.x, b.y - th, {0.12f, 0.38f + 0.1f * h, 0.16f});
                }
            }
        }
    }
    if (ses_->hasRival()) {                                        // bitis cizgisi
        const double fs = RoadSession::kStartS + ses_->raceLength();
        if (fs > ps - 5 && fs < ps + 300) {
            const RoadPoint q = R.at(fs), q2 = R.at(fs + 1.5);
            for (int k = 0; k < 10; ++k) {
                const double o0 = -hw + k * (2 * hw / 10), o1 = o0 + 2 * hw / 10;
                const Proj a = project(vp, q.x - o0 * std::sin(q.heading), q.y + o0 * std::cos(q.heading), q.z + 0.02, W, H);
                const Proj bq = project(vp, q.x - o1 * std::sin(q.heading), q.y + o1 * std::cos(q.heading), q.z + 0.02, W, H);
                const Proj c2 = project(vp, q2.x - o1 * std::sin(q2.heading), q2.y + o1 * std::cos(q2.heading), q2.z + 0.02, W, H);
                const Proj d2 = project(vp, q2.x - o0 * std::sin(q2.heading), q2.y + o0 * std::cos(q2.heading), q2.z + 0.02, W, H);
                if (!a.ok || !bq.ok || !c2.ok || !d2.ok) continue;
                const Color cc = (k & 1) ? Color{1, 1, 1} : Color{0.05f, 0.05f, 0.05f};
                r.tri(a.x, a.y, bq.x, bq.y, c2.x, c2.y, cc); r.tri(a.x, a.y, c2.x, c2.y, d2.x, d2.y, cc);
            }
        }
    }
    if (ses_->mode() == RoadSession::Mode::Karma) {
        // Viraj yaklasim levhalari: 300/200/100 m (beyaz, 3/2/1 kirmizi serit) ve viraj girisinde yon levhasi (sari ok).
        // Levhalar sagda; yon levhasi virajin dis tarafinda.
        auto post = [&](double sb, double off, double h0, double h1, Proj& top, Proj& bot) {
            const RoadPoint p = R.at(sb);
            const double bx = p.x - off * std::sin(p.heading), by = p.y + off * std::cos(p.heading);
            const Proj base = project(vp, bx, by, p.z, W, H);
            top = project(vp, bx, by, p.z + h1, W, H);
            bot = project(vp, bx, by, p.z + h0, W, H);
            if (!base.ok || !top.ok || !bot.ok) return 0.0f;
            const float sc = pxPerM / base.w;
            r.rect(base.x - 0.07f * sc, top.y, base.x + 0.07f * sc, base.y, {0.55f, 0.55f, 0.58f});
            return sc;
        };
        for (const RoadSection& q : R.sections()) {
            for (int k = 3; k >= 1; --k) {
                const double sb = q.entry - 100.0 * k;
                if (sb < ps - 30 || sb > ps + 320) continue;
                Proj t, m;
                const float sc = post(sb, -(hw + 3.0), 0.9, 2.7, t, m);
                if (sc <= 0) continue;
                const float hw2 = 0.6f * sc, hgt = m.y - t.y;
                r.rect(t.x - hw2, t.y, t.x + hw2, m.y, {0.95f, 0.95f, 0.95f});
                for (int i = 0; i < k; ++i) {
                    const float yy = t.y + hgt * (0.12f + 0.3f * i);
                    r.rect(t.x - hw2 * 0.85f, yy, t.x + hw2 * 0.85f, yy + hgt * 0.16f, {0.85f, 0.1f, 0.1f});
                }
            }
            const double se = q.entry - 15.0;
            if (se > ps - 30 && se < ps + 320) {
                const double kk = R.at(q.entry + 40.0).curvature;         // sola donus (+) -> levha sagda, ok sola
                Proj t, m;
                const float sc = post(se, kk > 0 ? -(hw + 3.0) : (hw + 3.0), 1.0, 2.4, t, m);
                if (sc > 0) {
                    const float hw2 = 1.0f * sc;
                    r.rect(t.x - hw2, t.y, t.x + hw2, m.y, {0.98f, 0.8f, 0.1f});
                    const float ts = std::clamp((m.y - t.y) / 9.0f, 1.0f, 8.0f);
                    r.textCentered(t.x, (t.y + m.y) * 0.5f - 3.5f * ts, kk > 0 ? "<<<" : ">>>", ts, {0.08f, 0.08f, 0.08f});
                }
            }
        }
    }
    r.flush2D();
    // Diger araclar (uzaktan yakina), sonra oyuncu. z: yol yuksekligi + suspansiyon; pitch: gidis yonundeki egim
    struct Obj { double d, x, y, psi, z, pitch; int id; };
    auto carModel = [](double x, double y, double z, double psi, double pitch) {
        return matMul(matMul(matTranslate((float)x, (float)z, (float)-y), matRotY((float)psi)), matRotZ((float)std::atan(pitch)));
    };
    std::vector<Obj> objs;
    for (const TrafficCar& t : ses_->traffic()) {
        if (t.s < ps - 12 || t.s > ps + 320) continue;
        const RoadPoint q = R.at(t.s);
        Obj o{}; ses_->trafficPose(t, o.x, o.y, o.psi); o.id = t.carId;
        o.z = q.z; o.pitch = t.oncoming ? -q.grade : q.grade;
        o.d = (o.x - ex) * dx + (o.y - ey) * dy; objs.push_back(o);   // kamera bakis yonunde derinlik
    }
    if (RoadCar* rv = ses_->rival()) {
        const VehicleSim& rs = rv->sim();
        Obj o{0, rs.posX(), rs.posY(), rs.heading(), rv->elevation() + rs.suspension().heave(), rs.grade(), ses_->rivalCarId()};
        o.d = (o.x - ex) * dx + (o.y - ey) * dy;
        if (o.d > 2 && o.d < 340) objs.push_back(o);
    }
    std::sort(objs.begin(), objs.end(), [](const Obj& a, const Obj& c) { return a.d > c.d; });
    for (const Obj& o : objs) {
        if (o.d < 3.0) continue;                                   // kameraya cok yakin / arkasinda
        r.drawCar(o.id, 0, 0, W, H, proj, view, carModel(o.x, o.y, o.z, o.psi, o.pitch));
    }
    r.drawCar(carId_, 0, 0, W, H, proj, view, carModel(X, Y, zCar + sim.suspension().heave(), sim.heading(), sim.grade()));
}

void RoadScreen::drawHud(Renderer& r) {
    RoadCar& Pc = ses_->player();
    const VehicleSim& sim = Pc.sim();
    const PowertrainCore& pt = const_cast<VehicleSim&>(sim).powertrain();
    char b[96];
    // Ust serit (sol/sag kaydiricilarin arasi)
    const float x0 = land_ ? 126.0f : 8.0f, infoY = land_ ? 40.0f : 62.0f;
    if (land_) r.rect(120, 0, 578, 36, {0.02f, 0.02f, 0.04f, 0.72f});
    else { r.rect(0, 0, W, 58, {0.02f, 0.02f, 0.04f, 0.72f}); r.rect(0, 58, W, 74, {0.02f, 0.02f, 0.04f, 0.5f}); }
    std::snprintf(b, sizeof b, "%3.0f", sim.speed() * app_.settings.speedFactor());
    r.text(x0, 4, b, 4, {1, 1, 1});
    r.text(x0 + 74, 22, app_.settings.speedUnit(), 1, {0.7f, 0.7f, 0.75f});
    const int gear = pt.gear();
    const bool autoBox = cockpit_.lever() == Cockpit::Lever::Automatic;
    std::string gs = gear == 0 ? "N" : std::to_string(gear);
    if (autoBox && cockpit_.autoPos() != Cockpit::AutoPos::D) gs = cockpit_.autoPos() == Cockpit::AutoPos::P ? "P" : "N";
    r.text(x0 + 106, 4, gs, 4, Pc.grinding() ? Color{1.0f, 0.2f, 0.15f} : Color{1.0f, 0.62f, 0.05f});
    const float red = (float)sim.engineSpec().redlineRpm, fill = std::clamp((float)pt.rpm() / (red * 1.05f), 0.0f, 1.0f);
    const float rx = x0 + 136, rw = land_ ? 94.0f : 100.0f;
    r.rect(rx, 6, rx + rw, 16, {0.15f, 0.15f, 0.18f});
    r.rect(rx, 6, rx + rw * fill, 16, pt.rpm() > red * 0.9 ? Color{0.95f, 0.2f, 0.3f} : Color{0.2f, 0.85f, 0.3f});
    std::snprintf(b, sizeof b, "%5.0f RPM", pt.rpm());
    r.text(rx, 22, b, 1, {1, 1, 1});
    const bool tiltOn = app_.settings.tiltSteer;
    if (app_.tiltAvailable) button(r, tiltBtn_, tiltOn ? "EGIM ACIK" : "EGIM KAPALI", tiltOn ? Color{0.12f, 0.35f, 0.18f, 0.85f} : Color{0.25f, 0.25f, 0.28f, 0.85f}, 1);
    if (!sim.hasTc()) button(r, assistBtn_, sim.hasAbs() ? "ABS  TC YOK" : "ABS/TC YOK", Color{0.25f, 0.25f, 0.28f, 0.85f}, 1);
    else button(r, assistBtn_, Pc.assist ? (sim.tcActive() ? "TC !" : "TC ACIK") : "TC KAPALI",
                Pc.assist ? (sim.tcActive() ? Color{0.75f, 0.55f, 0.05f, 0.9f} : Color{0.12f, 0.35f, 0.18f, 0.85f}) : Color{0.45f, 0.18f, 0.1f, 0.85f}, 1);
    // Ikinci satir: moda gore bilgi
    if (const FlowScorer* fl = ses_->flow()) {
        const int tl = (int)std::ceil(ses_->flowTimeLeft());
        std::snprintf(b, sizeof b, "SURE %d:%02d  YAKIN %d  APEX %d", tl / 60, tl % 60, fl->stats().nearMisses + fl->stats().oncomingMisses, fl->stats().apexes);
        r.text(x0, infoY, b, 1, tl <= 10 ? Color{1.0f, 0.4f, 0.3f} : Color{0.75f, 0.8f, 0.9f});
        const std::string sc = money(fl->score()).substr(1);
        r.rect(x0 - 6, infoY + 12, x0 + 6 + r.textWidth(sc, 3), infoY + 38, {0.02f, 0.02f, 0.04f, 0.6f});
        r.text(x0, infoY + 16, sc, 3, {1, 1, 1});
        if (fl->combo() > 1) {
            std::snprintf(b, sizeof b, "X%d", fl->combo());
            r.textCentered(W / 2.0f, infoY + 20, b, 4, kUiGold);
            const float f = (float)(fl->comboLeft() / FlowScorer::kComboTime);
            r.rect(W / 2.0f - 40, infoY + 52, W / 2.0f - 40 + 80 * f, infoY + 56, kUiGold);
        }
    } else if (ses_->hasRival()) {
        if (ses_->mode() == RoadSession::Mode::Karma) {                // bolum gostergesi: DRAG / VIRAJ
            const double s = Pc.s();
            const RoadSection* nextQ = nullptr;
            for (const RoadSection& q : ses_->road().sections()) if (q.curvy && q.entry > s - 1.0) { nextQ = &q; break; }
            if (nextQ && s >= nextQ->s0 && s < nextQ->entry) {
                // Viraj yaklasimi: kalan mesafe + onerilen giris hizi (en dar R'de ~0.8 g). Hiz, kalan mesafede
                // ~7 m/s^2 ile frenlenemeyecek kadar yuksekse kirmizi (simdi fren!)
                const double left = nextQ->entry - s, v = sim.speed();
                const double vRec = std::sqrt(0.8 * 9.81 * nextQ->minR);
                const bool late = v > vRec && (v * v - vRec * vRec) / (2.0 * 7.0) > left - 15.0;
                std::snprintf(b, sizeof b, "VIRAJ %3.0f M  ONERILEN %3.0f %s", left, vRec * app_.settings.speedFactor(), app_.settings.speedUnit());
                const float w = r.textWidth(b, 2) + 16;
                r.rect(W / 2.0f - w / 2, infoY + 30, W / 2.0f + w / 2, infoY + 52, late ? Color{0.6f, 0.05f, 0.05f, 0.85f} : Color{0.35f, 0.28f, 0.02f, 0.8f});
                r.textCentered(W / 2.0f, infoY + 34, b, 2, late ? Color{1.0f, 1.0f, 1.0f} : Color{1.0f, 0.85f, 0.3f});
                if (late) r.textCentered(W / 2.0f, infoY + 56, "FRENE BAS!", 2, {1.0f, 0.3f, 0.2f});
                std::snprintf(b, sizeof b, "VIRAJ YAKLASIYOR");
            }
            else if (ses_->road().curvyAt(s)) std::snprintf(b, sizeof b, "VIRAJ BOLUMU");
            else if (nextQ) std::snprintf(b, sizeof b, "DRAG  VIRAJA %.0f M", nextQ->entry - s);
            else std::snprintf(b, sizeof b, "DRAG  BITIS DUZLUGU");
            r.text(x0, infoY + 12, b, 1, ses_->road().curvyAt(s) ? Color{1.0f, 0.75f, 0.2f} : Color{0.5f, 0.85f, 1.0f});
        }
        const double left = std::max(0.0, RoadSession::kStartS + ses_->raceLength() - Pc.s());
        const double gap = ses_->gapMeters();
        std::snprintf(b, sizeof b, "%s  KALAN %.2f KM  %+.0f M   %.1f S", gap >= 0 ? "1." : "2.", left / 1000.0, gap, ses_->raceTime());
        r.text(x0, infoY, b, 1, gap >= 0 ? Color{0.4f, 1.0f, 0.5f} : Color{1.0f, 0.6f, 0.3f});
    } else {
        std::snprintf(b, sizeof b, "%.2f KM  %.2f G  KAYMA %2.0f  EGIM %+.0f%%", Pc.s() / 1000.0, std::fabs(sim.lateralAccel()) / 9.81,
                      std::fabs(sim.bodySlipAngle()) * 57.3, std::fabs(sim.grade()) < 0.005 ? 0.0 : sim.grade() * 100.0);
        r.text(x0, infoY, b, 1, {0.75f, 0.8f, 0.9f});
    }
    if (app_.settings.showFps) {
        std::snprintf(b, sizeof b, "%2.0fFPS %4.1fMS", app_.fps(), app_.updateMs());
        r.text((land_ ? 576 : W - 4) - r.textWidth(b, 1), land_ ? 40 : 78, b, 1, {0.45f, 0.5f, 0.45f});
    }
    if (Pc.offRoad()) r.textCentered(W / 2.0f, land_ ? 104 : 110, "YOL DISI", 2, {1.0f, 0.4f, 0.2f});
    if (msgT_ > 0) {
        const float w = r.textWidth(msg_, 2) + 16;
        const float my = land_ ? 120 : 140;
        r.rect(W / 2.0f - w / 2, my, W / 2.0f + w / 2, my + 22, {0.02f, 0.02f, 0.04f, 0.7f});
        r.textCentered(W / 2.0f, my + 4, msg_, 2, {1.0f, 0.85f, 0.3f});
    }
    if (ses_->phase() == RoadSession::Phase::Countdown) {
        std::snprintf(b, sizeof b, "%d", (int)std::ceil(ses_->countdown()));
        r.textCentered(W / 2.0f, land_ ? 150 : 220, b, 8, {1.0f, 0.2f, 0.15f});
    }
#ifndef __ANDROID__
    r.text(land_ ? 142 : 8, land_ ? 346 : 80, "A/D DIREKS. W GAZ S FREN BOSLUK DEBR. 1-6/N E/Q", 1, {0.55f, 0.75f, 1.0f});
#endif
}

void RoadScreen::drawResults(Renderer& r) {
    char b[96];
    const float oy = land_ ? 0.0f : 120.0f, lx = W / 2.0f - 120;
    r.rect(W / 2.0f - 170, 50 + oy, W / 2.0f + 170, 310 + oy, {0.03f, 0.03f, 0.05f, 0.93f});
    if (ses_->mode() == RoadSession::Mode::Flow) {
        const FlowScorer::Stats& st = ses_->flow()->stats();
        r.textCentered(W / 2.0f, 62 + oy, "SURE BITTI", 3, kUiGold);
        r.textCentered(W / 2.0f, 90 + oy, money(st.score).substr(1), 4, {1, 1, 1});
        if (record_) r.textCentered(W / 2.0f, 124 + oy, "YENI REKOR!", 2, {0.3f, 1.0f, 0.4f});
        std::snprintf(b, sizeof b, "YAKIN GECIS %d  (KARSI %d)   APEX %d   KOMBO X%d", st.nearMisses + st.oncomingMisses, st.oncomingMisses, st.apexes, st.bestCombo);
        r.textCentered(W / 2.0f, 150 + oy, b, 1, {0.85f, 0.85f, 0.9f});
        std::snprintf(b, sizeof b, "EN YUKSEK HIZ %.0f %s   KARSI SERITTE %.0f S   CARPISMA %d", st.topSpeed * app_.settings.speedFactor(),
                      app_.settings.speedUnit(), st.oncomingTime, st.crashes);
        r.textCentered(W / 2.0f, 166 + oy, b, 1, {0.85f, 0.85f, 0.9f});
    } else {
        r.textCentered(W / 2.0f, 66 + oy, ses_->playerWon() ? "KAZANDIN" : "KAYBETTIN", 3, ses_->playerWon() ? Color{0.3f, 1.0f, 0.4f} : Color{1.0f, 0.3f, 0.2f});
        r.text(lx, 110 + oy, ("SEN   " + secStr(ses_->playerTime())).c_str(), 2, {1, 1, 1});
        r.text(lx, 136 + oy, ("RAKIP " + secStr(ses_->rivalTime())).c_str(), 2, {1, 1, 1});
        std::snprintf(b, sizeof b, "CARPISMA %d", ses_->collisions());
        r.text(lx, 166 + oy, b, 1, {0.8f, 0.8f, 0.85f});
    }
    std::snprintf(b, sizeof b, "ODUL $%ld", prize_);
    r.textCentered(W / 2.0f, 200 + oy, b, 3, kUiGold);
    if (autoClutchPenalty()) r.textCentered(W / 2.0f, 232 + oy, "OTOMATIK DEBRIYAJ: ODUL %75", 1, {0.9f, 0.6f, 0.3f});
    r.textCentered(W / 2.0f, 280 + oy, "DOKUN / ENTER: GARAJ", 1, {0.7f, 0.75f, 0.9f});
}

void RoadScreen::render(Renderer& r) {
    r.begin(W, H, {0.36f, 0.55f, 0.28f});
    if (menu_ || !ses_) { drawMenu(r); r.flush2D(); return; }
    drawWorld(r);
    drawHud(r);
    cockpit_.render(r, ses_->player().sim().powertrain().gear(), ses_->player().grinding());
    if (ses_->mode() != RoadSession::Mode::Free && ses_->phase() == RoadSession::Phase::Finished) drawResults(r);
    r.flush2D();
}

void RoadScreen::pointerDown(int id, float x, float y) {
    if (menu_ || !ses_) {
        if (free_.hit(x, y)) start(RoadSession::Mode::Free);
        else if (flow_.hit(x, y)) start(RoadSession::Mode::Flow);
        else if (race_.hit(x, y)) start(RoadSession::Mode::Race);
        else if (touge_.hit(x, y)) start(RoadSession::Mode::Race, RoadSession::Kind::Touge);
        else if (karma_.hit(x, y)) start(RoadSession::Mode::Karma);
        return;
    }
    if (ses_->phase() == RoadSession::Phase::Finished && finT_ > 1.0) { app_.goGarage(); return; }
    if (cockpit_.pointerDown(id, x, y)) return;
    if (assistBtn_.hit(x, y)) toggleAssist();
    else if (tiltBtn_.hit(x, y) && app_.tiltAvailable) toggleTilt();
}
// Yolda yapilan secimler ayarlara da yazilir (sonraki surus ayni modla baslar)
void RoadScreen::toggleAssist() {
    RoadCar& P = ses_->player();
    if (!P.sim().hasTc()) { flash("BU ARACTA TC YOK - ECU + ELEKTRONIK", 2.0); return; }
    P.assist = !P.assist;
    app_.settings.assist = P.assist; app_.saveSettings();
    flash(P.assist ? "CEKIS KONTROLU ACIK" : "CEKIS KONTROLU KAPALI", 1.5);
}
void RoadScreen::toggleTilt() {
    app_.settings.tiltSteer = !app_.settings.tiltSteer; app_.saveSettings();
    flash(app_.settings.tiltSteer ? "EGIM DIREKSIYONU ACIK" : "EGIM DIREKSIYONU KAPALI", 1.5);
}

void RoadScreen::pointerMove(int id, float x, float y) { if (ses_ && !menu_) cockpit_.pointerMove(id, x, y); }
void RoadScreen::pointerUp(int id) { if (ses_ && !menu_) cockpit_.pointerUp(id); }

void RoadScreen::key(Key k, bool down) {
    if (menu_ || !ses_) {
        if (!down) return;
        if (k == Key::Clutch) start(RoadSession::Mode::Free);
        else if (k == Key::Enter) start(RoadSession::Mode::Race);
        else if (k == Key::PageUp) start(RoadSession::Mode::Race, RoadSession::Kind::Touge);
        else if (k == Key::PageDown) start(RoadSession::Mode::Flow);
        else if (k == Key::Gear1) start(RoadSession::Mode::Karma);
        else if (k == Key::Back) app_.goGarage();
        return;
    }
    switch (k) {
    case Key::Left: kL_ = down; break;
    case Key::Right: kR_ = down; break;
    case Key::Enter:
        if (down && ses_->phase() == RoadSession::Phase::Finished) app_.goGarage();
        break;
    case Key::PageDown: if (down) toggleAssist(); break;
    case Key::Back: if (down) app_.goGarage(); break;
    default: cockpit_.key(k, down); break;
    }
}

} // namespace zk
