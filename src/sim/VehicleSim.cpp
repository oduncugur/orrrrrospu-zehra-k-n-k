#include "VehicleSim.h"
#include <algorithm>
#include <cmath>

namespace zk {

VehicleSim::VehicleSim(const VehicleSimConfig& cfg) : cfg_(cfg) {
    const VehicleDef* car = cfg.car;
    eng_ = car ? buildEngineSpec(*car) : EngineSpec::K20Default();
    gbx_ = car ? buildGearbox(*car) : GearboxSpec{};
    eng_.gasketMm = cfg.gasketMm; eng_.fuel = cfg.fuel;
    if (!car) {
        eng_.valvetrain = cfg.valvetrain;
        if (cfg.plenum) { eng_.intake = IntakeType::Plenum; eng_.name = "K20A (Plenum, 16V i-VTEC)"; }
    }
    double Tmax = 0.0;
    for (auto* c : {&eng_.lowCam, &eng_.highCam}) for (auto& pr : *c) Tmax = std::max(Tmax, pr.second);
    ClutchSpec clutch;
    if (car) clutch.maxTorque = Tmax * 1.7;          // sokak baskisi: motor torkunun ~1.7 kati
    pt_ = std::make_unique<PowertrainCore>(eng_, clutch, DiffSpec{}, gbx_);
    drive_ = car ? car->drive : Drive::FWD;
    boxType_ = car ? gearboxTable()[car->gearbox].type : Gearbox::HPattern;
    // Cekis dagitimi: AWD'de merkez dagitim %40 on / %60 arka (sabit oranli)
    frontShare_ = drive_ == Drive::FWD ? 1.0 : drive_ == Drive::RWD ? 0.0 : 0.40;
    dL_ = drive_ == Drive::RWD ? 2 : 0;
    // Aks capi: fabrika muhendisligi, 1. viteste tepe motor torkunda stok kopma gerilmesinin ~%55'i
    AxleSpec axle = cfg.stockAxles ? AxleSpec::Stock() : AxleSpec::Chromoly();
    if (car) {
        const double G1 = gbx_.ratios.front() * gbx_.finalDrive;
        const double perSide = Tmax * G1 * gbx_.efficiency * 0.5 * std::max(frontShare_, 1.0 - frontShare_);
        axle.diameterMm = std::cbrt(16.0 * perSide / (3.14159265 * 0.55 * AxleSpec::Stock().tauUltMPa * 1e6)) * 1000.0;
    }
    fail_ = std::make_unique<DrivetrainFailure>(axle, LubeSpec{cfg.drySump, cfg.oilLiters, 3.0});

    TireParams slick;                           // tahrikli aks: yapiskan drag slick, dovme jant
    TireParams skinny; skinny.muPeak = 1.0; skinny.wheelMass = 9.0; skinny.B = 10.0; // serbest aks: ince "skinny"
    const double ambient = 25.0;
    const bool fDriven = frontShare_ > 0.0, rDriven = frontShare_ < 1.0;
    for (int i = 0; i < 4; ++i) {
        const bool d = i < 2 ? fDriven : rDriven;
        w_.emplace_back(d ? slick : skinny, d ? cfg.slickPsi : 32.0, d ? 55.0 : 30.0, ambient);
    }
    if (cfg.laneAsymmetry) w_[dL_].setSurfaceMu(0.96);

    fuelDensity_ = (cfg.fuel == FuelType::E85) ? 0.785 : 0.745;
    baseMass_ = (car ? car->massKg : 1080.0) + 75.0;      // kuru arac + surucu
    fuelKg_ = cfg.fuelLiters * fuelDensity_;
    const double hCoG = (car ? 0.36 * car->heightM : 0.50) - (cfg.drySump ? 0.012 : 0.0);
    vl_ = VehicleLoad{baseMass_ + fuelKg_, car ? car->wheelbaseM : 2.62, car ? car->widthM * 0.85 : 1.50, hCoG,
                      car ? car->frontWeight : 0.62};
    // Suspansiyon: kasa tipine gore dogal frekans (Hz); yaris araclari sert
    fRide_ = 1.6;
    if (car) {
        switch (car->body) {
        case Body::Super: fRide_ = 2.2; break;   case Body::Roadster: fRide_ = 1.9; break;
        case Body::Muscle: fRide_ = 1.4; break;  case Body::SUV: case Body::Pickup: case Body::Van: fRide_ = 1.2; break;
        case Body::Sedan: case Body::Wagon: fRide_ = 1.45; break; default: fRide_ = 1.7; break;
        }
        if (!car->streetLegal) fRide_ = 2.8;
    }
    susp_ = std::make_unique<Suspension>(SuspensionSetup::fromVehicle(vl_.mass, vl_.wheelbase, vl_.track, vl_.hCoG,
                                                                      vl_.frontStatic, fRide_, fRide_ * 1.1, 0.30, 0.60));
    for (int i = 0; i < 4; ++i) susp_->setTirePressure(i, w_[i].psi());
    road_ = std::make_unique<RoadProfile>(RoadProfile::preset(cfg.road));
    if (car) {
        const double cd = car->body == Body::Super ? 0.34 : car->body == Body::SUV || car->body == Body::Pickup ? 0.45
                        : car->body == Body::Van ? 0.48 : car->body == Body::Muscle ? 0.40 : 0.34;
        CdA_ = cd * car->widthM * car->heightM * 0.85;
    }
    brakeTotal_ = 7000.0 * (baseMass_ / 1155.0);
}

double VehicleSim::defaultLaunchRpm() const {
    return cfg_.car ? std::clamp(0.55 * eng_.redlineRpm, 3000.0, 6500.0) : 6500.0;
}

void VehicleSim::step(double dt, const VehicleInputs& in) {
    PowertrainCore& pt = *pt_;
    DrivetrainFailure& fail = *fail_;
    suspEvents_.clear();

    // ---- guc aktarma ----
    pt.setAxleSnapped(fail.snapped(0), fail.snapped(1));
    pt.setIgnitionKilled(fail.bearingSpun());
    const double fs = frontShare_;
    const double wLin = drive_ == Drive::AWD ? fs * w_[0].omega() + (1 - fs) * w_[2].omega() : w_[dL_].omega();
    const double wRin = drive_ == Drive::AWD ? fs * w_[1].omega() + (1 - fs) * w_[3].omega() : w_[dL_ + 1].omega();
    pt.step(dt, wLin, wRin, fail.oilPressureFactor());
    {
        const double sh = std::max(fs, 1.0 - fs);
        fail.updateAxles(dt, sh * pt.axleTorqueL(), sh * pt.axleTorqueR());
    }
    fail.updateOil(dt, axF_, 0.0, pt.rpm());

    // ---- suspansiyon (2 kHz) -> dinamik tekerlek yukleri ----
    vl_.mass = baseMass_ + fuelKg_;
    if (++suspCounter_ >= 50) {
        suspCounter_ = 0;
        susp_->step(50 * dt, *road_, dist_, axRaw_, 0.0);
        for (int c = 0; c < 4; ++c) {
            if (susp_->airborne(c) && !wasAir_[c]) suspEvents_.push_back({c, true, dist_});
            if (susp_->onBumpStop(c) && !wasStop_[c]) suspEvents_.push_back({c, false, dist_});
            wasAir_[c] = susp_->airborne(c); wasStop_[c] = susp_->onBumpStop(c);
        }
    }
    const double bias = 0.65;
    const double bF = in.brake * brakeTotal_ * bias * 0.5;
    const double bR = in.brake * brakeTotal_ * (1 - bias) * 0.5 + in.handbrake * 1500.0;
    double sumFx = 0.0;
    for (int i = 0; i < 4; ++i) {
        w_[i].setNormalLoad(susp_->tireLoad(i));
        const double share = i < 2 ? fs : 1.0 - fs;
        const double Ta = share * ((i % 2 == 0) ? pt.axleTorqueL() : pt.axleTorqueR());
        w_[i].step(dt, V_, Ta, i < 2 ? bF : bR);
        sumFx += w_[i].Fx();
        const double h = w_[i].consumeHapticPulse();
        if (h > 0.0) haptic_ = std::max(haptic_, h);
    }

    // ---- govde ----
    const double rho = 1.20;
    const double drag = 0.5 * rho * CdA_ * V_ * V_;
    if (in.held) { axRaw_ = 0.0; V_ = 0.0; }                 // line-lock / el freni tutar
    else {
        axRaw_ = (sumFx - drag) / vl_.mass;
        V_ += axRaw_ * dt;
        if (V_ < 0.0) V_ = 0.0;
    }
    axF_ += (axRaw_ - axF_) * std::min(1.0, dt / 0.08);       // govde pitch gecikmesi (suspansiyon)
    dist_ += V_ * dt;
    fuelKg_ = std::max(0.0, cfg_.fuelLiters * fuelDensity_ - pt.fuelGrams() * 1e-3);
}

} // namespace zk
