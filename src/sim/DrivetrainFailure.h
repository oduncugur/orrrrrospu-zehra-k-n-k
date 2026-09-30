// ZEHRA KINIK - Faz 1: Aks kirilmasi (snapped axle) ve karter yagsiz kalma (oil starvation)
#pragma once
#include <string>
#include <vector>

namespace zk {

struct AxleSpec {
    std::string material = "Krom-Molibden 4340";
    double diameterMm   = 27.0;   // en ince (spline) kesit
    double tauYieldMPa  = 750.0;  // kalici burulma (twist) baslangici
    double tauUltMPa    = 900.0;  // kopma
    static AxleSpec Stock()    { return {"Stok SAE 1045 (induksiyon sertlestirilmis)", 27.0, 470.0, 600.0}; }
    static AxleSpec Chromoly() { return {}; }
    static AxleSpec Race()     { return {"Yaris 300M", 27.0, 900.0, 1050.0}; }
};

struct LubeSpec {
    bool   drySump      = false;
    double oilLiters    = 4.5;
    double starveLimitS = 3.0;    // tam yukte ~3 sn yagsiz -> yatak sarar
};

class DrivetrainFailure {
public:
    DrivetrainFailure(const AxleSpec& a, const LubeSpec& l) : axle_(a), lube_(l) {}

    // tau = 16*T / (pi*d^3)
    void updateAxles(double dt, double torqueL, double torqueR);
    // ax, ay [m/s^2]; rpm; yag pompasi girisi (pickup) acikta mi?
    void updateOil(double dt, double ax, double ay, double rpm);

    double shearMPa(int side)   const { return shear_[side]; }
    double peakShearMPa()       const { return peak_; }
    bool   snapped(int side)    const { return snapped_[side]; }
    double twistDeg(int side)   const { return twist_[side]; }
    double oilPressureFactor()  const { return oilFactor_; }
    double bearingDamage()      const { return damage_ / lube_.starveLimitS; } // 0..1
    bool   bearingSpun()        const { return spun_; }
    bool   pickupUncovered()    const { return uncovered_; }
    double criticalG()          const;
    const AxleSpec& axle()      const { return axle_; }
    std::vector<std::string> drainEvents() { auto v = std::move(events_); events_.clear(); return v; }

private:
    AxleSpec axle_; LubeSpec lube_;
    double shear_[2] = {0, 0}, twist_[2] = {0, 0}, peak_ = 0.0;
    bool   snapped_[2] = {false, false};
    double oilFactor_ = 1.0, damage_ = 0.0, starvedFrac_ = 0.0;
    bool   uncovered_ = false, spun_ = false;
    std::vector<std::string> events_;
};

} // namespace zk
