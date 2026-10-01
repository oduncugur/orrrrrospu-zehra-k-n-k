#include "DragRace.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace zk {

namespace {
constexpr double kRollout = 0.178;     // stage isigini terk etme (~7 inc)
constexpr double kClutchHold = 0.60;   // bu degerin ustunde debriyaj basili: arac frende tutulur
constexpr double k60 = 18.288, k330 = 100.584, k660 = 201.168, k1000 = 304.8;

std::string fmt(const char* f, double a) { char b[96]; std::snprintf(b, sizeof b, f, a); return b; }
} // namespace

DragRace::DragRace(int playerCarId, int opponentCarId, TreeType tree, uint32_t seed, bool withBurnout,
                   const Tune* playerTune, const Tune* opponentTune)
    : tree_(tree), phase_(withBurnout ? RacePhase::Burnout : RacePhase::Staging), rng_(seed * 2654435761u + 7u) {
    lanes_.resize(2);
    const int ids[2] = {playerCarId, opponentCarId};
    const Tune* tunes[2] = {playerTune, opponentTune};
    for (int i = 0; i < 2; ++i) {
        LaneState& L = lanes_[i];
        L.car = findVehicle(ids[i]);
        VehicleSimConfig cfg;
        cfg.car = L.car;
        if (tunes[i]) { tunes_[i] = *tunes[i]; cfg.tune = &tunes_[i]; }
        cfg.road = "drag";
        cfg.fuelLiters = 8.0;
        L.sim = std::make_unique<VehicleSim>(cfg);
        L.sim->powertrain().setGear(1);
        L.sim->powertrain().setClutchPedal(1.0);
    }
    lanes_[1].aiReaction = 0.06 + (rnd() % 1000) / 1000.0 * 0.20;   // .060 - .260
    treeDelay_ = 0.8 + (rnd() % 1000) / 1000.0 * 1.2;
}

uint32_t DragRace::rnd() { rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5; return rng_; }

double DragRace::greenTime() const {
    if (treeStart_ < 0) return 1e9;
    return treeStart_ + (tree_ == TreeType::Pro ? 0.4 : 1.5);
}

int DragRace::treeLights() const {
    int m = 0;
    if (treeStart_ >= 0 && clock_ >= treeStart_) {
        const double t = clock_ - treeStart_;
        if (tree_ == TreeType::Pro) { if (t < 0.4) m |= 7; }
        else {
            if (t >= 0.0 && t < 1.5) m |= 1;
            if (t >= 0.5 && t < 1.5) m |= 2;
            if (t >= 1.0 && t < 1.5) m |= 4;
        }
        if (clock_ >= greenTime()) m |= 8;
    }
    if (lanes_[0].slip.redLight) m |= 16;
    if (lanes_[1].slip.redLight) m |= 32;
    return m;
}

void DragRace::skipBurnout() {
    if (phase_ == RacePhase::Burnout) { phase_ = RacePhase::Staging; phaseT_ = 0.0; events_.push_back("Burnout bitti - stage'e gec"); }
}

void DragRace::advance(double realDt, const PlayerControls& pc) {
    if (phase_ == RacePhase::Finished && lanes_[0].sim->speed() < 0.1 && lanes_[1].sim->speed() < 0.1) return;
    LaneState& P = lanes_[0];
    PowertrainCore& pt = P.sim->powertrain();
    const Gearbox box = P.sim->gearboxType();
    // ---- vites istekleri (kare basina bir kez; otopilotta oyuncu girdisi yok sayilir) ----
    if (autopilot_) { P.grind = false; }
    else if (box == Gearbox::HPattern && pc.requestedGear >= 0 && pc.requestedGear != pt.gear()) {
        const int g = std::min(pc.requestedGear, pt.gearCount());
        if (pc.clutch >= 0.55 || g == 0) { pt.setGear(g); P.grind = false; }   // bosa almak debriyajsiz da olur
        else if (!P.grind) { P.grind = true; events_.push_back("DISLI CITIRTISI! Vites icin debriyaja bas"); }
    }
    if (pc.requestedGear < 0 || pc.requestedGear == pt.gear()) P.grind = false;
    if (!autopilot_ && (box == Gearbox::Dogbox || box == Gearbox::DCT) && pc.paddle != 0 && prevPaddle_ == 0) {
        const int g = std::clamp(pt.gear() + pc.paddle, 1, pt.gearCount());
        if (g != pt.gear()) { pt.setGear(g); P.shiftT = 0.0; }
    }
    prevPaddle_ = pc.paddle;

    acc_ += std::min(realDt, 0.1);
    while (acc_ >= kStep) {
        stepPhysics(pc);
        clock_ += kStep; phaseT_ += kStep;
        acc_ -= kStep;
    }
}

void DragRace::playerDrive(LaneState& L, const PlayerControls& pc, VehicleInputs& in) {
    PowertrainCore& pt = L.sim->powertrain();
    const Gearbox box = L.sim->gearboxType();
    double throttle = pc.throttle, clutch = pc.clutch;
    const bool beforeLeave = !L.left;

    // Otomatik debriyaj: DCT (launch control) ve tork konvertorlu otomatik
    if (box == Gearbox::DCT || box == Gearbox::TorqueConverter) {
        const bool holding = pc.brake > 0.5 && beforeLeave;
        if (box == Gearbox::TorqueConverter) {
            // Tork konvertoru (PowertrainCore): frende beklerken N + devir stall'da (brake-torque karsiligi,
            // defaultLaunchRpm); fren birakilinca akiskan kavrama — ani kilitlenme/darbe yok
            clutch = holding ? 1.0 : 0.0;
            if (!holding && pt.gear() < pt.gearCount() && pt.rpm() > L.sim->shiftRpm() && L.shiftT < 0) {
                pt.setGear(pt.gear() + 1); L.shiftT = 0.0;
            }
        } else {
            // DCT launch control: fren birakilinca kavrama ~150 ms'de rampa ile kapanir
            if (holding) L.autoRelease = -1.0;
            else if (L.autoRelease < 0.0) L.autoRelease = clock_;
            clutch = holding ? 1.0 : std::max(0.0, 1.0 - (clock_ - L.autoRelease) / 0.15);
        }
        in.held = holding;
    } else {
        in.held = clutch > kClutchHold && beforeLeave && phase_ != RacePhase::Run;
    }
    // Stage/agac: oyuncu debriyaja (DCT/otomatikte frene) ilk kez basana kadar arac frende tutulur ve motor
    // bosta kalir; boylece stage'e girerken rolantide surunup haksiz kirmizi isik yakilmaz.
    if ((phase_ == RacePhase::Staging || phase_ == RacePhase::Tree) && beforeLeave) {
        const bool autoBox = box == Gearbox::DCT || box == Gearbox::TorqueConverter;
        if (autoBox ? pc.brake > 0.5 : pc.clutch > kClutchHold) L.armed = true;
        if (!L.armed) { clutch = 1.0; in.held = true; }
    }
    if (phase_ == RacePhase::Burnout) in.held = true;          // line-lock: on frenler kilitli
    in.brake = pc.brake * (in.held ? 0.0 : 1.0);

    // Vites gecisi: dogbox 35 ms ateslemesi kesme, DCT 60 ms ortusme
    L.cutIgnition = false;
    if (L.shiftT >= 0.0) {
        const double dur = box == Gearbox::Dogbox ? 0.035 : box == Gearbox::DCT ? 0.060 : 0.30;
        if (box == Gearbox::Dogbox) { throttle = 0.0; L.cutIgnition = true; }
        L.shiftT += kStep;
        if (L.shiftT > dur) L.shiftT = -1.0;
    }
    pt.setTwoStep(pc.twoStep && beforeLeave && phase_ != RacePhase::Burnout, L.sim->defaultLaunchRpm());
    if (L.slip.finished) { throttle = 0.0; clutch = 1.0; in.brake = std::max(in.brake, 0.45); }   // parasut + fren
    pt.setThrottle(throttle);
    pt.setClutchPedal(clutch);
}

void DragRace::aiDrive(LaneState& L, VehicleInputs& in) {
    PowertrainCore& pt = L.sim->powertrain();
    const Gearbox box = L.sim->gearboxType();
    const double go = greenTime() + L.aiReaction;
    if (L.slip.finished || L.slip.broke) {
        pt.setThrottle(0.0); pt.setClutchPedal(1.0); in.brake = 0.45;
        return;
    }
    const bool tc = box == Gearbox::TorqueConverter;
    if (clock_ < go || phase_ == RacePhase::Burnout || phase_ == RacePhase::Staging) {
        pt.setGear(1);
        // Debriyaj basili (otomatikte N) + 2-step; otomatikte kalkis devri stall devri (defaultLaunchRpm)
        pt.setClutchPedal(1.0);
        pt.setThrottle(phase_ == RacePhase::Tree ? 1.0 : 0.0);
        pt.setTwoStep(true, L.sim->defaultLaunchRpm());
        in.held = true;
        return;
    }
    pt.setTwoStep(false, 0.0);
    const double t = clock_ - go;
    // Kalkis: manuel/dogbox 120 ms clutch dump, DCT launch control 150 ms rampa (oyuncuyla ayni), otomatik konvertor
    double clutch = tc ? 0.0 : std::max(0.0, 1.0 - t / (box == Gearbox::DCT ? 0.15 : 0.12));
    const double kap = 0.5 * (L.sim->wheel(L.sim->drivenLeft()).kappa() + L.sim->wheel(L.sim->drivenRight()).kappa());
    if (t > 0.12) L.aiFoot = std::clamp(L.aiFoot + kStep * 8.0 * (0.12 - kap), pt.rpm() < 5500.0 ? 1.0 : 0.35, 1.0);
    double thr = L.aiFoot;
    L.cutIgnition = false;
    if (tc) {
        // Otomatik: vites aninda, en az 0.30 s arayla (oyuncu ile ayni)
        if (L.shiftT < 0.0 && pt.rpm() > L.sim->shiftRpm() && pt.gear() < pt.gearCount()) { pt.setGear(pt.gear() + 1); L.shiftT = 0.0; }
        if (L.shiftT >= 0.0) { L.shiftT += kStep; if (L.shiftT > 0.30) L.shiftT = -1.0; }
        pt.setClutchPedal(clutch);
        pt.setThrottle(thr);
        return;
    }
    // Aks hissi: kalkis/vites sonrasi aks gerilmesi kopma sinirinin %85'ini asarsa debriyaji ~80 ms kaydir,
    // gazi hafiflet (deneyimli surucunun "feather"i). Olculdu: stok guclu araclarda aks kirilmasini azaltir.
    {
        const DrivetrainFailure& f = L.sim->failure();
        const double ratio = std::max(f.shearMPa(0), f.shearMPa(1)) / f.axle().tauUltMPa;
        if (ratio > 0.85) L.aiFeatherT = 0.08;
        if (L.aiFeatherT > 0.0) {
            L.aiFeatherT -= kStep;
            clutch = std::max(clutch, 0.45);                              // isirma bolgesinde kaydir
            thr = std::min(thr, 0.7);
        }
    }
    if (L.shiftT < 0.0 && pt.rpm() > L.sim->shiftRpm() && pt.gear() < pt.gearCount()) L.shiftT = 0.0;
    if (L.shiftT >= 0.0 && (box == Gearbox::Dogbox || box == Gearbox::DCT)) {
        if (L.shiftT == 0.0) pt.setGear(pt.gear() + 1);
        const double dur = box == Gearbox::Dogbox ? 0.035 : 0.060;
        if (box == Gearbox::Dogbox) { thr = 0.0; L.cutIgnition = true; }
        L.shiftT += kStep;
        if (L.shiftT > dur) L.shiftT = -1.0;
    } else if (L.shiftT >= 0.0) {                                       // H-desen: ~270 ms
        if (L.shiftT < 0.10)      { clutch = 1.0; thr = 0.0; }
        else if (L.shiftT < 0.17) { clutch = 1.0; thr = 0.3; if (L.shiftT - kStep < 0.10) pt.setGear(pt.gear() + 1); }
        else if (L.shiftT < 0.27) { clutch = 1.0 - (L.shiftT - 0.17) / 0.10; }
        else L.shiftT = -1.0;
        if (L.shiftT >= 0.0) L.shiftT += kStep;
    }
    pt.setClutchPedal(clutch);
    pt.setThrottle(thr);
}

void DragRace::timing(LaneState& L, int idx) {
    const double d = L.sim->distance(), v = L.sim->speed();
    const char* who = idx == 0 ? "SEN" : "RAKIP";
    if (!L.left && d > kRollout) {
        L.left = true; L.leaveTime = clock_;
        L.slip.reaction = clock_ - greenTime();
        if (clock_ < greenTime()) {
            L.slip.redLight = true;
            events_.push_back(std::string(who) + ": KIRMIZI ISIK! (" + fmt("%+.3f", L.slip.reaction) + ")");
        }
    }
    if (!L.left || L.slip.finished) return;
    const double et = clock_ - L.leaveTime;
    if (L.slip.sixtyFt < 0 && d >= k60) L.slip.sixtyFt = et;
    if (L.slip.t330 < 0 && d >= k330) L.slip.t330 = et;
    if (L.slip.eighth < 0 && d >= k660) { L.slip.eighth = et; L.slip.eighthKmh = v * 3.6; }
    if (L.slip.t1000 < 0 && d >= k1000) L.slip.t1000 = et;
    if (d >= kQuarterMile) {
        L.slip.quarter = et; L.slip.trapKmh = v * 3.6; L.slip.finished = true;
        events_.push_back(std::string(who) + ": BITIS " + fmt("%.3f s", et) + " @ " + fmt("%.1f km/h", v * 3.6));
    }
}

void DragRace::stepPhysics(const PlayerControls& pc) {
    // ---- faz gecisleri ----
    if (phase_ == RacePhase::Burnout && (phaseT_ > 20.0 || (autopilot_ && phaseT_ > 1.5))) skipBurnout();
    if (phase_ == RacePhase::Staging && phaseT_ > 1.0) {
        for (auto& L : lanes_) L.staged = true;
        events_.push_back("PRE-STAGE / STAGE - Agac basliyor");
        phase_ = RacePhase::Tree; phaseT_ = 0.0;
        treeStart_ = clock_ + treeDelay_;
    }
    if (phase_ == RacePhase::Tree && clock_ >= greenTime()) {
        phase_ = RacePhase::Run; phaseT_ = 0.0;
        events_.push_back("YESIL!");
    }

    for (int i = 0; i < 2; ++i) {
        LaneState& L = lanes_[i];
        VehicleInputs in;
        if (i == 0 && !autopilot_) playerDrive(L, pc, in); else aiDrive(L, in);
        L.sim->step(kStep, in);
        timing(L, i);
        if (!L.slip.broke && (L.sim->failure().snapped(0) || L.sim->failure().snapped(1))) {
            L.slip.broke = true; L.slip.note = "AKS KIRILDI";
            events_.push_back(std::string(i == 0 ? "SEN" : "RAKIP") + ": AKS KIRILDI!");
        }
        if (!L.slip.stalled && L.sim->powertrain().stalled() && !L.slip.finished) {
            L.slip.stalled = true;
            events_.push_back(std::string(i == 0 ? "SEN" : "RAKIP") + ": MOTOR STOP ETTI");
            if (i == 0 && phase_ != RacePhase::Run) { L.sim->powertrain().restart(); L.slip.stalled = false; }  // agacta: mars
        }
    }
    for (auto& e : lanes_[0].sim->powertrain().drainEvents()) (void)e;   // VTEC vb. olaylar HUD'da gosterilir
    for (auto& e : lanes_[1].sim->powertrain().drainEvents()) (void)e;

    // ---- kazanan ----
    if (phase_ == RacePhase::Run && winner_ < 0) {
        const LaneState &A = lanes_[0], &B = lanes_[1];
        auto out = [](const LaneState& L) { return L.slip.redLight || L.slip.broke || (L.slip.stalled && !L.slip.finished); };
        if (A.slip.redLight && B.slip.redLight) winner_ = A.leaveTime > B.leaveTime ? 0 : 1;   // once yakan kaybeder
        else if (A.slip.redLight) winner_ = 1;
        else if (B.slip.redLight) winner_ = 0;
        else if (A.slip.finished && !B.slip.finished) winner_ = 0;
        else if (B.slip.finished && !A.slip.finished) winner_ = 1;
        else if (A.slip.finished && B.slip.finished) winner_ = 0;   // ayni adimda: oyuncu lehine
        else if (out(A) && out(B)) winner_ = 1;
        else if (out(A) && !out(B) && B.left) winner_ = 1;
        else if (out(B) && !out(A) && A.left) winner_ = 0;
        if (winner_ >= 0) events_.push_back(winner_ == 0 ? "KAZANDIN!" : "KAYBETTIN");
    }
    const bool doneA = lanes_[0].slip.finished || lanes_[0].slip.broke || lanes_[0].slip.stalled;
    const bool doneB = lanes_[1].slip.finished || lanes_[1].slip.broke || lanes_[1].slip.stalled;
    if (phase_ == RacePhase::Run && ((doneA && doneB) || clock_ - greenTime() > 40.0)) {
        phase_ = RacePhase::Finished; phaseT_ = 0.0;
        if (winner_ < 0) winner_ = lanes_[0].sim->distance() >= lanes_[1].sim->distance() ? 0 : 1;
    }
}

} // namespace zk
