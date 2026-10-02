#include "RoadCar.h"
#include "sim/PartTables.h"

#include <algorithm>
#include <cmath>

namespace zk {

RoadCar::RoadCar(const VehicleDef* car, const Tune* tune, const RoadPath& road, double s0, double laneOffset)
    : road_(road), lane_(laneOffset) {
    if (tune) tune_ = *tune;          // yoksa varsayilan Tune: sokak lastigi (drag ayari degil)
    if (tune_.roadTire > 0 && tune_.roadTire < (int)tireTable().size()) {   // yol lastigi takimi: drag lastigi yerine
        tune_.tireSel = tune_.roadTire; tune_.tires = (TireType)tireTable()[tune_.roadTire].type; tune_.psi = 0;
    }
    hasTune_ = true;
    VehicleSimConfig c;
    c.car = car; c.tune = &tune_; c.planar = true; c.road = "acikyol"; c.laneAsymmetry = false;
    sim_ = std::make_unique<VehicleSim>(c);
    const RoadPoint p = road_.at(s0);
    sim_->resetPose(p.x - laneOffset * std::sin(p.heading), p.y + laneOffset * std::cos(p.heading), p.heading);
    sim_->powertrain().setGear(1);
    sim_->powertrain().setClutchPedal(1.0);
    hint_ = (int)(s0 / RoadPath::kStep);
    road_.project(sim_->posX(), sim_->posY(), hint_, s_, lat_);
}

void RoadCar::recover(double backM) {
    const RoadPoint p = road_.at(std::max(0.0, s_ - backM));
    sim_->resetPose(p.x - lane_ * std::sin(p.heading), p.y + lane_ * std::cos(p.heading), p.heading);
    sim_->powertrain().setGear(1); sim_->powertrain().restart();
    launching_ = true; launchPedal_ = 1.0; shiftT_ = -1;
    hint_ = std::max(0, (int)(p.s / RoadPath::kStep));
    road_.project(sim_->posX(), sim_->posY(), hint_, s_, lat_);
    offT_ = 0; recovered_ = true;
}

void RoadCar::bump(double f) { sim_->scaleVelocity(f); }

// Otomatik vites karari. D: vites noktasi gaza oranli (hafif gazda verimli devir ~%40 kesici, tam gazda kesici - 500);
// asagi vites, alt viteste devir yukari vites noktasinin %80'inin altinda kaliyorsa (gidip gelme yok). Tam gazda kickdown:
// alt vites kesiciye 700 kala sigiyorsa hemen. S: hep yuksek devir. M: yalniz oyuncu (+/-); stop etmesin diye alt vites.
// TCU yazilimi (0-2): verimli devir ve tam gaz noktasi yukari, vitesler arasi bekleme kisa.
void RoadCar::autoShift(int gear, double rpm, double thr) {
    const PowertrainCore& pt = sim_->powertrain();
    const double red = sim_->engineSpec().redlineRpm, idle = sim_->engineSpec().idleRpm;
    const int tcu = hasTune_ ? std::clamp(tune_.swTcu, 0, 2) : 0;
    auto shift = [&](int to) { shiftT_ = 0; target_ = to; sinceShift_ = 0; };
    if (autoMode == 2) {
        if (gear > 1 && rpm < idle + 300.0 && sinceShift_ > 0.5) shift(gear - 1);
        return;
    }
    const bool sport = autoMode == 1;
    const double eff = std::max(idle + 1300.0, (0.40 + 0.04 * tcu) * red);
    const double top = red - (sport ? 250.0 : 500.0 - 100.0 * tcu);
    const double k = std::clamp((thr - 0.15) / 0.70, 0.0, 1.0);
    const double up = sport ? (thr > 0.25 ? top : 0.72 * red) : eff + (top - eff) * std::pow(k, 1.3);
    if (sinceShift_ < 0.8 - 0.2 * tcu && !(thr > 0.85 && sinceShift_ > 0.35)) return;
    // Yukari: cikis devri (konvertor kaymasiz) vites noktasini gecti ya da motor kesiciye dayandi (kayma ile)
    if ((rpm > up || pt.rpm() > red - 120.0) && gear < pt.gearCount()) { shift(gear + 1); return; }
    if (gear <= 1) return;
    const double rLow = rpm * pt.gearRatio(gear - 1) / std::max(pt.gearRatio(gear), 1e-3);
    if (thr > 0.85 && rLow < red - 700.0 && rpm < top - 400.0) { shift(gear - 1); return; }   // kickdown
    if (sinceShift_ > 1.0 && rLow < up * 0.80 && rpm < (sport ? 0.55 * red : std::max(idle + 700.0, eff * 0.72))) shift(gear - 1);
}

void RoadCar::requestShift(int dir) {
    if (shiftT_ >= 0) return;
    PowertrainCore& pt = sim_->powertrain();
    target_ = std::clamp(pt.gear() + dir, 1, pt.gearCount());
    if (target_ != pt.gear()) { shiftT_ = 0; sinceShift_ = 0; }
}

void RoadCar::requestGear(int g) {
    PowertrainCore& pt = sim_->powertrain();
    g = std::clamp(g, 0, pt.gearCount());
    if (shiftT_ >= 0 || g == pt.gear()) return;
    target_ = g; shiftT_ = 0; sinceShift_ = 0;
}

// Oyuncu debriyaji (H-desen): pedal ve vites dogrudan; debriyajsiz vites girmez (citirti), stop edince
// debriyaja basinca mars
void RoadCar::playerClutch(const RoadControls& c) {
    PowertrainCore& pt = sim_->powertrain();
    if (pt.stalled()) {
        if (!stalledEv_ && !launching_) stalledEv_ = true;
        launching_ = true;                                               // stop bayragi (bir kez mesaj)
        if (c.clutch > 0.6) { pt.restart(); launching_ = false; }
    } else launching_ = false;
    grind_ = false;
    if (c.gear >= 0 && c.gear != pt.gear()) {
        // Debriyaj %40 basili yeterli; bosa almak debriyajsiz olur; gaz kesik ve devir uyumluysa (+-%15) debriyajsiz gecer
        bool matched = false;
        if (c.gear > 0 && c.gear <= pt.gearCount() && c.throttle < 0.15) {
            const double wheelRpm = sim_->speed() / sim_->wheel(sim_->drivenLeft()).rEff() * 9.5493;
            const double target = wheelRpm * sim_->gearboxSpec().ratios[c.gear - 1] * sim_->gearboxSpec().finalDrive;
            matched = target > 900.0 && std::fabs(pt.rpm() - target) < 0.15 * target;
        }
        if (c.clutch >= 0.40 || c.gear == 0 || matched) pt.setGear(std::min(c.gear, pt.gearCount()));
        else grind_ = true;
    }
    pt.setClutchPedal(c.clutch);
    pt.setThrottle(std::clamp(c.throttle, 0.0, 1.0));
}

// Otomatik debriyaj/vites: kalkista devir tutulur, vites degisiminde gaz kesilip debriyaj basilir.
// Sirali (dogbox/DCT) vites hizli: dogbox 35 ms atesleme kesme, DCT 60 ms; debriyaj yalniz kalkista.
void RoadCar::driverAssist(double dt, double thrIn, bool neutral) {
    PowertrainCore& pt = sim_->powertrain();
    const double v = sim_->speed();
    const Gearbox box = sim_->gearboxType();
    const bool seq = box == Gearbox::Dogbox || box == Gearbox::DCT;
    // otomatik debriyaj cezasi: gec kavrar; TCU yazilimi tork konvertorlu otomatikte gecisi kisaltir (seviye basina %22)
    const double k = (slowClutch ? 1.6 : 1.0) * (pt.converter() && hasTune_ ? 1.0 - 0.22 * std::clamp(tune_.swTcu, 0, 2) : 1.0);
    double clutch = 0.0, thr = thrIn;
    if (pt.stalled()) { pt.restart(); pt.setGear(manual ? std::max(pt.gear(), 1) : 1); launching_ = true; launchPedal_ = 1.0; stalledEv_ = true; }
    const bool launchGear = manual ? pt.gear() >= 1 : pt.gear() == 1;
    if (shiftT_ >= 0.0 && seq) {
        shiftT_ += dt;
        const double dur = box == Gearbox::Dogbox ? 0.035 : 0.060;
        if (pt.gear() != target_) pt.setGear(target_);
        if (box == Gearbox::Dogbox) thr = 0.0;
        if (shiftT_ > dur) shiftT_ = -1.0;
    } else if (shiftT_ >= 0.0) {
        shiftT_ += dt;
        clutch = shiftT_ < 0.12 * k ? 1.0 : std::max(0.0, 1.0 - (shiftT_ - 0.12 * k) / (0.14 * k));
        if (shiftT_ < 0.12 * k) thr = 0.0;
        if (shiftT_ > 0.08 * k && pt.gear() != target_) pt.setGear(target_);
        if (shiftT_ > 0.26 * k) shiftT_ = -1.0;
    } else if (launchGear && v < 3.0 && (launching_ || thrIn < 0.05)) {
        if (thrIn < 0.05 && v < 1.0) launchPedal_ = 1.0;                      // durus: debriyaj basili (stop etmez)
        else {   // kalkis: devir gaza gore 1500-2700'de tutulur, pedal isirma noktasindan devre gore birakilir
            const double hold = launchRpm > 0.0 ? launchRpm : 1500.0 + 1200.0 * thrIn;
            launchPedal_ = std::min(launchPedal_, 0.65);
            launchPedal_ = std::clamp(launchPedal_ + (pt.rpm() < hold ? 0.8 : -1.6) / k * dt, 0.0, 1.0);
        }
        launching_ = launchPedal_ > 0.0;
        clutch = launchPedal_;
    } else {
        launching_ = false;
        const int gear = pt.gear();
        sinceShift_ += dt;
        // Vites karari: otomatikte (konvertor kayar) arac hizindan cikis devri (patinaj sisirmez), digerlerinde motor devri
        double shiftRpm = pt.rpm();
        if (pt.converter())
            shiftRpm = std::min(pt.rpm(), v / sim_->wheel(sim_->drivenLeft()).rEff() * pt.totalRatio() * 9.5493);
        if (!manual) autoShift(gear, shiftRpm, thrIn);
        if (!manual && gear > 1 && v < 2.0) { shiftT_ = 0; target_ = 1; launching_ = true; launchPedal_ = 1.0; }
        if (pt.rpm() < sim_->engineSpec().idleRpm * 0.9 && (manual ? gear >= 1 : gear == 1)) { launching_ = true; launchPedal_ = 0.6; }
    }
    if (neutral || pt.gear() == 0) clutch = 1.0;                         // N / P: konvertor / debriyaj ayrik
    pt.setClutchPedal(clutch);
    pt.setThrottle(std::clamp(thr, 0.0, 1.0));
}

void RoadCar::update(double dt, const RoadControls& c) {
    const double v = sim_->speed();
    if (c.clutch >= 0.0) playerClutch(c);
    else {
        if (c.gear >= 0) requestGear(c.gear);
        if (c.autoMode >= 0) { autoMode = c.autoMode; manual = false; if (c.shift != 0 && autoMode == 2) requestShift(c.shift); }
        else if (c.shift != 0) { manual = true; requestShift(c.shift); }
        sim_->setReverse(c.reverse, c.throttle);
        driverAssist(dt, c.reverse ? 0.0 : c.throttle, c.neutral || c.reverse);
    }
    sim_->setSurfaceMu((offRoad() ? 0.55 : 1.0) * gripMul);           // cim/toprak; yagmur
    {   // yol egimi arac yonune izdusurulur (ters yonde giderken yokus inis olur)
        const RoadPoint p = road_.at(s_);
        sim_->setGrade(p.grade * std::cos(sim_->heading() - p.heading));
    }
    VehicleInputs in; in.steer = c.steer; in.brake = c.brake;
    sim_->setTractionControl(assist);                                   // TC yalniz aracta varsa (fabrika / ECU kiti)
    if (stability && v > 5.0 && std::fabs(c.steer) < 0.05 && !(assist && sim_->hasTc())) {
        // Duz yol dengesi (her aracta, surucu refleksi): direksiyon duzken arka kayarsa hafif karsi direksiyon, cok
        // kayarsa gaz biraz kesilir. Virajda (direksiyon cevrili) devreye girmez: tam gaz virajda kayma serbest.
        const double beta = sim_->bodySlipAngle();
        const double excess = beta - std::clamp(beta, -0.02, 0.02);
        in.steer = std::clamp(c.steer + 0.8 * excess, -0.3, 0.3);
        const double over = std::fabs(beta) - 0.07;
        if (over > 0) sim_->powertrain().setThrottle(sim_->powertrain().throttle() * std::max(0.35, 1.0 - over * 4.0));
    }
    if (assist && sim_->hasTc() && v > 3.0) {
        // ESP benzeri: arka kayarsa otomatik karsi direksiyon + gaz kesme
        const double beta = sim_->bodySlipAngle();
        // Normal virajdaki kucuk govde kaymasina (~3 deg) karismaz; yalnizca fazlasina karsi direksiyon
        const double excess = beta - std::clamp(beta, -0.05, 0.05);
        in.steer = std::clamp(c.steer + 1.0 * excess, -0.5, 0.5);
        const double over = std::fabs(beta) - 0.10;
        if (over > 0) sim_->powertrain().setThrottle(sim_->powertrain().throttle() * std::max(0.15, 1.0 - over * 5.0));
    }
    acc_ += dt;
    while (acc_ >= kStep) { sim_->step(kStep, in); acc_ -= kStep; }
    sim_->drainFailureEvents();
    road_.project(sim_->posX(), sim_->posY(), hint_, s_, lat_);
    offT_ = std::fabs(lat_) > road_.halfWidthAt(s_) + 12.0 ? offT_ + dt : 0.0;
    if (offT_ > 1.5) recover();
}

RoadControls RoadCar::aiControls(double laneOffset, double pace, double speedCap) const {
    RoadControls c;
    const double v = sim_->speed();
    // Yol takibi: istenen egrilik = yol egriligi + serit/yon hatasi duzeltmesi; direksiyon = kinematik
    // on besleme + yaw hizi geri beslemesi (lastik gecikmesi/understeer'de asiri direksiyonu onler)
    const RoadPoint here = road_.at(s_);
    double eh = sim_->heading() - here.heading;
    eh = std::remainder(eh, 2.0 * 3.14159265358979);
    const double e = lat_ - laneOffset, Ld = 8.0 + 0.5 * v;
    const double kDes = road_.at(s_ + 0.45 * v).curvature - 2.0 * e / (Ld * Ld) - 1.6 * eh / Ld;
    const double Lw = sim_->vehicleLoad().wheelbase;
    c.steer = std::clamp(Lw * kDes + 0.08 * (v * kDes - sim_->yawRate()), -0.5, 0.5);
    const double aLat = pace * 9.81, aBrake = 5.0;
    double vMax = speedCap;                                              // fren mesafesi icindeki en dar yer
    for (double d = 0; d < v * v / (2 * aBrake) + 30.0; d += 8.0) {
        const double k = std::max(std::fabs(road_.at(s_ + d).curvature), 1e-4);
        vMax = std::min(vMax, std::sqrt(aLat / k + 2 * aBrake * d));
    }
    c.throttle = std::clamp((vMax - v) * 0.4 + 0.2, 0.0, 1.0);                // yumusak gaz (ac-kapa degil)
    c.throttle *= std::clamp(1.0 - (std::fabs(sim_->bodySlipAngle()) - 0.04) * 12.0, 0.0, 1.0);   // kayarsa gazi kes
    c.brake = v > vMax + 1.5 ? std::clamp((v - vMax) / 4.0, 0.2, 1.0) : 0.0;
    return c;
}

double RoadCar::tireSlipSpeed() const {
    const double v = sim_->speed(), f = offRoad() ? 0.2 : 1.0;
    double slip = 0;
    for (int i = 0; i < 4; ++i) {
        const WheelSimulation& w = sim_->wheel(i);
        if (w.Fz() < 100) continue;
        slip = std::max({slip, std::fabs(w.omega() * w.rEff() - v) * f, std::fabs(w.slipAngle()) * v * 0.9 * f});
    }
    return slip;
}

} // namespace zk
