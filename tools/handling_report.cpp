// Handling raporu: (1) 2. viteste dusuk devirden tam gaz: teker kaymasi + ivme (guclu araclar), (2) yol yarisi
// YZ'siyle viraj: en buyuk govde kaymasi ve donme. Lastik takimlarina gore (yol lastigi = roadTire).
#include "game/RoadCar.h"
#include "game/RoadPath.h"
#include "garage/VehicleCatalog.h"
#include "sim/PartTables.h"
#include <cmath>
#include <cstdio>
#include <string>

using namespace zk;

static int tireIdx(const char* name) {
    for (int i = 0; i < (int)tireTable().size(); ++i) if (std::string(tireTable()[i].name) == name) return i;
    return 0;
}

static void pull(int carId, const Tune& t, const char* label) {
    const RoadPath straight(7u, 20000.0, 4000.0);
    RoadCar a(findVehicle(carId), &t, straight, 0.0, -1.8);
    a.manual = true; a.assist = false;
    RoadControls k; k.throttle = 0.25; k.gear = 1;
    for (int i = 0; i < 600 && a.sim().speed() < 30 / 3.6; ++i) a.update(1.0 / 60.0, k);
    k.gear = 2; k.throttle = 0.0;
    for (int i = 0; i < 30; ++i) a.update(1.0 / 60.0, k);           // 2. vites, ~30 km/h
    const double v0 = a.sim().speed();
    double maxSlip = 0;
    k.throttle = 1.0;
    for (int i = 0; i < 90; ++i) {                                     // 1.5 s tam gaz
        a.update(1.0 / 60.0, k);
        const auto& w = a.sim().wheel(a.sim().drivenLeft());
        maxSlip = std::max(maxSlip, (w.omega() * w.rEff() - a.sim().speed()) / std::max(3.0, a.sim().speed()));
    }
    std::printf("  %-26s 2. vites 30 km/h tam gaz 1.5 s: ivme %.2f g, en buyuk kayma %.2f, beta %.1f deg\n", label,
                (a.sim().speed() - v0) / 1.5 / 9.81, maxSlip, a.sim().bodySlipAngle() * 57.3);
}

static double pace_ = 0.95;
static void corner(int carId, const Tune& t, const char* label) {
    const RoadPath twisty(31u, 6000.0, 60.0, 3.6, 0.0);
    RoadCar a(findVehicle(carId), &t, twisty, 0.0, -1.8);
    a.manual = false; a.assist = false; a.esp = false;
    double maxBeta = 0; int spins = 0; bool inSpin = false;
    for (int i = 0; i < 60 * 70; ++i) {
        RoadControls k = a.aiControls(-1.8, pace_);                     // hedef yanal g
        a.update(1.0 / 60.0, k);
        const double b = std::fabs(a.sim().bodySlipAngle());
        maxBeta = std::max(maxBeta, b);
        if (b > 0.5 && !inSpin) { ++spins; inSpin = true; }
        if (b < 0.1) inSpin = false;
    }
    std::printf("  %-26s viraj 70 s: %.2f km, en buyuk beta %.1f deg, donme %d\n", label, a.s() / 1000.0, maxBeta * 57.3, spins);
}

int main() {
    struct C { int id; const char* n; } cars[] = {{122, "E46 M3"}, {269, "Hellcat"}, {227, "Mustang"}};
    const char* tires[] = {"SOKAK", "UHP 275/35", "R-COMPOUND 275", "PIST SLICK 285"};
    for (auto& c : cars) {
        std::printf("%s\n", c.n);
        for (const char* tn : tires) {
            Tune t; t.roadTire = tireIdx(tn);
            const std::string lab = std::string(tn);
            pull(c.id, t, lab.c_str());
            for (double p : {0.80, 0.95}) { pace_ = p; corner(c.id, t, (lab + (p < 0.9 ? " @0.80g" : " @0.95g")).c_str()); }
        }
    }
}
