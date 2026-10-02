// ZEHRA KINIK - Acik yolda bir arac: duzlemsel fizik + surus yardimi (otomatik debriyaj/vites, ESP)
// + yol koordinati + kurtarma. Oyuncu ve yapay zeka rakip ayni sinifi kullanir (headless test edilebilir).
#pragma once
#include "game/RoadPath.h"
#include "sim/Tune.h"
#include "sim/VehicleSim.h"

#include <memory>

namespace zk {

struct RoadControls {
    double steer = 0, throttle = 0, brake = 0;
    double clutch = -1.0;   // >= 0: oyuncu debriyaji (pedal 0..1, 1 = basili); < 0: otomatik debriyaj
    int    gear = -1;       // H-desen kolu: istenen vites (0 = bos); -1: istek yok
    int    shift = 0;       // sirali vites darbesi (+1 / -1)
    bool   neutral = false; // otomatik sanziman N / P (P'de durunca fren de tutar)
    int    autoMode = -1;   // otomatik sanziman: 0 D, 1 S (spor), 2 M (elle +/-, shift ile); -1: otomatik kol yok
    bool   reverse = false; // otomatik R: geri vites
};

class RoadCar {
public:
    static constexpr double kStep = 5e-5;
    RoadCar(const VehicleDef* car, const Tune* tune, const RoadPath& road, double s0, double laneOffset);

    // Bir kare: surus yardimi + fizik (kStep adimlarla) + yol izdusumu + gerekirse kurtarma
    void update(double dt, const RoadControls& c);
    // Yapay zeka: laneOffset'teki seride saf takip + ileriye bakan viraj hizi. pace: 0.6-1.0 (yanal g payi)
    RoadControls aiControls(double laneOffset, double pace, double speedCap = 1e9) const;

    VehicleSim& sim() { return *sim_; }
    const VehicleSim& sim() const { return *sim_; }
    double s() const { return s_; }
    double lateral() const { return lat_; }
    double elevation() const { return road_.at(s_).z; }      // yol yuksekligi (cizim)
    bool offRoad() const { return std::fabs(lat_) > road_.halfWidthAt(s_) + 1.5; }   // banket (1.5 m) asfalt sayilir
    double tireSlipSpeed() const;          // ses icin

    bool manual = false, assist = true;
    double gripMul = 1.0;                  // hava durumu: yagmurda ~0.72 (islak asfalt)
    double launchRpm = 0.0;                // > 0: otomatik debriyajda kalkis devri (YZ drag kalkisi); 0: gaza gore 1500-2700
    bool slowClutch = false;               // otomatik debriyaj cezasi: kalkis ve vites gecisinde gec kavrar
    int autoMode = 0;                      // otomatik sanziman: 0 D, 1 S, 2 M (elle; vites yalniz requestShift)
    void requestShift(int dir);            // manuel: +1 / -1
    void requestGear(int g);               // H-desen (otomatik debriyaj): dogrudan vites (0 = bos)
    bool grinding() const { return grind_; }   // oyuncu debriyajsiz vites denedi (dis citirtisi)
    // Olaylar (okununca sifirlanir)
    bool takeRecovered() { const bool r = recovered_; recovered_ = false; return r; }
    bool takeStalled() { const bool r = stalledEv_; stalledEv_ = false; return r; }
    void recover(double backM = 20.0);
    void bump(double speedFactor);         // carpisma: hiz kaybi
    void nudge(double dx, double dy) { sim_->nudge(dx, dy); }   // temas: konumu it

private:
    void driverAssist(double dt, double thrIn, bool neutral);
    void playerClutch(const RoadControls& c);
    bool grind_ = false;
    const RoadPath& road_;
    Tune tune_; bool hasTune_ = false;
    std::unique_ptr<VehicleSim> sim_;
    int hint_ = 0; double s_ = 0, lat_ = 0, lane_ = -1.8;
    double launchPedal_ = 1.0; bool launching_ = true;
    double shiftT_ = -1, sinceShift_ = 0; int target_ = 1;
    void autoShift(int gear, double rpm, double thr);   // otomatik: gaza gore vites noktasi, kickdown, S / M
    double acc_ = 0, offT_ = 0;
    bool recovered_ = false, stalledEv_ = false;
};

} // namespace zk
