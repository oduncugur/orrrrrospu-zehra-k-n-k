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
    std::printf("[4] Trafik fren yapar: oyuncu karsi seritte durur, gelen araclar carpmamali\n");
    {
        Tune t;
        RoadSession rs(RoadSession::Mode::Free, 5, &t, 0, nullptr, 5u);
        int braked = 0, hitWhileStopped = 0;
        for (int i = 0; i < 60 * 150; ++i) {
            RoadControls c = rs.player().aiControls(+RoadSession::kLane, 0.5, 8.0);
            if (i > 60 * 12) { c.throttle = 0; c.brake = 1.0; }
            rs.update(1.0 / 60.0, c);
            for (const TrafficCar& tc : rs.traffic()) if (tc.oncoming && tc.braking) { ++braked; break; }
            if (rs.takeCrash() && i > 60 * 24) ++hitWhileStopped;   // oyuncu durduktan sonra ona carpan var mi
        }
        std::printf("    durmus oyuncuya carpma %d (toplam %d), fren yapilan kare %d\n", hitWhileStopped, rs.collisions(), braked);
        CHECK(hitWhileStopped == 0 && braked > 0, "gelen trafik duran araca carpmadan durdu");
    }
    std::printf("[5] Dag yolu (touge) 3 km yaris (#5 vs #78)\n");
    {
        Tune t;
        RoadSession rs(RoadSession::Mode::Race, 5, &t, 78, &t, 9u, RoadSession::Kind::Touge);
        double minR = 1e9; for (const auto& p : rs.road().points()) if (std::fabs(p.curvature) > 1e-6) minR = std::min(minR, 1.0 / std::fabs(p.curvature));
        double t0 = 0; int recov = 0;
        while (rs.phase() != RoadSession::Phase::Finished && t0 < 400.0) {
            double cap = 1e9;
            for (const TrafficCar& tc : rs.traffic())
                if (!tc.oncoming && tc.s > rs.player().s() && tc.s - rs.player().s() < 30.0) cap = std::min(cap, tc.v);
            rs.update(1.0 / 60.0, rs.player().aiControls(-rs.lane(), 0.55, cap));
            if (rs.player().takeRecovered()) ++recov;
            t0 += 1.0 / 60.0;
        }
        std::printf("    en dar viraj R=%.0f m | oyuncu %.1f s (%.0f m), rakip %.1f s, kurtarma %d\n", minR, rs.playerTime(),
                    rs.player().s(), rs.rivalTime(), recov);
        CHECK(minR < 40.0, "dag yolu gercekten dar virajli");
        CHECK(rs.rivalTime() > 0 && rs.rivalTime() < 250.0, "rakip dag yolunu bitirdi");
    }
    std::printf("[6] Yol egimi (yukseklik profili)\n");
    {
        const RoadPath flat(20250930u, 20000.0, 90.0);
        const RoadPath hilly(20250930u, 20000.0, 90.0, 3.6, 0.05);
        double maxG = 0, zMin = 0, zMax = 0, maxStep = 0, maxDiff = 0;
        bool startFlat = true;
        const auto& P = hilly.points();
        for (size_t i = 0; i < P.size(); ++i) {
            maxG = std::max(maxG, std::fabs(P[i].grade));
            zMin = std::min(zMin, P[i].z); zMax = std::max(zMax, P[i].z);
            if (P[i].s < 150.0 && (P[i].z != 0.0 || P[i].grade != 0.0)) startFlat = false;
            if (i) maxStep = std::max(maxStep, std::fabs(P[i].z - P[i - 1].z));
            maxDiff = std::max(maxDiff, std::hypot(P[i].x - flat.points()[i].x, P[i].y - flat.points()[i].y));
        }
        std::printf("    en dik %%%.1f, yukseklik %.0f..%.0f m\n", maxG * 100, zMin, zMax);
        CHECK(maxG <= 0.05 + 1e-9 && maxG > 0.03, "egim sinirda (%5) ve gercekten var");
        CHECK(zMax - zMin > 10.0, "tepe/cukur farki > 10 m");
        CHECK(startFlat, "baslangic duzlugu duz (geri sayim yokusta olmaz)");
        CHECK(maxStep <= 0.05 * RoadPath::kStep + 1e-6, "yukseklik surekli (adim basina <= egim x 2 m)");
        CHECK(maxDiff == 0.0, "egim viraj programini degistirmez");
        // Fizik: bosta (vites 0), frensiz; yokus asagi hizlanir, duzde durur
        auto roll = [](double grade) {
            VehicleSimConfig c; c.car = findVehicle(5); c.planar = true; c.road = "acikyol"; c.laneAsymmetry = false;
            Tune t; c.tune = &t;
            VehicleSim s(c);
            s.powertrain().setGear(0);
            s.setGrade(grade);
            for (int i = 0; i < 5 * 20000; ++i) s.step(5e-5, VehicleInputs{});
            return s.speed();
        };
        const double vDown = roll(-0.08), vFlat = roll(0.0);
        std::printf("    5 s bosta: %%8 inis %.2f m/s (surtunmesiz %.2f), duz %.2f m/s\n", vDown, 9.81 * 0.08 / std::sqrt(1.0064) * 5, vFlat);
        CHECK(vDown > 2.8 && vDown < 3.95, "yokus asagi yer cekimiyle hizlanir (yuvarlanma direnci kadar eksik)");
        CHECK(vFlat < 0.05, "duzde kendiliginden hareket yok");
    }
    std::printf(failures ? "\nSONUC: %d test KALDI\n" : "\nSONUC: tum testler gecti\n", failures);
    return failures ? 1 : 0;
}
