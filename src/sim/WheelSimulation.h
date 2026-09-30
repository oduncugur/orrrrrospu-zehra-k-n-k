// ZEHRA KINIK - Faz 1: Tekerlek / lastik simulasyonu
// Birimler: SI (m, kg, s, N, N*m, rad/s). Sicaklik: Celsius. Basinc: PSI.
#pragma once

namespace zk {

struct TireParams {
    double wheelMass    = 13.0;   // kg, jant+lastik (dovme jant ~13, dokum ~17)
    double radius       = 0.300;  // m, yuksuz yaricap
    // Pacejka Magic Formula (boyuna): F = Fz*mu*sin(C*atan(B*s - E*(B*s - atan(B*s))))
    double B = 11.0, C = 1.65, E = 0.40;
    double muPeak       = 1.45;   // drag slick tepe surtunme katsayisi
    double tempIdeal    = 92.5;   // C, 80-105 C penceresinin ortasi
    double tempWidth    = 0.00012;// 1/C^2, sicaklik penceresi daralmasi
    double loadNominal  = 3500.0; // N, yuk hassasiyeti referansi
    double loadSens     = 0.06;   // Fz arttikca mu dusumu
    double Crr          = 0.012;  // yuvarlanma direnci katsayisi
    double relaxLength  = 0.22;   // m, lastik gevseme boyu (transient kayma)
    double treadHeatCap = 3000.0; // J/K, taban kaucugu isil kutlesi
    double slipHeatFrac = 0.25;   // kayma gucunun tabana giden orani
    double flatSpotRate = 2.0e-7; // mm/J, kilitli kaymada eriyen kaucuk
};

class WheelSimulation {
public:
    WheelSimulation(const TireParams& p, double psi, double tempC, double ambientC);

    // Dinamik yuk: statik + boyuna + yanal transfer (VehicleLoad ile hesaplanir)
    void setNormalLoad(double Fz) { Fz_ = Fz > 0.0 ? Fz : 0.0; }
    void setSurfaceMu(double s)   { surfaceMu_ = s; }
    void setPressure(double psi)  { psi_ = psi; }

    // I_w * dw/dt = T_axle - T_brake - Fx*r_eff - Crr*Fz*r_eff
    void step(double dt, double Vx, double T_axle, double T_brakeCapacity);

    double omega()        const { return omega_; }
    double kappa()        const { return kappa_; }      // transient (gevsemeli) kayma
    double kappaSteady()  const { return kappaSS_; }    // (w*r - Vx)/max(|Vx|,0.1)
    double Fx()           const { return Fx_; }
    double Fz()           const { return Fz_; }
    double psi()          const { return psi_; }
    double tempC()        const { return temp_; }
    double inertia()      const { return inertia_; }
    double rEff()         const;
    double gripMu()       const;
    double flatSpotMm()   const { return flatSpot_; }
    bool   locked()       const { return locked_; }
    // Flat-spot her turda bir haptik vuruntu uretir; 0 = vuruntu yok
    double consumeHapticPulse() { double h = haptic_; haptic_ = 0.0; return h; }
    int    hapticPulseCount() const { return hapticCount_; }

    // Pacejka (spec formulu); sigma = kayma orani ya da kayma acisi (rad)
    static double magicFormula(double sigma, double Fz, double mu, double B, double C, double E);

private:
    TireParams p_;
    double psi_, temp_, ambient_;
    double inertia_;          // 0.5*m*r^2
    double omega_ = 0.0;
    double kappa_ = 0.0, kappaSS_ = 0.0;
    double Fx_ = 0.0, Fz_ = 0.0;
    double surfaceMu_ = 1.0;
    double flatSpot_ = 0.0;   // mm
    double revPhase_ = 0.0;
    double haptic_ = 0.0;
    int    hapticCount_ = 0;
    bool   locked_ = false;
};

// Dinamik yuk transferi
struct VehicleLoad {
    double mass, wheelbase, track, hCoG, frontStatic; // frontStatic: on aks agirlik orani
    // ax>0 hizlanma (yuk arkaya), ay>0 sola viraj (yuk saga)
    // cikti: FL, FR, RL, RR
    void compute(double ax, double ay, double downforceF, double downforceR, double out[4]) const;
};

} // namespace zk
