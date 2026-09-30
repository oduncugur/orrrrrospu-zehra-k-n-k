#include "DrivetrainFailure.h"
#include <algorithm>
#include <cmath>

namespace zk {

static constexpr double kPi = 3.14159265358979323846;

void DrivetrainFailure::updateAxles(double dt, double TL, double TR) {
    const double d = axle_.diameterMm * 1e-3;
    const double T[2] = {TL, TR};
    const char* name[2] = {"SOL", "SAG"};
    for (int i = 0; i < 2; ++i) {
        if (snapped_[i]) { shear_[i] = 0.0; continue; }
        const double tau = 16.0 * std::fabs(T[i]) / (kPi * d * d * d) * 1e-6; // MPa
        shear_[i] = tau;
        peak_ = std::max(peak_, tau);
        if (tau > axle_.tauUltMPa) {
            snapped_[i] = true;
            events_.push_back(std::string("!!! ") + name[i] + " AKS KIRILDI (" + axle_.material + ", tau=" +
                              std::to_string((int)tau) + " MPa > " + std::to_string((int)axle_.tauUltMPa) + " MPa)");
        } else if (tau > axle_.tauYieldMPa) {
            // Plastik burulma: akma sinirini asan kisim kalici aci biriktirir
            const double before = twist_[i];
            twist_[i] += dt * 400.0 * (tau - axle_.tauYieldMPa) / axle_.tauYieldMPa;
            if (before < 1.0 && twist_[i] >= 1.0)
                events_.push_back(std::string(name[i]) + " aks kalici burulma yapti (bukuldu)");
        }
    }
}

double DrivetrainFailure::criticalG() const {
    // Yag seviyesi dustukce pickup daha kucuk egimde acikta kalir
    return 0.90 + 0.15 * (lube_.oilLiters - 3.5);
}

void DrivetrainFailure::updateOil(double dt, double ax, double ay, double rpm) {
    if (spun_) { oilFactor_ = 0.0; return; }
    if (lube_.drySump) { uncovered_ = false; oilFactor_ = 1.0; return; }
    const double gTot = std::sqrt(ax * ax + ay * ay) / 9.81;
    uncovered_ = gTot > criticalG();
    // Pompa havayi yuttukca basinc gecikmeli duser (hidrolik hat + kopuklenme)
    const double target = uncovered_ ? 1.0 : 0.0;
    starvedFrac_ += (target - starvedFrac_) * std::min(1.0, dt / 0.25);
    oilFactor_ = 1.0 - 0.85 * starvedFrac_;
    // Yatak hasari: devrin karesiyle olcekli (hidrodinamik film yuku)
    const double load = std::pow(std::max(rpm, 0.0) / 7000.0, 2.0);
    damage_ += dt * starvedFrac_ * load;
    if (!uncovered_) damage_ = std::max(0.0, damage_ - dt * 0.02); // yag donunce hafif toparlanma
    if (damage_ >= lube_.starveLimitS) {
        spun_ = true;
        events_.push_back("!!! KRANK YATAGI SARDI (islak karter yagsiz kaldi) - MOTOR KILITLENDI");
    }
}

} // namespace zk
