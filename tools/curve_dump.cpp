// Guc egrisi dokumu: arac + parca seti icin her 500 devirde HP (motor egrisi denge analizi)
#include "sim/VehicleSim.h"
#include "sim/Tune.h"
#include "garage/VehicleCatalog.h"
#include <cstdio>
#include <cstdlib>

using namespace zk;

static double torqueAt(const TorqueCurve& c, double rpm) {
    if (c.empty()) return 0;
    if (rpm <= c.front().first) return c.front().second;
    for (size_t i = 1; i < c.size(); ++i)
        if (rpm <= c[i].first) { const double k = (rpm - c[i - 1].first) / (c[i].first - c[i - 1].first); return c[i - 1].second + (c[i].second - c[i - 1].second) * k; }
    return c.back().second;
}

static void dump(const char* name, const VehicleDef& v, const Tune& t) {
    VehicleSimConfig cfg; cfg.car = &v; cfg.tune = &t;
    const VehicleSim s(cfg);
    const EngineSpec& e = s.engineSpec();
    std::printf("%-22s kesici %5.0f supap %5.0f |", name, e.redlineRpm, s.valveSafeRpm());
    for (double r = 2000; r <= e.redlineRpm + 1; r += 500) {
        const double tq = std::max(torqueAt(e.lowCam, r), e.highCam.empty() ? 0.0 : torqueAt(e.highCam, r));
        std::printf(" %4.0f", tq * r / 7120.9);
    }
    std::printf("\n");
}

int main(int argc, char** argv) {
    const VehicleDef& v = *findVehicle(argc > 1 ? std::atoi(argv[1]) : 217);
    std::printf("%s  (HP her 500 devirde, 2000'den)\n", v.fullName().c_str());
    Tune stock; dump("stok", v, stock);
    Tune cam; cam.cam = 8; cam.valve = 6; dump("kam8 + supap6", v, cam);
    Tune ce = cam; ce.ecuHw = 9; ce.swRev = 30; dump("kam8 + supap6 + ECU+1500", v, ce);
    Tune e2; e2.ecuHw = 9; e2.swRev = 30; dump("yalniz ECU+1500", v, e2);
    Tune sc = ce; sc.superch = 6; dump("+ kompresor 6", v, sc);
    Tune tb = ce; tb.turbo = 8; tb.fuelSys = 6; dump("+ turbo 8 + yakit", v, tb);
}
