#include "WheelSimulation.h"
#include <algorithm>
#include <cmath>

namespace zk {

static constexpr double kPi = 3.14159265358979323846;

WheelSimulation::WheelSimulation(const TireParams& p, double psi, double tempC, double ambientC)
    : p_(p), psi_(psi), temp_(tempC), ambient_(ambientC) {
    inertia_ = 0.5 * p_.wheelMass * p_.radius * p_.radius;
}

double WheelSimulation::magicFormula(double s, double Fz, double mu, double B, double C, double E) {
    const double Bs = B * s;
    return Fz * mu * std::sin(C * std::atan(Bs - E * (Bs - std::atan(Bs))));
}

double WheelSimulation::rEff() const {
    // Dikey yay sertligi PSI ile artar; efektif yuvarlanma yaricapi ~ r0 - delta/3
    const double kVert = 120000.0 + 6500.0 * psi_;           // N/m
    const double deflection = Fz_ / kVert;
    // Yuksek devirde santrifuj buyume (drag slickleri belirgin uzar)
    const double growth = 1.5e-7 * omega_ * omega_ * p_.radius * (30.0 / std::max(psi_, 8.0));
    return p_.radius - deflection / 3.0 + growth;
}

double WheelSimulation::gripMu() const {
    const double dT = temp_ - p_.tempIdeal;
    const double tempF = std::clamp(1.0 - p_.tempWidth * dT * dT, 0.55, 1.0);
    // Dusuk PSI: yanak burulmasi (wrinkle wall) + genis temas yuzeyi. Yuksek PSI: az surtunme
    const double psiF = std::clamp(1.0 + 0.012 * (28.0 - psi_), 0.85, 1.15);
    const double loadF = 1.0 - p_.loadSens * (Fz_ / p_.loadNominal - 1.0);
    const double flatF = 1.0 - std::min(0.15, flatSpot_ * 0.05); // flat-spot temas kaybi
    return p_.muPeak * tempF * psiF * loadF * flatF * surfaceMu_;
}

void WheelSimulation::step(double dt, double Vx, double T_axle, double T_brakeCap) {
    const double r = rEff();
    const double absV = std::fabs(Vx);

    // Spec kayma orani (gosterim/telemetri icin)
    kappaSS_ = (omega_ * r - Vx) / std::max(absV, 0.1);

    // Gevseme boylu transient kayma: sigma*dk/dt = (w*r - Vx) - (|Vx|+v0)*k
    // Dusuk hizda sayisal sertligi onler, fiziksel olarak lastik karkasinin esnemesini temsil eder.
    const double v0 = 0.5;
    kappa_ += dt * ((omega_ * r - Vx) - (absV + v0) * kappa_) / p_.relaxLength;
    kappa_ = std::clamp(kappa_, -1.0, 25.0);

    Fx_ = magicFormula(kappa_, Fz_, gripMu(), p_.B, p_.C, p_.E);

    const double crr = p_.Crr * (1.0 + 0.01 * std::max(0.0, 32.0 - psi_)) * (1.0 + flatSpot_ * 0.2);
    const double T_roll = crr * Fz_ * r * std::tanh(omega_ * 4.0);
    const double T_net = T_axle - Fx_ * r - T_roll;

    // Fren: Coulomb surtunme; tekerlek durursa ve kapasite yeterliyse kilitli kalir
    if (T_brakeCap > 0.0 && std::fabs(omega_) < 1e-3 && T_brakeCap >= std::fabs(T_net)) {
        omega_ = 0.0;
    } else {
        const double Tb = (std::fabs(omega_) > 1e-3) ? -T_brakeCap * (omega_ > 0 ? 1.0 : -1.0)
                                                     : -std::clamp(T_net, -T_brakeCap, T_brakeCap);
        const double prev = omega_;
        omega_ += dt * (T_net + Tb) / inertia_;
        if (T_brakeCap > 0.0 && prev * omega_ < 0.0 && std::fabs(T_net) <= T_brakeCap) omega_ = 0.0;
    }

    // Kilitlenme & flat-spot: ABS yok, tekerlek durmus ama arac kayiyor
    locked_ = absV > 1.5 && std::fabs(omega_ * r) < 0.05 * absV;
    const double slipSpeed = std::fabs(omega_ * r - Vx);
    const double slipPower = std::fabs(Fx_) * slipSpeed;
    if (locked_) {
        // Sicak kaucuk daha kolay erir
        const double meltF = 1.0 + std::max(0.0, temp_ - 60.0) * 0.02;
        flatSpot_ += p_.flatSpotRate * slipPower * dt * meltF;
    }

    // Flat-spot haptigi: her tam turda bir darbe (siddet ~ derinlik)
    revPhase_ += std::fabs(omega_) * dt;
    if (revPhase_ >= 2.0 * kPi) {
        revPhase_ -= 2.0 * kPi;
        if (flatSpot_ > 0.05) { haptic_ = std::min(1.0, flatSpot_ / 2.0); ++hapticCount_; }
    }

    // Sicaklik: kayma isisi + histerezis (dusuk PSI'da yanak esnemesi) - konveksiyon
    const double hyst = 0.25 * crr * Fz_ * absV * (1.0 + 0.03 * std::max(0.0, 30.0 - psi_));
    const double cool = (12.0 + 6.0 * absV) * (temp_ - ambient_);
    temp_ += dt * (p_.slipHeatFrac * slipPower + hyst - cool) / p_.treadHeatCap;
}

void VehicleLoad::compute(double ax, double ay, double dfF, double dfR, double out[4]) const {
    const double g = 9.81;
    const double W = mass * g;
    // dFz_long = m*ax*h/L  (on aksdan arkaya)
    const double dLong = mass * ax * hCoG / wheelbase;
    double front = W * frontStatic - dLong + dfF;
    double rear  = W * (1.0 - frontStatic) + dLong + dfR;
    front = std::max(front, 0.0); rear = std::max(rear, 0.0);
    // dFz_lat = m*ay*h/W_track (aks yuk oranina gore dagitilir)
    const double dLat = mass * ay * hCoG / track;
    const double fShare = front / std::max(front + rear, 1.0);
    out[0] = std::max(0.0, front * 0.5 - dLat * fShare * 0.5);
    out[1] = std::max(0.0, front * 0.5 + dLat * fShare * 0.5);
    out[2] = std::max(0.0, rear * 0.5 - dLat * (1 - fShare) * 0.5);
    out[3] = std::max(0.0, rear * 0.5 + dLat * (1 - fShare) * 0.5);
}

} // namespace zk
