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

#include <algorithm>
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
    double vx() const { return vx_; }                    // boylamsal hiz (geri viteste negatif)
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
    void nudge(double dx, double dy) { X_ += dx; Y_ += dy; }
    // Carpisma (duzlemsel): dunya eksenlerinde hiz, kutle, yaw ataleti ve dunya impulsu (rx, ry: temas noktasi - agirlik merkezi)
    void worldVelocity(double& vx, double& vy) const {
        const double c = std::cos(psi_), s = std::sin(psi_);
        vx = vx_ * c - vy_ * s; vy = vx_ * s + vy_ * c;
    }
    double mass() const { return vl_.mass; }
    double yawInertia() const { return Iz_; }
    void applyImpulse(double jx, double jy, double rx, double ry) {
        const double c = std::cos(psi_), s = std::sin(psi_), m = vl_.mass;
        vx_ += (jx * c + jy * s) / m;
        vy_ += (-jx * s + jy * c) / m;
        r_ += (rx * jy - ry * jx) / Iz_;
        if (vx_ < vxMin()) vx_ = vxMin();             // geri vites yoksa geri gitmez (planar adimla ayni kural)
        V_ = vx_;
    }
    void scaleVelocity(double f) { vx_ *= f; vy_ *= f; r_ *= f; V_ = vx_; }   // carpisma: hiz kaybi
    void resetPose(double x, double y, double psi) { X_ = x; Y_ = y; psi_ = psi; vx_ = vy_ = r_ = 0.0; V_ = 0.0; }
    void setSurfaceMu(double mu) { for (auto& w : w_) w.setSurfaceMu(mu); }   // asfalt 1.0, cim/toprak ~0.55
    // Yol egimi arac burnu yonunde (dz/ds, yokus yukari +); yalnizca duzlemsel modda (drag 1B fizigi duz kalir)
    void setGrade(double g) { grade_ = std::clamp(g, -0.3, 0.3); }
    double grade() const { return grade_; }
    // Geri vites (otomatik R): motor bos, gaz ile en fazla ~16 km/h geri cekis (basit tahrik)
    void setReverse(bool on, double thr) { reverse_ = on; revThr_ = on ? std::clamp(thr, 0.0, 1.0) : 0.0; }
    bool reverse() const { return reverse_; }
    double vxMin() const { return reverse_ ? -4.5 : 0.0; }
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
    // Elektronik: ABS ve cekis kontrolu (fabrika ya da ECU kiti). TC surucu tarafindan kapatilabilir; ABS hep acik.
    bool hasAbs() const { return absAvail_; }
    bool hasTc() const { return tcAvail_; }
    void setTractionControl(bool on) { tcOn_ = on && tcAvail_; }
    bool tractionControl() const { return tcOn_; }
    bool tcActive() const { return tcLim_ < 0.98; }   // su an tork kesiyor (gosterge)
    double shiftRpm() const { return eng_.redlineRpm - 250.0; }
    double baseMassKg() const { return baseMass_; }
    double axleDiameterMm() const { return fail_->axle().diameterMm; }
    double rideFreqHz() const { return fRide_; }
    const RoadProfile& road() const { return *road_; }
    const VehicleSimConfig& config() const { return cfg_; }
    // Nitro (NOS): tam gaz + 3000 rpm ustu + viteste otomatik; tup suresi (s) biter
    bool hasNitrous() const { return nosHp_ > 0.0; }
    double nosHp() const { return nosHp_; }
    double nosBottleS() const { return nosBottle_; }
    void setNosFill(double f) { nosLeft_ = nosBottle_ * std::clamp(f, 0.0, 1.0); }   // kariyer: tupte kalan (0..1)
    bool nitrousActive() const { return nosActive_; }
    double nitrousLeft() const { return nosBottle_ > 0 ? nosLeft_ / nosBottle_ : 0.0; }
    double boostNow() const;                        // anlik manifold basinci (bar; gaz kesik: vakum, atmosferik: 0)
    double boostMax() const { return boostTot_; }   // tam dolmus basinc (bar)
    double downforceN() const { return dfK_ * speed() * speed(); }
    // Motor isisi (C) ve zorlanma: 0..1 (1 = patladi / kirildi); dayanim N*m
    double coolantC() const { return coolT_; }
    // Vuruntu: yakitin oktani, motorun istedigi oktan (boost, sikistirma, ECU haritasi, ara sogutucu; sicakta artar).
    // Yetmezse vuruntu sensorlu ECU avansi geri ceker (guc duser); STANDALONE / YARIS HARITASI korumaz -> motor hasari.
    double octane() const { return octane_; }
    double octaneRequired() const { return knockReq_; }
    bool knocking() const { return knockNow_; }
    double tireWearGained() const { return tireWear_; }
    double valveSafeRpm() const { return valveSafeRpm_; }   // supaplarin guvenli devri (ustu: supap atmasi -> motor hasari)   // bu surusteki lastik asinmasi (patinaj / kayma; kariyere yazilir)
    double engineStress() const { return std::min(1.0, stress_); }
    double gearboxStress() const { return std::min(1.0, gbStress_); }
    bool engineBlown() const { return engBlown_; }
    bool gearboxBroken() const { return gbBroken_; }
    double engineRatingNm() const { return engRating_; }
    double gearboxRatingNm() const { return gbRating_; }
    std::vector<std::string> drainFailEvents() { auto v = std::move(failEvents_); failEvents_.clear(); return v; }

private:
    void stepPlanar(double dt, const VehicleInputs& in);
    void updateElectronics(double dt, double brake);
    void updateNitrous(double dt);
    void updateHeatAndStress(double dt);
    double coolCap_ = 0, coolLow_ = 0.4, coolT_ = 88.0, heatLim_ = 1.0, teF_ = 0, stress_ = 0, gbStress_ = 0;
    double octane_ = 100, knockReq_ = 0, knockLim_ = 1.0, tmax_ = 1.0, boostTot_ = 0, boostFac_ = 0, icCredit_ = 0, ecuAgg_ = 0;
    bool knockSensor_ = true, knockNow_ = false, knockWarned_ = false;
    double tireWear_ = 0, valveSafeRpm_ = 0, fineKnock_ = 0, heatMul_ = 1.0;
    bool reverse_ = false; double revThr_ = 0.0;
    double spoolLo_ = 0, spoolHi_ = 0; bool superOnly_ = false;
    bool valveWarned_ = false;
    double engRating_ = 0, gbRating_ = 0;
    bool engBlown_ = false, gbBroken_ = false;
    std::vector<std::string> failEvents_;
    double nosHp_ = 0, nosLeft_ = 0, nosBottle_ = 0, dfK_ = 0, tcSlip_ = 0.10, absSlip_ = 0.12;
    bool nosActive_ = false;
    bool absAvail_ = false, tcAvail_ = false, tcOn_ = false;
    double absF_[4] = {1.0, 1.0, 1.0, 1.0}, tcLim_ = 1.0;
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
    double X_ = 0, Y_ = 0, psi_ = 0, vx_ = 0, vy_ = 0, r_ = 0, ayRaw_ = 0, Iz_ = 1500, grade_ = 0;
    int suspCounter_ = 0;
    bool wasAir_[4] = {false, false, false, false}, wasStop_[4] = {false, false, false, false};
    std::vector<SuspEvent> suspEvents_;
};

} // namespace zk
