// ZEHRA KINIK - Acik yol ekrani (yatay 640x360): prosedurel yol, arkadan kamera, duzlemsel fizik.
// Kontroller (Cockpit): analog gaz / fren (/ debriyaj), sanzimana gore vites kolu. Direksiyon: telefonda egim,
// masaustunde klavye (ekranda sag/sol tusu yok).
#include "Screens.h"
#include "app/Hints.h"
#include "Ui.h"
#include "app/Looks.h"
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
        karma_ = {90, 212, 314, 258}; chase_ = {326, 212, 550, 258};
        assistBtn_ = {470, 2, 576, 34}; tiltBtn_ = {362, 2, 466, 34};
    } else {
        free_ = {40, 186, 320, 234}; flow_ = {40, 244, 320, 292}; race_ = {40, 302, 320, 350}; touge_ = {40, 360, 320, 408};
        karma_ = {40, 418, 320, 466}; chase_ = {40, 476, 320, 524};
        assistBtn_ = {252, 4, 356, 26}; tiltBtn_ = {252, 30, 356, 52};
    }
    cockpit_.setPortrait(!land_);
}

RoadScreen::RoadScreen(App& app, int carId, const Tune* tune) : app_(app), carId_(carId) {
    app_.hint(HintRoad, hintTexts()[HintRoad]);
    setupLayout();
    if (tune) tune_ = *tune;
    app_.setVoiceTuned(0, carId, &tune_);
    app_.setVoice(1, nullptr);
    autopilot_ = std::getenv("ZK_AUTOPILOT") != nullptr;
    if (const char* m = std::getenv("ZK_ROAD_MODE")) {
        const std::string n = m;
        start(n == "free" ? RoadSession::Mode::Free : n == "flow" ? RoadSession::Mode::Flow : n == "karma" ? RoadSession::Mode::Karma
              : n == "chase" ? RoadSession::Mode::Chase : RoadSession::Mode::Race,
              n == "touge" ? RoadSession::Kind::Touge : RoadSession::Kind::Highway);
    }
    else if (autopilot_) start(RoadSession::Mode::Free);
    if (app_.activeEvent >= 0) {                                         // lig etkinligi: mod dogrudan
        switch (leagueEvents()[app_.activeEvent].mode) {
        case EventMode::Road: start(RoadSession::Mode::Race); break;
        case EventMode::Touge: start(RoadSession::Mode::Race, RoadSession::Kind::Touge); break;
        case EventMode::Karma: start(RoadSession::Mode::Karma); break;
        case EventMode::Flow: start(RoadSession::Mode::Flow); break;
        case EventMode::Chase:
            start(RoadSession::Mode::Chase, leagueEvents()[app_.activeEvent].league == 3 ? RoadSession::Kind::Touge : RoadSession::Kind::Highway);
            break;
        default: break;
        }
    }
}

void RoadScreen::start(RoadSession::Mode m, RoadSession::Kind kind) {
    int rival = 0; Tune rt;
    if (m == RoadSession::Mode::Race || m == RoadSession::Mode::Karma || m == RoadSession::Mode::Chase) {
        const Opponent o = app_.activeEvent >= 0 ? app_.lastOpp : app_.career.pickOpponentFor((uint32_t)(app_.career.races * 7919 + 17));
        app_.lastOpp = o;
        rival = o.carId; rt = o.tune;
        app_.setVoiceTuned(1, rival, &rt);
    }
    static uint32_t runs = 0;                                      // ayni oturumda her surus farkli yol/trafik
    const uint32_t seed = (uint32_t)(app_.career.races + 1 + (m == RoadSession::Mode::Flow ? runs++ : 0)) * 2654435761u;
    ses_ = std::make_unique<RoadSession>(m, carId_, &tune_, rival, &rt, seed, kind);
    if (app_.career.car().carId == carId_) ses_->player().sim().setNosFill(app_.career.car().nosFill);   // tupte kalan
    {   // Ortam: gece %30, yagmur %25 (tohumdan); test icin ZK_NIGHT / ZK_RAIN
        const uint32_t w = seed * 2246822519u + 0x9E3779B9u;
        night_ = (w >> 7) % 100 < 30 || std::getenv("ZK_NIGHT");
        rain_ = (w >> 17) % 100 < 25 || std::getenv("ZK_RAIN");
        if (std::getenv("ZK_DAY")) night_ = rain_ = false;
        ses_->setRain(rain_);
        app_.rainSound(rain_);
        lastGear_ = -2;
    }
    if (RoadCar* rv = ses_->rival(); rv && app_.activeEvent >= 0 && m != RoadSession::Mode::Chase) {   // isimli rakip: patron daha keskin
        rv->slowClutch = app_.eventHandicap > 1.25;
        ses_->setRivalPace(0.55 + (1.6 - std::clamp(app_.eventHandicap, 1.0, 1.6)) * 0.15);
    }
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
    if (m == RoadSession::Mode::Chase) app_.hint(HintChase, hintTexts()[HintChase]);
    if (night_ || rain_) msgNote_ = std::string(night_ ? "GECE" : "") + (night_ && rain_ ? " + " : "") + (rain_ ? "YAGMUR: TUTUS DUSUK" : "");
    else msgNote_.clear();
    flash(m == RoadSession::Mode::Free ? "SERBEST SURUS"
          : m == RoadSession::Mode::Flow ? "OTOBAN AKISI: YAKIN GEC, HIZLI GIT"
          : m == RoadSession::Mode::Karma ? "KARMA: DUZDE DRAG, VIRAJDA SURUS"
          : m == RoadSession::Mode::Chase ? "POLIS! 400 M ACIL VE TUT"
          : kind == RoadSession::Kind::Touge ? "DAG YOLU 3 KM" : "YOL YARISI 4 KM", 2.5);
}

bool RoadScreen::autoClutchPenalty() const {
    return cockpit_.lever() == Cockpit::Lever::HPattern && !cockpit_.clutchPedal();
}

void RoadScreen::finishRace() {
    rewarded_ = true;
    if (autopilot_) return;
    if (app_.activeEvent >= 0) {                                         // lig etkinligi
        int pink = 0;
        const long score = ses_->flow() ? ses_->flow()->score() : 0;
        prize_ = app_.career.recordEvent(app_.activeEvent, ses_->rival() ? ses_->playerWon() : false, 0.0, score, &pink);
        if (pink > 0) app_.eventNote = std::string("PINK SLIP: ") + upper(findVehicle(pink)->model) + " SENIN!";
        else if (pink < 0) app_.eventNote = "PINK SLIP: ARABANI KAYBETTIN";
    }
    else if (ses_->mode() == RoadSession::Mode::Flow) prize_ = app_.career.recordFlow(ses_->flow()->score(), &record_);
    else if (ses_->mode() == RoadSession::Mode::Chase) prize_ = app_.career.recordChase(ses_->playerWon(), ses_->collisions());
    else if (ses_->rival()) app_.career.recordRace(*findVehicle(ses_->rivalCarId()), ses_->playerWon(), 0.0, &prize_,
                                                         ses_->prizeScale() * (app_.lastOpp.estEt > 0 && app_.lastOpp.playerEt > 0 ? prizeDifficulty(app_.lastOpp.playerEt - app_.lastOpp.estEt) : 1.0));
    else return;
    // Otomatik debriyaj (H-desen) odul cezasi: kazanilan paranin %25'i geri alinir
    if (autoClutchPenalty() && prize_ > 0) {
        const long cut = prize_ - (long)(prize_ * Settings::kAutoClutchPrize);
        app_.career.money -= cut; app_.career.earnings -= cut; prize_ -= cut;
    }
    const VehicleSim& ps = ses_->player().sim();
    app_.career.recordDamage(false, ps.failure().bearingDamage(), ps.failure().bearingSpun(), ps.gearboxBroken(), ps.engineStress(), ps.tireWearGained());
    app_.career.recordNosUse(ps.nitrousLeft());
    app_.saveCareer();
}

void RoadScreen::update(double dt) {
    msgT_ -= dt;
    if (menu_ || !ses_) { app_.voice(0, 900, 0, false, false, 0.6f); app_.tire(0, 0); app_.tire(1, 0); app_.wind(0); return; }
    RoadCar& P = ses_->player();
    const double v = P.sim().speed();
    cockpit_.update(dt);
    cockpit_.setPadPedals(app_.padThrottle(), app_.padBrake());
    if (cockpit_.takeSeated()) app_.haptic(18, 160);                     // vites yuvaya oturdu: kisa "tik"
    // Direksiyon: hiza gore sinirli (kinematik yanal ivme ~1.1 g), rampali; klavye ya da telefon egimi
    // Hiza gore sinir + kayma payi: arka kayarken (govde kayma acisi) karsi direksiyon icin tam aci acilir
    const double slipAllow = std::min(0.45, std::fabs(P.sim().bodySlipAngle()) * 1.3);
    const double maxSteer = std::clamp(P.sim().vehicleLoad().wheelbase * 1.1 * 9.81 / std::max(v * v, 1.0) + slipAllow, 0.035, 0.50);
    double target = (kL_ ? maxSteer : 0.0) - (kR_ ? maxSteer : 0.0);
    if (!kL_ && !kR_ && app_.tiltAvailable && app_.settings.tiltSteer) {   // olu bolge %6
        const double t = std::clamp((app_.settings.tiltInvert ? -1.0 : 1.0) * app_.tilt() * app_.settings.tiltSens / 100.0, -1.0, 1.0), dz = 0.06;
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
    case Cockpit::Lever::Automatic: {
        const Cockpit::AutoPos ap = cockpit_.autoPos();
        c.neutral = ap == Cockpit::AutoPos::P || ap == Cockpit::AutoPos::N;
        c.reverse = ap == Cockpit::AutoPos::R;
        c.autoMode = ap == Cockpit::AutoPos::S ? 1 : ap == Cockpit::AutoPos::M ? 2 : 0;
        c.shift = cockpit_.takeShift();
        if (ap == Cockpit::AutoPos::P && std::fabs(v) < 0.5) c.brake = std::max(c.brake, 0.6);   // park kilidi
        break;
    }
    }
    if (autopilot_) {
        double cap = 1e9;
        for (const TrafficCar& t : ses_->traffic())
            if (!t.oncoming && t.s > P.s() && t.s - P.s() < 40.0) cap = std::min(cap, t.v);
        c = P.aiControls(-ses_->lane(), 0.55, cap);
        P.manual = false; P.slowClutch = false;
    }
    ses_->update(dt, c);
    // Teker donusu ve lastik dumani
    spinP_ += P.sim().wheel(0).omega() * dt;
    envT_ += dt;
    if (RoadCar* rv = ses_->rival()) spinR_ += rv->sim().wheel(0).omega() * dt;
    spawnSmoke(P, dt);
    if (RoadCar* rv = ses_->rival()) spawnSmoke(*rv, dt);
    for (Puff& p : smoke_) { p.x += p.vx * dt; p.y += p.vy * dt; p.z += p.vz * dt; p.vz *= 0.98; p.life -= dt; p.size += dt * 1.6; }
    smoke_.erase(std::remove_if(smoke_.begin(), smoke_.end(), [](const Puff& p) { return p.life <= 0; }), smoke_.end());
    if (P.grinding()) {
        // Debriyajsiz vites girmedi: kol gercek vitese geri seker (kol ile gercek vites hic ayrismasin; eskiden kol
        // 5'te kalip arac alt viteste gidiyor, debriyaja basinca 5 aniden giriyordu)
        cockpit_.setKnobGear(pt.gear());
        flash("DEBRIYAJ!", 0.6);
        app_.haptic(60, 200);
    }
    for (auto& m : ses_->drainMessages()) flash(m);
    for (auto& m : P.sim().drainFailEvents()) { flash(m, 3.0); app_.haptic(400, 255); }
    if (ses_->takeCrash()) {                                             // carpisma: titresim, kamera sarsintisi, kivilcim
        app_.haptic(220, 255);
        shakeT_ = 0.45;
        const VehicleSim& s0 = P.sim();
        const double hc = std::cos(s0.heading()), hs = std::sin(s0.heading());
        for (int k = 0; k < 34 && sparks_.size() < 160; ++k) {
            const double a = hashf(k * 7 + (int)(envT_ * 100)) * 6.2831853, sp = 3.0 + 7.0 * hashf(k * 13 + 5);
            Puff q{s0.posX() + 2.0 * hc, s0.posY() + 2.0 * hs, Pc0z() + 0.4, s0.speed() * hc * 0.6 + sp * std::cos(a),
                   s0.speed() * hs * 0.6 + sp * std::sin(a), 1.5 + 4.0 * hashf(k * 3 + 1), 0.35 + 0.4 * hashf(k * 11), 0};
            sparks_.push_back(q);
        }
    }
    shakeT_ = std::max(0.0, shakeT_ - dt);
    for (Puff& q : sparks_) { q.x += q.vx * dt; q.y += q.vy * dt; q.z += q.vz * dt; q.vz -= 9.8 * dt; q.life -= dt;
                              if (q.z < Pc0z()) { q.z = Pc0z(); q.vz = -q.vz * 0.35; q.vx *= 0.7; q.vy *= 0.7; } }
    sparks_.erase(std::remove_if(sparks_.begin(), sparks_.end(), [](const Puff& q) { return q.life <= 0; }), sparks_.end());
    {   // Gaz kesme patlamasi: yuksek devirde ani gaz birakma -> 0.45 s egzozdan alev / patlama
        const double thr = pt.throttleEffective();
        if (prevThr_ > 0.6 && thr < 0.2 && pt.rpm() > 0.62 * P.sim().engineSpec().redlineRpm) popT_ = 0.45;
        prevThr_ = thr;
        popT_ = std::max(0.0, popT_ - dt);
    }
    if (ses_->mode() != RoadSession::Mode::Free && ses_->phase() == RoadSession::Phase::Finished) {
        finT_ += dt;
        if (!rewarded_) finishRace();
    }
    camPsi_ += std::remainder(P.sim().heading() - camPsi_, 6.283185307179586) * std::min(1.0, dt * 4.0);

    app_.voice(0, pt.rpm(), pt.throttleEffective(), pt.limiterHit(), pt.gear() > 0, 1.0f);
    if (pt.gear() != lastGear_) { if (lastGear_ > -2) app_.sfxShift(); lastGear_ = pt.gear(); }
    app_.nitrousSound(P.sim().nitrousActive());
    app_.tire(0, P.tireSlipSpeed());
    app_.wind(v);
    if (RoadCar* rv = ses_->rival()) {
        // Rakip sesi mesafeye gore kisilir
        const double d = std::hypot(rv->sim().posX() - P.sim().posX(), rv->sim().posY() - P.sim().posY());
        PowertrainCore& rp = rv->sim().powertrain();
        app_.voice(1, rp.rpm(), rp.throttleEffective(), rp.limiterHit(), rp.gear() > 0, (float)std::clamp(8.0 / (d + 8.0), 0.0, 0.8));
        app_.siren(ses_->mode() == RoadSession::Mode::Chase && ses_->phase() != RoadSession::Phase::Finished ? (float)std::clamp(25.0 / (d + 25.0), 0.05, 1.0) : 0.0f);
        app_.tire(1, 0.0);
    } else app_.tire(1, 0.0);
    if (std::getenv("ZK_ROAD_LOG")) { static double t = 0, nx = 0; t += dt; if (t >= nx) { nx += 0.5;
        std::printf("t=%.1f v=%.1f g%d s=%.1f lat=%.2f gap=%.1f phase=%d\n", t, v, pt.gear(), P.s(), P.lateral(), ses_->gapMeters(), (int)ses_->phase()); } }
}

double RoadScreen::Pc0z() const { return ses_ ? ses_->player().elevation() : 0.0; }

// Yol bolgesi (s'ye gore, 700 m'lik bloklar): 0 kir, 1 sehir, 2 tunel. Tohum: yolun kendisi (ayni yarista sabit)
int RoadScreen::zoneAt(double s) const {
    const bool mtn = ses_->kind() == RoadSession::Kind::Touge;
    const int block = (int)(s / 350.0);
    if (block < 1) return 0;                                           // baslangic acik alanda
    const float h = hashf(block * 13 + (int)ses_->raceLength());
    if (mtn) return h < 0.18f ? 2 : 0;
    if (ses_->mode() == RoadSession::Mode::Karma && ses_->road().curvyAt(s)) return 0;
    return h < 0.32f ? 1 : h < 0.40f ? 2 : 0;
}

// Lastik dumani: tahrikli / kayan tekerlerin kayma hizi 5 m/s ustunde; yogunluk kaymayla artar
void RoadScreen::spawnSmoke(const RoadCar& car, double dt) {
    const VehicleSim& sm = car.sim();
    const double slip = car.tireSlipSpeed();
    if (slip < 5.0 || car.offRoad()) return;
    const double rate = std::min(40.0, (slip - 5.0) * 6.0);                       // puf / s
    static double acc = 0;
    acc += rate * dt;
    const double c = std::cos(sm.heading()), sn = std::sin(sm.heading());
    const double back = -0.32 * sm.vehicleLoad().wheelbase - 0.3, half = 0.75;
    while (acc >= 1.0 && smoke_.size() < 220) {
        acc -= 1.0;
        const double side = (smoke_.size() & 1) ? half : -half;
        const double jx = ((smoke_.size() * 37) % 11) / 11.0 - 0.5;
        Puff p;
        p.x = sm.posX() + back * c - side * sn; p.y = sm.posY() + back * sn + side * c; p.z = car.elevation() + 0.25;
        p.vx = sm.speed() * 0.15 * c + jx; p.vy = sm.speed() * 0.15 * sn + jx; p.vz = 0.6 + 0.4 * (jx + 0.5);
        p.life = 1.6; p.size = 0.6;
        smoke_.push_back(p);
    }
}

void RoadScreen::drawMenu(Renderer& r) {
    r.gradientV(0, 0, W, H, {0.10f, 0.12f, 0.2f}, {0.05f, 0.05f, 0.07f});
    const float ty = land_ ? 30 : 110, by = chase_.y1;                   // baslik / dugmelerin alti
    r.textCentered(W / 2.0f, ty, "ACIK YOL", 4, {1.0f, 0.62f, 0.05f});
    r.textCentered(W / 2.0f, ty + 40, "ARA TASLAK", 1, {0.6f, 0.6f, 0.65f});
    button(r, free_, "SERBEST SURUS", Color{0.15f, 0.45f, 0.7f}, 2);
    button(r, flow_, "OTOBAN AKISI 2 DK", Color{0.1f, 0.5f, 0.35f}, 2);
    button(r, race_, "YOL YARISI 4 KM", kUiOrange, 2);
    button(r, touge_, "DAG YOLU 3 KM", Color{0.55f, 0.2f, 0.6f}, 2);
    button(r, karma_, land_ ? "KARMA" : "KARMA: DRAG + VIRAJ", Color{0.7f, 0.15f, 0.15f}, 2);
    button(r, chase_, "POLIS KACIS", Color{0.12f, 0.2f, 0.55f}, 2);
    r.textCentered(W / 2.0f, by + 18, "AKIS: YAKIN GECIS + HIZ + VIRAJ = SKOR", 1, {0.7f, 0.7f, 0.75f});
    r.textCentered(W / 2.0f, by + 30, "YARISLAR: RAKIP + TRAFIK, ODULLU", 1, {0.7f, 0.7f, 0.75f});
    if (app_.career.bestFlow > 0)
        r.textCentered(W / 2.0f, by + 46, "AKIS REKORU " + money(app_.career.bestFlow).substr(1), 2, kUiGold);
#ifndef __ANDROID__
    r.textCentered(W / 2.0f, by + 74, land_ ? "BOSLUK SERBEST  PGDN AKIS  ENTER YARIS  PGUP DAG  1 KARMA  2 POLIS"
                                            : "BOSLUK SERBEST PGDN AKIS ENTER YARIS PGUP DAG 1 KARMA 2 POLIS", 1, {0.55f, 0.75f, 1.0f});
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
    {   // Kamera sarsintisi: carpismada sert (sonumlu), yuksek hizda hafif titresim
        const double v = sim.speed();
        const double amp = shakeT_ * shakeT_ * 1.4 + std::clamp((v - 38.0) / 60.0, 0.0, 1.0) * 0.022;
        if (amp > 1e-4) {
            const double t = envT_;
            ez += amp * (std::sin(t * 61.0) * 0.6 + std::sin(t * 37.0 + 1.3) * 0.4);
            const double lat = amp * (std::sin(t * 47.0 + 0.7) * 0.6 + std::sin(t * 29.0) * 0.4);
            ex += lat * -sp; ey += lat * cp;
        }
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
    const Color grass0 = mtn ? Color{0.2f, 0.36f, 0.2f} : Color{0.36f, 0.55f, 0.28f};
    // Ortam isigi: gece karanlik (farlar / lambalar aydinlatir), yagmurda kapali gok
    const float amb = night_ ? 0.26f : rain_ ? 0.72f : 1.0f;
    const Color grass{grass0.r * amb, grass0.g * amb, grass0.b * amb};
    const Color skyLow = night_ ? Color{0.10f, 0.11f, 0.20f} : rain_ ? Color{0.55f, 0.57f, 0.60f}
                       : mtn ? Color{0.95f, 0.55f, 0.35f} : Color{0.85f, 0.70f, 0.55f};
    r.setSceneLight(night_ ? 0.12f : rain_ ? 0.6f : 1.0f, night_ ? 0.40f : rain_ ? 0.8f : 1.0f);
    // Hava perspektifi: uzaklastikca renk ufuk rengine karisir (derinlik hissi, uzaktaki cizgiler sakinlesir)
    const Color fogCol{skyLow.r * 0.85f + 0.10f, skyLow.g * 0.85f + 0.10f, skyLow.b * 0.85f + 0.12f};
    auto fog = [&](Color c, float w) {
        const float f = std::clamp((w - (rain_ ? 20.0f : 40.0f)) / (rain_ ? 220.0f : 360.0f), 0.0f, 0.85f);
        return Color{c.r + (fogCol.r - c.r) * f, c.g + (fogCol.g - c.g) * f, c.b + (fogCol.b - c.b) * f, c.a};
    };
    r.rect(0, horizon, W, H, grass);
    r.gradientV(0, 0, W, horizon, night_ ? Color{0.01f, 0.01f, 0.04f} : rain_ ? Color{0.36f, 0.38f, 0.42f}
                                  : mtn ? Color{0.18f, 0.2f, 0.42f} : Color{0.30f, 0.45f, 0.85f}, skyLow);
    if (night_) for (int k = 0; k < 70; ++k) r.rect(hashf(k * 3) * W, hashf(k * 7 + 1) * horizon * 0.9f, hashf(k * 3) * W + 1.2f,
                                                    hashf(k * 7 + 1) * horizon * 0.9f + 1.2f, {1, 1, 1, 0.3f + 0.6f * hashf(k)});
    {   // Gunes ve bulutlar: dunya yonune sabit (donuste gokyuzu doner)
        const float yaw = (float)std::atan2(dy, dx), hfov = 2.0f * std::atan(std::tan((float)camFov * 0.5f) * W / H);
        auto azX = [&](float az) { return W * 0.5f - std::remainder(az - yaw, 6.2831853f) / hfov * W; };
        const float sx = azX(mtn ? 2.2f : 0.9f), sy = horizon - (mtn ? 0.12f : 0.32f) * horizon;
        if (!night_ && !rain_ && sx > -60 && sx < W + 60) {
            for (int k = 3; k >= 0; --k) r.circle(sx, sy, 10.0f + k * 9.0f, 20, {1.0f, 0.92f, 0.7f, k ? 0.10f : 0.95f});
        }
        for (int k = 0; k < 9; ++k) {
            const float cx = azX(k * 0.7f + 0.3f * hashf(k * 11)), cy = horizon * (0.18f + 0.5f * hashf(k * 5 + 1));
            if (cx < -80 || cx > W + 80) continue;
            const float w0 = 26.0f + 30.0f * hashf(k * 3);
            for (int j = 0; j < 4; ++j)
                r.circle(cx + (j - 1.5f) * w0 * 0.45f, cy + (j % 2) * 3.0f, w0 * (0.32f + 0.12f * (j % 3)) * (rain_ ? 1.8f : 1.0f), 14,
                         night_ ? Color{0.2f, 0.2f, 0.25f, 0.25f} : rain_ ? Color{0.3f, 0.32f, 0.35f, 0.55f} : Color{1.0f, 1.0f, 1.0f, 0.28f});
        }
    }
    if (mtn)   // uzak daglar (siluet)
        for (int k = 0; k < 10; ++k) { const float x0 = k * 70.0f - 30.0f, hh = 30.0f + 25.0f * hashf(k + 3); r.tri(x0, horizon, x0 + 100, horizon, x0 + 50, horizon - hh, {0.25f, 0.24f, 0.36f}); }
    if (sideCam) {
        // Drag gorunumu paralaks: uzak tepe katmanlari yol ilerleyisiyle yavas kayar (hiz hissi); takip kamerasinda soner
        const float a = 1.0f - (float)camBlend_;
        for (int layer = 0; layer < 2; ++layer) {
            const float speed = layer == 0 ? 0.35f : 0.9f, period = layer == 0 ? 120.0f : 80.0f;
            const float off = std::fmod((float)ps * speed, period);
            const Color c = layer == 0 ? Color{0.45f, 0.52f, 0.68f, a} : Color{0.32f, 0.45f, 0.36f, a};
            for (int k = -1; k <= (int)(W / period) + 1; ++k) {
                const int id = k + (int)std::floor((float)ps * speed / period);
                const float x0 = k * period - off, hh = (layer == 0 ? 26.0f : 14.0f) + (layer == 0 ? 22.0f : 12.0f) * hashf(id * 3 + layer * 101);
                r.tri(x0 - period * 0.2f, horizon, x0 + period * 1.2f, horizon, x0 + period * 0.5f, horizon - hh, c);
            }
        }
    }
    r.rect(0, horizon, W, horizon + 2, {0.35f, 0.42f, 0.38f});

    const double hw = R.halfWidth();
    const int i0 = std::max(0, (int)((ps - (sideCam ? 70.0 : 8.0)) / RoadPath::kStep)), n = (int)R.points().size();
    const int i1 = std::min(n - 2, i0 + (sideCam ? 200 : 160));
    const auto& P = R.points();
    auto edge = [&](int i, double off) {
        const RoadPoint& p = P[i];
        return project(vp, p.x - off * std::sin(p.heading), p.y + off * std::cos(p.heading), p.z, W, H);
    };
    auto pt = [&](int i, double off, double h) {                       // yol noktasi + yanal ofset + yukseklik
        const RoadPoint& p = P[std::clamp(i, 0, n - 1)];
        return project(vp, p.x - off * std::sin(p.heading), p.y + off * std::cos(p.heading), p.z + h, W, H);
    };
    // Kenara gore ofset: kenar (|off| >= hw - 0.3) noktanin kendi genisligine kayar (yol daralir / genisler)
    auto edgeW = [&](int i, double off, double fromEdge) {
        const double w = P[i].hw - fromEdge;
        return edge(i, off >= 0 ? w : -w);
    };
    for (int i = i1; i >= i0; --i) {
        const int j = i + 1;
        const Proj aL = edgeW(i, 1, -1.0), aR = edgeW(i, -1, -1.0), bL = edgeW(j, 1, -1.0), bR = edgeW(j, -1, -1.0);
        if (!aL.ok || !aR.ok || !bL.ok || !bR.ok) continue;
        const bool band = ((i / 3) & 1) != 0;
        const int zone = zoneAt(P[i].s);                                    // 0 kir, 1 sehir, 2 tunel
        // Isik: ortam + oyuncunun farlari (onunde 75 m) + sokak lambasi havuzlari (sehir / gece)
        float lit = amb;
        if (night_) {
            const double d = P[i].s - ps;
            if (d > -3.0 && d < 75.0) lit += 0.85f * (float)(1.0 - std::max(0.0, d) / 75.0);
            if (zone == 1 || zone == 2) { const double ls = std::fmod(P[i].s, 40.0); lit += 0.55f * (float)std::max(0.0, 1.0 - std::min(ls, 40.0 - ls) / 14.0); }
            lit = std::min(lit, 1.0f);
        }
        auto L = [&](Color c) { return Color{c.r * lit, c.g * lit, c.b * lit, c.a}; };
        if (zone != 2) {   // Arazi: yol yuksekligini izleyen 60 m'lik cim seritleri (tepede yol havada kalmasin); sehirde kaldirim
            const Proj gL0 = edgeW(i, 1, -60.0), gL1 = edgeW(j, 1, -60.0), gR0 = edgeW(i, -1, -60.0), gR1 = edgeW(j, -1, -60.0);
            const Color g0 = zone == 1 ? Color{0.42f * amb, 0.42f * amb, 0.44f * amb} : band ? grass : Color{grass.r * 0.94f, grass.g * 0.96f, grass.b * 0.94f};
            const Color gc = fog(night_ && zone == 1 ? L({0.42f, 0.42f, 0.44f}) : g0, aL.w);
            if (gL0.ok && gL1.ok) { r.tri(gL0.x, gL0.y, aL.x, aL.y, bL.x, bL.y, gc); r.tri(gL0.x, gL0.y, bL.x, bL.y, gL1.x, gL1.y, gc); }
            if (gR0.ok && gR1.ok) { r.tri(aR.x, aR.y, gR0.x, gR0.y, gR1.x, gR1.y, gc); r.tri(aR.x, aR.y, gR1.x, gR1.y, bR.x, bR.y, gc); }
        }
        const Color curb = fog(mtn ? (band ? Color{0.62f, 0.64f, 0.66f} : Color{0.8f, 0.8f, 0.78f})      // dag: celik bariyer
                                   : (band ? Color{0.85f, 0.15f, 0.12f} : Color{0.92f, 0.92f, 0.9f}), aL.w);
        { const Color cb = fog(L(curb), aL.w); r.tri(aL.x, aL.y, aR.x, aR.y, bR.x, bR.y, cb); r.tri(aL.x, aL.y, bR.x, bR.y, bL.x, bL.y, cb); }
        const Proj cL = edgeW(i, 1, 0.0), cR = edgeW(i, -1, 0.0), dL = edgeW(j, 1, 0.0), dR = edgeW(j, -1, 0.0);
        // Asfalt: hafif yama/renk degisimi (hash) + tekerlek izi koyulugu
        const float patch = 0.015f * (hashf(i / 4) - 0.5f);
        const float wet = rain_ ? 0.72f : 1.0f;                              // islak asfalt koyu
        const Color asp = fog(L(band ? Color{(0.30f + patch) * wet, (0.30f + patch) * wet, (0.32f + patch) * wet}
                                     : Color{(0.27f + patch) * wet, (0.27f + patch) * wet, (0.29f + patch) * wet}), cL.w);
        r.tri(cL.x, cL.y, cR.x, cR.y, dR.x, dR.y, asp); r.tri(cL.x, cL.y, dR.x, dR.y, dL.x, dL.y, asp);
        if (cL.w < 120.0f)                                         // tekerlek izleri (serit merkezinin iki yani)
            for (double lo : {-ses_->lane() - 0.75, -ses_->lane() + 0.75, ses_->lane() - 0.75, ses_->lane() + 0.75}) {
                const Proj t0 = edge(i, lo + 0.25), t1 = edge(i, lo - 0.25), t2 = edge(j, lo - 0.25), t3 = edge(j, lo + 0.25);
                const Color tc{0.0f, 0.0f, 0.0f, 0.07f};
                r.tri(t0.x, t0.y, t1.x, t1.y, t2.x, t2.y, tc); r.tri(t0.x, t0.y, t2.x, t2.y, t3.x, t3.y, tc);
            }
        {   // kenar cizgileri (beyaz, surekli)
            const Color ec = fog(L({0.92f, 0.92f, 0.9f}), cL.w);
            for (double sg : {-1.0, 1.0}) {
                const Proj e0 = edgeW(i, sg, 0.12), e1 = edgeW(i, sg, 0.27), e2 = edgeW(j, sg, 0.27), e3 = edgeW(j, sg, 0.12);
                r.tri(e0.x, e0.y, e1.x, e1.y, e2.x, e2.y, ec); r.tri(e0.x, e0.y, e2.x, e2.y, e3.x, e3.y, ec);
            }
        }
        if (P[i].hw > 5.0 && (i % 5) < 2) {                        // genis yol (2x2): ek serit kesik cizgileri
            const Color wc = fog(L({0.92f, 0.92f, 0.9f}), cL.w);
            for (double sg : {-1.0, 1.0}) {
                const double o = sg * P[i].hw * 0.5, o2 = sg * P[j].hw * 0.5;
                const Proj m0 = edge(i, o + 0.07), m1 = edge(i, o - 0.07), m2 = edge(j, o2 + 0.07), m3 = edge(j, o2 - 0.07);
                r.tri(m0.x, m0.y, m1.x, m1.y, m3.x, m3.y, wc); r.tri(m0.x, m0.y, m3.x, m3.y, m2.x, m2.y, wc);
            }
        }
        if ((i % 5) < 2) {                                         // orta kesik cizgi (4 m cizgi, 6 m bosluk)
            const Proj m0 = edge(i, 0.08), m1 = edge(i, -0.08), m2 = edge(j, 0.08), m3 = edge(j, -0.08);
            const Color mc = fog(L({0.95f, 0.9f, 0.6f}), m0.w);
            r.tri(m0.x, m0.y, m1.x, m1.y, m3.x, m3.y, mc); r.tri(m0.x, m0.y, m3.x, m3.y, m2.x, m2.y, mc);
        }
        if (zone == 2) {                                           // tunel: duvar + tavan + tavan lambalari
            const float bt = band ? 1.0f : 0.88f;                           // bant golgesi: derinlik hissi
            const Color ceil = fog(L({0.25f * bt, 0.24f * bt, 0.23f * bt}), cL.w);
            for (double sg : {-1.0, 1.0}) {
                const float sd = (sg > 0 ? 0.9f : 1.0f) * bt;
                const Color wall = fog(L({0.46f * sd, 0.44f * sd, 0.40f * sd}), cL.w);
                const Proj w0 = pt(i, sg * (P[i].hw + 1.2), 0), w1 = pt(j, sg * (P[j].hw + 1.2), 0), w2 = pt(j, sg * (P[j].hw + 1.2), 6.0), w3 = pt(i, sg * (P[i].hw + 1.2), 6.0);
                if (w0.ok && w1.ok && w2.ok && w3.ok) { r.tri(w0.x, w0.y, w1.x, w1.y, w2.x, w2.y, wall); r.tri(w0.x, w0.y, w2.x, w2.y, w3.x, w3.y, wall); }
                const Proj k2 = pt(j, sg * (P[j].hw + 1.2), 0.9), k3 = pt(i, sg * (P[i].hw + 1.2), 0.9);   // alt kusak (sari-siyah)
                const Color kc = fog(L(((i / 2) & 1) ? Color{0.85f, 0.7f, 0.15f} : Color{0.12f, 0.12f, 0.12f}), cL.w);
                if (w0.ok && w1.ok && k2.ok && k3.ok) { r.tri(w0.x, w0.y, w1.x, w1.y, k2.x, k2.y, kc); r.tri(w0.x, w0.y, k2.x, k2.y, k3.x, k3.y, kc); }
            }
            const Proj c0 = pt(i, P[i].hw + 1.2, 6.0), c1 = pt(i, -P[i].hw - 1.2, 6.0), c2 = pt(j, -P[j].hw - 1.2, 6.0), c3 = pt(j, P[j].hw + 1.2, 6.0);
            if (c0.ok && c1.ok && c2.ok && c3.ok) { r.tri(c0.x, c0.y, c1.x, c1.y, c2.x, c2.y, ceil); r.tri(c0.x, c0.y, c2.x, c2.y, c3.x, c3.y, ceil); }
            if (i % 5 == 0) {
                const Proj l0 = pt(i, 0.6, 5.9), l1 = pt(i, -0.6, 5.9), l2 = pt(j, -0.6, 5.9), l3 = pt(j, 0.6, 5.9);
                const Color lc = fog({1.0f, 0.85f, 0.55f}, l0.w);
                if (l0.ok && l1.ok && l2.ok && l3.ok) { r.tri(l0.x, l0.y, l1.x, l1.y, l2.x, l2.y, lc); r.tri(l0.x, l0.y, l2.x, l2.y, l3.x, l3.y, lc); }
            }
            continue;
        }
        if (zone == 1 && i % 10 == 0) {                            // sehir: binalar (cephe + uc yuz, pencereler)
            for (int side = -1; side <= 1; side += 2) {
                if (camBlend_ < 0.5 && side < 0) continue;
                const int key = i * 2 + (side > 0);
                const double d = P[i].hw + 5.0 + 3.0 * hashf(key * 5), hgt = 8.0 + 30.0 * hashf(key * 11) * hashf(key * 3 + 1), dep = 14.0;
                const int ie = std::min(i + 9, n - 1);
                const Proj f0 = pt(i, side * d, 0), f1 = pt(ie, side * d, 0), f2 = pt(ie, side * d, hgt), f3 = pt(i, side * d, hgt);
                const Proj e0 = pt(i, side * (d + dep), 0), e3 = pt(i, side * (d + dep), hgt);
                if (!f0.ok || !f1.ok || !f2.ok || !f3.ok || !e0.ok || !e3.ok) continue;
                const float hcol = hashf(key * 17);
                const Color base{0.55f + 0.25f * hcol, 0.52f + 0.2f * hashf(key * 19), 0.50f + 0.18f * hashf(key * 23)};
                const Color face = fog(L({base.r * 0.85f, base.g * 0.85f, base.b * 0.85f}), f0.w), endc = fog(L({base.r * 0.62f, base.g * 0.62f, base.b * 0.62f}), f0.w);
                r.tri(f0.x, f0.y, e0.x, e0.y, e3.x, e3.y, endc); r.tri(f0.x, f0.y, e3.x, e3.y, f3.x, f3.y, endc);
                r.tri(f0.x, f0.y, f1.x, f1.y, f2.x, f2.y, face); r.tri(f0.x, f0.y, f2.x, f2.y, f3.x, f3.y, face);
                if (f0.w < 260.0f) {                                   // pencereler (gece yanik)
                    const int floors = std::max(2, (int)(hgt / 3.5)), cols = 5;
                    for (int fl = 0; fl < floors; ++fl)
                        for (int cl = 0; cl < cols; ++cl) {
                            const float u0 = (cl + 0.25f) / cols, u1 = (cl + 0.75f) / cols, v0 = (fl + 0.3f) / floors, v1 = (fl + 0.75f) / floors;
                            auto bl = [&](float u, float v) { return std::pair<float, float>{f0.x + (f1.x - f0.x) * u + (f3.x - f0.x) * v + (f2.x - f1.x - f3.x + f0.x) * u * v,
                                                                                            f0.y + (f1.y - f0.y) * u + (f3.y - f0.y) * v + (f2.y - f1.y - f3.y + f0.y) * u * v}; };
                            const auto a = bl(u0, v0), b = bl(u1, v0), c = bl(u1, v1), dd = bl(u0, v1);
                            const bool on = night_ && hashf(key * 31 + fl * 7 + cl) > 0.45f;
                            const Color wcol = on ? Color{1.0f, 0.85f, 0.5f, 0.9f} : fog(L({0.18f, 0.22f, 0.28f}), f0.w);
                            r.tri(a.first, a.second, b.first, b.second, c.first, c.second, wcol); r.tri(a.first, a.second, c.first, c.second, dd.first, dd.second, wcol);
                        }
                }
            }
        }
        if ((zone == 1 || P[i].hw > 4.6) && i % 20 == 0) {         // sokak lambasi (40 m), gece isik huzmesi
            const double side = ((i / 20) & 1) ? 1.0 : -1.0;
            if (!(camBlend_ < 0.5 && side < 0)) {
                const Proj p0 = pt(i, side * (P[i].hw + 1.3), 0), p1 = pt(i, side * (P[i].hw + 1.3), 8.0), p2 = pt(i, side * (P[i].hw - 0.6), 8.0);
                if (p0.ok && p1.ok && p2.ok) {
                    const float sc = pxPerM / p0.w;
                    const Color pc = fog(L({0.45f, 0.46f, 0.5f}), p0.w);
                    r.rect(p0.x - 0.09f * sc, p1.y, p0.x + 0.09f * sc, p0.y, pc);
                    r.tri(p1.x, p1.y, p2.x, p2.y, p2.x, p2.y + 0.15f * sc, pc);
                    if (night_) { r.circle(p2.x, p2.y + 0.1f * sc, 1.6f * sc, 12, {1.0f, 0.85f, 0.55f, 0.18f}); r.circle(p2.x, p2.y + 0.1f * sc, 0.35f * sc, 8, {1.0f, 0.95f, 0.8f}); }
                }
            }
        }
        if (mtn && zone == 0 && i % 2 == 0) {                      // dag: bir yanda kaya duvari
            const double side = hashf((int)(P[i].s / 400.0) * 7 + 3) > 0.5 ? 1.0 : -1.0;
            if (!(camBlend_ < 0.5 && side < 0)) {
                const int j2 = std::min(i + 2, n - 1);
                const Proj k0 = pt(i, side * (P[i].hw + 2.5), 0), k1 = pt(j2, side * (P[j2].hw + 2.5), 0);
                const Proj k2 = pt(j2, side * (P[j2].hw + 6.0), 14.0 + 4.0 * hashf(j2)), k3 = pt(i, side * (P[i].hw + 6.0), 14.0 + 4.0 * hashf(i));
                if (k0.ok && k1.ok && k2.ok && k3.ok) {
                    const float t = 0.85f + 0.15f * hashf(i * 13);
                    const Color rc = fog(L({0.42f * t, 0.38f * t, 0.33f * t}), k0.w);
                    r.tri(k0.x, k0.y, k1.x, k1.y, k2.x, k2.y, rc); r.tri(k0.x, k0.y, k2.x, k2.y, k3.x, k3.y, rc);
                }
            }
        }
        if (zone == 0 && !mtn && i % 75 == 37 && hashf(i) > 0.4f) {  // reklam panosu
            const double side = hashf(i * 3) > 0.5 ? 1.0 : -1.0;
            if (!(camBlend_ < 0.5 && side < 0)) {
                const Proj b0 = pt(i, side * (P[i].hw + 9.0), 4.0), b1 = pt(std::min(i + 4, n - 1), side * (P[i].hw + 9.0), 4.0);
                const Proj b2 = pt(std::min(i + 4, n - 1), side * (P[i].hw + 9.0), 8.0), b3 = pt(i, side * (P[i].hw + 9.0), 8.0), g0 = pt(i, side * (P[i].hw + 9.0), 0.0);
                if (b0.ok && b1.ok && b2.ok && b3.ok && g0.ok) {
                    static const Color ads[4] = {{0.85f, 0.2f, 0.15f}, {0.15f, 0.45f, 0.85f}, {0.95f, 0.75f, 0.1f}, {0.2f, 0.7f, 0.35f}};
                    const Color ac = fog(night_ ? Color{ads[i % 4].r * 0.6f, ads[i % 4].g * 0.6f, ads[i % 4].b * 0.6f} : ads[i % 4], b0.w);
                    const float sc = pxPerM / g0.w;
                    r.rect(g0.x - 0.1f * sc, b0.y, g0.x + 0.1f * sc, g0.y, fog(L({0.35f, 0.35f, 0.38f}), g0.w));
                    r.tri(b0.x, b0.y, b1.x, b1.y, b2.x, b2.y, ac); r.tri(b0.x, b0.y, b2.x, b2.y, b3.x, b3.y, ac);
                }
            }
        }
        if (zone == 0 && i % 8 == 0) {                             // kenar agaclari / direkleri (her 16 m)
            for (int side = -1; side <= 1; side += 2) {
                if (camBlend_ < 0.5 && side < 0) continue;            // drag gorunumu: kamera tarafindaki agaclar gorusu kapatir
                const float h = hashf(i * 2 + (side > 0));
                if (h < (mtn ? 0.08f : 0.35f)) continue;
                const Proj b = edge(i, side * (P[i].hw + 5.0 + 20.0 * hashf(i * 7 + side)));
                if (!b.ok) continue;
                const float sc = pxPerM / b.w;
                const float th = (5.0f + 4.0f * h) * sc, tw = (1.6f + h) * sc;
                if (h > 0.8f) {                                      // elektrik diregi + travers
                    const Color pc = fog(L({0.35f, 0.28f, 0.2f}), b.w);
                    r.rect(b.x - 0.12f * sc, b.y - 7.5f * sc, b.x + 0.12f * sc, b.y, pc);
                    r.rect(b.x - 0.9f * sc, b.y - 7.2f * sc, b.x + 0.9f * sc, b.y - 7.0f * sc, pc);
                } else {                                             // cam agaci: golge + govde + iki kat yaprak (isikli / golgeli yari)
                    const Color leaf = fog(L({0.12f, 0.38f + 0.1f * h, 0.16f}), b.w), leafD = fog(L({0.08f, 0.28f + 0.08f * h, 0.12f}), b.w);
                    r.circle(b.x + 0.4f * sc, b.y, tw * 0.9f, 10, {0.0f, 0.0f, 0.0f, 0.18f});
                    r.rect(b.x - tw * 0.12f, b.y - th * 0.35f, b.x + tw * 0.12f, b.y, fog(L({0.35f, 0.24f, 0.14f}), b.w));
                    r.tri(b.x - tw, b.y - th * 0.28f, b.x, b.y - th * 0.28f, b.x, b.y - th * 0.78f, leafD);
                    r.tri(b.x, b.y - th * 0.28f, b.x + tw, b.y - th * 0.28f, b.x, b.y - th * 0.78f, leaf);
                    r.tri(b.x - tw * 0.75f, b.y - th * 0.55f, b.x, b.y - th * 0.55f, b.x, b.y - th, leafD);
                    r.tri(b.x, b.y - th * 0.55f, b.x + tw * 0.75f, b.y - th * 0.55f, b.x, b.y - th, leaf);
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
    if (sideCam && sim.speed() > 12.0) {
        // Hiz cizgileri (drag gorunumu): ust ve alt kenarda, hizla uzar ve yogunlasir; arac ortada temiz kalir
        const float k = std::min(1.0f, (float)(sim.speed() - 12.0) / 35.0f) * (1.0f - (float)camBlend_);
        const int n = 6 + (int)(14 * k);
        for (int i = 0; i < n; ++i) {
            const float h = hashf(i * 13 + 7), y = h < 0.5f ? horizon * (0.15f + 1.6f * h * 0.5f) : H - (H - horizon) * 0.35f * (h - 0.5f) * 2.0f;
            const float len = 30.0f + 160.0f * k * (0.5f + hashf(i * 5 + 1));
            const float x = W - std::fmod((float)ps * 28.0f * (0.6f + hashf(i * 7 + 3)) + hashf(i) * (W + len), W + len);
            r.rect(x, y, x + len, y + 1.5f, {1.0f, 1.0f, 1.0f, 0.15f + 0.3f * k});
        }
    }
    for (const Puff& p : smoke_) {                                       // lastik dumani (araclardan once: arkada kalir)
        const Proj q = project(vp, p.x, p.y, p.z + p.size * 0.5, W, H);
        if (!q.ok || q.w < 1.0f) continue;
        const float rad = (float)p.size * pxPerM / q.w;
        const float a = (float)std::clamp(p.life / 1.6, 0.0, 1.0) * 0.32f * std::clamp((q.w - 2.0f) / 7.0f, 0.15f, 1.0f);   // kameraya yakin: saydam
        const float sl = night_ ? 0.42f : rain_ ? 0.8f : 1.0f;            // gece duman karanlik
        r.circle(q.x, q.y, rad, 12, {0.86f * sl, 0.86f * sl, 0.88f * sl, a});
    }
    r.flush2D();
    // Diger araclar (uzaktan yakina), sonra oyuncu. z: yol yuksekligi + suspansiyon; pitch: gidis yonundeki egim
    struct Obj { double d, x, y, psi, z, pitch; int id; float spin = 0, steer = 0; bool police = false; };
    auto carModel = [](double x, double y, double z, double psi, double pitch) {
        return matMul(matMul(matTranslate((float)x, (float)z, (float)-y), matRotY((float)psi)), matRotZ((float)std::atan(pitch)));
    };
    std::vector<Obj> objs;
    for (const TrafficCar& t : ses_->traffic()) {
        if (t.s < ps - 12 || t.s > ps + 320) continue;
        const RoadPoint q = R.at(t.s);
        Obj o{}; ses_->trafficPose(t, o.x, o.y, o.psi); o.id = t.carId;
        o.z = q.z; o.pitch = t.oncoming ? -q.grade : q.grade;
        o.spin = (float)((t.oncoming ? -t.s : t.s) / 0.31);
        o.d = (o.x - ex) * dx + (o.y - ey) * dy; objs.push_back(o);   // kamera bakis yonunde derinlik
    }
    if (RoadCar* rv = ses_->rival()) {
        const VehicleSim& rs = rv->sim();
        Obj o{0, rs.posX(), rs.posY(), rs.heading(), rv->elevation() + rs.suspension().heave(), rs.grade(), ses_->rivalCarId(),
              (float)spinR_, (float)std::clamp(std::atan(rs.yawRate() * rs.vehicleLoad().wheelbase / std::max(rs.speed(), 3.0)), -0.5, 0.5)};
        o.d = (o.x - ex) * dx + (o.y - ey) * dy;
        o.police = ses_->mode() == RoadSession::Mode::Chase;
        if (o.d > 2 && o.d < 340) objs.push_back(o);
    }
    std::sort(objs.begin(), objs.end(), [](const Obj& a, const Obj& c) { return a.d > c.d; });
    for (const Obj& o : objs) {
        if (o.d < 3.0) continue;                                   // kameraya cok yakin / arkasinda
        if (o.police) {
            Renderer::CarLook pl; pl.paintOn = true; pl.paint[0] = pl.paint[1] = pl.paint[2] = 0.93f;
            pl.stripe = 3; pl.stripeCol[0] = 0.08f; pl.stripeCol[1] = 0.12f; pl.stripeCol[2] = 0.45f;
            r.setCarLook(pl);
        }
        r.drawCar(o.id, 0, 0, W, H, proj, view, carModel(o.x, o.y, o.z, o.psi, o.pitch), o.spin, o.steer);
        if (o.police) {                                            // tepe lambasi: kirmizi / mavi donusumlu
            const VehicleDef* pv = findVehicle(o.id);
            const double hz = o.z + pv->heightM + 0.08;
            const bool ph = std::fmod(envT_ * 5.0, 1.0) < 0.5;
            for (int side = -1; side <= 1; side += 2) {
                const double lx = o.x - side * 0.35 * std::sin(o.psi), ly = o.y + side * 0.35 * std::cos(o.psi);
                const Proj q = project(vp, lx, ly, hz, W, H);
                if (!q.ok || q.w < 1.0f) continue;
                const bool on = (side < 0) == ph;
                const Color c = side < 0 ? Color{1.0f, 0.15f, 0.1f} : Color{0.2f, 0.4f, 1.0f};
                const float k = (float)pxPerM / q.w;
                r.circle(q.x, q.y, 0.16f * k, 8, on ? c : Color{c.r * 0.3f, c.g * 0.3f, c.b * 0.3f});
                if (on) r.circle(q.x, q.y, (night_ ? 1.6f : 0.7f) * k, 14, {c.r, c.g, c.b, night_ ? 0.22f : 0.15f});
            }
        }
    }
    r.setCarLook(app_.career.car().carId == carId_ ? lookOf(app_.career.car()) : lookOf(&tune_));
    r.drawCar(carId_, 0, 0, W, H, proj, view, carModel(X, Y, zCar + sim.suspension().heave(), sim.heading(), sim.grade()),
              (float)spinP_, (float)steer_);
    {   // Egzoz alevi: devir kesici ya da gaz kesme patlamasi (titrek, rastgele)
        const PowertrainCore& ptc = const_cast<VehicleSim&>(sim).powertrain();
        const bool burst = (ptc.limiterHit() && ptc.rpm() > 0.6 * sim.engineSpec().redlineRpm) || popT_ > 0;
        if (burst && hashf((int)(envT_ * 45)) > 0.4f) {
            const double hl = findVehicle(carId_)->lengthM * 0.5 + 0.05, h = sim.heading();
            const double fx = X - hl * std::cos(h) + 0.3 * std::sin(h), fy = Y - hl * std::sin(h) - 0.3 * std::cos(h);
            const Proj q = project(vp, fx, fy, zCar + sim.suspension().heave() + 0.28, W, H);
            if (q.ok && q.w > 1.0f) {
                const float k = (float)pxPerM / q.w * (0.7f + 0.6f * hashf((int)(envT_ * 90) + 3));
                r.circle(q.x, q.y, 0.24f * k, 12, {1.0f, 0.45f, 0.08f, 0.55f});
                r.circle(q.x, q.y, 0.14f * k, 10, {1.0f, 0.8f, 0.3f, 0.85f});
                r.circle(q.x, q.y, 0.06f * k, 8, {0.75f, 0.85f, 1.0f, 0.95f});
            }
        }
    }
    for (const Puff& q : sparks_) {                                    // kivilcimlar: hiz yonunde kisa cizgi
        const Proj a = project(vp, q.x, q.y, q.z, W, H), b = project(vp, q.x - q.vx * 0.025, q.y - q.vy * 0.025, q.z - q.vz * 0.025, W, H);
        if (!a.ok || !b.ok) continue;
        const float al = (float)std::clamp(q.life / 0.4, 0.0, 1.0);
        r.tri(a.x, a.y, b.x, b.y, a.x + 1.5f, a.y + 1.5f, {1.0f, 0.85f, 0.4f, al});
        r.tri(a.x, a.y, b.x + 1.2f, b.y, b.x, b.y + 1.2f, {1.0f, 0.6f, 0.2f, al});
    }
    if (rain_) {                                                       // yagmur: egik damla cizgileri (hizla egilir)
        const float slant = 0.15f + (float)std::min(0.6, sim.speed() / 60.0);
        for (int k = 0; k < 140; ++k) {
            const float x0 = std::fmod(hashf(k * 5) * (W + 80) + (float)envT_ * 40.0f * slant * 6.0f, (float)W + 80) - 40;
            const float y0 = std::fmod(hashf(k * 9 + 2) * H + (float)envT_ * (520.0f + 200.0f * hashf(k)), (float)H);
            const float len = 10.0f + 10.0f * hashf(k * 13);
            r.tri(x0, y0, x0 + 0.9f, y0, x0 - slant * len + 0.9f, y0 + len, {0.75f, 0.8f, 0.9f, 0.35f});
        }
    }
}

// Polis yakinken ekran kenarlarinda kirmizi / mavi cakar yansimasi (arkadaki polis gorunmese de hissedilir)
static void policeStrobe(Renderer& r, float W, float H, double gap, double t) {
    const float k = (float)std::clamp(1.0 - gap / 70.0, 0.0, 1.0);
    if (k <= 0.0f) return;
    const bool ph = std::fmod(t * 5.0, 1.0) < 0.5;
    const Color red{1.0f, 0.1f, 0.05f, 0.0f}, blue{0.1f, 0.3f, 1.0f, 0.0f};
    for (int side = 0; side < 2; ++side) {
        Color c = (side == 0) == ph ? red : blue;
        Color c0 = c; c0.a = 0.28f * k;
        const float x0 = side ? W - W * 0.18f : 0, x1 = side ? W : W * 0.18f;
        for (int i = 0; i < 6; ++i) {                                  // yumusak kenar: 6 kat
            Color ci = c0; ci.a *= (6 - i) / 6.0f;
            const float a = side ? x1 - (x1 - x0) * (i + 1) / 6.0f : x0 + (x1 - x0) * i / 6.0f;
            r.rect(a, 0, a + (x1 - x0) / 6.0f, H, ci);
        }
    }
}

void RoadScreen::drawHud(Renderer& r) {
    if (ses_->mode() == RoadSession::Mode::Chase && ses_->phase() == RoadSession::Phase::Run && ses_->gapMeters() > -5.0)
        policeStrobe(r, (float)W, (float)H, ses_->gapMeters(), envT_);
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
    Color gc = {1.0f, 0.62f, 0.05f};
    if (autoBox) {
        const Cockpit::AutoPos ap = cockpit_.autoPos();
        if (ap == Cockpit::AutoPos::P) gs = "P"; else if (ap == Cockpit::AutoPos::R) gs = "R"; else if (ap == Cockpit::AutoPos::N) gs = "N";
        else if (ap == Cockpit::AutoPos::S) gc = {1.0f, 0.3f, 0.25f};          // spor: kirmizi
        else if (ap == Cockpit::AutoPos::M) gc = {0.45f, 0.8f, 1.0f};          // elle: mavi
    }
    r.text(x0 + 106, 4, gs, 4, Pc.grinding() ? Color{1.0f, 0.2f, 0.15f} : gc);
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
    } else if (ses_->mode() == RoadSession::Mode::Chase) {               // polis: mesafe, yakalanma, kacis
        const double gap = ses_->gapMeters();
        std::snprintf(b, sizeof b, "POLIS %+.0f M   %.1f S", -gap, ses_->raceTime());
        r.text(x0, infoY, b, 1, gap < 40 ? Color{1.0f, 0.4f, 0.3f} : Color{0.6f, 0.75f, 1.0f});
        const float bw = 120, by = infoY + 14;
        r.text(x0, by, "YAKALANMA", 1, {1.0f, 0.5f, 0.45f});
        r.rect(x0 + 62, by, x0 + 62 + bw, by + 7, {0.15f, 0.15f, 0.2f});
        r.rect(x0 + 62, by, x0 + 62 + bw * (float)ses_->bustLevel(), by + 7, {1.0f, 0.2f, 0.15f});
        r.text(x0, by + 11, "KACIS", 1, {0.5f, 1.0f, 0.6f});
        r.rect(x0 + 62, by + 11, x0 + 62 + bw, by + 18, {0.15f, 0.15f, 0.2f});
        const float ep = (float)std::max(ses_->escapeProgress(), std::clamp(gap / RoadSession::kEscapeGap, 0.0, 1.0) * 0.5);
        r.rect(x0 + 62, by + 11, x0 + 62 + bw * ep, by + 18, {0.3f, 0.95f, 0.45f});
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
    {   // Su sicakligi ve nitro (varsa): ust seridin altinda kucuk gostergeler
        const float gx = land_ ? 470.0f : W - 120.0f, gy = land_ ? 54.0f : 82.0f;
        const double T = sim.coolantC();
        std::snprintf(b, sizeof b, "SU %3.0fC", T);
        r.text(gx, gy, b, 1, T > 108 ? Color{1.0f, 0.3f, 0.2f} : T > 100 ? kUiGold : Color{0.6f, 0.75f, 0.9f});
        if (sim.hasNitrous()) {
            r.rect(gx + 56, gy, gx + 106, gy + 7, {0.15f, 0.15f, 0.2f});
            r.rect(gx + 56, gy, gx + 56 + 50 * (float)sim.nitrousLeft(), gy + 7, sim.nitrousActive() ? Color{0.3f, 0.7f, 1.0f} : Color{0.2f, 0.45f, 0.8f});
            r.text(gx + 56, gy + 9, "NOS", 1, {0.5f, 0.75f, 1.0f});
        }
        if (sim.engineBlown()) r.textCentered(W / 2.0f, land_ ? 90 : 96, "MOTOR PATLADI", 2, {1.0f, 0.25f, 0.2f});
        else if (sim.gearboxBroken()) r.textCentered(W / 2.0f, land_ ? 90 : 96, "SANZIMAN KIRIK", 2, {1.0f, 0.25f, 0.2f});
    }
    if (msgT_ > 0) {
        const float w = r.textWidth(msg_, 2) + 16;
        const float my = land_ ? 120 : 140;
        r.rect(W / 2.0f - w / 2, my, W / 2.0f + w / 2, my + 22, {0.02f, 0.02f, 0.04f, 0.7f});
        r.textCentered(W / 2.0f, my + 4, msg_, 2, {1.0f, 0.85f, 0.3f});
    }
    if (ses_->mode() == RoadSession::Mode::Karma) {
        // Drag agaci (Sportsman): stage isiklari, 3 amber .5 s arayla, yesil
        const int L = ses_->treeLights();
        if (ses_->phase() == RoadSession::Phase::Countdown || L) {
            const float cx = land_ ? 545.0f : W - 34.0f, ty = land_ ? 66.0f : 96.0f;   // sagda: arac ve mesaj acik kalir
            r.rect(cx - 26, ty, cx + 26, ty + 132, {0.06f, 0.06f, 0.07f, 0.92f});
            r.rect(cx - 3, ty + 132, cx + 3, ty + 150, {0.2f, 0.2f, 0.22f});
            for (int side = -1; side <= 1; side += 2) {
                const float x = cx + side * 12.0f;
                r.circle(x, ty + 10, 4, 10, {0.95f, 0.95f, 0.85f});            // pre-stage / stage
                r.circle(x, ty + 22, 4, 10, {0.95f, 0.95f, 0.85f});
                for (int k = 0; k < 3; ++k)
                    r.circle(x, ty + 42 + k * 22, 8, 14, (L >> k) & 1 ? Color{1.0f, 0.62f, 0.05f} : Color{0.22f, 0.16f, 0.05f});
                r.circle(x, ty + 110, 8, 14, (L & 8) ? Color{0.2f, 1.0f, 0.3f} : Color{0.05f, 0.2f, 0.07f});
            }
        }
        if (ses_->reaction() > 0 && ses_->raceTime() < ses_->reaction() + 2.5) {
            std::snprintf(b, sizeof b, "TEPKI %.3f  RAKIP %.3f", ses_->reaction(), ses_->rivalReaction());
            r.textCentered(W / 2.0f, land_ ? 64 : 90, b, 1, ses_->reaction() <= ses_->rivalReaction() ? Color{0.4f, 1.0f, 0.5f} : Color{1.0f, 0.6f, 0.3f});
        }
    } else if (ses_->phase() == RoadSession::Phase::Countdown) {
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
    } else if (ses_->mode() == RoadSession::Mode::Chase) {
        const bool esc = ses_->playerWon();
        r.textCentered(W / 2.0f, 66 + oy, esc ? "KACTIN!" : "YAKALANDIN!", 3, esc ? Color{0.3f, 1.0f, 0.4f} : Color{1.0f, 0.3f, 0.2f});
        std::snprintf(b, sizeof b, "SURE %.1f S   MESAFE %.2f KM", ses_->raceTime(), (ses_->player().s() - 90.0) / 1000.0);
        r.textCentered(W / 2.0f, 110 + oy, b, 1, {0.85f, 0.85f, 0.9f});
        std::snprintf(b, sizeof b, "CARPISMA %d%s", ses_->collisions(), esc && ses_->collisions() == 0 ? "   TEMIZ KACIS +%50" : "");
        r.textCentered(W / 2.0f, 128 + oy, b, 1, {0.8f, 0.8f, 0.85f});
    } else {
        r.textCentered(W / 2.0f, 66 + oy, ses_->playerWon() ? "KAZANDIN" : "KAYBETTIN", 3, ses_->playerWon() ? Color{0.3f, 1.0f, 0.4f} : Color{1.0f, 0.3f, 0.2f});
        r.text(lx, 110 + oy, ("SEN   " + secStr(ses_->playerTime())).c_str(), 2, {1, 1, 1});
        r.text(lx, 136 + oy, ("RAKIP " + secStr(ses_->rivalTime())).c_str(), 2, {1, 1, 1});
        std::snprintf(b, sizeof b, "CARPISMA %d", ses_->collisions());
        r.text(lx, 166 + oy, b, 1, {0.8f, 0.8f, 0.85f});
    }
    if (prize_ < 0) std::snprintf(b, sizeof b, "CEZA $%ld", -prize_);
    else std::snprintf(b, sizeof b, "ODUL $%ld", prize_);
    r.textCentered(W / 2.0f, 200 + oy, b, 3, prize_ < 0 ? Color{1.0f, 0.35f, 0.3f} : kUiGold);
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
        else if (chase_.hit(x, y)) start(RoadSession::Mode::Chase);
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
        else if (k == Key::Gear2) start(RoadSession::Mode::Chase);
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
