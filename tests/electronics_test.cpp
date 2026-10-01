// ABS / cekis kontrolu: katalog (yalniz gercekte olan araclarda), fizik etkisi, ECU ile ekleme, kayit
#include "game/Career.h"
#include "game/RoadCar.h"
#include "sim/VehicleSim.h"

#include <cmath>
#include <cstdio>

using namespace zk;
static int failures = 0;
#define CHECK(cond, msg) do { if (cond) std::printf("  GECTI  %s\n", msg); else { std::printf("  KALDI  %s\n", msg); ++failures; } } while (0)

// Duz yolda 100 km/h'den tam fren (direksiyon duz). Donus: durus mesafesi, en kotu flat-spot (mm)
static void brakeTest(int carId, const Tune& t, double& dist, double& flat, double& lockedFrac) {
    VehicleSimConfig c; c.car = findVehicle(carId); c.tune = &t; c.planar = true; c.road = "acikyol"; c.laneAsymmetry = false;
    VehicleSim s(c);
    s.powertrain().setGear(0);
    s.applyImpulse(s.mass() * 100 / 3.6, 0, 0, 0);
    // tekerlekleri arac hizina getir (suruklenmeden baslasin)
    VehicleInputs coast;
    for (int i = 0; i < 4000; ++i) s.step(5e-5, coast);
    const double x0 = s.posX();
    VehicleInputs in; in.brake = 1.0;
    int lockedSteps = 0, steps = 0; flat = 0;
    for (int i = 0; i < 20 * 20000 && s.speed() > 0.3; ++i) {
        s.step(5e-5, in);
        bool any = false;
        for (int w = 0; w < 4; ++w) { if (s.wheel(w).locked()) any = true; flat = std::max(flat, s.wheel(w).flatSpotMm()); }
        lockedSteps += any; ++steps;
    }
    dist = s.posX() - x0;
    lockedFrac = steps ? double(lockedSteps) / steps : 0.0;
}

int main() {
    std::printf("[1] Katalog: fabrika donanimi\n");
    {
        auto has = [](int id, bool a, bool t) { const VehicleDef* v = findVehicle(id); return v->abs == a && v->tc == t; };
        CHECK(has(217, false, false), "Tofas Sahin: ABS yok, TC yok");
        CHECK(has(313, false, false) && has(184, false, false), "McLaren F1 / Ferrari F40: ikisi de yok");
        CHECK(has(52, true, false), "R34 GT-R: ABS var, TC yok");
        CHECK(has(19, true, true), "NSX NA1: ABS + TCS");
        CHECK(has(227, true, true) && has(36, true, true), "Mustang S550 / GR Supra: ikisi de var");
        CHECK(has(23, false, false) && has(84, false, false), "yaris araclari: yok");
        int a = 0, tc = 0;
        for (const VehicleDef& v : vehicleCatalog()) { a += v.abs; tc += v.tc; }
        std::printf("    324 aracin %d'inde ABS, %d'inde TC\n", a, tc);
        CHECK(tc < a && a < 324, "her aracta yok; TC olan ABS'den az");
        int bad = 0;
        for (const VehicleDef& v : vehicleCatalog()) if (v.tc && !v.abs) ++bad;
        CHECK(bad == 0, "TC olan her aracta ABS de var");
    }

    std::printf("[2] ABS (#217 Sahin, ECU ile eklenen kit): 100 km/h tam fren\n");
    {
        Tune no, kit; kit.absKit = true; kit.ecu = 1;
        double dNo, fNo, dAbs, fAbs, lNo, lAbs;
        brakeTest(217, no, dNo, fNo, lNo);
        brakeTest(217, kit, dAbs, fAbs, lAbs);
        std::printf("    ABS yok: %.1f m, kilitli %%%.0f, flat-spot %.2f mm | ABS: %.1f m, kilitli %%%.1f, flat-spot %.3f mm\n",
                    dNo, lNo * 100, fNo, dAbs, lAbs * 100, fAbs);
        CHECK(lNo > 0.5 && fNo > 0.0, "ABS'siz tam frende tekerlek kilitli kalir, lastik duzlesir");
        CHECK(lAbs < 0.05 && fAbs < fNo * 0.2, "ABS kilitlenmeyi onler (yalniz kisa cevrim anlari)");
        CHECK(dAbs < dNo, "ABS ile durus mesafesi kisa");
        VehicleSimConfig c; c.car = findVehicle(217); c.tune = &kit; const VehicleSim s(c);
        VehicleSimConfig c2; c2.car = findVehicle(217); c2.tune = &no; const VehicleSim s2(c2);
        CHECK(s.hasAbs() && !s2.hasAbs(), "kit takiliyken ABS var, degilken yok");
    }

    std::printf("[3] Cekis kontrolu (#269 Hellcat, sokak lastigi): tam gaz kalkis, 2 s\n");
    {
        const RoadPath road(20250930u, 20000.0, 90.0);
        auto launch = [&](bool tcOn, double& maxKap, double& dist) {
            RoadCar car(findVehicle(269), nullptr, road, 0.0, -1.8);
            car.assist = tcOn;
            RoadControls c; c.throttle = 1.0;
            maxKap = 0; dist = 0;
            for (int i = 0; i < 120; ++i) {
                car.update(1.0 / 60.0, c);
                if (i > 30) maxKap = std::max(maxKap, std::max(car.sim().wheel(2).kappa(), car.sim().wheel(3).kappa()));
            }
            dist = car.s();
        };
        double kOn, dOn, kOff, dOff;
        launch(true, kOn, dOn); launch(false, kOff, dOff);
        std::printf("    TC acik: en buyuk kayma %.2f, 2 s'de %.1f m | TC kapali: kayma %.2f, %.1f m\n", kOn, dOn, kOff, dOff);
        CHECK(kOn < kOff * 0.6, "TC patinaji sinirlar");
        CHECK(dOn >= dOff * 0.95, "TC ile kalkis en az ayni hizda (patinaj kaybi az)");
        // TC'siz aracta "yardim" acik olsa da TC calismaz
        RoadCar sahin(findVehicle(217), nullptr, road, 0.0, -1.8);
        sahin.assist = true;
        sahin.update(1.0 / 60.0, RoadControls{});
        CHECK(!sahin.sim().hasTc() && !sahin.sim().tractionControl(), "TC'siz aracta yardim acik olsa da TC yok");
    }

    std::printf("[4] Parca dukkani: ELEKTRONIK (ECU sarti, fabrikada olan satilmaz), kayit\n");
    {
        Career c = Career::newGame();                                // #217 Sahin
        c.money = 100000;
        std::string why;
        CHECK(!c.buyPart(PartCat::Electronics, 2, &why) && why == "ONCE ECU GEREKLI", "ECU olmadan ABS/TC takilmaz");
        CHECK(c.buyPart(PartCat::Ecu, 1, &why), "ECU yukseltmesi alindi");
        CHECK(c.buyPart(PartCat::Electronics, 2, &why) && c.car().tune.absKit && c.car().tune.tcKit, "ECU ile ABS + TC takildi");
        Career d;
        CHECK(Career::parse(c.serialize(), d) && d.car().tune.absKit && d.car().tune.tcKit, "kayit gidis-donus");
        CHECK(!partAvailable(PartCat::Electronics, 1, *findVehicle(52), &why) && why == "FABRIKADA VAR", "R34: ABS fabrikada var");
        CHECK(partAvailable(PartCat::Electronics, 2, *findVehicle(52), &why, nullptr), "R34: TC eklenebilir (ABS fabrikada)");
        CHECK(!partAvailable(PartCat::Electronics, 2, *findVehicle(227), &why) && why == "FABRIKADA VAR", "Mustang S550: ikisi de fabrikada");
    }

    std::printf(failures ? "\nSONUC: %d test KALDI\n" : "\nSONUC: tum testler gecti\n", failures);
    return failures ? 1 : 0;
}
