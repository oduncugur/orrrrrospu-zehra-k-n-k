// Acik yol testleri: yapay zeka surucusu prosedurel yolu yoldan cikmadan tamamlamali.
#include "game/Career.h"
#include "game/RoadCar.h"
#include "game/RoadSession.h"
#include "garage/VehicleCatalog.h"
#include <cmath>
#include <cstdio>
#include <string>

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
            const double ps = rs.player().s();                         // once kendi seridinde hizlan, sonra ic karsi seride gec
            const double tgt = i < 60 * 4 ? rs.rightLane(ps) : rs.road().laneOffset(ps, true, std::max(0, rs.road().lanesBack(ps) - 1));
            RoadControls c = rs.player().aiControls(tgt, 0.5, 8.0);
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
    std::printf("[7] Surus kontrol modlari (oyuncu debriyaji, otomatik debriyaj, sirali, otomatik N)\n");
    {
        const RoadPath road2(20250930u, 20000.0, 90.0);
        // H-desen + oyuncu debriyaji (#217 Sahin): debriyaj bas, 1. vites, gaz, yavas birak; 2. vitese debriyajsiz -> girmez
        RoadCar h(findVehicle(217), nullptr, road2, 0.0, -1.8);
        h.manual = true;
        RoadControls c; c.clutch = 1.0; c.gear = 1; c.throttle = 0.0;
        for (int i = 0; i < 30; ++i) h.update(1.0 / 60.0, c);
        double t = 0;
        for (; t < 6.0; t += 1.0 / 60.0) {                                 // 1.2 s'de debriyaji birak, gaz %60
            c.clutch = std::max(0.0, 1.0 - t / 1.2); c.throttle = 0.6;
            h.update(1.0 / 60.0, c);
        }
        const double v1 = h.sim().speed();
        c.gear = 2; c.clutch = 0.0;                                         // debriyajsiz vites denemesi
        h.update(1.0 / 60.0, c);
        const bool grind = h.grinding() && h.sim().powertrain().gear() == 1;
        c.clutch = 1.0; h.update(1.0 / 60.0, c);                            // debriyajla girer
        const int g2 = h.sim().powertrain().gear();
        std::printf("    oyuncu debriyaji: 6 s'de %.0f km/h, debriyajsiz 2. vites %s, debriyajla vites %d\n", v1 * 3.6, grind ? "girmedi" : "GIRDI", g2);
        CHECK(v1 > 8.0 && !h.sim().powertrain().stalled(), "oyuncu debriyajiyla kalkti, stop etmedi");
        CHECK(grind && g2 == 2, "debriyajsiz vites girmez (citirti), debriyajla girer");
        // H-desen + otomatik debriyaj: kol ile vites, debriyaji arac kullanir; yavas debriyaj cezasi kalkisi geciktirir
        auto autoClutch = [&](bool slow) {
            RoadCar a(findVehicle(217), nullptr, road2, 0.0, -1.8);
            a.manual = true; a.slowClutch = slow;
            RoadControls k; k.gear = 1; k.throttle = 1.0;
            double tt = 0; int geared = 0;
            for (; tt < 20.0 && a.sim().speed() < 60 / 3.6; tt += 1.0 / 60.0) {
                if (a.sim().powertrain().rpm() > a.sim().shiftRpm() - 300 && k.gear < 3) ++k.gear, ++geared;
                a.update(1.0 / 60.0, k);
            }
            return std::make_pair(tt, a.sim().powertrain().gear());
        };
        const auto fast = autoClutch(false), slow = autoClutch(true);
        std::printf("    otomatik debriyaj 0-60: %.2f s (vites %d), yavas debriyaj cezasi: %.2f s\n", fast.first, fast.second, slow.first);
        CHECK(fast.first < 20.0 && fast.second >= 2, "otomatik debriyajla kol vitesi gecer, hizlanir");
        CHECK(slow.first > fast.first + 0.1, "otomatik debriyaj cezasi (gec kavrama) suresi uzatir");
        // Sirali (#23 dogbox): +1 darbeyle vites hizli gecer
        RoadCar s(findVehicle(23), nullptr, road2, 0.0, -1.8);
        RoadControls sc; sc.throttle = 1.0;
        for (int i = 0; i < 180; ++i) s.update(1.0 / 60.0, sc);
        const int sg0 = s.sim().powertrain().gear();
        sc.shift = +1; s.update(1.0 / 60.0, sc); sc.shift = 0;
        for (int i = 0; i < 6; ++i) s.update(1.0 / 60.0, sc);
        CHECK(s.sim().powertrain().gear() == sg0 + 1, "sirali: +1 darbe bir vites buyutur (<0.1 s)");
        // Otomatik (#36) N: konvertor ayrik, gazla ilerlemez
        RoadCar o(findVehicle(36), nullptr, road2, 0.0, -1.8);
        RoadControls oc; oc.throttle = 1.0; oc.neutral = true;
        for (int i = 0; i < 120; ++i) o.update(1.0 / 60.0, oc);
        std::printf("    otomatik N'de tam gaz 2 s: %.2f m/s, %.0f rpm\n", o.sim().speed(), o.sim().powertrain().rpm());
        CHECK(o.sim().speed() < 0.3, "otomatik N: gazla ilerlemez");
    }
    std::printf("[8] Karma yaris: duz (drag) -> viraj blogu -> duz -> viraj blogu -> bitis\n");
    {
        const RoadPath k = RoadPath::karma(3u, 0.04);
        const auto& sec = k.sections();
        std::printf("    uzunluk %.2f km, virajli bolumler:", k.length() / 1000.0);
        for (const RoadSection& q : sec) std::printf(" [%.0f-%.0f m]", q.s0, q.s1);
        std::printf("\n");
        CHECK(sec.size() == 2 && sec[0].curvy && sec[1].curvy, "iki viraj blogu");
        CHECK(sec.size() == 2 && sec[0].entry >= 2800.0 && sec[1].entry > sec[0].s1 + 1000.0, "once uzun duzluk (>= 2.8 km), bloklar arasi duzluk");
        CHECK(sec.size() == 2 && std::fabs(sec[0].s0 - (sec[0].entry - RoadPath::kKarmaLead)) < 1e-9 && sec[0].minR >= 70.0 && sec[0].minR <= 220.0,
              "virajli bolum ilk virajdan 450 m once baslar (fren + kamera), en dar R bilinir");
        double maxKStraight = 0, maxKCurve = 0;
        for (const RoadPoint& p : k.points()) {
            if (k.curvyAt(p.s)) maxKCurve = std::max(maxKCurve, std::fabs(p.curvature));
            else maxKStraight = std::max(maxKStraight, std::fabs(p.curvature));
        }
        CHECK(maxKStraight < 1e-9 && maxKCurve > 1.0 / 230.0, "duz bolum gercekten duz, viraj bolumu R < 230 m");
        // Oturum: iki YZ (oyuncu duzde serit takibi, virajda YZ), sonuclanir; ayni tohum ayni sonuc
        double res[2][2];
        for (int run = 0; run < 2; ++run) {
            Tune t;
            RoadSession rs(RoadSession::Mode::Karma, 5, &t, 227, &t, 11u);
            double t0 = 0;
            while (rs.phase() != RoadSession::Phase::Finished && t0 < 600.0) {
                rs.update(1.0 / 60.0, rs.player().aiControls(-rs.lane(), 0.6));
                t0 += 1.0 / 60.0;
            }
            res[run][0] = rs.playerTime(); res[run][1] = rs.rivalTime();
            if (run == 0) {
                std::printf("    karma (%.2f km, trafik %zu): oyuncu %.1f s, rakip %.1f s, carpisma %d\n", rs.raceLength() / 1000.0,
                            rs.traffic().size(), rs.playerTime(), rs.rivalTime(), rs.collisions());
                CHECK(rs.phase() == RoadSession::Phase::Finished && rs.traffic().empty(), "karma yaris sonuclandi, trafik yok (kapali yol)");
                CHECK(rs.playerTime() > 0 || rs.rivalTime() > 0, "en az bir arac bitirdi");
            }
        }
        CHECK(res[0][0] == res[1][0] && res[0][1] == res[1][1], "ayni tohum + girdi = ayni sonuc");
        // Drag agaci + tepki + rakip kalkis plani + mesafe odulu
        {
            Tune t;
            RoadSession rs(RoadSession::Mode::Karma, 5, &t, 227, &t, 11u);
            int seen = 0; double t0 = 0;
            while (t0 < 6.0) {
                seen |= rs.treeLights();
                RoadControls c; c.throttle = rs.phase() == RoadSession::Phase::Run ? 1.0 : 0.3;
                rs.update(1.0 / 60.0, c);
                t0 += 1.0 / 60.0;
            }
            std::printf("    agac isiklari 0x%x, tepki %.3f s, rakip tepki %.3f s, rakip kalkis %.0f rpm, odul carpani %.2f\n",
                        seen, rs.reaction(), rs.rivalReaction(), rs.rival()->launchRpm, rs.prizeScale());
            CHECK(seen == 15, "uc amber ve yesil yandi");
            CHECK(rs.reaction() > 0.0 && rs.reaction() < 1.0, "oyuncu tepki suresi olculdu");
            CHECK(rs.rivalReaction() >= 0.15 && rs.rivalReaction() <= 0.35 && rs.rival()->launchRpm > 1500.0, "rakip: insan tepkisi + drag kalkis devri");
            CHECK(rs.prizeScale() > 1.5 && rs.prizeScale() <= 2.5, "karma odulu mesafeyle olceklenir");
        }
    }
    std::printf("[9] Polis kovalamacasi: duran yakalanir, hizli giden kacar\n");
    {
        Tune t;
        {   // oyuncu kalkmaz: polis gelir, yakalanir
            RoadSession rs(RoadSession::Mode::Chase, 5, &t, 5, &t, 7u);
            double t0 = 0;
            while (rs.phase() != RoadSession::Phase::Finished && t0 < 90.0) { RoadControls c; c.brake = 1.0; rs.update(1.0 / 60.0, c); t0 += 1.0 / 60.0; }
            std::printf("    duran oyuncu: %.1f s, yakalanma %.2f\n", t0, rs.bustLevel());
            CHECK(rs.phase() == RoadSession::Phase::Finished && !rs.playerWon(), "duran oyuncu yakalanir");
        }
        {   // oyuncu cok daha guclu arac (#78), polis #5: 5 farkli yolda cogunlukla kacmali. Trafik kazasi tek kosuyu
            // degistirebilir; platformlar arasi kayan nokta farki tek tohumlu sonucu ceviriyordu (Linux CI).
            int esc = 0;
            for (uint32_t seed = 7; seed < 12; ++seed) {
                RoadSession rs(RoadSession::Mode::Chase, 78, &t, 5, &t, seed);
                double t0 = 0;
                while (rs.phase() != RoadSession::Phase::Finished && t0 < 400.0) { rs.update(1.0 / 60.0, rs.player().aiControls(-rs.lane(), 0.6)); t0 += 1.0 / 60.0; }
                esc += rs.phase() == RoadSession::Phase::Finished && rs.playerWon();
                std::printf("    guclu arac (yol %u): %.1f s, fark %.0f m, %s\n", seed, t0, rs.gapMeters(), rs.playerWon() ? "KACTI" : "YAKALANDI");
            }
            CHECK(esc >= 2, "guclu arac kacabilir (5 yolda en az 2)");
        }
        {   // ayni arac: polis takip eder (oyuncu YZ ile surse bile polis 400 m'den fazla geride kalmaz ilk 20 s)
            RoadSession rs(RoadSession::Mode::Chase, 227, &t, 227, &t, 7u);
            double t0 = 0, maxGap = -1e9;
            while (t0 < 23.0 && rs.phase() != RoadSession::Phase::Finished) {
                rs.update(1.0 / 60.0, rs.player().aiControls(-rs.lane(), 0.55)); t0 += 1.0 / 60.0; maxGap = std::max(maxGap, rs.gapMeters());
            }
            std::printf("    ayni arac 20 s: en buyuk fark %.0f m\n", maxGap);
            CHECK(maxGap < RoadSession::kEscapeGap, "esit arac: polis yakin takip eder");
        }
    }
    std::printf("[A] Otomatik sanziman: gaza oranli vites, kickdown, S, M, geri vites\n");
    {
        const RoadPath road3(20250930u, 20000.0, 90.0);
        const VehicleDef* sup = nullptr;
        for (int i = 0; i < 1000 && !sup; ++i) if (const VehicleDef* d = findVehicle(i); d && d->ref == std::string("Toyota GR Supra A90")) sup = d;
        CHECK(sup != nullptr, "otomatik (ZF 8HP) test araci bulundu");
        // Ilk yukari vitesin devri: verilen gaz ve mod ile duzde kalkis
        auto firstShiftRpm = [&](double thr, int mode) {
            RoadCar a(sup, nullptr, road3, 0.0, -1.8);
            RoadControls k; k.throttle = thr; k.autoMode = mode;
            double peak = 0;
            for (double t = 0; t < 20.0; t += 1.0 / 60.0) {
                const int g = a.sim().powertrain().gear();
                a.update(1.0 / 60.0, k);
                if (g == 1) peak = std::max(peak, a.sim().powertrain().rpm());
                if (a.sim().powertrain().gear() >= 2) return peak;
            }
            return -1.0;
        };
        const double red = RoadCar(sup, nullptr, road3, 0.0, -1.8).sim().engineSpec().redlineRpm;
        const double light = firstShiftRpm(0.30, 0), full = firstShiftRpm(1.0, 0), sport = firstShiftRpm(0.30, 1);
        std::printf("    1->2 devri: %%30 gaz D %.0f, tam gaz D %.0f, %%30 gaz S %.0f (kesici %.0f)\n", light, full, sport, red);
        CHECK(light > 0 && light < 0.62 * red, "hafif gazda verimli devirde vites atar");
        CHECK(full > red - 800 && full <= red + 50, "tam gazda kesiciye ~500 kala vites atar");
        CHECK(sport > light + 1000, "S modunda ayni gazda daha yuksek devirde vites");
        {   // tam gazda vites yalniz buyur (patinaj / konvertor kaymasi 1-2 arasinda gidip gelmeye yol aciyordu)
            RoadCar a(sup, nullptr, road3, 0.0, -1.8);
            RoadControls k; k.throttle = 1.0;
            int prev = 1, downs = 0;
            for (double t = 0; t < 8.0; t += 1.0 / 60.0) { a.update(1.0 / 60.0, k); const int g = a.sim().powertrain().gear(); downs += g > 0 && g < prev; if (g > 0) prev = g; }
            std::printf("    tam gaz 8 s: son vites %d, vites dusurme %d\n", prev, downs);
            CHECK(downs == 0 && prev >= 3, "tam gazda vites gidip gelmez");
        }
        {   // kickdown: hafif gazla 4. vitese kadar, sonra tam gaz -> en az bir vites asagi
            RoadCar a(sup, nullptr, road3, 0.0, -1.8);
            RoadControls k; k.throttle = 0.30;
            double t = 0;
            for (; t < 40.0 && a.sim().powertrain().gear() < 4; t += 1.0 / 60.0) a.update(1.0 / 60.0, k);
            const int g0 = a.sim().powertrain().gear();
            k.throttle = 1.0;
            int gMin = g0;
            for (double u = 0; u < 1.0; u += 1.0 / 60.0) { a.update(1.0 / 60.0, k); gMin = std::min(gMin, a.sim().powertrain().gear()); }
            std::printf("    kickdown: %d. viteste tam gaz -> %d\n", g0, gMin);
            CHECK(g0 >= 4 && gMin < g0, "tam gazda kickdown (vites dusurur)");
        }
        {   // M: oyuncu vites atmadikca 1. viteste kalir; + ile 2'ye gecer
            RoadCar a(sup, nullptr, road3, 0.0, -1.8);
            RoadControls k; k.throttle = 1.0; k.autoMode = 2;
            for (double t = 0; t < 4.0; t += 1.0 / 60.0) a.update(1.0 / 60.0, k);
            const int g1 = a.sim().powertrain().gear();
            k.shift = 1; a.update(1.0 / 60.0, k); k.shift = 0;
            for (double t = 0; t < 0.5; t += 1.0 / 60.0) a.update(1.0 / 60.0, k);
            const int g2 = a.sim().powertrain().gear();
            std::printf("    M modu: 4 s tam gazda vites %d, + sonrasi %d\n", g1, g2);
            CHECK(g1 == 1 && g2 == 2, "M modunda vitesi yalniz oyuncu atar");
        }
        {   // R: geri gider, ~16 km/h ile sinirli; D'ye donunce ileri gider
            RoadCar a(sup, nullptr, road3, 500.0, -1.8);
            RoadControls k; k.throttle = 1.0; k.reverse = true; k.autoMode = 0;
            for (double t = 0; t < 5.0; t += 1.0 / 60.0) a.update(1.0 / 60.0, k);
            const double vr = a.sim().vx();
            k.reverse = false; k.throttle = 0.0; k.brake = 1.0;
            for (double t = 0; t < 2.0; t += 1.0 / 60.0) a.update(1.0 / 60.0, k);
            k.brake = 0.0; k.throttle = 0.6;
            for (double t = 0; t < 4.0; t += 1.0 / 60.0) a.update(1.0 / 60.0, k);
            const double vf = a.sim().speed();
            std::printf("    geri vites: %.1f km/h, sonra D: %.0f km/h\n", vr * 3.6, vf * 3.6);
            CHECK(vr < -2.0 && vr > -4.6 && std::isfinite(vr), "R ile geri gider (hiz sinirli)");
            CHECK(vf > 10.0, "D'ye donunce ileri gider");
        }
    }
    std::printf("[D] Duz yolda tam gaz: guclu arkadan itisli arac kopmaz\n");
    {
        const RoadPath straight(7u, 20000.0, 4000.0);                       // neredeyse duz yol
        const VehicleDef* sup = nullptr;
        for (int i = 0; i < 1000 && !sup; ++i) if (const VehicleDef* d = findVehicle(i); d && d->ref == std::string("Ford Mustang GT S550")) sup = d;
        if (!sup) sup = findVehicle(227);
        Tune t; t.tires = TireType::Street;
        RoadCar a(sup, &t, straight, 0.0, -1.8);
        a.manual = false; a.assist = false; a.stability = true;
        RoadControls k; k.throttle = 1.0; k.steer = 0.0;
        double maxBeta = 0, maxLat = 0;
        for (double tt = 0; tt < 14.0; tt += 1.0 / 60.0) {
            a.update(1.0 / 60.0, k);
            if (a.sim().speed() > 5) maxBeta = std::max(maxBeta, std::fabs(a.sim().bodySlipAngle()));
            maxLat = std::max(maxLat, std::fabs(a.lateral() + 1.8));
        }
        std::printf("    %s: %.0f km/h, en buyuk govde kaymasi %.1f derece, serit sapmasi %.2f m\n", sup->model, a.sim().speed() * 3.6, maxBeta * 57.3, maxLat);
        CHECK(maxBeta < 0.10, "duz yolda govde kaymasi < 6 derece");
    }
    std::printf("[F] The Run: 40 rakip, tarzlar, yakit, benzinlik, siralama\n");
    {
        RoadSession rs(RoadSession::Mode::Marathon, 5, nullptr, 0, nullptr, 3u);
        std::vector<RunEntrant> f;
        const int ids[8] = {5, 227, 217, 36, 255, 269, 78, 100};
        for (int i = 0; i < 40; ++i) f.push_back({ids[i % 8], Tune{}, i % StyleCount});
        rs.setRunField(f, 300.0);
        double t = 0; int pStops = 0; bool pw = false;
        while (rs.phase() != RoadSession::Phase::Finished && t < 1800.0) {
            RoadControls c = rs.player().aiControls(rs.rightLane(rs.player().s()), 0.55);
            rs.pitControls(0, 0.55, c);
            rs.update(1.0 / 60.0, c); t += 1.0 / 60.0;
            if (rs.refueling(0) && !pw) ++pStops;
            pw = rs.refueling(0);
        }
        int fin = 0, dry = 0; double pitsBy[StyleCount] = {0}, nBy[StyleCount] = {0}, l100[StyleCount] = {0};
        for (const Runner& R : rs.runField().runners()) {
            fin += R.finished; dry += R.dry;
            pitsBy[R.style] += R.pits; nBy[R.style] += 1;
            l100[R.style] += R.usedL / std::max(1.0, rs.realKm() * (R.s - rs.startS()) / rs.raceLength()) * 100.0;
        }
        for (int k = 0; k < StyleCount; ++k) { pitsBy[k] /= std::max(1.0, nBy[k]); l100[k] /= std::max(1.0, nBy[k]); }
        std::printf("    oyuncu: %d. / %d, %.0f s, %d mola; rakip bitiren %d, yakitsiz %d\n", rs.runPosition(), rs.runCount(), rs.playerTime(), pStops, fin, dry);
        for (int k = 0; k < StyleCount; ++k) std::printf("    %-8s ort. mola %.1f, %.1f L/100km\n", runStyleName(k), pitsBy[k], l100[k]);
        CHECK(rs.playerTime() > 0 && pStops >= 1, "oyuncu benzinlikte durup etabi bitirir");
        CHECK(dry == 0, "rakiplerin yakiti yolda bitmez (strateji)");
        CHECK(pitsBy[StyleFlatOut] > pitsBy[StyleEco], "dip gaz eko suruculerden daha cok benzinlige girer");
        CHECK(l100[StyleEco] < l100[StyleFlatOut], "eko surucu 100 km'de daha az yakar");
        CHECK(rs.runPosition() >= 1 && rs.runPosition() <= rs.runCount(), "siralama tutarli");
    }
    std::printf("[B] Fren: hafif basista kilitlenmez, tam basista guclu yavaslar (ABS'siz Sahin)\n");
    {
        const RoadPath straight(7u, 20000.0, 4000.0);
        auto brakeRun = [&](double pedal, double& maxSlip, double& decel) {
            RoadCar a(findVehicle(217), nullptr, straight, 0.0, -1.8);
            a.manual = false; a.assist = false;
            RoadControls k; k.throttle = 1.0;
            for (double t = 0; t < 30.0 && a.sim().speed() < 100 / 3.6; t += 1.0 / 60.0) a.update(1.0 / 60.0, k);
            const double v0 = a.sim().speed();
            k.throttle = 0.0; k.brake = pedal; maxSlip = 0;
            for (int i = 0; i < 60; ++i) {
                a.update(1.0 / 60.0, k);
                for (int w = 0; w < 4; ++w) {
                    const auto& wh = a.sim().wheel(w);
                    maxSlip = std::max(maxSlip, (a.sim().speed() - wh.omega() * wh.rEff()) / std::max(a.sim().speed(), 3.0));
                }
            }
            decel = (v0 - a.sim().speed()) / 1.0 / 9.81;
        };
        double sL, dL, sF, dF, sM, dM;
        brakeRun(0.35, sL, dL); brakeRun(1.0, sF, dF); brakeRun(0.6, sM, dM);
        std::printf("    %%35 pedal: en buyuk teker kaymasi %.2f, %.2f g | tam pedal: kayma %.2f, %.2f g\n", sL, dL, sF, dF);
        CHECK(sL < 0.15 && dL > 0.15, "hafif frende teker kilitlenmez, yavaslar");
        CHECK(dF > 0.7, "tam frende guclu yavaslama (> 0.7 g; ABS'siz arac tam basista kilitlenebilir)");
        std::printf("    %%60 pedal: kayma %.2f, %.2f g\n", sM, dM);
        CHECK(sM < 0.2 && dM > 0.5, "%60 frende kilitlenmez, guclu yavaslar");
    }
    std::printf("[E] ESP: ani serit degistirmede (120 km/h, E46 RWD) govde kaymasi ESP ile kucuk\n");
    {
        const RoadPath straight(7u, 20000.0, 4000.0);
        auto fishhook = [&](bool esp, double& maxBeta, double& lat) {
            RoadCar a(findVehicle(122), nullptr, straight, 0.0, -1.8);
            a.manual = false; a.assist = false; a.esp = esp;
            RoadControls k; k.throttle = 1.0;
            for (double t = 0; t < 40.0 && a.sim().speed() < 120 / 3.6; t += 1.0 / 60.0) a.update(1.0 / 60.0, k);
            maxBeta = 0;
            for (int i = 0; i < 240; ++i) {                             // 0.5 s sola, 0.5 s saga, sonra duz; gaz acik
                k.throttle = 0.6; k.steer = i < 30 ? 0.10 : i < 60 ? -0.10 : 0.0;
                a.update(1.0 / 60.0, k);
                maxBeta = std::max(maxBeta, std::fabs(a.sim().bodySlipAngle()));
            }
            lat = a.lateral();
        };
        double bOff, bOn, lOff, lOn;
        fishhook(false, bOff, lOff); fishhook(true, bOn, lOn);
        std::printf("    en buyuk govde kaymasi: ESP kapali %.1f deg, acik %.1f deg\n", bOff * 57.3, bOn * 57.3);
        CHECK(bOn < bOff * 0.8 || bOn < 0.05, "ESP savrulmayi azaltir");
        CHECK(std::isfinite(lOn), "ESP ile kararli");
    }
    std::printf("[G] DSG takili arac D konumunda kendisi vites atar (otomatik kol)\n");
    {
        const RoadPath straight(7u, 20000.0, 4000.0);
        Tune t;
        for (int i = 0; i < (int)partOptions(PartCat::Gearbox).size(); ++i)
            if (std::string(partOptions(PartCat::Gearbox)[i].name) == "VW DQ500 DSG 7") t.gearSwap = i;
        RoadCar a(findVehicle(122), &t, straight, 0.0, -1.8);
        CHECK(t.gearSwap > 0 && a.sim().gearboxType() == Gearbox::DCT, "E46 + DQ500 = cift kavrama");
        a.manual = false;
        RoadControls k; k.throttle = 1.0; k.autoMode = 0;              // D
        int maxGear = 0;
        for (int i = 0; i < 60 * 15; ++i) { a.update(1.0 / 60.0, k); maxGear = std::max(maxGear, a.sim().powertrain().gear()); }
        std::printf("    15 s tam gaz D: %.0f km/h, en yuksek vites %d\n", a.sim().speed() * 3.6, maxGear);
        CHECK(maxGear >= 4 && a.sim().speed() > 150 / 3.6, "D konumunda otomatik vites");
    }
    std::printf(failures ? "\nSONUC: %d test KALDI\n" : "\nSONUC: tum testler gecti\n", failures);
    return failures ? 1 : 0;
}
