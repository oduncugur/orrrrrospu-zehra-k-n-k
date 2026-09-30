// ZEHRA KINIK - Suspansiyon ve yol bozuklugu modeli
//
// 7 serbestlik dereceli surus modeli (full-car ride model):
//   govde: dikey (heave) z, yunuslama (pitch) theta, yalpa (roll) phi   + 4 x yaysiz kutle dikey z_u
// Her kose: helezon yay + cift yonlu (bump/rebound) amortisor (digresif diz noktali) + progresif
// takoz (bump stop) + lastik dikey yayi (PSI'ya bagli, SADECE basi: lastik yerden kesilebilir).
// Viraj denge cubugu (ARB) on/arka. Boyuna/yanal ivme govdeye atalet momenti olarak uygulanir ->
// yuk transferi artik suspansiyon dinamiginden dogar (pitch/roll gecikmesi, sekme, tekerlek kalkmasi).
//
// Yol: ISO 8608 puruzluluk sinifi (A..H) + ayrik ozellikler (kasis, cukur, dilatasyon derzi, rogar,
// arnavut kaldirimi, tumsek/sicrama). Sol ve sag iz kismen iliskili.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace zk {

enum class RoadFeatureType { SpeedBump, Pothole, ExpansionJoint, Manhole, Cobblestone, Crest };

struct RoadFeature {
    RoadFeatureType type;
    double x;        // baslangic (m)
    double length;   // boyuna uzunluk (m)
    double height;   // + tumsek, - cukur (m)
    int    side;     // -1 sol iz, +1 sag iz, 0 iki iz
};

class RoadProfile {
public:
    // isoClass: 'A' (yeni asfalt) .. 'H' (cok bozuk); seed deterministik (rollback netcode icin)
    RoadProfile(char isoClass = 'A', uint32_t seed = 1);
    void addFeature(const RoadFeature& f) { features_.push_back(f); }
    // side: -1 sol, +1 sag
    double height(double x, int side) const;
    static RoadProfile preset(const std::string& name);   // "drag", "otoban", "sehir", "koy", "dag"
    std::string describe() const;

private:
    struct Wave { double k, phase, amp; };
    char cls_;
    std::vector<Wave> common_, left_, right_;
    std::vector<RoadFeature> features_;
};

struct SuspensionCorner {
    double springK      = 30000.0; // N/m (tekerlek hizasina indirgenmis)
    double bumpC        = 2200.0;  // N*s/m, basma
    double reboundC     = 4200.0;  // N*s/m, uzama (genelde 1.5-2.5x bump)
    double kneeVel      = 0.12;    // m/s, digresif valf acilma hizi
    double kneeRatio    = 0.45;    // diz sonrasi egim orani
    double travelBump   = 0.07;    // m, takoza kadar basma
    double travelDroop  = 0.09;    // m, tam uzama
    double bumpStopK    = 250000.0;// N/m, progresif takoz
    double unsprungKg   = 40.0;
    double tireK        = 220000.0;// N/m
    double tireC        = 150.0;
};

struct SuspensionSetup {
    SuspensionCorner front, rear;
    double arbFront = 18000.0, arbRear = 9000.0;  // N/m (tekerlek hizasinda fark basina)
    // Sanki-statik referans: araç olculeri
    double mass = 1200.0, wheelbase = 2.6, trackF = 1.5, trackR = 1.5, hCoG = 0.5, frontWeight = 0.6;
    double Iyy = 0.0, Ixx = 0.0;   // 0: olculerden tahmin
    // Dogal frekans (Hz) ve sonum oranindan yay/amortisor ureten yardimci
    static SuspensionSetup fromVehicle(double mass, double wheelbase, double track, double hCoG, double frontWeight,
                                       double freqF, double freqR, double zetaBump, double zetaRebound);
};

class Suspension {
public:
    explicit Suspension(const SuspensionSetup& s);
    void setTirePressure(int corner, double psi);  // lastik dikey sertligi PSI ile
    // x: aracin yol uzerindeki konumu, ax/ay: govde ivmesi (m/s^2). Kose sirasi FL, FR, RL, RR.
    void step(double dt, const RoadProfile& road, double x, double ax, double ay);
    double tireLoad(int c) const   { return Fz_[c]; }
    bool   airborne(int c) const   { return airTime_[c] >= kAirMin; } // >15 ms temassiz
    double airTime(int c) const    { return airTime_[c]; }
    static constexpr double kAirMin = 0.015;
    bool   onBumpStop(int c) const { return bumpStop_[c]; }
    double suspTravel(int c) const { return travel_[c]; }   // + basma
    double heave() const { return z_; }
    double pitchDeg() const { return th_ * 57.2958; }
    double rollDeg() const  { return ph_ * 57.2958; }
    double bodyAccelG() const { return zAcc_ / 9.81; }
    // Olay sayaclari (telemetri / haptik)
    int airborneEvents(int c) const { return airEvents_[c]; }
    int bumpStopEvents(int c) const { return stopEvents_[c]; }
    double peakLoad(int c) const { return peakFz_[c]; }

private:
    double damper(const SuspensionCorner& s, double v) const;
    SuspensionSetup s_;
    double cx_[4], cy_[4];                // kose konumu (CoG'ye gore): x ileri, y sol
    double staticDefl_[4], unsprungEq_[4];
    double z_ = 0, vz_ = 0, th_ = 0, wth_ = 0, ph_ = 0, wph_ = 0, zAcc_ = 0;
    double zu_[4] = {0, 0, 0, 0}, vu_[4] = {0, 0, 0, 0};
    double Fz_[4] = {0, 0, 0, 0}, travel_[4] = {0, 0, 0, 0}, peakFz_[4] = {0, 0, 0, 0};
    double tireK_[4];
    bool   bumpStop_[4] = {false, false, false, false}, wasAir_[4] = {false, false, false, false};
    bool   wasStop_[4] = {false, false, false, false};
    int    airEvents_[4] = {0, 0, 0, 0}, stopEvents_[4] = {0, 0, 0, 0};
    double staticFz_[4];
    double airTime_[4] = {0, 0, 0, 0};
};

} // namespace zk
