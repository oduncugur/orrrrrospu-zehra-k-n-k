#include "VehicleSim.h"
#include <algorithm>
#include <cmath>

namespace zk {

namespace {
// Parca etkileri icin tork egrisi carpani (rpm'e bagli olabilir: turbo kiti spool)
void scaleCurves(EngineSpec& e, double (*f)(double rpm, const void*), const void* ctx) {
    for (auto* c : {&e.lowCam, &e.highCam}) for (auto& pr : *c) pr.second *= f(pr.first, ctx);
}
struct TurboCtx { double full, spoolStart, spoolFull; };
double turboMul(double rpm, const void* c) {
    const TurboCtx& t = *static_cast<const TurboCtx*>(c);
    const double x = std::clamp((rpm - t.spoolStart) / (t.spoolFull - t.spoolStart), 0.0, 1.0);
    return 1.0 + (t.full - 1.0) * x * x * (3 - 2 * x);
}
double constMul(double, const void* c) { return *static_cast<const double*>(c); }
double curveMax(const EngineSpec& e) {
    double m = 0.0;
    for (auto* c : {&e.lowCam, &e.highCam}) for (auto& pr : *c) m = std::max(m, pr.second);
    return m;
}
} // namespace

VehicleSim::VehicleSim(const VehicleSimConfig& cfg) : cfg_(cfg) {
    const VehicleDef* car = cfg.car;
    const Tune* tune = cfg.tune;
    if (tune) { cfg_.drySump = tune->drySump; cfg_.fuel = tune->fuel; }
    eng_ = car ? buildEngineSpec(*car) : EngineSpec::K20Default();
    gbx_ = car ? buildGearbox(*car) : GearboxSpec{};
    eng_.gasketMm = cfg_.gasketMm; eng_.fuel = cfg_.fuel;
    if (!car) {
        eng_.valvetrain = cfg.valvetrain;
        if (cfg.plenum) { eng_.intake = IntakeType::Plenum; eng_.name = "K20A (Plenum, 16V i-VTEC)"; }
    }
    const double TmaxFactory = curveMax(eng_);          // aks capi fabrika torkuna gore boyutlanir
    DiffSpec diff;
    if (tune) {
        // ---- parcalar ----
        const EngineDef* ed = car ? &engineTable()[car->engine] : nullptr;
        const bool forced = ed && (ed->induction == Induction::Turbo || ed->induction == Induction::TwinTurbo ||
                                   ed->induction == Induction::Supercharger);
        double mul = (1.0 + 0.03 * tune->intake) * (1.0 + 0.03 * tune->exhaust) *
                     (1.0 + tune->ecu * ((forced || tune->turbo) ? 0.12 : 0.05));
        scaleCurves(eng_, constMul, &mul);
        if (tune->turbo > 0) {
            const double baseBoost = ed ? ed->boostBar : 0.0, add = 0.6 * tune->turbo;
            TurboCtx t{(1.0 + baseBoost + add) / (1.0 + baseBoost), eng_.redlineRpm * (0.32 + 0.08 * tune->turbo),
                       eng_.redlineRpm * (0.55 + 0.07 * tune->turbo)};
            scaleCurves(eng_, turboMul, &t);
        }
        if (tune->finalDrive > 0.0) gbx_.finalDrive = tune->finalDrive;
        switch (tune->diff) {
        case DiffType::Open: diff.preload = 0.0; diff.plateFactor = 0.0; break;
        case DiffType::OneAndHalfWay: break;                                   // 45/60 rampa (varsayilan)
        case DiffType::TwoWay: diff.rampDecelDeg = 45.0; diff.preload = 80.0; break;
        // Spool (kaynakli): tam kilide yakin; 2500 Nm, 50 us adimda sayisal kararlilik siniri icinde
        case DiffType::Spool: diff.preload = 2500.0; diff.plateFactor = 1.0; break;
        }
    }
    double Tmax = curveMax(eng_);
    ClutchSpec clutch;
    if (car) clutch.maxTorque = Tmax * 1.7;          // sokak baskisi: motor torkunun ~1.7 kati
    if (tune) { static const double cm[4] = {1.0, 1.3, 1.6, 2.0}; clutch.maxTorque *= cm[std::clamp(tune->clutch, 0, 3)]; }
    pt_ = std::make_unique<PowertrainCore>(eng_, clutch, diff, gbx_);
    drive_ = car ? car->drive : Drive::FWD;
    boxType_ = car ? gearboxTable()[car->gearbox].type : Gearbox::HPattern;
    // Cekis dagitimi: AWD'de merkez dagitim %40 on / %60 arka (sabit oranli)
    frontShare_ = drive_ == Drive::FWD ? 1.0 : drive_ == Drive::RWD ? 0.0 : 0.40;
    dL_ = drive_ == Drive::RWD ? 2 : 0;
    // Aks capi (fabrika muhendisligi): stok debriyajin aktarabilecegi en yuksek torkta (1.7 x FABRIKA tepe
    // torku, 1. vites) stok celigin kopma dayanimimin %75'i. Stok arac stok kalkista saglam kalir; guc ve
    // debriyaj yukseltildikce stok aksin kirilma riski gercekci bicimde dogar.
    const int axleLevel = tune ? std::clamp(tune->axles, 0, 2) : (cfg.stockAxles ? 0 : 1);
    AxleSpec axle = axleLevel == 2 ? AxleSpec::Race() : axleLevel == 1 ? AxleSpec::Chromoly() : AxleSpec::Stock();
    if (car) {
        const GearboxSpec factory = buildGearbox(*car);
        const double G1 = factory.ratios.front() * factory.finalDrive;
        const double perSide = 1.7 * TmaxFactory * G1 * factory.efficiency * 0.5 * std::max(frontShare_, 1.0 - frontShare_);
        const double dStock = std::cbrt(16.0 * perSide / (3.14159265 * 0.75 * AxleSpec::Stock().tauUltMPa * 1e6)) * 1000.0;
        axle.diameterMm = dStock * (axleLevel == 2 ? 1.25 : axleLevel == 1 ? 1.12 : 1.0);   // yukseltme akslari kalindir
    }
    fail_ = std::make_unique<DrivetrainFailure>(axle, LubeSpec{cfg_.drySump, cfg.oilLiters, 3.0});

    const double ambient = 25.0;
    const bool fDriven = frontShare_ > 0.0, rDriven = frontShare_ < 1.0;
    if (!tune) {
        TireParams slick;                           // tahrikli aks: yapiskan drag slick, dovme jant
        TireParams skinny; skinny.muPeak = 1.0; skinny.wheelMass = 9.0; skinny.B = 10.0; // serbest aks: ince "skinny"
        for (int i = 0; i < 4; ++i) {
            const bool d = i < 2 ? fDriven : rDriven;
            w_.emplace_back(d ? slick : skinny, d ? cfg.slickPsi : 32.0, d ? 55.0 : 30.0, ambient);
        }
    } else {
        // Lastik tipi: sokak / yari-slick / drag slick (tahrikli aks); serbest aks sokak lastigi
        // Sicaklik penceresi: sokak lastigi soguk da tutar (genis pencere), yari-slick ~75 C ister,
        // drag slick dar pencereli (burnout sart). Soguk sokak lastigi ~%97, soguk slick ~%55 tutus.
        TireParams street; street.muPeak = 1.05; street.B = 10.0; street.wheelMass = 16.0;
        street.tempIdeal = 55.0; street.tempWidth = 0.00004;
        TireParams semi;   semi.muPeak = 1.25;   semi.B = 11.0;   semi.wheelMass = 15.0;
        semi.tempIdeal = 75.0;   semi.tempWidth = 0.00007;
        TireParams slick;  // 1.45, dovme jant
        const TireParams* dt = tune->tires == TireType::DragSlick ? &slick : tune->tires == TireType::SemiSlick ? &semi : &street;
        const double defPsi = tune->tires == TireType::DragSlick ? 16.0 : tune->tires == TireType::SemiSlick ? 26.0 : 32.0;
        const double defTemp = tune->tires == TireType::DragSlick ? 55.0 : tune->tires == TireType::SemiSlick ? 45.0 : 35.0;
        for (int i = 0; i < 4; ++i) {
            const bool d = i < 2 ? fDriven : rDriven;
            if (d) w_.emplace_back(*dt, tune->psi > 0 ? tune->psi : defPsi, defTemp, ambient);
            else   w_.emplace_back(street, 32.0, 30.0, ambient);
        }
    }
    if (cfg.laneAsymmetry) w_[dL_].setSurfaceMu(0.96);

    fuelDensity_ = (cfg_.fuel == FuelType::E85) ? 0.785 : 0.745;
    baseMass_ = (car ? car->massKg : 1080.0) + 75.0 - (tune ? Tune::weightKg(tune->weight) : 0);   // kuru arac + surucu - hafifletme
    fuelKg_ = cfg.fuelLiters * fuelDensity_;
    const double hCoG = (car ? 0.36 * car->heightM : 0.50) - (cfg_.drySump ? 0.012 : 0.0);
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
    // Yaw ataleti: ~ m * (dingil/2)^2 * 1.1 (tipik binek: 1200 kg, 2.6 m -> ~2200 kg m^2)
    Iz_ = vl_.mass * std::pow(0.5 * vl_.wheelbase, 2.0) * 1.1 + 0.08 * vl_.mass * vl_.track * vl_.track;
}

double VehicleSim::defaultLaunchRpm() const {
    return cfg_.car ? std::clamp(0.55 * eng_.redlineRpm, 3000.0, 6500.0) : 6500.0;
}

void VehicleSim::step(double dt, const VehicleInputs& in) {
    if (cfg_.planar) { stepPlanar(dt, in); return; }
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

// ------------------------------------------------------------------------------------------------
// Duzlemsel dinamik: govde ekseninde vx (ileri), vy (sola), r (yaw, sola +). Tekerlek konumlari CoG'ye gore.
//   m (vx' - r vy) = SumFx - suruklemex      m (vy' + r vx) = SumFy - suruklemey      Iz r' = Mz
// Guc aktarma, ariza ve suspansiyon 1B yolla ayni; suspansiyona gercek yanal ivme verilir.
// ------------------------------------------------------------------------------------------------
void VehicleSim::stepPlanar(double dt, const VehicleInputs& in) {
    PowertrainCore& pt = *pt_;
    DrivetrainFailure& fail = *fail_;
    suspEvents_.clear();

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
    fail.updateOil(dt, axF_, ayRaw_, pt.rpm());

    vl_.mass = baseMass_ + fuelKg_;
    if (++suspCounter_ >= 50) {
        suspCounter_ = 0;
        susp_->step(50 * dt, *road_, dist_, axRaw_, ayRaw_);
        for (int c = 0; c < 4; ++c) {
            if (susp_->airborne(c) && !wasAir_[c]) suspEvents_.push_back({c, true, dist_});
            if (susp_->onBumpStop(c) && !wasStop_[c]) suspEvents_.push_back({c, false, dist_});
            wasAir_[c] = susp_->airborne(c); wasStop_[c] = susp_->onBumpStop(c);
        }
    }

    const double a = vl_.wheelbase * (1.0 - vl_.frontStatic), b = vl_.wheelbase * vl_.frontStatic, t2 = 0.5 * vl_.track;
    const double xs[4] = {a, a, -b, -b}, ys[4] = {t2, -t2, t2, -t2};
    const double bias = 0.65;
    const double bF = in.brake * brakeTotal_ * bias * 0.5;
    const double bR = in.brake * brakeTotal_ * (1 - bias) * 0.5 + in.handbrake * 1500.0;
    double Fx = 0.0, Fy = 0.0, Mz = 0.0;
    for (int i = 0; i < 4; ++i) {
        const double d = i < 2 ? in.steer : 0.0, cd = std::cos(d), sd = std::sin(d);
        const double vxi = vx_ - r_ * ys[i], vyi = vy_ + r_ * xs[i];            // tekerlek temas noktasi hizi (govde)
        const double vlong = vxi * cd + vyi * sd, vlat = -vxi * sd + vyi * cd;  // tekerlek ekseni
        w_[i].setNormalLoad(susp_->tireLoad(i));
        const double share = i < 2 ? fs : 1.0 - fs;
        const double Ta = share * ((i % 2 == 0) ? pt.axleTorqueL() : pt.axleTorqueR());
        w_[i].stepPlanar(dt, vlong, vlat, Ta, i < 2 ? bF : bR);
        const double fx = w_[i].Fx(), fy = w_[i].Fy();
        const double bx = fx * cd - fy * sd, by = fx * sd + fy * cd;           // govde eksenine
        Fx += bx; Fy += by;
        Mz += xs[i] * by - ys[i] * bx;
        const double h = w_[i].consumeHapticPulse();
        if (h > 0.0) haptic_ = std::max(haptic_, h);
    }
    const double V = std::sqrt(vx_ * vx_ + vy_ * vy_);
    const double drag = 0.5 * 1.20 * CdA_ * V * V;
    if (V > 1e-6) { Fx -= drag * vx_ / V; Fy -= drag * vy_ / V; }

    if (in.held) { vx_ = vy_ = r_ = 0.0; axRaw_ = ayRaw_ = 0.0; }
    else {
        const double m = vl_.mass;
        axRaw_ = Fx / m; ayRaw_ = Fy / m;                                      // hissedilen ivme (suspansiyon, yag)
        vx_ += (axRaw_ + r_ * vy_) * dt;
        vy_ += (ayRaw_ - r_ * vx_) * dt;
        r_ += Mz / Iz_ * dt;
        if (vx_ < 0.0) vx_ = 0.0;                                              // geri vites yok
        // Duran arac: kalinti yanal hiz ve yaw sonumlenir (statik surtunme)
        if (vx_ < 0.3 && std::fabs(vy_) < 0.3) { vy_ *= 0.999; r_ *= 0.999; }
    }
    axF_ += (axRaw_ - axF_) * std::min(1.0, dt / 0.08);
    psi_ += r_ * dt;
    const double c = std::cos(psi_), s = std::sin(psi_);
    X_ += (vx_ * c - vy_ * s) * dt;
    Y_ += (vx_ * s + vy_ * c) * dt;
    dist_ += std::sqrt(vx_ * vx_ + vy_ * vy_) * dt;
    V_ = vx_;
    fuelKg_ = std::max(0.0, cfg_.fuelLiters * fuelDensity_ - pt.fuelGrams() * 1e-3);
}

} // namespace zk
