// ZEHRA KINIK - Faz 1: Motor, VTEC, ITB, BSFC, analog debriyaj, 1.5-Way LSD
#pragma once
#include <string>
#include <utility>
#include <vector>

namespace zk {

using TorqueCurve = std::vector<std::pair<double, double>>; // (RPM, N*m) tam gaz net tork

enum class Valvetrain { V8, V16, V20 };
enum class IntakeType { ITB, Plenum };
enum class FuelType { Pump95, Race100, E85 };

struct EngineSpec {
    std::string name     = "K20A (ITB, 16V i-VTEC)";
    int    cylinders     = 4;
    double boreMm        = 86.0, strokeMm = 86.0;
    double chamberCc     = 43.5;  // yanma odasi + deck hacmi
    double gasketMm      = 0.70;  // kapak contasi kalinligi -> CR
    double inertia       = 0.13;  // kg*m^2 krank+volan
    double idleRpm       = 900.0, redlineRpm = 8600.0;
    double vtecRpm       = 5800.0, vtecHyst = 200.0, vtecMinOilBar = 2.8;
    Valvetrain valvetrain = Valvetrain::V16;
    bool   stagedValves  = true;  // dusuk devirde tek emme subabi (swirl)
    IntakeType intake    = IntakeType::ITB;
    FuelType fuel        = FuelType::Race100;
    TorqueCurve lowCam, highCam;
    static EngineSpec K20Default();
};

struct ClutchSpec {
    double maxTorque = 420.0;           // N*m, yeni balata soguk kapasite
    double biteStart = 0.62, biteEnd = 0.32; // pedal konumu (1 = basili)
    double heatCap   = 6000.0;          // J/K balata+baski
    double fadeStart = 250.0;           // C
};

struct DiffSpec {
    // 1.5-Way plaka LSD: hizlanma rampasi 45 derece, gaz kesme (overrun) rampasi 60 derece
    double rampAccelDeg = 45.0, rampDecelDeg = 60.0;
    double preload      = 60.0;   // N*m
    double plateFactor  = 0.55;   // mu_plaka*n_yuzey*r_plaka/r_rampa
};

struct GearboxSpec {
    std::vector<double> ratios{3.267, 2.130, 1.517, 1.147, 0.921, 0.738};
    double finalDrive = 4.76;
    double efficiency = 0.93;
};

class PowertrainCore {
public:
    PowertrainCore(const EngineSpec& e, const ClutchSpec& c, const DiffSpec& d, const GearboxSpec& g);

    void setThrottle(double pedal)      { throttlePedal_ = pedal; }
    double throttle() const             { return throttlePedal_; }
    void setClutchPedal(double pedal)   { clutchPedal_ = pedal; } // 0 = birakili (kavrali), 1 = basili
    void setGear(int g)                 { gear_ = g; }            // 0 = bos, 1..N
    void setTwoStep(bool armed, double rpm) { twoStep_ = armed; twoStepRpm_ = rpm; }
    void setIgnitionKilled(bool k)      { killed_ = k; }          // motor kilitlendi vs.
    void restart() { stalled_ = false; omegaE_ = e_.idleRpm / 9.5493; }
    void setAxleSnapped(bool l, bool r) { snappedL_ = l; snappedR_ = r; }
    // Cekis kontrolu: pozitif motor torku carpani (1 = mudahale yok)
    void setTorqueLimit(double f) { torqueLimit_ = f; }
    void setTorqueAdd(double nm) { torqueAdd_ = nm; }      // nitro: gazla orantili ek tork
    double torqueLimit() const { return torqueLimit_; }
    // Tork konvertoru (otomatik sanziman): debriyaj yerine akiskan kavrama. Pompa torku K w^2 (1 - SR^6),
    // cikis = TR(SR) x pompa (TR: kalkista tr0, SR 0.85'te 1). Stall devrinde tam gaz motor torku = K w^2.
    // Pedal >= 0.99: bos (N). Ani kilitlenme olmaz: kalkista tork yumusak gelir.
    void setConverter(bool on, double stallRpm, double tr0 = 1.9);
    bool converter() const { return converter_; }
    static constexpr double kConverterTr0 = 1.9;

    // Bir alt-adim: tekerlek hizlari ve yag basinci carpanindan aks torklarini uretir
    void step(double dt, double wL, double wR, double oilFactor);

    double axleTorqueL() const { return TL_; }
    double axleTorqueR() const { return TR_; }
    double rpm()         const;
    double engineOmega() const { return omegaE_; }
    double engineTorque()const { return Te_; }
    double clutchTorque()const { return Tc_; }
    double clutchTempC() const { return clutchTemp_; }
    double oilPressureBar() const { return oilBar_; }
    double fuelGrams()   const { return fuelG_; }
    double fuelFlowGs()  const { return fuelFlow_ * 1000.0; }
    double powerHp()     const;
    double compressionRatio() const;
    double throttleEffective() const { return thrEff_; }
    bool   vtecActive()  const { return vtec_; }
    bool   stalled()     const { return stalled_; }
    bool   limiterHit()  const { return cut_; }
    int    gear()        const { return gear_; }
    int    gearCount()   const { return (int)gb_.ratios.size(); }
    double gearRatio(int g) const { return g >= 1 && g <= gearCount() ? gb_.ratios[g - 1] : 0.0; }
    double totalRatio()  const;
    const EngineSpec& engine() const { return e_; }
    // olay mesajlari (VTEC gecisi, stop etme vb.)
    std::vector<std::string> drainEvents() { auto v = std::move(events_); events_.clear(); return v; }

private:
    double wotTorque(double rpm) const;
    double frictionTorque(double rpm) const;
    double bsfc(double rpm) const; // kg/(HP*saat)

    EngineSpec e_; ClutchSpec c_; DiffSpec d_; GearboxSpec gb_;
    double throttlePedal_ = 0.0, thrEff_ = 0.0, clutchPedal_ = 1.0;
    int    gear_ = 0;
    bool   twoStep_ = false; double twoStepRpm_ = 4500.0;
    bool   killed_ = false, snappedL_ = false, snappedR_ = false;
    double torqueLimit_ = 1.0, torqueAdd_ = 0.0;
    bool   converter_ = false; double convK_ = 0.0, convTr0_ = 1.9, convTiMax_ = 0.0;
    double omegaE_;
    double Te_ = 0.0, Tc_ = 0.0, TL_ = 0.0, TR_ = 0.0;
    double clutchTemp_ = 60.0, oilBar_ = 0.0;
    double fuelG_ = 0.0, fuelFlow_ = 0.0;
    bool   vtec_ = false, stalled_ = false, cut_ = false;
    std::vector<std::string> events_;
};

} // namespace zk
