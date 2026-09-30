// Acik yol testleri: yapay zeka surucusu prosedurel yolu yoldan cikmadan tamamlamali.
#include "game/RoadCar.h"
#include "game/RoadSession.h"
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
    std::printf("[3] Yol yarisi 4 km (#5 oyuncu-YZ vs #227 rakip, trafik)\n");
    {
        Tune t;
        RoadSession rs(RoadSession::Mode::Race, 5, &t, 227, &t, 77u);
        double t0 = 0; bool moved = false;
        while (rs.phase() != RoadSession::Phase::Finished && t0 < 400.0) {
            // oyuncu: basit YZ, trafik arkasinda bekler
            double cap = 1e9;
            for (const TrafficCar& tc : rs.traffic())
                if (!tc.oncoming && tc.s > rs.player().s() && tc.s - rs.player().s() < 40.0) cap = std::min(cap, tc.v);
            RoadControls c = rs.player().aiControls(-RoadSession::kLane, 0.55, cap);
            rs.update(1.0 / 60.0, c);
            if (rs.phase() == RoadSession::Phase::Countdown && rs.player().sim().speed() > 0.5) moved = true;
            t0 += 1.0 / 60.0;
        }
        std::printf("    oyuncu %.1f s, rakip %.1f s, kazanan %s, oyuncu carpisma %d\n", rs.playerTime(), rs.rivalTime(),
                    rs.playerWon() ? "OYUNCU" : "RAKIP", rs.collisions());
        CHECK(!moved, "geri sayimda arac kipirdamadi");
        CHECK(rs.phase() == RoadSession::Phase::Finished && rs.rivalTime() > 60.0 && rs.rivalTime() < 250.0, "rakip trafikte 4 km'yi makul surede bitirdi");
        CHECK(rs.playerTime() > 0.0 ? rs.playerWon() == (rs.rivalTime() <= 0 || rs.playerTime() < rs.rivalTime()) : !rs.playerWon(),
              "sonuc tutarli (bitiremeyen oyuncu kaybeder)");
    }
    std::printf(failures ? "\nSONUC: %d test KALDI\n" : "\nSONUC: tum testler gecti\n", failures);
    return failures ? 1 : 0;
}
