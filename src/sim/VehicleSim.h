// ZEHRA KINIK - Tam arac fizigi (tek sinif): guc aktarma + 4 tekerlek + suspansiyon + ariza + govde.
// Konsol simulasyonu, drag yarisi, yapay zeka rakip ve (ileride) sunucu dogrulamasi ayni sinifi kullanir.
// Deterministiktir: ayni girdi dizisi ayni sonucu verir (rollback netcode on kosulu).
#pragma once
#include "garage/VehicleCatalog.h"
#include "sim/DrivetrainFailure.h"
#include "sim/PowertrainCore.h"
#include "sim/Suspension.h"
#include "sim/Tune.h"
#include "sim/WheelSimulation.h"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace zk {

struct VehicleSimConfig {
    const VehicleDef* car = nullptr;   // nullptr: referans K20A hatchback (Faz 1 araci)
    bool   stockAxles = false, drySump = false;
    double oilLiters = 4.5, slickPsi = 16.0, fuelLiters = 8.0, gasketMm = 0.70;
    FuelType fuel = FuelType::Race100;
    Valvetrain valvetrain = Valvetrain::V16;   // yalnizca referans arac
    bool   plenum = false;                      // yalnizca referans arac
    std::string road = "drag";
    bool   laneAsymmetry = true;                // sol iz daha az lastik kaplamali (LSD davranisini gosterir)
    const Tune* tune = nullptr;                 // nullptr: Faz 1 drag kurulumu (slick, krom-moly, 1.5-way)
    bool   planar = false;                      // true: duzlemsel dinamik (viraj, yaw); false: 1B drag fizigi (birebir)
};

struct VehicleInputs {
    double brake = 0.0;       // 0..1 servis freni (ABS yok, %65 on / %35 arka)
    double handbrake = 0.0;   // 0..1 arka hidrolik el freni
    bool   held = false;      // line-lock / stage: arac yerinde tutulur (burnout, agac)
    double steer = 0.0;       // on tekerlek direksiyon acisi (rad, sola +); yalnizca duzlemsel modda
};

// Suspansiyon olayi (tekerlek havalanmasi ya da takoza vurma), olustugu andaki konumla
struct SuspEvent { int corner; bool airborne; double atDistance; };

class VehicleSim {
public:
    explicit VehicleSim(const VehicleSimConfig& cfg);

    // Surucu kontrolleri PowertrainCore uzerinden (gaz, debriyaj, vites, 2-step) + frenler burada
    PowertrainCore& powertrain() { return *pt_; }
    const PowertrainCore& powertrain() const { return *pt_; }
    // Bir fizik alt adimi (onerilen dt = 1e-5 s)
    void step(double dt, const VehicleInputs& in);
    // Son adimda olusan suspansiyon olaylari (her adimda yenilenir)
    const std::vector<SuspEvent>& suspEvents() const { return suspEvents_; }

    // Durum
    double speed() const { return cfg_.planar ? std::sqrt(vx_ * vx_ + vy_ * vy_) : V_; }
    // Duzlemsel durum (dunya: X ileri baslangic yonu, Y sola; psi sola donus +)
    double posX() const { return X_; }
    double posY() const { return Y_; }
    double heading() const { return psi_; }
    double vxBody() const { return vx_; }
    double vyBody() const { return vy_; }
    double yawRate() const { return r_; }
    double lateralAccel() const { return ayRaw_; }
    double bodySlipAngle() const { return std::atan2(vy_, std::max(std::fabs(vx_), 0.5)); }
    double distance() const { return dist_; }
    double accel() const { return axRaw_; }
    double accelFiltered() const { return axF_; }
    double fuelKg() const { return fuelKg_; }
    double hapticIntensity() const { return haptic_; }
    const WheelSimulation& wheel(int i) const { return w_[i]; }
    // Aci yolda arac kurtarma: konum/yon ayarla, hizlar sifir
    void scaleVelocity(double f) { vx_ *= f; vy_ *= f; r_ *= f; V_ = vx_; }   // carpisma: hiz kaybi
    void resetPose(double x, double y, double psi) { X_ = x; Y_ = y; psi_ = psi; vx_ = vy_ = r_ = 0.0; V_ = 0.0; }
    void setSurfaceMu(double mu) { for (auto& w : w_) w.setSurfaceMu(mu); }   // asfalt 1.0, cim/toprak ~0.55
    int drivenLeft() const { return dL_; }
    int drivenRight() const { return dL_ + 1; }
    const DrivetrainFailure& failure() const { return *fail_; }
    std::vector<std::string> drainFailureEvents() { return fail_->drainEvents(); }
    const Suspension& suspension() const { return *susp_; }
    const VehicleLoad& vehicleLoad() const { return vl_; }

    // Kurulum bilgisi
    const EngineSpec& engineSpec() const { return eng_; }
    const GearboxSpec& gearboxSpec() const { return gbx_; }
    Gearbox gearboxType() const { return boxType_; }
    Drive drive() const { return drive_; }
    double defaultLaunchRpm() const;
    double shiftRpm() const { return eng_.redlineRpm - 250.0; }
    double baseMassKg() const { return baseMass_; }
    double axleDiameterMm() const { return fail_->axle().diameterMm; }
    double rideFreqHz() const { return fRide_; }
    const RoadProfile& road() const { return *road_; }
    const VehicleSimConfig& config() const { return cfg_; }

private:
    void stepPlanar(double dt, const VehicleInputs& in);
    VehicleSimConfig cfg_;
    EngineSpec eng_; GearboxSpec gbx_;
    Gearbox boxType_ = Gearbox::HPattern;
    Drive drive_ = Drive::FWD;
    double frontShare_ = 1.0;
    int dL_ = 0;
    std::unique_ptr<PowertrainCore> pt_;
    std::unique_ptr<DrivetrainFailure> fail_;
    std::vector<WheelSimulation> w_;
    std::unique_ptr<Suspension> susp_;
    std::unique_ptr<RoadProfile> road_;
    VehicleLoad vl_{};
    double baseMass_ = 0, fuelDensity_ = 0.745, fuelKg_ = 0, CdA_ = 0.68, brakeTotal_ = 7000, fRide_ = 1.6;
    double V_ = 0, dist_ = 0, axRaw_ = 0, axF_ = 0, haptic_ = 0;
    double X_ = 0, Y_ = 0, psi_ = 0, vx_ = 0, vy_ = 0, r_ = 0, ayRaw_ = 0, Iz_ = 1500;
    int suspCounter_ = 0;
    bool wasAir_[4] = {false, false, false, false}, wasStop_[4] = {false, false, false, false};
    std::vector<SuspEvent> suspEvents_;
};

} // namespace zk
