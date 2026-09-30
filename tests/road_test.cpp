// Acik yol testleri: yapay zeka surucusu prosedurel yolu yoldan cikmadan tamamlamali.
#include "game/RoadCar.h"
#include "garage/VehicleCatalog.h"
#include <cmath>
#include <cstdio>

using namespace zk;
static int failures = 0;
#define CHECK(cond, msg) do { if (cond) std::printf("  GECTI  %s\n", msg); else { std::printf("  KALDI  %s\n", msg); ++failures; } } while (0)

int main() {
    const RoadPath road(20250930u, 20000.0, 90.0);
    std::printf("[1] Yol geometrisi: %.0f m, %zu nokta\n", road.length(), road.points().size());
    {
        double maxK = 0; for (const auto& p : road.points()) maxK = std::max(maxK, std::fabs(p.curvature));
        std::printf("    en dar yaricap %.0f m\n", 1.0 / maxK);
        CHECK(road.length() > 19900 && 1.0 / maxK >= 89.0, "uzunluk ve en dar viraj");
        int h = 0; double s, lat; const RoadPoint p = road.at(5000.0);
        road.project(p.x - 2.0 * std::sin(p.heading), p.y + 2.0 * std::cos(p.heading), h = 2490, s, lat);
        CHECK(std::fabs(s - 5000.0) < 0.5 && std::fabs(lat - 2.0) < 0.05, "izdusum: s ve yanal sapma");
    }
    for (int id : {5, 227, 78}) {
        const VehicleDef* v = findVehicle(id);
        std::printf("[2] YZ surucu 6 km (#%d %s)\n", id, v->ref.c_str());
        RoadCar car(v, nullptr, road, 0.0, -1.8);
        int recov = 0; double t = 0, maxLat = 0, top = 0;
        while (car.s() < 6000.0 && t < 600.0) {
            car.update(1.0 / 60.0, car.aiControls(-1.8, 0.55));
            t += 1.0 / 60.0;
            if (car.takeRecovered()) ++recov;
            if (t > 3) maxLat = std::max(maxLat, std::fabs(car.lateral() + 1.8));
            top = std::max(top, car.sim().speed());
        }
        std::printf("    sure %.1f s (ort %.0f km/h, tepe %.0f km/h), kurtarma %d, en buyuk serit sapmasi %.2f m\n",
                    t, 6000.0 / t * 3.6, top * 3.6, recov, maxLat);
        CHECK(car.s() >= 6000.0 && recov == 0, "yoldan cikmadan tamamladi");
        CHECK(maxLat < 4.0, "serit sapmasi < 4 m (banketi en fazla ~1 m asar)");
    }
    std::printf(failures ? "\nSONUC: %d test KALDI\n" : "\nSONUC: tum testler gecti\n", failures);
    return failures ? 1 : 0;
}
