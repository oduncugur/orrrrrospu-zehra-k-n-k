#include "RoadCar.h"

#include <algorithm>
#include <cmath>

namespace zk {

RoadCar::RoadCar(const VehicleDef* car, const Tune* tune, const RoadPath& road, double s0, double laneOffset)
    : road_(road), lane_(laneOffset) {
    if (tune) tune_ = *tune;          // yoksa varsayilan Tune: sokak lastigi (drag ayari degil)
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

void RoadCar::requestShift(int dir) {
    if (shiftT_ >= 0) return;
    PowertrainCore& pt = sim_->powertrain();
    target_ = std::clamp(pt.gear() + dir, 1, pt.gearCount());
    if (target_ != pt.gear()) { shiftT_ = 0; sinceShift_ = 0; }
}

// Otomatik debriyaj/vites: kalkista devir tutulur, vites degisiminde gaz kesilip debriyaj basilir
void RoadCar::driverAssist(double dt, double thrIn) {
    PowertrainCore& pt = sim_->powertrain();
    const double v = sim_->speed();
    double clutch = 0.0, thr = thrIn;
    if (pt.stalled()) { pt.restart(); pt.setGear(1); launching_ = true; launchPedal_ = 1.0; stalledEv_ = true; }
    if (shiftT_ >= 0.0) {
        shiftT_ += dt;
        clutch = shiftT_ < 0.12 ? 1.0 : std::max(0.0, 1.0 - (shiftT_ - 0.12) / 0.14);
        if (shiftT_ < 0.12) thr = 0.0;
        if (shiftT_ > 0.08 && pt.gear() != target_) pt.setGear(target_);
        if (shiftT_ > 0.26) shiftT_ = -1.0;
    } else if (pt.gear() == 1 && v < 3.0 && (launching_ || thrIn < 0.05)) {
        if (thrIn < 0.05 && v < 1.0) launchPedal_ = 1.0;                      // durus: debriyaj basili (stop etmez)
        else {   // kalkis: devir gaza gore 1500-2700'de tutulur, pedal isirma noktasindan devre gore birakilir
            const double hold = 1500.0 + 1200.0 * thrIn;
            launchPedal_ = std::min(launchPedal_, 0.65);
            launchPedal_ = std::clamp(launchPedal_ + (pt.rpm() < hold ? 0.8 : -1.6) * dt, 0.0, 1.0);
        }
        launching_ = launchPedal_ > 0.0;
        clutch = launchPedal_;
    } else {
        launching_ = false;
        const int gear = pt.gear();
        sinceShift_ += dt;
        if (!manual && sinceShift_ > 0.8) {
            if (pt.rpm() > sim_->shiftRpm() - 150 && gear < pt.gearCount()) { shiftT_ = 0; target_ = gear + 1; sinceShift_ = 0; }
            else if (pt.rpm() < 0.36 * sim_->engineSpec().redlineRpm && gear > 1 && sinceShift_ > 1.5) { shiftT_ = 0; target_ = gear - 1; sinceShift_ = 0; }
        }
        if (gear > 1 && v < 2.0) { shiftT_ = 0; target_ = 1; launching_ = true; launchPedal_ = 1.0; }
        if (pt.rpm() < sim_->engineSpec().idleRpm * 0.9 && gear == 1) { launching_ = true; launchPedal_ = 0.6; }
    }
    pt.setClutchPedal(clutch);
    pt.setThrottle(std::clamp(thr, 0.0, 1.0));
}

void RoadCar::update(double dt, const RoadControls& c) {
    const double v = sim_->speed();
    driverAssist(dt, c.throttle);
    sim_->setSurfaceMu(offRoad() ? 0.55 : 1.0);                       // cim/toprak
    {   // yol egimi arac yonune izdusurulur (ters yonde giderken yokus inis olur)
        const RoadPoint p = road_.at(s_);
        sim_->setGrade(p.grade * std::cos(sim_->heading() - p.heading));
    }
    VehicleInputs in; in.steer = c.steer; in.brake = c.brake;
    if (assist && v > 3.0) {
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
    offT_ = std::fabs(lat_) > road_.halfWidth() + 12.0 ? offT_ + dt : 0.0;
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
