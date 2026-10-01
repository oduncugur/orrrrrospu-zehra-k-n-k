#include "PowertrainCore.h"
#include <algorithm>
#include <cmath>

namespace zk {

static constexpr double kPi = 3.14159265358979323846;
static constexpr double kRadToRpm = 60.0 / (2.0 * kPi);

static double interp(const TorqueCurve& c, double x) {
    if (x <= c.front().first) return c.front().second;
    if (x >= c.back().first)  return c.back().second;
    for (size_t i = 1; i < c.size(); ++i) {
        if (x <= c[i].first) {
            const double t = (x - c[i - 1].first) / (c[i].first - c[i - 1].first);
            return c[i - 1].second + t * (c[i].second - c[i - 1].second);
        }
    }
    return c.back().second;
}

EngineSpec EngineSpec::K20Default() {
    EngineSpec s;
    // Dusuk kam: sokak profili, 5000'de tepe yapip nefessiz kalir
    s.lowCam  = {{800, 110}, {2000, 150}, {3000, 165}, {4000, 172}, {5000, 175},
                 {5800, 168}, {6500, 150}, {7500, 125}, {8600, 95}};
    // Yuksek kam (yaris): alt devirde zayif, 5800 sonrasi agresif
    s.highCam = {{800, 85}, {2000, 112}, {3000, 135}, {4000, 155}, {5000, 180},
                 {5800, 196}, {6500, 206}, {7000, 210}, {7500, 206}, {8000, 199}, {8600, 182}};
    return s;
}

PowertrainCore::PowertrainCore(const EngineSpec& e, const ClutchSpec& c, const DiffSpec& d, const GearboxSpec& g)
    : e_(e), c_(c), d_(d), gb_(g) {
    omegaE_ = e_.idleRpm / kRadToRpm;
}

double PowertrainCore::rpm() const { return omegaE_ * kRadToRpm; }

void PowertrainCore::setConverter(bool on, double stallRpm, double tr0) {
    converter_ = on;
    convTr0_ = tr0;
    const double ws = stallRpm / kRadToRpm;
    convK_ = on && ws > 1.0 ? wotTorque(stallRpm) / (ws * ws) : 0.0;
    // Vites gecisinde hiz orani aniden duser ve K w^2 yuksek devirde motor torkunun kat kat ustune cikar; gercek
    // sanzimanda bunu kayan kavrama elemanlari sinirlar (vites darbesi). Sinir: tepe motor torku x 1.1.
    double tmax = 0.0;
    for (const TorqueCurve* c : {&e_.lowCam, &e_.highCam}) for (const auto& p : *c) tmax = std::max(tmax, p.second);
    convTiMax_ = 1.1 * tmax;
}
double PowertrainCore::powerHp() const { return std::max(0.0, Te_ * omegaE_) / 745.7; }

double PowertrainCore::totalRatio() const {
    if (gear_ <= 0 || gear_ > (int)gb_.ratios.size()) return 0.0;
    return gb_.ratios[gear_ - 1] * gb_.finalDrive;
}

double PowertrainCore::compressionRatio() const {
    const double boreCm = e_.boreMm / 10.0;
    const double area = kPi / 4.0 * boreCm * boreCm;
    const double Vs = area * e_.strokeMm / 10.0;
    const double Vc = e_.chamberCc + area * e_.gasketMm / 10.0;
    return (Vs + Vc) / Vc;
}

double PowertrainCore::wotTorque(double r) const {
    double t = interp(vtec_ ? e_.highCam : e_.lowCam, r);
    // Otto verimi: eta = 1 - CR^(1-gamma); referans CR 11.5
    const double g = 1.33;
    const double eta = 1.0 - std::pow(compressionRatio(), 1.0 - g);
    const double etaRef = 1.0 - std::pow(11.5, 1.0 - g);
    t *= eta / etaRef;
    switch (e_.valvetrain) {
        case Valvetrain::V8:  t *= (r < 4000 ? 1.06 : std::max(0.6, 1.06 - 0.00014 * (r - 4000))); break; // choke
        case Valvetrain::V20: t *= (r > 6500 ? 1.0 + 0.00004 * (r - 6500) : 0.98); break;
        default: break;
    }
    if (e_.stagedValves && r < 2500 && !vtec_) t *= 1.04;      // swirl ile hizli yanma
    if (e_.intake == IntakeType::ITB && r > 6000) t *= 1.03;    // kisa emme yolu rezonansi
    if (e_.fuel == FuelType::E85) t *= 1.06;                    // soguk sarj + acik avans
    if (e_.fuel == FuelType::Pump95) t *= 0.96;                 // vuruntu icin geri avans
    return t;
}

double PowertrainCore::frictionTorque(double r) const {
    // Surtunme + pompalama kaybi (kapali gaz motor freni)
    return 12.0 + 0.0032 * r + 1.2e-7 * r * r;
}

double PowertrainCore::bsfc(double r) const {
    // kg/(HP*h): tork tepesinde en verimli, uc devirde ve dusuk yukte kotu
    double b = 0.215 + 0.03 * std::pow((r - 5500.0) / 3500.0, 2.0);
    if (thrEff_ > 0.9) b *= 1.10;                 // tam gaz zengin karisim (guc AFR ~12.5)
    if (e_.fuel == FuelType::E85) b *= 1.35;       // %35 fazla debi
    return b;
}

void PowertrainCore::step(double dt, double wL, double wR, double oilFactor) {
    const double r = rpm();

    // Yag basinci ve VTEC (devir + yag basinci esigi, histerezisli)
    oilBar_ = std::min(5.5, 0.9 + r / 1600.0) * oilFactor;
    if (killed_ || stalled_) oilBar_ = 0.0;
    const bool wantHigh = vtec_ ? (r > e_.vtecRpm - e_.vtecHyst) : (r > e_.vtecRpm);
    const bool newVtec = wantHigh && oilBar_ >= e_.vtecMinOilBar && throttlePedal_ > 0.5;
    if (newVtec != vtec_) {
        vtec_ = newVtec;
        events_.push_back(vtec_ ? "VTEC KICKED IN YO! (yuksek kam @" + std::to_string((int)r) + " rpm)"
                                : "VTEC kapandi (dusuk kam @" + std::to_string((int)r) + " rpm)");
    }

    // Gaz kelebegi tepkisi: ITB silindir dibinde, emme plenumu doldurma gecikmesi yok
    const double tau = (e_.intake == IntakeType::ITB) ? 0.004 : 0.070;
    thrEff_ += (throttlePedal_ - thrEff_) * std::min(1.0, dt / tau);

    // Rolanti kontrolu (IACV) + ateslemesi kesiciler
    double thr = thrEff_;
    if (r < e_.idleRpm) thr = std::max(thr, std::clamp((e_.idleRpm - r) / 400.0, 0.0, 0.25) + 0.035);
    cut_ = r > e_.redlineRpm || (twoStep_ && r > twoStepRpm_);
    double Te;
    if (killed_ || stalled_) Te = -frictionTorque(r) * 3.0 * std::tanh(omegaE_);
    else if (cut_)           Te = -frictionTorque(r);
    else                     Te = wotTorque(r) * thr - frictionTorque(r) * (1.0 - thr) + (torqueAdd_ > 0.0 ? torqueAdd_ * thr : 0.0);
    if (Te > 0.0 && torqueLimit_ < 1.0) Te *= torqueLimit_;          // cekis kontrolu (atesleme geciktirme/kesme)
    Te_ = Te;

    // Analog debriyaj: pedal konumundan kavrama; balata sicakliginda fade
    const double eng = std::clamp((c_.biteStart - clutchPedal_) / (c_.biteStart - c_.biteEnd), 0.0, 1.0);
    const double engage = eng * eng * (3.0 - 2.0 * eng);
    const double fade = 1.0 - std::clamp((clutchTemp_ - c_.fadeStart) * 0.002, 0.0, 0.5);
    const double Tcap = c_.maxTorque * engage * fade;

    const double G = totalRatio();
    const double wC = G * 0.5 * (wL + wR);          // debriyaj cikis mili
    const double dW = (G != 0.0) ? (omegaE_ - wC) : 0.0;
    double Tout = 0.0;                              // sanziman girisine giden tork (konvertorde tork carpimi)
    if (converter_) {
        Tc_ = 0.0;
        if (G != 0.0 && clutchPedal_ < 0.99) {
            const double wE = omegaE_, wT = std::max(0.0, wC);
            if (wT <= wE) {                         // cekis: pompa turbini surer
                const double sr = wE > 1e-6 ? wT / wE : 1.0;
                Tc_ = std::min(convK_ * wE * wE * (1.0 - std::pow(sr, 6.0)), convTiMax_);
                Tout = Tc_ * (sr < 0.85 ? convTr0_ - (convTr0_ - 1.0) * sr / 0.85 : 1.0);
            } else {                                // gaz kesme: turbin pompayi surer (motor freni), carpim yok
                const double sr = wT > 1e-6 ? wE / wT : 1.0;
                Tc_ = -std::min(convK_ * wT * wT * (1.0 - std::pow(sr, 6.0)), convTiMax_);
                Tout = Tc_;
            }
        }
        if (snappedL_ && snappedR_) Tc_ = Tout = 0.0;
    } else {
        Tc_ = (G != 0.0) ? Tcap * std::tanh(dW / 1.0) : 0.0;
        if (snappedL_ && snappedR_) Tc_ = 0.0; // iki aks kopuk: sanziman cikisi bosta doner
        Tout = Tc_;
        clutchTemp_ += dt * (std::fabs(Tc_ * dW) * 0.9 - 8.0 * (clutchTemp_ - 40.0)) / c_.heatCap;
    }

    omegaE_ += dt * (Te - Tc_) / e_.inertia;
    if (omegaE_ < 0.0) omegaE_ = 0.0;
    if (!stalled_ && !killed_ && rpm() < 300.0) {
        stalled_ = true;
        events_.push_back("MOTOR STOP ETTI (stall)!");
    }

    // 1.5-Way LSD: Tbias = preload + |Tin| * k/tan(rampa); gaz kesmede daha dik rampa -> daha az kilit
    const double eff = (Tout >= 0.0) ? gb_.efficiency : 1.0 / gb_.efficiency;
    const double Tin = Tout * G * eff;
    const double ramp = (Tin >= 0.0 ? d_.rampAccelDeg : d_.rampDecelDeg) * kPi / 180.0;
    const double Tbias = d_.preload + std::fabs(Tin) * d_.plateFactor / std::tan(ramp);
    const double Tfr = Tbias * std::tanh((wL - wR) / 0.3); // hizli donen taraftan yavasa aktarim
    if (snappedL_ && snappedR_)      { TL_ = TR_ = 0.0; }
    else if (snappedL_) { TL_ = 0.0; TR_ = std::clamp(Tin, -Tbias, Tbias); }  // sadece kilit torku gecer
    else if (snappedR_) { TR_ = 0.0; TL_ = std::clamp(Tin, -Tbias, Tbias); }
    else { TL_ = 0.5 * (Tin - Tfr); TR_ = 0.5 * (Tin + Tfr); }

    // BSFC: dm/dt [kg/s] = HP * BSFC / 3600 ; kapali gazda yakit kesme (DFCO), rolantide min debi
    double flow = 0.0;
    if (!killed_ && !stalled_ && !cut_) {
        flow = powerHp() * bsfc(r) / 3600.0;
        if (thrEff_ < 0.02 && r > 1500.0) flow = 0.0;
        else flow = std::max(flow, 0.00025);
    }
    fuelFlow_ = flow;
    fuelG_ += flow * dt * 1000.0;
}

} // namespace zk
