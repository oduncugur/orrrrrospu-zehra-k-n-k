// ZEHRA KINIK - Tam arac fizigi (tek sinif): guc aktarma + 4 tekerlek + suspansiyon + ariza + govde.
// Konsol simulasyonu, drag yarisi, yapay zeka rakip ve (ileride) sunucu dogrulamasi ayni sinifi kullanir.
// Deterministiktir: ayni girdi dizisi ayni sonucu verir (rollback netcode on kosulu).
#pragma once
#include "garage/VehicleCatalog.h"
#include "sim/DrivetrainFailure.h"
#include "sim/PowertrainCore.h"
#include "sim/Suspension.h"
#include "sim/WheelSimulation.h"

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
};

struct VehicleInputs {
    double brake = 0.0;       // 0..1 servis freni (ABS yok, %65 on / %35 arka)
    double handbrake = 0.0;   // 0..1 arka hidrolik el freni
    bool   held = false;      // line-lock / stage: arac yerinde tutulur (burnout, agac)
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
    double speed() const { return V_; }
    double distance() const { return dist_; }
    double accel() const { return axRaw_; }
    double accelFiltered() const { return axF_; }
    double fuelKg() const { return fuelKg_; }
    double hapticIntensity() const { return haptic_; }
    const WheelSimulation& wheel(int i) const { return w_[i]; }
    int drivenLeft() const { return dL_; }
    int drivenRight() const { return dL_ + 1; }
    const DrivetrainFailure& failure() const { return *fail_; }
    std::vector<std::string> drainFailureEvents() { return fail_->drainEvents(); }
    const Suspension& suspension() const { return *susp_; }

    // Kurulum bilgisi
    const EngineSpec& engineSpec() const { return eng_; }
    const GearboxSpec& gearboxSpec() const { return gbx_; }
    Gearbox gearboxType() const { return boxType_; }
    Drive drive() const { return drive_; }
    double defaultLaunchRpm() const;
    double shiftRpm() const { return eng_.redlineRpm - 250.0; }
    double baseMassKg() const { return baseMass_; }
    double rideFreqHz() const { return fRide_; }
    const RoadProfile& road() const { return *road_; }
    const VehicleSimConfig& config() const { return cfg_; }

private:
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
    int suspCounter_ = 0;
    bool wasAir_[4] = {false, false, false, false}, wasStop_[4] = {false, false, false, false};
    std::vector<SuspEvent> suspEvents_;
};

} // namespace zk
