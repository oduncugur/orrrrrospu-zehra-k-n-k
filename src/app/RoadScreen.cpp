// ZEHRA KINIK - Acik yol ekrani (yatay 640x360): prosedurel yol, arkadan kamera, duzlemsel fizik.
// Kontroller (Cockpit): analog gaz / fren (/ debriyaj), sanzimana gore vites kolu. Direksiyon: telefonda egim,
// masaustunde klavye (ekranda sag/sol tusu yok).
#include "Screens.h"
#include "Gauges.h"
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
void triP(Renderer& r, const Proj& a, const Proj& b, const Proj& c, Color col) { r.triZ(a.x, a.y, a.w, b.x, b.y, b.w, c.x, c.y, c.w, col); }
float hashf(int i) { unsigned x = (unsigned)i * 2654435761u; x ^= x >> 13; x *= 0x5bd1e995u; x ^= x >> 15; return (x & 0xFFFF) / 65535.0f; }
std::string secStr(double t) { char b[16]; std::snprintf(b, sizeof b, "%.1f S", t); return t > 0 ? b : "BITIREMEDI"; }
} // namespace

// Ekran yonu ayardan (yatay 640x360 / dikey 360x640): menu dugmeleri, HUD dugmeleri, kokpit yerlesimi
void RoadScreen::setupLayout() {
    land_ = !app_.settings.roadPortrait;
    W = land_ ? 640 : 360; H = land_ ? 360 : 640;
    if (land_) {
        free_ = {90, 100, 314, 146}; flow_ = {326, 100, 550, 146}; race_ = {90, 156, 314, 202}; touge_ = {326, 156, 550, 202};
        karma_ = {90, 212, 314, 258}; chase_ = {326, 212, 550, 258}; marathon_ = {90, 268, 550, 300};
        assistBtn_ = {470, 2, 576, 34}; tiltBtn_ = {362, 2, 466, 34}; adasBtn_ = {362, 38, 466, 66};
    } else {
        free_ = {40, 186, 320, 234}; flow_ = {40, 244, 320, 292}; race_ = {40, 302, 320, 350}; touge_ = {40, 360, 320, 408};
        karma_ = {40, 418, 320, 466}; chase_ = {40, 476, 320, 524}; marathon_ = {40, 534, 320, 582};
        assistBtn_ = {252, 4, 356, 26}; tiltBtn_ = {252, 30, 356, 52}; adasBtn_ = {252, 56, 356, 78};
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
    if (app_.runPlan.active) { start(RoadSession::Mode::Marathon); return; }   // sehirler arasi etap
    if (const char* m = std::getenv("ZK_ROAD_MODE")) {
        const std::string n = m;
        start(n == "free" ? RoadSession::Mode::Free : n == "flow" ? RoadSession::Mode::Flow : n == "karma" ? RoadSession::Mode::Karma
              : n == "chase" ? RoadSession::Mode::Chase : n == "marathon" ? RoadSession::Mode::Marathon : RoadSession::Mode::Race,
              n == "touge" ? RoadSession::Kind::Touge : RoadSession::Kind::Highway);
    }
    else if (autopilot_) start(RoadSession::Mode::Free);
    if (app_.activeEvent >= 0) {                                         // lig etkinligi: mod dogrudan
        switch (leagueEvents()[app_.activeEvent].mode) {
        case EventMode::Road: start(RoadSession::Mode::Race); break;
        case EventMode::Touge: start(RoadSession::Mode::Race, RoadSession::Kind::Touge); break;
        case EventMode::Karma: start(RoadSession::Mode::Karma); break;
        case EventMode::Flow: start(RoadSession::Mode::Flow); break;
        case EventMode::Marathon: start(RoadSession::Mode::Marathon); break;
        case EventMode::Chase:
            start(RoadSession::Mode::Chase, leagueEvents()[app_.activeEvent].league == 3 ? RoadSession::Kind::Touge : RoadSession::Kind::Highway);
            break;
        default: break;
        }
    }
}

void RoadScreen::start(RoadSession::Mode m, RoadSession::Kind kind) {
    int rival = 0; Tune rt;
    if (m == RoadSession::Mode::Race || m == RoadSession::Mode::Karma || m == RoadSession::Mode::Chase || m == RoadSession::Mode::Marathon) {
        const Opponent o = app_.activeEvent >= 0 ? app_.lastOpp : app_.career.pickOpponentFor((uint32_t)(app_.career.races * 7919 + 17));
        app_.lastOpp = o;
        rival = o.carId; rt = o.tune;
        app_.setVoiceTuned(1, rival, &rt);
    }
    static uint32_t runs = 0;                                      // ayni oturumda her surus farkli yol/trafik
    const uint32_t seed = (uint32_t)(app_.career.races + 1 + (m == RoadSession::Mode::Flow ? runs++ : 0)) * 2654435761u;
    ses_ = std::make_unique<RoadSession>(m, carId_, &tune_, rival, &rt, seed, kind, app_.runPlan.active ? app_.runPlan.realKm : 300.0);
    if (m == RoadSession::Mode::Marathon) {                              // alan: seyahat plani ya da kariyer seviyesinde 20 arac
        if (app_.runPlan.active) {
            ses_->setRunField(app_.runPlan.field, app_.runPlan.realKm);
            if (app_.runPlan.fuelL >= 0) ses_->player().sim().setFuelLevel(app_.runPlan.fuelL);   // onceki etaptan kalan
        } else ses_->setRunField(app_.career.runField(20, seed), 300.0);
        l100_ = 0; usedRun_ = 0; lastRunS_ = ses_->player().s(); lastFuel_ = ses_->player().sim().fuelLiters();
    }
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
        ses_->setRivalPace(0.47 + (1.6 - std::clamp(app_.eventHandicap, 1.0, 1.6)) * 0.12);   // patron ~0.53, siradan ~0.48 g
    }
    camPsi_ = ses_->player().sim().heading();
    RoadCar& P = ses_->player();
    P.assist = app_.settings.assist;
    P.stability = true;                                                 // duz yol dengesi (oyuncu)
    P.esp = app_.settings.esp;
    adasMode_ = 0; adasLevel_ = adasLevel(*findVehicle(carId_), tune_);
    if (const char* a = std::getenv("ZK_ADAS")) { adasLevel_ = 4; adasMode_ = std::atoi(a); ccSpeed_ = 80.0 / 3.6; }   // test: surus yardimi acik baslar
    // Vites kolu sanziman tipinden: H-desen (oyuncu ya da otomatik debriyaj), otomatik P-N-D, sirali +/-
    const Gearbox box = P.sim().gearboxType();
    const int gears = P.sim().powertrain().gearCount();
    if (box == Gearbox::HPattern) {
        cockpit_.configure(Cockpit::Lever::HPattern, gears, !app_.settings.autoClutch);
        P.manual = true; P.slowClutch = app_.settings.autoClutch;
    } else if (box == Gearbox::TorqueConverter || box == Gearbox::DCT) {   // DSG / PDK da P-R-N-D-S + M (+/-) kapisi
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
          : m == RoadSession::Mode::Marathon ? "THE RUN: VERIMLI SUR, YAKIT AZALINCA BENZINLIGE GIR"
          : kind == RoadSession::Kind::Touge ? "DAG YOLU 3 KM" : "YOL YARISI 4 KM", 2.5);
}

// Sonuctan cikis: seyahatte sonraki etap (yeni ekran), varista harita, aksi halde garaj
void RoadScreen::leaveResults() {
    if (ses_->mode() == RoadSession::Mode::Marathon && app_.runPlan.active && rewarded_) {
        app_.continueTravel();
        return;
    }
    if (ses_->mode() == RoadSession::Mode::Marathon && !app_.eventNote.empty()) { app_.goMap(); return; }
    app_.goGarage();
}

bool RoadScreen::autoClutchPenalty() const {
    return cockpit_.lever() == Cockpit::Lever::HPattern && !cockpit_.clutchPedal();
}

void RoadScreen::finishRace() {
    rewarded_ = true;
    if (autopilot_) return;
    {   // kilometre sayaci: surulen yol (The Run: etabin temsil ettigi gercek mesafe orani)
        const double m = std::max(0.0, ses_->player().s() - ses_->startS());
        app_.career.addKm(m / 1000.0 * (ses_->mode() == RoadSession::Mode::Marathon ? ses_->compression() : 1.0));
    }
    if (app_.activeEvent >= 0) {                                         // lig etkinligi
        int pink = 0;
        const long score = ses_->flow() ? ses_->flow()->score() : 0;
        const bool won = ses_->mode() == RoadSession::Mode::Marathon ? ses_->runPosition() <= 3 : ses_->rival() ? ses_->playerWon() : false;   // etap: podyum
        prize_ = app_.career.recordEvent(app_.activeEvent, won, 0.0, score, &pink);
        if (pink > 0) app_.eventNote = std::string("PINK SLIP: ") + upper(findVehicle(pink)->model) + " SENIN!";
        else if (pink < 0) app_.eventNote = "PINK SLIP: ARABANI KAYBETTIN";
    }
    else if (ses_->mode() == RoadSession::Mode::Flow) prize_ = app_.career.recordFlow(ses_->flow()->score(), &record_);
    else if (ses_->mode() == RoadSession::Mode::Chase) prize_ = app_.career.recordChase(ses_->playerWon(), ses_->collisions());
    else if (ses_->mode() == RoadSession::Mode::Marathon) {
        prize_ = app_.runPlan.active && app_.runPlan.solo ? 0 : app_.career.recordRun(ses_->runPosition(), ses_->runCount(), ses_->realKm());   // tek basina: odul yok
        if (app_.runPlan.active) app_.nextTravelLeg(ses_->player().sim().fuelLiters());   // sehir ilerler; sonraki etap / varis
    }
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
        // Kararli ve hiza duyarli egim: ek suzgec (~0.12 s); olu bolge %5; tepki egrisi hizla sertlesir (u^p, p 1 -> 1.8):
        // yuksek hizda kucuk egimler az direksiyon verir, telefon cok cevrilince tepki normale yaklasir (tam kilit ayni)
        const double raw = std::clamp((app_.settings.tiltInvert ? -1.0 : 1.0) * app_.tilt() * app_.settings.tiltSens / 100.0, -1.0, 1.0);
        tiltF_ += (raw - tiltF_) * std::min(1.0, dt / 0.12);
        const double dz = 0.08, t = tiltF_;                           // olu bolge %8 (el titremesi / sensor sapmasi cekmesin)
        const double u = std::fabs(t) < dz ? 0.0 : (t - std::copysign(dz, t)) / (1.0 - dz);
        const double p = 1.0 + 0.8 * std::clamp((v - 8.0) / 32.0, 0.0, 1.0);
        target = std::copysign(std::pow(std::fabs(u), p), u) * maxSteer;
    }
    const double rate = (std::fabs(target) > std::fabs(steer_) ? 1.0 : 2.5) * dt;
    steer_ += std::clamp(target - steer_, -rate, rate);
    // Karma: duz bolumde drag gorunumu (yandan kamera) ve arac seridi kendi tutar; virajli bolumde 3B surus
    const bool run = ses_->mode() == RoadSession::Mode::Marathon;
    const bool karma = ses_->mode() == RoadSession::Mode::Karma || run;
    const bool curvy = !karma || ses_->road().curvyAt(P.s());
    camBlend_ += std::clamp((curvy ? 1.0 : 0.0) - camBlend_, -dt / 0.9, dt / 0.9);
    if (karma && !curvy) {
        if (run) {   // The Run duzlugu (2B): jiroskop serit degistirmez; onde yavas arac varsa bos seride kendisi gecer (klavye de olur)
            const int nf = ses_->road().lanesFwd(P.s());
            laneCool_ -= dt;
            if (laneCool_ <= 0 && (kL_ || kR_)) { runLane_ += kL_ ? 1 : -1; laneCool_ = 0.6; }
            else if (laneCool_ <= 0) {
                auto blocked = [&](int ln, double ahead) {                   // seritte onde (ya da yanda) arac var mi
                    const double lo = ses_->road().laneOffset(P.s(), false, std::clamp(ln, 0, nf - 1));
                    for (const Runner& Rn : ses_->runField().runners())
                        if (Rn.s > P.s() - 6.0 && Rn.s < P.s() + ahead && std::fabs(Rn.lane - lo) < 1.6 && (ahead < 10.0 || Rn.v < v - 0.5)) return true;
                    for (const TrafficCar& t : ses_->traffic())                // trafik de (carpisma acik)
                        if (!t.oncoming && t.s + trafficHalfLen(t.carId) > P.s() - 6.0 && t.s - trafficHalfLen(t.carId) < P.s() + ahead &&
                            std::fabs(t.lane - lo) < 1.6 && (ahead < 10.0 || t.v < v - 0.5)) return true;
                    return false;
                };
                if (blocked(runLane_, 20.0 + 1.2 * v))
                    for (int cand : {runLane_ + 1, runLane_ - 1})
                        if (cand >= 0 && cand < nf && !blocked(cand, 8.0) && !blocked(cand, 20.0 + 1.2 * v)) { runLane_ = cand; laneCool_ = 1.0; break; }
            }
            runLane_ = std::clamp(runLane_, 0, nf - 1);
            steer_ = P.aiControls(ses_->road().laneOffset(P.s(), false, runLane_), 0.6).steer;
        } else steer_ = P.aiControls(ses_->rightLane(P.s()), 0.6).steer;
    } else if (run) {                                                    // virajda: bulundugu seridi hatirla
        int best = 0; double bd = 1e9;
        for (int k = 0; k < ses_->road().lanesFwd(P.s()); ++k) { const double d = std::fabs(P.lateral() - ses_->road().laneOffset(P.s(), false, k)); if (d < bd) { bd = d; best = k; } }
        runLane_ = best;
    }

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
        c.gear = std::max(0, cockpit_.knobGear());
        c.reverse = cockpit_.knobGear() < 0;                              // R: vites bos + geri itis
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
        c = P.aiControls(ses_->rightLane(P.s()), 0.55, cap);
        if (ses_->mode() == RoadSession::Mode::Marathon) ses_->pitControls(0, 0.55, c);   // otopilot da benzinlige girer
        P.manual = false; P.slowClutch = false;
    }
    applyAdas(c, dt, kL_ || kR_);   // serit takip / otonom: jiroskop yok sayilir, yalniz tus direksiyonu
    ses_->update(dt, c);
    // Teker donusu ve lastik dumani
    spinP_ += std::clamp(P.sim().wheel(0).omega() * dt, -0.55, 0.55);   // gorsel: vagon tekerlegi yanilsamasi olmasin
    envT_ += dt;
    if (RoadCar* rv = ses_->rival()) spinR_ += std::clamp(rv->sim().wheel(0).omega() * dt, -0.55, 0.55);
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
    {   double es = 0, el = 0;
        if (ses_->takeExplosion(es, el)) {                                 // benzinlik patladi: alev topu, kivilcim, sarsinti
            const RoadPoint q = ses_->road().at(es);
            const double ex = q.x - el * std::sin(q.heading), ey = q.y + el * std::cos(q.heading);
            for (int k = 0; k < 140 && sparks_.size() < 400; ++k) {
                const double a = hashf(k * 7 + 3) * 6.2831853, up = 4.0 + 12.0 * hashf(k * 11), sp = 3.0 + 14.0 * hashf(k * 13 + 1);
                sparks_.push_back({ex, ey, q.z + 1.0, sp * std::cos(a), sp * std::sin(a), up, 1.0 + 1.5 * hashf(k * 3), 0});
            }
            for (int k = 0; k < 30; ++k)
                smoke_.push_back({ex + 3.0 * (hashf(k) - 0.5), ey + 3.0 * (hashf(k * 5) - 0.5), q.z + 1.0 + 2.0 * hashf(k * 9), 0.0, 0.0, 2.5, 3.0 + 3.0 * hashf(k * 2), 2.5 + 2.0 * hashf(k * 3)});
            shakeT_ = 1.2; app_.haptic(600, 255);
        }
    }
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
    app_.tire(0, P.tireSlipSpeed(), P.tireLockSpeed());
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
int RoadScreen::zoneAt(double s) const { return ses_->zoneAt(s); }   // cizim = carpisma (RoadSession)

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
    const float ty = land_ ? 30 : 110, by = marathon_.y1;                // baslik / dugmelerin alti
    r.textCentered(W / 2.0f, ty, "ACIK YOL", 4, {1.0f, 0.62f, 0.05f});
    r.textCentered(W / 2.0f, ty + 40, "ARA TASLAK", 1, {0.6f, 0.6f, 0.65f});
    button(r, free_, "SERBEST SURUS", Color{0.15f, 0.45f, 0.7f}, 2);
    button(r, flow_, "OTOBAN AKISI 2 DK", Color{0.1f, 0.5f, 0.35f}, 2);
    button(r, race_, "YOL YARISI 4 KM", kUiOrange, 2);
    button(r, touge_, "DAG YOLU 3 KM", Color{0.55f, 0.2f, 0.6f}, 2);
    button(r, karma_, land_ ? "KARMA" : "KARMA: DRAG + VIRAJ", Color{0.7f, 0.15f, 0.15f}, 2);
    button(r, chase_, "POLIS KACIS", Color{0.12f, 0.2f, 0.55f}, 2);
    button(r, marathon_, "THE RUN: 300 KM ETAP, 20 ARAC", Color{0.45f, 0.35f, 0.08f}, 2);
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
    const Mat4 proj = matPerspective((float)camFov, (float)W / H, 0.3f, 2500.0f);
    const Mat4 view = matLookAt((float)ex, (float)ez, (float)-ey, (float)tx, (float)tz, (float)-ty);
    const Mat4 vp = matMul(proj, view);
    r.beginWorldDepth(0.3f, 2500.0f);                                   // dunya derinlik yazar: araclar binalarin arkasinda kalir
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
        const float f = std::clamp((w - (rain_ ? 30.0f : 80.0f)) / (rain_ ? 400.0f : 1300.0f), 0.0f, 0.85f);   // uzak gorus
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
    if (!mtn && !sideCam) {
        // Uzak manzara (dunya yonune sabit, bulutlar gibi): iki kat yuvarlak tepe; sehirde gokdelen silueti
        const float yaw = (float)std::atan2(dy, dx), hfov = 2.0f * std::atan(std::tan((float)camFov * 0.5f) * W / H);
        auto azX = [&](float az) { return W * 0.5f - std::remainder(az - yaw, 6.2831853f) / hfov * W; };
        const bool city = zoneAt(ps) == 1;
        for (int layer = 0; layer < 2; ++layer) {
            const float k0 = layer == 0 ? 0.55f : 0.35f;                     // uzak kat sise karisir
            const Color hc{(0.34f + 0.2f * k0) * amb + fogCol.r * k0 * 0.5f, (0.48f + 0.08f * k0) * amb + fogCol.g * k0 * 0.4f,
                           (0.36f + 0.25f * k0) * amb + fogCol.b * k0 * 0.5f};
            const int nseg = 64;
            for (int k = 0; k < nseg; ++k) {
                const float a0 = 6.2831853f * k / nseg, a1 = 6.2831853f * (k + 1) / nseg;
                const float x0 = azX(a0), x1 = azX(a1);
                if ((x0 < -40 && x1 < -40) || (x0 > W + 40 && x1 > W + 40) || std::fabs(x1 - x0) > W) continue;
                auto hgt = [&](int kk) {                                         // yumusak gurultu (iki frekans)
                    const float u = kk * 0.37f + layer * 11.0f;
                    return (layer == 0 ? 22.0f : 12.0f) * (0.55f + 0.45f * std::sin(u) * std::cos(u * 0.53f + 1.7f)) + 6.0f * hashf(kk * 7 + layer);
                };
                const float h0 = hgt(k) * H / 360.0f, h1 = hgt(k + 1) * H / 360.0f;
                r.tri(x0, horizon, x1, horizon, x1, horizon - h1, hc); r.tri(x0, horizon, x1, horizon - h1, x0, horizon - h0, hc);
            }
            if (city && layer == 0)                                         // gokdelenler: uzak, puslu
                for (int k = 0; k < 40; ++k) {
                    const float x = azX(k * 0.157f + 0.05f * hashf(k)), bw = (8.0f + 10.0f * hashf(k * 3)) * W / 640.0f;
                    if (x < -20 || x > W + 20) continue;
                    const float bh = (20.0f + 55.0f * hashf(k * 5) * hashf(k * 9 + 1)) * H / 360.0f;
                    const Color bc{0.42f * amb + fogCol.r * 0.35f, 0.45f * amb + fogCol.g * 0.35f, 0.52f * amb + fogCol.b * 0.35f};
                    r.rect(x - bw * 0.5f, horizon - bh, x + bw * 0.5f, horizon, bc);
                    if (night_) for (int w2 = 0; w2 < 6; ++w2) if (hashf(k * 13 + w2) > 0.55f)
                        r.rect(x - bw * 0.3f + (w2 % 2) * bw * 0.35f, horizon - bh + 4 + (w2 / 2) * 6, x - bw * 0.3f + (w2 % 2) * bw * 0.35f + 2, horizon - bh + 6 + (w2 / 2) * 6, {1.0f, 0.85f, 0.5f, 0.8f});
                }
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
    const int i0 = std::max(0, (int)((ps - (sideCam ? 140.0 : 90.0)) / RoadPath::kStep)), n = (int)R.points().size();
    const int i1 = std::min(n - 2, i0 + (sideCam ? 500 : 745));   // gorus: ~1400 m ileri, 90 m geri
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
            const Color g0 = zone == 1 ? Color{0.42f * amb, 0.42f * amb, 0.44f * amb} : grass;   // tek ton (seritli ton yandan parsel gibi gorunuyordu)
            const Color gc = camBlend_ < 0.5 && zone != 1 ? g0 : fog(night_ && zone == 1 ? L({0.42f, 0.42f, 0.44f}) : g0, aL.w);   // yandan: sissiz (segment segment ton farki kama gibi gorunuyordu)
            if (gL0.ok && gL1.ok) { triP(r, gL0, aL, bL, gc); triP(r, gL0, bL, gL1, gc); }
            if (gR0.ok && gR1.ok && camBlend_ >= 0.5) { triP(r, aR, gR0, gR1, gc); triP(r, aR, gR1, bR, gc); }   // yandan: kamera tarafi seridi
            // kameraya cok yakin / arkasindan gecen ucgenler bozuk izdusum (koyu kama) yapiyordu; zemin zaten cim rengi
        }
        if (zone == 0) {                                                // banket: toprak / cakil (cim ile bordur arasi)
            const Proj sL0 = edgeW(i, 1, -2.6), sL1 = edgeW(j, 1, -2.6), sR0 = edgeW(i, -1, -2.6), sR1 = edgeW(j, -1, -2.6);
            const float v = 0.92f + 0.08f * hashf(i * 3);
            const Color sc = fog(L(mtn ? Color{0.45f * v, 0.42f * v, 0.38f * v} : Color{0.52f * v, 0.46f * v, 0.36f * v}), aL.w);
            if (sL0.ok && sL1.ok) { triP(r, sL0, aL, bL, sc); triP(r, sL0, bL, sL1, sc); }
            if (sR0.ok && sR1.ok) { triP(r, aR, sR0, sR1, sc); triP(r, aR, sR1, bR, sc); }
        }
        const Color curb = fog(mtn ? (band ? Color{0.62f, 0.64f, 0.66f} : Color{0.8f, 0.8f, 0.78f})      // dag: celik bariyer
                                   : (band ? Color{0.85f, 0.15f, 0.12f} : Color{0.92f, 0.92f, 0.9f}), aL.w);
        { const Color cb = fog(L(curb), aL.w); triP(r, aL, aR, bR, cb); triP(r, aL, bR, bL, cb); }
        const Proj cL = edgeW(i, 1, 0.0), cR = edgeW(i, -1, 0.0), dL = edgeW(j, 1, 0.0), dR = edgeW(j, -1, 0.0);
        // Asfalt: hafif yama/renk degisimi (hash) + tekerlek izi koyulugu
        const float patch = 0.015f * (hashf(i / 4) - 0.5f);
        const float wet = rain_ ? 0.72f : 1.0f;                              // islak asfalt koyu
        const Color asp = fog(L(band ? Color{(0.30f + patch) * wet, (0.30f + patch) * wet, (0.32f + patch) * wet}
                                     : Color{(0.27f + patch) * wet, (0.27f + patch) * wet, (0.29f + patch) * wet}), cL.w);
        triP(r, cL, cR, dR, asp); triP(r, cL, dR, dL, asp);
        if (cL.w < 45.0f) {                                            // agrega benekleri (acik / koyu tas)
            for (int k = 0; k < 6; ++k) {
                const float hx = hashf(i * 17 + k * 5), hy = hashf(i * 23 + k * 11);
                const Proj q = pt(i, -P[i].hw + 2.0 * P[i].hw * hx, 0.01);
                if (!q.ok) continue;
                const float sz = std::max(0.6f, 0.06f * pxPerM / q.w);
                const float v = hy > 0.5f ? 0.40f : 0.18f;
                r.setDepthW(q.w - 0.02f);
                r.rect(q.x - sz, q.y - sz * 0.5f, q.x + sz, q.y + sz * 0.5f, fog(L({v * wet, v * wet, (v + 0.02f) * wet, 0.7f}), q.w));
            }
        }
        if (i % 6 == 0 && cL.w < 120.0f && hashf(i * 41) > 0.55f) {   // yama dikisi (koyu katran cizgisi, enine)
            const Proj u0 = pt(i, -P[i].hw, 0.012), u1 = pt(i, P[i].hw, 0.012);
            if (u0.ok && u1.ok) {
                const Color tc = fog(L({0.12f * wet, 0.12f * wet, 0.13f * wet, 0.6f}), cL.w);
                const float th = std::max(0.6f, 0.10f * pxPerM / cL.w);
                r.tri(u0.x, u0.y - th, u1.x, u1.y - th, u1.x, u1.y + th, tc);
                r.tri(u0.x, u0.y - th, u1.x, u1.y + th, u0.x, u0.y + th, tc);
            }
        }
        if (cL.w < 120.0f)                                         // tekerlek izleri (serit merkezinin iki yani)
            for (int tk = 0; tk < 2 * (int)std::lround(P[i].lf + P[i].lb); ++tk) {
                const double w2 = 2.0 * P[i].hw / std::max(1.0, P[i].lf + P[i].lb);
                const double lo = -P[i].hw + (tk / 2 + 0.5) * w2 + ((tk & 1) ? 0.75 : -0.75);
                const Proj t0 = edge(i, lo + 0.25), t1 = edge(i, lo - 0.25), t2 = edge(j, lo - 0.25), t3 = edge(j, lo + 0.25);
                const Color tc{0.0f, 0.0f, 0.0f, 0.07f};
                triP(r, t0, t1, t2, tc); triP(r, t0, t2, t3, tc);
            }
        {   // kenar cizgileri (beyaz, surekli)
            const Color ec = fog(L({0.92f, 0.92f, 0.9f}), cL.w);
            for (double sg : {-1.0, 1.0}) {
                const Proj e0 = edgeW(i, sg, 0.12), e1 = edgeW(i, sg, 0.27), e2 = edgeW(j, sg, 0.27), e3 = edgeW(j, sg, 0.12);
                triP(r, e0, e1, e2, ec); triP(r, e0, e2, e3, ec);
            }
        }
        if (night_ && i % 5 == 3 && cL.w < 160.0f) {                // gece: kedi gozu reflektorler (farda parlar)
            const Proj q = edge(i, 0.0);
            if (q.ok) { r.setDepthW(q.w - 0.05f); const float s2 = std::max(1.0f, 0.12f * pxPerM / q.w); r.rect(q.x - s2, q.y - s2 * 0.6f, q.x + s2, q.y, {1.0f, 0.85f, 0.4f, lit}); }
        }
        if (zone == 0 && !mtn && P[i].hw > 4.6) {                     // otoban: celik bariyer (ray + direk)
            for (double sg : {-1.0, 1.0}) {
                if (camBlend_ < 0.5 && sg < 0) continue;
                const double o0 = sg * (P[i].hw + 2.0), o1 = sg * (P[j].hw + 2.0);
                const Proj g0 = pt(i, o0, 0.55), g1 = pt(j, o1, 0.55), g2 = pt(j, o1, 0.80), g3 = pt(i, o0, 0.80), gb = pt(i, o0, 0.0);
                if (!g0.ok || !g1.ok || !g2.ok || !g3.ok) continue;
                const Color rc = fog(L(band ? Color{0.72f, 0.74f, 0.76f} : Color{0.62f, 0.64f, 0.67f}), g0.w);
                triP(r, g0, g1, g2, rc); triP(r, g0, g2, g3, rc);
                if (gb.ok && (i & 1) == 0) {
                    r.setDepthW(g0.w);
                    const float pw = std::max(0.6f, 0.08f * pxPerM / g0.w);
                    r.rect(gb.x - pw, g3.y, gb.x + pw, gb.y, fog(L({0.35f, 0.35f, 0.38f}), g0.w));
                }
            }
        }
        {   // Serit cizgileri: ayni yon seritleri arasi kesik beyaz (4 m cizgi / 6 m bosluk); gidis / gelis ayrimi cift
            // sari surekli, 3+3 ve ustunde beton orta refuj; tek yon yolda ayrim yok
            const RoadPoint &pa = P[i], &pb = P[j];
            auto bnd = [](const RoadPoint& p, bool back, double k) { const double w = 2.0 * p.hw / std::max(1.0, p.lf + p.lb); return back ? p.hw - k * w : -p.hw + k * w; };
            const int nf = (int)std::lround(pa.lf), nb = (int)std::lround(pa.lb);
            auto strip = [&](double oa, double ob, double hwid, Color c) {
                const Proj m0 = edge(i, oa + hwid), m1 = edge(i, oa - hwid), m2 = edge(j, ob + hwid), m3 = edge(j, ob - hwid);
                if (m0.ok && m1.ok && m2.ok && m3.ok) { triP(r, m0, m1, m3, c); triP(r, m0, m3, m2, c); }
            };
            if ((i % 5) < 2) {
                const Color wc = fog(L({0.92f, 0.92f, 0.9f}), cL.w);
                for (int k = 1; k < nf; ++k) strip(bnd(pa, false, k), bnd(pb, false, k), 0.07, wc);
                for (int k = 1; k < nb; ++k) strip(bnd(pa, true, k), bnd(pb, true, k), 0.07, wc);
            }
            if (nb > 0) {
                const double da = bnd(pa, false, pa.lf), db = bnd(pb, false, pb.lf);
                {   // gidis / gelis ayrimi: cift sari surekli (eski beton refuj carpismasizdi, kaldirildi)
                    const Color yc = fog(L({0.95f, 0.8f, 0.2f}), cL.w);
                    strip(da + 0.13, db + 0.13, 0.05, yc); strip(da - 0.13, db - 0.13, 0.05, yc);
                }
            }
        }
        if (zone != 2) {                                           // tunele yaklasirken / cikinca dag yanlarda yavasca yukselir (birden duvar gibi gelmesin)
            constexpr double kRamp = 160.0;
            double d = 1e9;
            for (double k = 2.0; k <= kRamp; k += 4.0) {
                if (zoneAt(P[i].s + k) == 2 || zoneAt(P[i].s - k) == 2) { d = k; break; }
            }
            if (d < kRamp) {
                const double u = 1.0 - d / kRamp, uj = std::clamp(u + (P[j].s - P[i].s) / kRamp * (zoneAt(P[i].s + d) == 2 ? 1.0 : -1.0), 0.0, 1.0);
                const double ease = u * u * (3.0 - 2.0 * u), easeJ = uj * uj * (3.0 - 2.0 * uj);
                const double hI = 11.0 * ease, hJ = 11.0 * easeJ, foot = 55.0;
                const float rk = (float)(0.92 + 0.08 * hashf(i / 3));
                const Color slopeC = fog(L({grass0.r * 0.8f * rk * amb, grass0.g * 0.85f * rk * amb, grass0.b * 0.75f * rk * amb}), cL.w);
                for (double sg : {-1.0, 1.0}) {
                    if (camBlend_ < 0.5 && sg < 0) continue;
                    const double wi = P[i].hw + 2.5, wj = P[j].hw + 2.5;
                    const Proj e0 = pt(i, sg * wi, 0.0), e1 = pt(j, sg * wj, 0.0);
                    const Proj t0 = pt(i, sg * (wi + 6.0), hI), t1 = pt(j, sg * (wj + 6.0), hJ);
                    const Proj f0 = pt(i, sg * (wi + foot), 0.0), f1 = pt(j, sg * (wj + foot), 0.0);
                    if (e0.ok && e1.ok && t0.ok && t1.ok) { triP(r, e0, e1, t1, slopeC); triP(r, e0, t1, t0, slopeC); }
                    if (t0.ok && t1.ok && f0.ok && f1.ok) { triP(r, t0, t1, f1, slopeC); triP(r, t0, f1, f0, slopeC); }
                }
            }
        }
        if (zone == 2) {                                           // tunel: duvar + tavan + tavan lambalari
            const float bt = band ? 1.0f : 0.88f;                           // bant golgesi: derinlik hissi
            const Color ceil = fog(L({0.25f * bt, 0.24f * bt, 0.23f * bt}), cL.w);
            for (double sg : {-1.0, 1.0}) {
                if (camBlend_ < 0.5 && sg < 0) continue;                  // yandan gorunum: kamera tarafi duvar arabayi kapatmasin
                const float sd = (sg > 0 ? 0.9f : 1.0f) * bt;
                const Color wall = fog(L({0.46f * sd, 0.44f * sd, 0.40f * sd}), cL.w);
                const Proj w0 = pt(i, sg * (P[i].hw + 1.2), 0), w1 = pt(j, sg * (P[j].hw + 1.2), 0), w2 = pt(j, sg * (P[j].hw + 1.2), 6.0), w3 = pt(i, sg * (P[i].hw + 1.2), 6.0);
                if (w0.ok && w1.ok && w2.ok && w3.ok) { triP(r, w0, w1, w2, wall); triP(r, w0, w2, w3, wall); }
                const Proj k2 = pt(j, sg * (P[j].hw + 1.2), 0.9), k3 = pt(i, sg * (P[i].hw + 1.2), 0.9);   // alt kusak (sari-siyah)
                const Color kc = fog(L(((i / 2) & 1) ? Color{0.85f, 0.7f, 0.15f} : Color{0.12f, 0.12f, 0.12f}), cL.w);
                if (w0.ok && w1.ok && k2.ok && k3.ok) { triP(r, w0, w1, k2, kc); triP(r, w0, k2, k3, kc); }
            }
            const Proj c0 = pt(i, P[i].hw + 1.2, 6.0), c1 = pt(i, -P[i].hw - 1.2, 6.0), c2 = pt(j, -P[j].hw - 1.2, 6.0), c3 = pt(j, P[j].hw + 1.2, 6.0);
            const Color ceilA{ceil.r, ceil.g, ceil.b, camBlend_ < 0.5 ? 0.25f : 1.0f};   // yandan: tavan saydam
            if (c0.ok && c1.ok && c2.ok && c3.ok) { triP(r, c0, c1, c2, ceilA); triP(r, c0, c2, c3, ceilA); }
            {   // Tunel bir dagin icinden gecer: tavanin ustunde sirt, iki yana cimenli yamac; giris / cikista kaya yuzu (agiz)
                const double ridge = 20.0 + 6.0 * std::sin(P[i].s * 0.004), foot = 55.0;
                const double ridgeJ = 20.0 + 6.0 * std::sin(P[j].s * 0.004);
                const float rk = (float)(0.92 + 0.08 * hashf(i / 3));
                const Color slopeC = fog(L({grass0.r * 0.8f * rk * amb, grass0.g * 0.85f * rk * amb, grass0.b * 0.75f * rk * amb}), cL.w);
                const Color rockC = fog(L({0.42f * rk, 0.39f * rk, 0.34f * rk}), cL.w);
                for (double sg : {-1.0, 1.0}) {
                    if (camBlend_ < 0.5 && sg < 0) continue;                  // yandan: kamera tarafi yamac arabayi kapatmasin
                    const double wi = P[i].hw + 1.2, wj = P[j].hw + 1.2;
                    // ust yamac: tavan kenari (6 m) -> sirt (orta, yuksek); dis yamac: tavan kenari -> zemin (foot)
                    const Proj a0 = pt(i, sg * wi, 6.0), a1 = pt(j, sg * wj, 6.0), r0 = pt(i, 0.0, ridge), r1 = pt(j, 0.0, ridgeJ);
                    if (a0.ok && a1.ok && r0.ok && r1.ok) { triP(r, a0, a1, r1, slopeC); triP(r, a0, r1, r0, slopeC); }
                    const Proj f0 = pt(i, sg * (wi + foot), 0.0), f1 = pt(j, sg * (wj + foot), 0.0);
                    const Proj m0 = pt(i, sg * (wi + foot * 0.35), ridge * 0.55), m1 = pt(j, sg * (wj + foot * 0.35), ridgeJ * 0.55);
                    if (a0.ok && a1.ok && m0.ok && m1.ok) { triP(r, a0, a1, m1, slopeC); triP(r, a0, m1, m0, slopeC); }
                    if (m0.ok && m1.ok && f0.ok && f1.ok) { triP(r, m0, m1, f1, slopeC); triP(r, m0, f1, f0, slopeC); }
                    if (m0.ok && m1.ok && r0.ok && r1.ok) { triP(r, m0, m1, r1, slopeC); triP(r, m0, r1, r0, slopeC); }
                }
                // agiz: bu segment tunelin ilk / son segmentiyse kaya yuzu (tunel deligi disinda kalan kisim)
                const bool entry = zoneAt(P[std::max(0, i - 1)].s) != 2, exitS = zoneAt(P[std::min(n - 1, j + 1)].s) != 2;
                if (entry || exitS) {
                    const int e = entry ? i : j;
                    const double w = P[e].hw + 1.2, rg = entry ? ridge : ridgeJ;
                    auto face = [&](double y0, double z0, double y1, double z1, double y2, double z2, double y3, double z3) {
                        const Proj q0 = pt(e, y0, z0), q1 = pt(e, y1, z1), q2 = pt(e, y2, z2), q3 = pt(e, y3, z3);
                        if (q0.ok && q1.ok && q2.ok && q3.ok) { triP(r, q0, q1, q2, rockC); triP(r, q0, q2, q3, rockC); }
                    };
                    face(-w, 6.0, w, 6.0, w * 0.4, rg * 0.95, -w * 0.4, rg * 0.95);            // agzin ustu
                    for (double sg : {-1.0, 1.0}) {
                        if (camBlend_ < 0.5 && sg < 0) continue;
                        face(sg * w, 0.0, sg * (w + foot * 0.35), 0.0, sg * (w + foot * 0.35), rg * 0.55, sg * w, 6.0);   // agiz yani
                        face(sg * w, 6.0, sg * (w + foot * 0.35), rg * 0.55, sg * w * 0.4, rg * 0.95, sg * w * 0.4, rg * 0.95);
                    }
                    const Proj b0 = pt(e, -w, 6.2), b1 = pt(e, w, 6.2), b2 = pt(e, w, 6.9), b3 = pt(e, -w, 6.9);   // beton alin bandi
                    if (b0.ok && b1.ok && b2.ok && b3.ok) { const Color bc = fog(L({0.62f, 0.61f, 0.58f}), cL.w); triP(r, b0, b1, b2, bc); triP(r, b0, b2, b3, bc); }
                }
            }
            if (i % 5 == 0) {
                const Proj l0 = pt(i, 0.6, 5.9), l1 = pt(i, -0.6, 5.9), l2 = pt(j, -0.6, 5.9), l3 = pt(j, 0.6, 5.9);
                const Color lc = fog({1.0f, 0.85f, 0.55f}, l0.w);
                if (l0.ok && l1.ok && l2.ok && l3.ok) { triP(r, l0, l1, l2, lc); triP(r, l0, l2, l3, lc); }
            }
            continue;
        }
        const auto stList = ses_->stations();
        for (size_t sti = 0; sti < stList.size(); ++sti) {           // benzinlik: sag tarafta beton saha + sacak + pompa + tabela
            const double st = stList[sti];
            if (!(P[i].s <= st + 0.5 * RoadSession::kStationLen && P[j].s > st + 0.5 * RoadSession::kStationLen)) continue;
            const bool dead = ses_->stationDestroyed((int)sti);
            const int ia = std::max(0, i - (int)(0.5 * RoadSession::kStationLen / RoadPath::kStep)), ib = std::min(n - 1, i + (int)(0.5 * RoadSession::kStationLen / RoadPath::kStep));
            const double o0 = -(P[i].hw + 0.6), o1 = -(P[i].hw + 13.0);
            const Proj c0 = pt(ia, o0, 0.02), c1 = pt(ib, o0, 0.02), c2 = pt(ib, o1, 0.02), c3 = pt(ia, o1, 0.02);
            if (c0.ok && c1.ok && c2.ok && c3.ok) { const Color cc = fog(L({0.62f, 0.62f, 0.60f}), c0.w); triP(r, c0, c1, c2, cc); triP(r, c0, c2, c3, cc); }
            const int ka = ia + (ib - ia) / 4, kb = ib - (ib - ia) / 4;          // sacak (5 m yukseklikte) + direkler
            const double so0 = -(P[i].hw + 3.0), so1 = -(P[i].hw + 10.0);
            const Proj r0 = pt(ka, so0, 5.0), r1 = pt(kb, so0, 5.0), r2 = pt(kb, so1, 5.0), r3 = pt(ka, so1, 5.0);
            const Proj r0b = pt(ka, so0, 4.4), r1b = pt(kb, so0, 4.4);
            for (int pk = 0; pk < 4; ++pk) {                                      // direkler
                const int pi = pk < 2 ? ka : kb; const double po = (pk % 2) ? so1 : so0;
                const Proj p0 = pt(pi, po, 0.0), p1 = pt(pi, po, 4.4);
                if (p0.ok && p1.ok) { r.setDepthW(p0.w); const float pw = std::max(0.8f, 0.15f * pxPerM / p0.w); r.rect(p0.x - pw, p1.y, p0.x + pw, p0.y, fog(L({0.85f, 0.85f, 0.88f}), p0.w)); }
            }
            for (int pk = 0; pk < 3 && !dead; ++pk) {                             // pompalar (patladiysa yok)
                const int pi = ka + (kb - ka) * (pk + 1) / 4;
                const Proj a0 = pt(pi, -(P[i].hw + 6.5), 0.0), a1 = pt(pi, -(P[i].hw + 6.5), 1.6);
                if (!a0.ok || !a1.ok) continue;
                r.setDepthW(a0.w);
                const float pw = std::max(1.0f, 0.4f * pxPerM / a0.w);
                r.rect(a0.x - pw, a1.y, a0.x + pw, a0.y, fog(L({0.85f, 0.15f, 0.12f}), a0.w));
                r.rect(a0.x - pw * 0.7f, a1.y + (a0.y - a1.y) * 0.15f, a0.x + pw * 0.7f, a1.y + (a0.y - a1.y) * 0.4f, fog(L({0.15f, 0.2f, 0.25f}), a0.w));
            }
            if (r0.ok && r1.ok && r2.ok && r3.ok) {
                const Color roof = dead ? Color{0.08f, 0.07f, 0.07f} : fog(night_ ? Color{0.95f, 0.95f, 0.9f} : L({0.92f, 0.92f, 0.94f}), r0.w),
                            band = dead ? Color{0.15f, 0.08f, 0.05f} : fog(L({0.85f, 0.15f, 0.12f}), r0.w);
                triP(r, r0, r1, r2, roof); triP(r, r0, r2, r3, roof);
                if (r0b.ok && r1b.ok) { triP(r, r0b, r1b, r1, band); triP(r, r0b, r1, r0, band); }   // kirmizi sacak bandi
            }
            if (dead) {                                                           // yanik saha: titreyen alev + duman
                for (int f = 0; f < 3; ++f) {
                    const Proj fp = pt(ka + (kb - ka) * (f + 1) / 4, -(P[i].hw + 6.5), 0.8);
                    if (!fp.ok) continue;
                    r.setDepthW(fp.w - 0.1f);
                    const float fs = pxPerM / fp.w * (1.1f + 0.4f * hashf((int)(envT_ * 20) + f * 7));
                    r.circle(fp.x, fp.y, fs * 1.2f, 12, {1.0f, 0.45f, 0.08f, 0.55f});
                    r.circle(fp.x, fp.y - fs * 0.4f, fs * 0.7f, 10, {1.0f, 0.85f, 0.3f, 0.7f});
                    r.circle(fp.x + fs * 0.3f, fp.y - fs * 2.2f, fs * 1.6f, 12, {0.15f, 0.14f, 0.14f, 0.35f});
                }
            }
            {   // tabela: yuksek direk + BENZIN + fiyat
                const Proj g0 = pt(ia, -(P[i].hw + 1.8), 0.0), g1 = pt(ia, -(P[i].hw + 1.8), 7.0);
                if (g0.ok && g1.ok) {
                    r.setDepthW(g0.w);
                    const float sc = pxPerM / g0.w;
                    r.rect(g0.x - 0.12f * sc, g1.y, g0.x + 0.12f * sc, g0.y, fog(L({0.5f, 0.5f, 0.55f}), g0.w));
                    r.rect(g1.x - 1.4f * sc, g1.y - 1.6f * sc, g1.x + 1.4f * sc, g1.y, fog(L({0.85f, 0.15f, 0.12f}), g0.w));
                    const float ts = std::min(4.0f, std::floor(sc * 0.07f));             // yazi tabelaya sigar (2.8 m)
                    if (ts >= 1.0f) { r.textCentered(g1.x, g1.y - 1.45f * sc, "BENZIN", ts, {1, 1, 1}); r.textCentered(g1.x, g1.y - 0.7f * sc, "42.90", ts, {1.0f, 0.9f, 0.3f}); }
                }
            }
        }
        if (zone == 1 && i % 10 == 0) {                            // sehir: binalar (cephe + uc yuz, pencereler)
            for (int side = -1; side <= 1; side += 2) {
                if (camBlend_ < 0.5 && side < 0) continue;
                const int key = i * 2 + (side > 0);
                const double d = P[i].hw + 5.0 + 3.0 * hashf(key * 5), hgt = 8.0 + 30.0 * hashf(key * 11) * hashf(key * 3 + 1), dep = 14.0;
                const int ie = std::min(i + 9, n - 1);
                // Basi kameranin arkasinda kalan bina: cephe gorunen ilk noktadan baslar (bina birden silinmez)
                int ib = i;
                while (ib < ie && !(pt(ib, side * d, 0).ok && pt(ib, side * d, hgt).ok)) ++ib;
                const Proj f0 = pt(ib, side * d, 0), f1 = pt(ie, side * d, 0), f2 = pt(ie, side * d, hgt), f3 = pt(ib, side * d, hgt);
                const Proj e0 = pt(i, side * (d + dep), 0), e3 = pt(i, side * (d + dep), hgt);
                if (ib >= ie || !f0.ok || !f1.ok || !f2.ok || !f3.ok) continue;
                const bool endFace = ib == i && e0.ok && e3.ok;
                const float hcol = hashf(key * 17);
                const Color base{0.55f + 0.25f * hcol, 0.52f + 0.2f * hashf(key * 19), 0.50f + 0.18f * hashf(key * 23)};
                const Color face = fog(L({base.r * 0.85f, base.g * 0.85f, base.b * 0.85f}), f0.w), endc = fog(L({base.r * 0.62f, base.g * 0.62f, base.b * 0.62f}), f0.w);
                if (endFace) { triP(r, f0, e0, e3, endc); triP(r, f0, e3, f3, endc); }
                triP(r, f0, f1, f2, face); triP(r, f0, f2, f3, face);
                {   // zemin kat dukkan bandi (renkli) + tente, cati korkulugu (acik renk)
                    const Proj s0 = pt(ib, side * d, 3.2), s1 = pt(ie, side * d, 3.2), c0 = pt(ib, side * d, hgt - 0.7), c1 = pt(ie, side * d, hgt - 0.7);
                    if (s0.ok && s1.ok && c0.ok && c1.ok) {
                        static const Color shop[5] = {{0.75f, 0.2f, 0.18f}, {0.15f, 0.42f, 0.7f}, {0.85f, 0.65f, 0.15f}, {0.2f, 0.55f, 0.3f}, {0.55f, 0.25f, 0.55f}};
                        const Color sc2 = fog(L(shop[key % 5]), f0.w), roof = fog(L({base.r * 1.05f, base.g * 1.05f, base.b * 1.05f}), f0.w);
                        triP(r, f0, f1, s1, fog(L({0.12f, 0.13f, 0.16f}), f0.w)); triP(r, f0, s1, s0, fog(L({0.12f, 0.13f, 0.16f}), f0.w));   // vitrin
                        const Proj a0 = pt(ib, side * (d - 1.2), 3.0), a1 = pt(ie, side * (d - 1.2), 3.0);
                        if (a0.ok && a1.ok) { triP(r, s0, s1, a1, sc2); triP(r, s0, a1, a0, sc2); }                                     // tente
                        triP(r, c0, c1, f2, roof); triP(r, c0, f2, f3, roof);                                                          // korkuluk
                        if (night_ && hashf(key * 41) > 0.5f) {                                                                       // neon tabela
                            const Proj n0 = pt(i + 3, side * (d - 0.1), 4.0), n1 = pt(i + 6, side * (d - 0.1), 4.8);
                            if (n0.ok && n1.ok) { r.setDepthW(n0.w - 0.1f); r.rect(std::min(n0.x, n1.x), std::min(n0.y, n1.y), std::max(n0.x, n1.x), std::max(n0.y, n1.y), {shop[(key + 2) % 5].r + 0.3f, shop[(key + 2) % 5].g + 0.3f, shop[(key + 2) % 5].b + 0.3f, 0.9f}); }
                        }
                    }
                }
                r.setDepthW(f0.w - 0.05f);
                if (f0.w < 260.0f) {                                   // pencereler (gece yanik)
                    const int floors = std::max(2, (int)(hgt / 3.5)), cols = 5;
                    for (int fl = 0; fl < floors; ++fl)
                        for (int cl = 0; cl < cols; ++cl) {
                            const float u0 = (cl + 0.25f) / cols, u1 = (cl + 0.75f) / cols;
                            const float vb = 3.6f / (float)hgt, vt = 1.0f - 0.9f / (float)hgt;   // dukkan katinin ustu .. korkulugun alti
                            const float v0 = vb + (vt - vb) * (fl + 0.3f) / floors, v1 = vb + (vt - vb) * (fl + 0.75f) / floors;
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
                    r.setDepthW(p0.w);
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
                    triP(r, k0, k1, k2, rc); triP(r, k0, k2, k3, rc);
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
                    r.setDepthW(g0.w);
                    r.rect(g0.x - 0.1f * sc, b0.y, g0.x + 0.1f * sc, g0.y, fog(L({0.35f, 0.35f, 0.38f}), g0.w));
                    // pano: koyu cerceve + renkli zemin + beyaz bant + yazi
                    const Color fr = fog(L({0.12f, 0.12f, 0.14f}), b0.w);
                    triP(r, b0, b1, b2, fr); triP(r, b0, b2, b3, fr);
                    const Proj i0 = pt(i, side * (P[i].hw + 9.05), 4.3), i1 = pt(std::min(i + 4, n - 1), side * (P[i].hw + 9.05), 4.3);
                    const Proj i2 = pt(std::min(i + 4, n - 1), side * (P[i].hw + 9.05), 7.7), i3 = pt(i, side * (P[i].hw + 9.05), 7.7);
                    if (i0.ok && i1.ok && i2.ok && i3.ok) {
                        triP(r, i0, i1, i2, ac); triP(r, i0, i2, i3, ac);
                        const Proj w0 = pt(i, side * (P[i].hw + 9.1), 5.3), w1 = pt(std::min(i + 4, n - 1), side * (P[i].hw + 9.1), 5.3);
                        const Proj w2 = pt(std::min(i + 4, n - 1), side * (P[i].hw + 9.1), 6.7), w3 = pt(i, side * (P[i].hw + 9.1), 6.7);
                        const Color wc = fog(L({0.95f, 0.95f, 0.93f}), b0.w);
                        if (w0.ok && w1.ok && w2.ok && w3.ok) {
                            triP(r, w0, w1, w2, wc); triP(r, w0, w2, w3, wc);
                            static const char* const kAd[4] = {"TURBO LASTIK", "OTOBAN PETROL", "SANAYI YAG", "GECE RADYO"};
                            const float tx = 0.25f * (w0.x + w1.x + w2.x + w3.x), ty = 0.25f * (w0.y + w1.y + w2.y + w3.y);
                            const float ts = std::floor(std::clamp(std::fabs(w1.x - w0.x) / 70.0f, 0.0f, 4.0f));
                            if (ts >= 1.0f) { r.setDepthW(w0.w - 0.05f); r.textCentered(tx, ty - 3.5f * ts, kAd[i % 4], ts, {0.1f, 0.1f, 0.12f}); }
                        }
                    }
                }
            }
        }
        if (zone == 0 && i % 3 == 1 && hashf(i * 17) > 0.55f) {     // calilar / kayalar (yol kenari, yakin)
            for (int side = -1; side <= 1; side += 2) {
                if (camBlend_ < 0.5 && side < 0) continue;
                const Proj b = edge(i, side * (P[i].hw + 3.2 + 4.0 * hashf(i * 23 + side)));
                if (!b.ok || b.w > 140.0f) continue;
                r.setDepthW(b.w);
                const float sc = pxPerM / b.w, s0 = (0.5f + 0.5f * hashf(i * 29 + side)) * sc;
                if (mtn) { r.tri(b.x - s0, b.y, b.x + s0, b.y, b.x + s0 * 0.2f, b.y - s0 * 0.9f, fog(L({0.45f, 0.42f, 0.38f}), b.w)); }
                else {
                    r.circle(b.x - s0 * 0.4f, b.y - s0 * 0.35f, s0 * 0.55f, 10, fog(L({0.14f, 0.36f, 0.15f}), b.w));
                    r.circle(b.x + s0 * 0.3f, b.y - s0 * 0.45f, s0 * 0.6f, 10, fog(L({0.2f, 0.46f, 0.2f}), b.w));
                }
            }
        }
        if (zone == 0 && aL.w < 60.0f && camBlend_ >= 0.5) {          // cim obekleri / cicek / tas (yakin; yandan gorunumde iri parca olur)
            for (int sd = -1; sd <= 1; sd += 2) {
                if (camBlend_ < 0.5 && sd < 0) continue;
                for (int k = 0; k < 3; ++k) {
                    const float h = hashf(i * 13 + k * 7 + (sd > 0) * 57);
                    const Proj b = pt(i, sd * (P[i].hw + 2.8 + 9.0 * hashf(i * 29 + k)), 0.0);
                    if (!b.ok) continue;
                    r.setDepthW(b.w);
                    const float sc = pxPerM / b.w;
                    if (h < 0.55f) {                                     // cim obegi (3 yaprak)
                        const Color gb = fog(L({grass0.r * 0.75f * amb, grass0.g * 0.85f * amb, grass0.b * 0.7f * amb}), b.w);
                        for (int bl = -1; bl <= 1; ++bl) r.tri(b.x + bl * 0.08f * sc, b.y, b.x + bl * 0.08f * sc + 0.05f * sc, b.y, b.x + bl * 0.16f * sc, b.y - (0.28f + 0.1f * h) * sc, gb);
                    } else if (h < 0.75f) {                              // cicek
                        const Color fc = h < 0.65f ? Color{0.95f, 0.85f, 0.2f} : Color{0.95f, 0.95f, 0.95f};
                        r.rect(b.x - 0.01f * sc, b.y - 0.22f * sc, b.x + 0.01f * sc, b.y, fog(L({0.2f, 0.45f, 0.18f}), b.w));
                        r.circle(b.x, b.y - 0.24f * sc, std::max(0.8f, 0.05f * sc), 6, fog(L(fc), b.w));
                    } else if (h > 0.9f) {                               // tas
                        r.circle(b.x, b.y - 0.08f * sc, std::max(1.0f, 0.18f * sc), 8, fog(L({0.48f, 0.47f, 0.45f}), b.w));
                        r.circle(b.x - 0.05f * sc, b.y - 0.12f * sc, std::max(0.6f, 0.09f * sc), 6, fog(L({0.62f, 0.61f, 0.58f}), b.w));
                    }
                }
            }
        }
        if (zone == 0 && i % 25 == 0 && !(P[i].hw > 4.6 && !mtn)) {    // delinator direkleri (beyaz, siyah bant, reflektor) her 50 m
            for (int sd = -1; sd <= 1; sd += 2) {
                if (camBlend_ < 0.5 && sd < 0) continue;
                const Proj b = pt(i, sd * (P[i].hw + 1.5), 0.0), t = pt(i, sd * (P[i].hw + 1.5), 1.0);
                if (!b.ok || !t.ok) continue;
                r.setDepthW(b.w);
                const float sc = pxPerM / b.w, w2 = std::max(0.6f, 0.06f * sc);
                r.rect(b.x - w2, t.y, b.x + w2, b.y, fog(L({0.92f, 0.92f, 0.9f}), b.w));
                r.rect(b.x - w2, t.y + 0.12f * sc, b.x + w2, t.y + 0.28f * sc, fog(L({0.08f, 0.08f, 0.08f}), b.w));
                r.rect(b.x - w2 * 0.6f, t.y + 0.15f * sc, b.x + w2 * 0.6f, t.y + 0.24f * sc, night_ ? Color{1.0f, 0.8f, 0.3f} : fog(L({0.95f, 0.6f, 0.15f}), b.w));
            }
        }
        if (zone == 0 && !mtn && P[i].hw <= 4.6 && i % 2 == 0 && aL.w < 110.0f) {   // kirsal ahsap cit (direk + iki ray)
            for (int sd = -1; sd <= 1; sd += 2) {
                if (camBlend_ < 0.5 && sd < 0) continue;
                if (hashf((i / 40) * 3 + (sd > 0)) < 0.5f) continue;      // citli / citsiz kesimler
                const double off = sd * (P[i].hw + 6.0);
                const Proj b0 = pt(i, off, 0.0), t0 = pt(i, off, 1.1), r0 = pt(i, off, 0.9), r1 = pt(i + 2, off, 0.9), q0 = pt(i, off, 0.5), q1 = pt(i + 2, off, 0.5);
                if (!b0.ok || !t0.ok) continue;
                r.setDepthW(b0.w);
                const float sc = pxPerM / b0.w;
                const Color wc = fog(L({0.42f, 0.30f, 0.18f}), b0.w);
                r.rect(b0.x - std::max(0.5f, 0.05f * sc), t0.y, b0.x + std::max(0.5f, 0.05f * sc), b0.y, wc);
                const float rt = std::max(0.5f, 0.03f * sc);
                if (r0.ok && r1.ok) { r.tri(r0.x, r0.y - rt, r1.x, r1.y - rt, r1.x, r1.y + rt, wc); r.tri(r0.x, r0.y - rt, r1.x, r1.y + rt, r0.x, r0.y + rt, wc); }
                if (q0.ok && q1.ok) { r.tri(q0.x, q0.y - rt, q1.x, q1.y - rt, q1.x, q1.y + rt, wc); r.tri(q0.x, q0.y - rt, q1.x, q1.y + rt, q0.x, q0.y + rt, wc); }
            }
        }
        if (zone == 0 && i % 8 == 4 && !mtn) {                        // calilar (agaclarin arasinda)
            for (int sd = -1; sd <= 1; sd += 2) {
                if (camBlend_ < 0.5 && sd < 0) continue;
                const float h = hashf(i * 3 + (sd > 0) * 7 + 1);
                if (h < 0.4f) continue;
                const Proj b = edge(i, sd * (P[i].hw + 4.0 + 10.0 * hashf(i * 11 + sd)));
                if (!b.ok) continue;
                r.setDepthW(b.w);
                const float sc = pxPerM / b.w, bw = (0.8f + 0.6f * h) * sc;
                r.circle(b.x, b.y - bw * 0.4f, bw * 0.75f, 10, fog(L({0.10f, 0.30f, 0.12f}), b.w));
                r.circle(b.x - bw * 0.35f, b.y - bw * 0.55f, bw * 0.5f, 10, fog(L({0.16f, 0.40f, 0.17f}), b.w));
                r.circle(b.x + bw * 0.3f, b.y - bw * 0.5f, bw * 0.45f, 10, fog(L({0.13f, 0.36f, 0.15f}), b.w));
            }
        }
        if (zone == 0 && i % 4 == 0) {                             // kenar agaclari / direkleri (her 8 m)
            for (int side = -1; side <= 1; side += 2) {
                if (camBlend_ < 0.5 && side < 0) continue;            // drag gorunumu: kamera tarafindaki agaclar gorusu kapatir
                const float h = hashf(i * 2 + (side > 0));
                if (h < (mtn ? 0.08f : 0.35f)) continue;
                const bool far = (i / 4) % 2 == 1;                         // her ikinci sira uzak (orman derinligi)
                const double latT = side * (P[i].hw + (far ? 28.0 + 26.0 * hashf(i * 5 + side) : 5.0 + 20.0 * hashf(i * 7 + side)));
                const Proj b = edge(i, latT);
                if (!b.ok) continue;
                auto groundShadow = [&](double rM) {                       // yere yatik golge (dunya uzayinda; ekran dairesi derinlikte kama birakiyordu)
                    const RoadPoint& q = P[i];
                    const double sx = std::cos(q.heading), sy = std::sin(q.heading), lx = -std::sin(q.heading), ly = std::cos(q.heading);
                    Proj ring[8];
                    for (int k = 0; k < 8; ++k) {
                        const double a = k * 3.14159265358979 / 4.0, u = rM * 1.3 * std::cos(a) + 0.6, v = rM * std::sin(a);
                        ring[k] = project(vp, q.x + latT * lx + u * sx + v * lx, q.y + latT * ly + u * sy + v * ly, q.z + 0.03, W, H);
                    }
                    const Proj c = project(vp, q.x + latT * lx + 0.6 * sx, q.y + latT * ly + 0.6 * sy, q.z + 0.03, W, H);
                    for (int k = 0; k < 8; ++k) if (c.ok && ring[k].ok && ring[(k + 1) % 8].ok) triP(r, c, ring[k], ring[(k + 1) % 8], {0.0f, 0.0f, 0.0f, 0.16f});
                };
                r.setDepthW(b.w);
                const float sc = pxPerM / b.w;
                const float th = (5.0f + 4.0f * h) * sc, tw = (1.6f + h) * sc;
                if (h > 0.8f) {                                      // elektrik diregi + travers
                    const Color pc = fog(L({0.35f, 0.28f, 0.2f}), b.w);
                    r.rect(b.x - 0.12f * sc, b.y - 7.5f * sc, b.x + 0.12f * sc, b.y, pc);
                    r.rect(b.x - 0.9f * sc, b.y - 7.2f * sc, b.x + 0.9f * sc, b.y - 7.0f * sc, pc);
                } else if (!mtn && hashf(i * 5 + side * 3) > 0.5f) {  // yuvarlak yaprakli agac: govde + 3 kat tac (golge / isik)
                    const Color c0 = fog(L({0.10f, 0.30f + 0.08f * h, 0.12f}), b.w), c1 = fog(L({0.16f, 0.42f + 0.1f * h, 0.17f}), b.w),
                                c2 = fog(L({0.26f, 0.55f + 0.1f * h, 0.24f}), b.w);
                    groundShadow(1.6 + h);
                    r.rect(b.x - tw * 0.11f, b.y - th * 0.45f, b.x + tw * 0.11f, b.y, fog(L({0.38f, 0.26f, 0.15f}), b.w));
                    r.circle(b.x, b.y - th * 0.62f, tw * 0.95f, 14, c0);
                    r.circle(b.x - tw * 0.18f, b.y - th * 0.70f, tw * 0.72f, 14, c1);
                    r.circle(b.x - tw * 0.32f, b.y - th * 0.80f, tw * 0.38f, 12, c2);
                } else {                                             // cam agaci: golge + govde + uc kat yaprak (isikli / golgeli yari)
                    const Color leaf = fog(L({0.12f, 0.38f + 0.1f * h, 0.16f}), b.w), leafD = fog(L({0.08f, 0.28f + 0.08f * h, 0.12f}), b.w);
                    groundShadow(1.4 + h);
                    r.rect(b.x - tw * 0.12f, b.y - th * 0.35f, b.x + tw * 0.12f, b.y, fog(L({0.35f, 0.24f, 0.14f}), b.w));
                    for (int tier = 0; tier < 3; ++tier) {
                        const float yb = th * (0.22f + 0.24f * tier), yt = th * (0.62f + 0.19f * tier), ww = tw * (1.05f - 0.25f * tier);
                        r.tri(b.x - ww, b.y - yb, b.x, b.y - yb, b.x, b.y - yt, leafD);
                        r.tri(b.x, b.y - yb, b.x + ww, b.y - yb, b.x, b.y - yt, leaf);
                    }
                }
            }
        }
    }
    if (ses_->hasRival() || ses_->mode() == RoadSession::Mode::Marathon) {   // bitis cizgisi
        const double fs = ses_->startS() + ses_->raceLength();
        if (fs > ps - 60 && fs < ps + 1400) {
            const RoadPoint q = R.at(fs), q2 = R.at(fs + 1.5);
            for (int k = 0; k < 10; ++k) {
                const double o0 = -hw + k * (2 * hw / 10), o1 = o0 + 2 * hw / 10;
                const Proj a = project(vp, q.x - o0 * std::sin(q.heading), q.y + o0 * std::cos(q.heading), q.z + 0.02, W, H);
                const Proj bq = project(vp, q.x - o1 * std::sin(q.heading), q.y + o1 * std::cos(q.heading), q.z + 0.02, W, H);
                const Proj c2 = project(vp, q2.x - o1 * std::sin(q2.heading), q2.y + o1 * std::cos(q2.heading), q2.z + 0.02, W, H);
                const Proj d2 = project(vp, q2.x - o0 * std::sin(q2.heading), q2.y + o0 * std::cos(q2.heading), q2.z + 0.02, W, H);
                if (!a.ok || !bq.ok || !c2.ok || !d2.ok) continue;
                const Color cc = (k & 1) ? Color{1, 1, 1} : Color{0.05f, 0.05f, 0.05f};
                triP(r, a, bq, c2, cc); triP(r, a, c2, d2, cc);
            }
        }
    }
    if (ses_->mode() == RoadSession::Mode::Karma || ses_->mode() == RoadSession::Mode::Marathon) {
        // Viraj yaklasim levhalari: 300/200/100 m (beyaz, 3/2/1 kirmizi serit) ve viraj girisinde yon levhasi (sari ok).
        // Levhalar sagda; yon levhasi virajin dis tarafinda.
        auto post = [&](double sb, double off, double h0, double h1, Proj& top, Proj& bot) {
            const RoadPoint p = R.at(sb);
            const double bx = p.x - off * std::sin(p.heading), by = p.y + off * std::cos(p.heading);
            const Proj base = project(vp, bx, by, p.z, W, H);
            top = project(vp, bx, by, p.z + h1, W, H);
            bot = project(vp, bx, by, p.z + h0, W, H);
            if (!base.ok || !top.ok || !bot.ok) return 0.0f;
            r.setDepthW(base.w);
            const float sc = pxPerM / base.w;
            r.rect(base.x - 0.07f * sc, top.y, base.x + 0.07f * sc, base.y, {0.55f, 0.55f, 0.58f});
            return sc;
        };
        for (const RoadSection& q : R.sections()) {
            for (int k = 3; k >= 1; --k) {
                const double sb = q.entry - 100.0 * k;
                if (sb < ps - 60 || sb > ps + 1400) continue;
                Proj t, m;
                if (camBlend_ < 0.5) continue;                             // yandan: kamera tarafinda, arabayi kapatir
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
            if (se > ps - 60 && se < ps + 1400) {
                const double kk = R.at(q.entry + 40.0).curvature;         // sola donus (+) -> levha sagda, ok sola
                Proj t, m;
                const float sc = (camBlend_ < 0.5 && kk > 0) ? 0.0f : post(se, kk > 0 ? -(hw + 3.0) : (hw + 3.0), 1.0, 2.4, t, m);
                if (sc > 0) {
                    const float hw2 = 1.0f * sc;
                    r.rect(t.x - hw2, t.y, t.x + hw2, m.y, {0.98f, 0.8f, 0.1f});
                    const float ts = std::clamp((m.y - t.y) / 9.0f, 1.0f, 8.0f);
                    r.textCentered(t.x, (t.y + m.y) * 0.5f - 3.5f * ts, kk > 0 ? "<<<" : ">>>", ts, {0.08f, 0.08f, 0.08f});
                }
            }
        }
    }
    r.setDepthW(1e9f);                                                 // efektler araclari ortmez
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
        if (t.s < ps - 60 || t.s > ps + 1400) continue;
        const RoadPoint q = R.at(t.s);
        Obj o{}; ses_->trafficPose(t, o.x, o.y, o.psi); o.id = t.carId;
        o.z = q.z; o.pitch = t.oncoming ? -q.grade : q.grade;
        o.spin = (float)(envT_ * std::min(t.v / 0.31, 33.0) * (t.oncoming ? -1.0 : 1.0));   // en fazla ~0.55 rad / kare
        o.d = (o.x - ex) * dx + (o.y - ey) * dy; objs.push_back(o);   // kamera bakis yonunde derinlik
    }
    if (ses_->mode() == RoadSession::Mode::Marathon)                   // The Run alani (gorus mesafesindekiler)
        for (const Runner& Rn : ses_->runField().runners()) {
            if (Rn.s < ps - 60 || Rn.s > ps + 1400) continue;
            const RoadPoint q = R.at(Rn.s);
            Obj o{}; o.x = q.x - Rn.lane * std::sin(q.heading); o.y = q.y + Rn.lane * std::cos(q.heading); o.psi = q.heading;
            o.id = Rn.carId; o.z = q.z; o.pitch = q.grade; o.spin = (float)Rn.spin; o.steer = (float)Rn.steer;
            o.d = (o.x - ex) * dx + (o.y - ey) * dy; objs.push_back(o);
        }
    if (RoadCar* rv = ses_->rival()) {
        const VehicleSim& rs = rv->sim();
        const double rh = R.at(rv->s()).heading, dpsi = std::remainder(rs.heading() - rh, 2.0 * 3.14159265358979);
        Obj o{0, rs.posX(), rs.posY(), rh + dpsi * camBlend_, rv->elevation() + rs.suspension().heave(), rs.grade(), ses_->rivalCarId(),
              (float)spinR_, (float)(camBlend_ * std::clamp(std::atan(rs.yawRate() * rs.vehicleLoad().wheelbase / std::max(rs.speed(), 3.0)), -0.5, 0.5))};
        o.d = (o.x - ex) * dx + (o.y - ey) * dy;
        o.police = ses_->mode() == RoadSession::Mode::Chase;
        if (o.d > 2 && o.d < 1400) objs.push_back(o);
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
                r.setDepthW(1e9f);
                r.circle(q.x, q.y, 0.16f * k, 8, on ? c : Color{c.r * 0.3f, c.g * 0.3f, c.b * 0.3f});
                if (on) r.circle(q.x, q.y, (night_ ? 1.6f : 0.7f) * k, 14, {c.r, c.g, c.b, night_ ? 0.22f : 0.15f});
            }
        }
    }
    {
        Renderer::CarLook lk = app_.career.car().carId == carId_ ? lookOf(app_.career.car()) : lookOf(&tune_);
        lk.pitch = (float)(sim.suspension().pitchDeg() / 57.2958 * 1.4);   // gorunur (hafif abartili) dalma / yatma
        lk.roll = (float)(sim.suspension().rollDeg() / 57.2958 * 1.4);
        r.setCarLook(lk);
    }
    {
        const double rh = R.at(ps).heading, dpsi = std::remainder(sim.heading() - rh, 2.0 * 3.14159265358979);
        r.drawCar(carId_, 0, 0, W, H, proj, view, carModel(X, Y, zCar + sim.suspension().heave(), rh + dpsi * camBlend_, sim.grade()),
                  (float)spinP_, (float)(steer_ * camBlend_));                 // 2B drag gorunumu: yola paralel, teker duz
    }
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
    r.endWorldDepth();
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
    const bool careerCar = app_.career.car().carId == carId_;
    const int gstyle = careerCar ? app_.career.car().gauge : 0;           // satin alinan kadran
    if (!gstyle) {
    std::snprintf(b, sizeof b, "%3.0f", sim.speed() * app_.settings.speedFactor());
    r.text(x0, 4, b, 4, {1, 1, 1});
    r.text(x0 + 74, 22, app_.settings.speedUnit(), 1, {0.7f, 0.7f, 0.75f});
    }
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
    if (gstyle) {                                                        // kadran: yatayda alt orta, dikeyde yol goruntusunun alti
        GaugeData gd;
        gd.vtec = pt.vtecActive(); gd.rpm = (float)pt.rpm(); gd.redline = (float)sim.engineSpec().redlineRpm; gd.shiftRpm = (float)sim.shiftRpm();
        gd.speed = (float)(sim.speed() * app_.settings.speedFactor()); gd.speedMax = app_.settings.speedFactor() > 3.0 ? 260.0f : 160.0f;
        gd.unit = app_.settings.speedUnit(); gd.gear = gs; gd.gearCol = Pc.grinding() ? Color{1.0f, 0.2f, 0.15f} : gc; gd.t = envT_;
        boostShown_ += ((float)sim.boostNow() - boostShown_) * 0.15f;     // ibre gecikmesi
        gd.boost = boostShown_; gd.boostMax = (float)sim.boostMax();
        if (land_) drawGaugeCluster(r, 142, 250, 398, 356, gstyle, app_.career.car().boostGauge, gd);
        else drawGaugeCluster(r, 40, 352, 320, 436, gstyle, app_.career.car().boostGauge, gd);
    } else {
    r.text(x0 + 106, 4, gs, 4, Pc.grinding() ? Color{1.0f, 0.2f, 0.15f} : gc);
    const float red = (float)sim.engineSpec().redlineRpm, fill = std::clamp((float)pt.rpm() / (red * 1.05f), 0.0f, 1.0f);
    const float rx = x0 + 136, rw = land_ ? 94.0f : 100.0f;
    r.rect(rx, 6, rx + rw, 16, {0.15f, 0.15f, 0.18f});
    r.rect(rx, 6, rx + rw * fill, 16, pt.rpm() > red * 0.9 ? Color{0.95f, 0.2f, 0.3f} : Color{0.2f, 0.85f, 0.3f});
    std::snprintf(b, sizeof b, "%5.0f RPM", pt.rpm());
    r.text(rx, 22, b, 1, {1, 1, 1});
    if (pt.vtecActive()) r.text(rx + r.textWidth(b, 1) + 6, 22, "VTEC", 1, {1.0f, 0.15f, 0.12f});
    }
    if (adasLevel_ > 0) {
        static const char* n[4] = {"YARDIM", "HIZ SAB.", "SERIT", "OTONOM"};
        char ab[32]; std::snprintf(ab, sizeof ab, adasMode_ ? "%s %.0f" : "%s", n[adasMode_], ccSpeed_ * 3.6);
        button(r, adasBtn_, ab, adasMode_ ? Color{0.12f, 0.30f, 0.50f, 0.85f} : Color{0.25f, 0.25f, 0.28f, 0.85f}, 1);
    }
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
    } else if (ses_->hasRival() || ses_->mode() == RoadSession::Mode::Marathon) {
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
        if (ses_->mode() == RoadSession::Mode::Marathon) {               // yakit + benzinlik + verim
            const double fl = sim.fuelLiters(), tank = sim.tankLiters(), ns = ses_->nextStation(Pc.s());
            {   // Anlik / ortalama tuketim (gercek L/100 km: surulen m x sikistirma); ECO isigi
                const double ds = Pc.s() - lastRunS_, df = lastFuel_ - fl;
                if (ds > 0.05 && df >= 0.0) l100_ += (df / (ds * ses_->compression()) * 1e5 - l100_) * 0.05;
                if (df > 0.0) usedRun_ += df;
                lastRunS_ = Pc.s(); lastFuel_ = fl;
                const double drivenReal = std::max(0.1, (Pc.s() - ses_->startS()) * ses_->compression() / 1000.0);
                const bool eco = l100_ < 14.0 && sim.speed() > 15.0;
                std::snprintf(b, sizeof b, "%.0f L/100  ORT %.0f", std::min(l100_, 99.0), usedRun_ / drivenReal * 100.0);
                r.text(x0, infoY + 25, b, 1, eco ? Color{0.3f, 1.0f, 0.4f} : l100_ > 35.0 ? Color{1.0f, 0.45f, 0.3f} : Color{0.85f, 0.85f, 0.9f});
                if (eco) { r.rect(x0 + r.textWidth(b, 1) + 6, infoY + 24, x0 + r.textWidth(b, 1) + 32, infoY + 34, {0.15f, 0.6f, 0.25f}); r.text(x0 + r.textWidth(b, 1) + 9, infoY + 26, "ECO", 1, {1, 1, 1}); }
            }
            const float bx = x0, by = infoY + 13, bw = 120;
            const bool low = fl < 1.5;
            r.rect(bx, by, bx + bw, by + 8, {0.12f, 0.12f, 0.14f, 0.9f});
            r.rect(bx, by, bx + bw * (float)std::clamp(fl / tank, 0.0, 1.0), by + 8, low && std::fmod(envT_, 0.5) < 0.25 ? Color{1.0f, 0.2f, 0.15f} : Color{1.0f, 0.75f, 0.15f});
            if (ses_->refueling(0)) std::snprintf(b, sizeof b, "YAKIT %.1f L  DOLDURULUYOR...", fl);
            else if (ns >= 0) std::snprintf(b, sizeof b, "YAKIT %.1f L  BENZINLIK %.1f KM", fl, ns / 1000.0);
            else std::snprintf(b, sizeof b, "YAKIT %.1f L  SON BENZINLIK GECTI", fl);
            r.text(bx + bw + 6, by, b, 1, low ? Color{1.0f, 0.45f, 0.3f} : Color{1.0f, 0.85f, 0.4f});
            if (ses_->inStation(Pc.s()) && !ses_->refueling(0) && fl < tank - 0.05)
                r.textCentered(W / 2.0f, infoY + 32, "SAG SERITTE DUR: YAKIT AL", 2, {1.0f, 0.85f, 0.3f});
        }
        const double left = std::max(0.0, ses_->startS() + ses_->raceLength() - Pc.s());
        if (ses_->mode() == RoadSession::Mode::Marathon) {
            std::snprintf(b, sizeof b, "SIRA %d / %d   KALAN %.0f KM   %d:%02d", ses_->runPosition(), ses_->runCount(),
                          left * ses_->compression() / 1000.0, (int)ses_->raceTime() / 60, (int)ses_->raceTime() % 60);
            r.text(x0, infoY, b, 1, ses_->runPosition() <= 3 ? Color{0.4f, 1.0f, 0.5f} : Color{1.0f, 0.85f, 0.4f});
        } else {
        const double gap = ses_->gapMeters();
        std::snprintf(b, sizeof b, "%s  KALAN %.2f KM  %+.0f M   %.1f S", gap >= 0 ? "1." : "2.", left / 1000.0, gap, ses_->raceTime());
        r.text(x0, infoY, b, 1, gap >= 0 ? Color{0.4f, 1.0f, 0.5f} : Color{1.0f, 0.6f, 0.3f});
        }
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
    } else if (ses_->mode() == RoadSession::Mode::Marathon) {
        const int pos = ses_->runPosition();
        std::snprintf(b, sizeof b, "%d. / %d", pos, ses_->runCount());
        r.textCentered(W / 2.0f, 62 + oy, pos == 1 ? "ETAP BIRINCISI!" : pos <= 3 ? "PODYUM!" : "ETAP BITTI", 3, pos <= 3 ? Color{0.3f, 1.0f, 0.4f} : kUiGold);
        r.textCentered(W / 2.0f, 92 + oy, b, 4, {1, 1, 1});
        const double drivenReal = std::max(0.1, ses_->raceLength() * ses_->compression() / 1000.0);
        std::snprintf(b, sizeof b, "SURE %d:%02d   %.0f KM   YAKIT %.0f L (%.0f L/100)", (int)ses_->playerTime() / 60, (int)ses_->playerTime() % 60,
                      drivenReal, usedRun_, usedRun_ / drivenReal * 100.0);
        r.textCentered(W / 2.0f, 136 + oy, b, 1, {0.85f, 0.85f, 0.9f});
        // Ilk 3 (rakip isim + tarz)
        std::vector<const Runner*> fin;
        for (const Runner& Rn : ses_->runField().runners()) if (Rn.finished) fin.push_back(&Rn);
        std::sort(fin.begin(), fin.end(), [](const Runner* a2, const Runner* b2) { return a2->finishT < b2->finishT; });
        for (size_t k = 0; k < fin.size() && k < 3; ++k) {
            std::snprintf(b, sizeof b, "%zu. %s (%s, %d MOLA)", k + 1 + (pos <= (int)k + 1 ? 1 : 0), fin[k]->name.c_str(), runStyleName(fin[k]->style), fin[k]->pits);
            r.textCentered(W / 2.0f, 154 + oy + k * 12.0f, b, 1, kUiDim);
        }
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
    if (prize_ < 0) std::snprintf(b, sizeof b, "KAYIP $%ld", -prize_);
    else std::snprintf(b, sizeof b, "ODUL $%ld", prize_);
    r.textCentered(W / 2.0f, 200 + oy, b, 3, prize_ < 0 ? Color{1.0f, 0.35f, 0.3f} : kUiGold);
    if (autoClutchPenalty()) r.textCentered(W / 2.0f, 232 + oy, "OTOMATIK DEBRIYAJ: ODUL %75", 1, {0.9f, 0.6f, 0.3f});
    if (ses_->mode() == RoadSession::Mode::Marathon && app_.runPlan.active) {
        std::snprintf(b, sizeof b, "SIMDI %s  -  SONRAKI ETAP: %s (%.0f KM)", Career::cityName(app_.career.city),
                      Career::cityName(app_.career.city + (app_.runPlan.target > app_.career.city ? 1 : -1)), app_.runPlan.realKm);
        r.textCentered(W / 2.0f, 256 + oy, b, 1, {0.55f, 0.85f, 1.0f});
        r.textCentered(W / 2.0f, 280 + oy, "DOKUN / ENTER: SONRAKI ETAP", 1, {0.7f, 0.75f, 0.9f});
    } else r.textCentered(W / 2.0f, 280 + oy, "DOKUN / ENTER: GARAJ", 1, {0.7f, 0.75f, 0.9f});
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
        else if (marathon_.hit(x, y)) start(RoadSession::Mode::Marathon);
        return;
    }
    if (ses_->phase() == RoadSession::Phase::Finished && finT_ > 1.0) { leaveResults(); return; }
    if (cockpit_.pointerDown(id, x, y)) return;
    if (assistBtn_.hit(x, y)) toggleAssist();
    else if (tiltBtn_.hit(x, y) && app_.tiltAvailable) toggleTilt();
    else if (adasBtn_.hit(x, y)) cycleAdas();
}

// Surus yardimi dongusu: KAPALI -> HIZ SAB. (adaptif varsa takip mesafeli) -> + SERIT -> OTONOM -> KAPALI
void RoadScreen::cycleAdas() {
    if (adasLevel_ <= 0) { flash("SURUS YARDIMI YOK (MODIFIYE > ECU)", 2.0); return; }
    const int maxMode = adasLevel_ >= 4 ? 3 : adasLevel_ >= 3 ? 2 : 1;
    adasMode_ = adasMode_ >= maxMode ? 0 : adasMode_ + 1;
    if (adasMode_ == 1 && ses_->player().sim().speed() < 30.0 / 3.6) {   // gercek araclardaki gibi: 30 km/h ustunde devreye girer
        adasMode_ = maxMode >= 3 ? 3 : 0;
        flash(adasMode_ ? "OTONOM SURUS" : "HIZ SABITLEYICI: 30 KM/H USTUNDE", 1.6);
        return;
    }
    if (adasMode_ == 1) { ccSpeed_ = ses_->player().sim().speed(); ccI_ = 0; }
    static const char* n[4] = {"YARDIM KAPALI", "HIZ SABITLEYICI", "SERIT TAKIP", "OTONOM SURUS"};
    char b[64];
    std::snprintf(b, sizeof b, adasMode_ ? "%s: %.0f KM/H" : "%s", adasMode_ == 1 && adasLevel_ >= 2 ? "ADAPTIF HIZ SAB." : n[adasMode_], ccSpeed_ * 3.6);
    flash(b, 1.5);
}

// Hiz sabitleyici (PI), adaptif (ondeki aracla 2 s + 8 m mesafe), serit takip (direksiyon girdisi yokken serit ortasi),
// otonom (YZ direksiyonu + adaptif hiz). Frene basmak hepsini kapatir; gaz pedali gecici olarak hizlandirir.
void RoadScreen::applyAdas(RoadControls& c, double dt, bool keySteer) {
    RoadCar& P = ses_->player();
    if (adasMode_ != 3 && autoDrive_) {                                   // otonomdan cikis: vites yine surucude
        autoDrive_ = false; P.manual = prevManual_; cockpit_.setKnobGear(P.sim().powertrain().gear()); ov_ = false;
    }
    if (adasMode_ == 0) return;
    if (c.brake > 0.05) { adasMode_ = 0; flash("YARDIM KAPANDI", 1.0); return; }   // fren pedali her yardimi kapatir
    if (adasMode_ == 3 && keySteer) { adasMode_ = 2; flash("OTONOM: DIREKSIYON SENDE (SERIT TAKIP)", 1.6); }
    const double v = P.sim().speed();
    const RoadPath& RR = ses_->road();
    // Otonom: hizi kendisi secer (ekonomik: otoban 90, sehir 50, dag 60 km/h), vites D / otomatik
    if (adasMode_ == 3) {
        if (!autoDrive_) { autoDrive_ = true; prevManual_ = P.manual; }
        P.manual = false;
        c.gear = -1; c.shift = 0; c.clutch = -1.0; c.neutral = false; c.reverse = false;
        if (c.autoMode >= 0) c.autoMode = 0;
        cockpit_.setKnobGear(P.sim().powertrain().gear());
        ccSpeed_ = (ses_->kind() == RoadSession::Kind::Touge ? 60.0 : ses_->zoneAt(P.s()) == 1 ? 50.0 : 90.0) / 3.6;
    }
    double target = ccSpeed_;
    // Adaptif: ayni seritte ondeki en yakin arac (trafik + The Run rakipleri)
    double gap = 1e9, leadV = 0, leadS = 0;
    if (adasLevel_ >= 2) {
        const double lat = adasMode_ == 3 && ov_ ? ovLat_ : P.lateral();
        auto lead = [&](double s, double lane, double lv) {
            const double d = s - P.s();
            if (d > 2.0 && d < 160.0 && std::fabs(lane - lat) < 1.7 && d < gap) { gap = d; leadV = lv; leadS = s; }
        };
        for (const TrafficCar& t : ses_->traffic()) if (!t.oncoming) lead(t.s, t.lane, t.v);
        if (ses_->mode() == RoadSession::Mode::Marathon) for (const Runner& Rn : ses_->runField().runners()) lead(Rn.s, Rn.lane, Rn.v);
        if (RoadCar* rv = ses_->rival()) lead(rv->s(), rv->lateral(), rv->sim().speed());
    }
    // Otonom sollama: onde yavas arac varsa bos seride (cok seritli) ya da gorus acik ve karsi serit bossa karsiya cikar
    if (adasMode_ == 3) {
        if (!ov_ && gap < 70.0 && leadV < target - 3.0 && !ses_->dragPart()) {
            const int nf = RR.lanesFwd(P.s());
            auto laneFree = [&](double off, bool back) {
                for (const TrafficCar& t : ses_->traffic()) {
                    if (std::fabs(t.lane - off) > 1.7) continue;
                    const double d = t.s - P.s();
                    if (back ? (d > -10.0 && d < 320.0) : (d > -18.0 && d < 45.0)) return false;   // karsidan gelen: 320 m bos
                }
                return true;
            };
            bool straight = true;                                          // sollama mesafesinde keskin viraj yok
            for (double d = 0; d < 260.0; d += 20.0) if (std::fabs(RR.at(P.s() + d).curvature) > 1.0 / 450.0) straight = false;
            for (int k = 0; k < nf && !ov_; ++k) {
                const double off = RR.laneOffset(P.s(), false, k);
                if (std::fabs(off - P.lateral()) > 2.0 && laneFree(off, false)) { ov_ = true; ovLat_ = off; }
            }
            if (!ov_ && nf == 1 && RR.lanesBack(P.s()) > 0 && straight) {
                const double off = RR.laneOffset(P.s(), true, 0);
                if (laneFree(off, true)) { ov_ = true; ovLat_ = off; ovBack_ = true; }
            }
            if (ov_) { ovUntil_ = leadS + 18.0; flash("OTONOM: SOLLAMA", 1.0); }
        }
        if (ov_) {
            target = std::max(target, leadV + 25.0 / 3.6);                   // sollarken kisa sure hizlanir
            if (P.s() > ovUntil_ + v * 0.8) { ov_ = false; ovBack_ = false; }   // gecildi: seride don
            gap = 1e9;
        }
    }
    if (gap < 1e8) {
        const double safe = 8.0 + 2.0 * v;
        target = std::min(target, std::max(0.0, leadV + (gap - safe) * 0.35));
        if (gap < safe * 0.6) c.brake = std::max(c.brake, std::clamp((safe * 0.6 - gap) / safe + (v - leadV) * 0.08, 0.0, 0.8));
    }
    // Viraj hizi: serit takip / otonomda onden gelen viraja gore yavaslama
    if (adasMode_ >= 2) {
        for (double d = 0; d < v * v / 8.0 + 40.0; d += 8.0) {
            const double k = std::max(std::fabs(RR.at(P.s() + d).curvature), 1e-4);
            target = std::min(target, std::sqrt(0.45 * 9.81 / k + 2.0 * 4.0 * d));
        }
    }
    const double e = target - v;
    ccI_ = std::clamp(ccI_ + e * dt, -6.0, 6.0);
    double thr = std::clamp(0.12 * e + 0.05 * ccI_ + 0.12, 0.0, 1.0);
    if (adasMode_ == 3 && !ov_) thr = std::min(thr, 0.45);                  // ekonomi: yumusak gaz, erken vites
    if (e < -2.5 && c.brake < 0.05) c.brake = std::clamp((-e - 2.5) * 0.08, 0.0, 0.35);
    c.throttle = std::max(adasMode_ == 3 ? 0.0 : c.throttle, c.brake > 0.05 ? 0.0 : thr);
    // Serit: jiroskop yok sayilir; tusla direksiyon yoksa serit ortasina (otonomda sollama seridi) kendisi oturur
    if (adasMode_ >= 2 && !keySteer && !ses_->dragPart()) {
        int best = 0; double bd = 1e9;
        for (int k = 0; k < RR.lanesFwd(P.s()); ++k) { const double d = std::fabs(P.lateral() - RR.laneOffset(P.s(), false, k)); if (d < bd) { bd = d; best = k; } }
        const double lane = adasMode_ == 3 && ov_ ? ovLat_ : RR.laneOffset(P.s(), false, best);
        c.steer = P.aiControls(lane, 0.45, target).steer;
    }
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
        if (down && ses_->phase() == RoadSession::Phase::Finished) leaveResults();
        break;
    case Key::PageDown: if (down) toggleAssist(); break;
    case Key::PageUp: if (down) cycleAdas(); break;
    case Key::Back: if (down) app_.goGarage(); break;
    default: cockpit_.key(k, down); break;
    }
}

} // namespace zk
