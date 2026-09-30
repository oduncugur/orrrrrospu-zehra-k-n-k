// Drag yarisi mantik testleri (basliksiz). Her test PASS/FAIL yazar; herhangi biri basarisizsa cikis kodu 1.
#include "game/DragRace.h"
#include <cmath>
#include <cstdio>
#include <functional>

using namespace zk;

static int failures = 0;
#define CHECK(cond, msg) do { if (cond) std::printf("  GECTI  %s\n", msg); else { std::printf("  KALDI  %s\n", msg); ++failures; } } while (0)

// Senaryolu oyuncu: agacta debriyaj basili + gaz, yesil + gecikme ile debriyaji birakir, redline'da H-desen vites atar
struct ScriptedPlayer {
    double launchDelay = 0.10;     // tetikleyici isiktan sonra gecikme
    bool   leaveOnLastAmber = false;  // sportsman teknigi: son amberde kalk (yesili beklemeden)
    bool   useClutchForShift = true;
    double shiftT = -1; int target = 1;
    PlayerControls operator()(DragRace& r, double greenAt, double dt) {
        PlayerControls pc;
        const auto& L = r.lane(0);
        const PowertrainCore& pt = L.sim->powertrain();
        const Gearbox box = L.sim->gearboxType();
        pc.throttle = r.phase() == RacePhase::Tree || r.phase() == RacePhase::Run ? 1.0 : 0.0;
        const double t = r.clock() - (greenAt + launchDelay);
        if (box == Gearbox::DCT || box == Gearbox::TorqueConverter) {
            pc.clutch = 0.0; pc.brake = t < 0 ? 1.0 : 0.0;
            if (box == Gearbox::DCT && t > 0 && pt.rpm() > L.sim->shiftRpm()) pc.paddle = +1;
            return pc;
        }
        pc.clutch = t < 0 ? 1.0 : std::max(0.0, 1.0 - t / 0.15);
        if (box == Gearbox::Dogbox) { if (t > 0 && pt.rpm() > L.sim->shiftRpm()) pc.paddle = +1; return pc; }
        // H-desen: 0.25 s'lik vites: debriyaj bas, vites, birak
        if (shiftT < 0 && t > 0.3 && pt.rpm() > L.sim->shiftRpm() && pt.gear() < pt.gearCount()) { shiftT = 0; target = pt.gear() + 1; }
        if (shiftT >= 0) {
            shiftT += dt;
            if (useClutchForShift) pc.clutch = shiftT < 0.15 ? 1.0 : std::max(0.0, 1.0 - (shiftT - 0.15) / 0.1);
            pc.throttle = shiftT < 0.15 ? 0.0 : 1.0;
            if (shiftT > 0.08) pc.requestedGear = target;
            if (shiftT > 0.25) shiftT = -1;
        }
        return pc;
    }
};

static void run(DragRace& r, ScriptedPlayer& p, double maxT = 60.0) {
    const double dt = 1.0 / 60.0;
    double greenAt = 1e9;
    while (r.clock() < maxT && !(r.phase() == RacePhase::Finished && r.lane(0).sim->speed() < 0.5)) {
        // Oyuncu yalnizca gordugu isiga tepki verir (1 kare gecikmeli)
        const int lights = r.treeLights();
        if (greenAt > 1e8 && ((lights & 8) || (p.leaveOnLastAmber && (lights & 4)))) greenAt = r.clock();
        // Agac yaninca yesil zamanini tahmin et: oyuncu yesili goremez, isigi bekler
        PlayerControls pc = p(r, greenAt, dt);
        r.advance(dt, pc);
        r.drainEvents();
    }
}

static void printSlip(const char* who, const TimeSlip& s) {
    std::printf("    %-6s RT %+.3f | 60ft %.3f | 1/8 %.3f @ %.1f | 1/4 %.3f @ %.1f km/h%s%s\n", who, s.reaction, s.sixtyFt,
                s.eighth, s.eighthKmh, s.quarter, s.trapKmh, s.redLight ? " [KIRMIZI]" : "", s.broke ? " [KIRIK]" : "");
}

int main() {
    std::printf("[1] Temiz kalkis, H-desen (#5 vs #5, sportsman)\n");
    {
        DragRace r(5, 5, TreeType::Sportsman, 42, false);
        ScriptedPlayer p; p.launchDelay = 0.10;
        run(r, p);
        printSlip("SEN", r.lane(0).slip); printSlip("RAKIP", r.lane(1).slip);
        CHECK(!r.lane(0).slip.redLight, "kirmizi isik yok");
        CHECK(r.lane(0).slip.finished, "oyuncu bitirdi");
        CHECK(r.lane(0).slip.reaction > 0.05 && r.lane(0).slip.reaction < 0.6, "reaksiyon makul (0.05-0.6 s)");
        CHECK(r.lane(0).slip.quarter > 13.0 && r.lane(0).slip.quarter < 18.0, "1/4 mil ET makul (13-18 s)");
        CHECK(r.lane(0).sim->powertrain().gear() >= 4, "vitesler atildi (>=4)");
        CHECK(r.winner() >= 0 && r.phase() == RacePhase::Finished, "yaris sonuclandi");
    }
    std::printf("[2] Erken kalkis -> kirmizi isik\n");
    {
        DragRace r(5, 5, TreeType::Sportsman, 42, false);
        ScriptedPlayer p; p.leaveOnLastAmber = true; p.launchDelay = 0.0;
        run(r, p);
        printSlip("SEN", r.lane(0).slip);
        CHECK(r.lane(0).slip.redLight, "kirmizi isik yakildi");
        CHECK(r.lane(0).slip.reaction < 0, "reaksiyon negatif");
        CHECK(r.winner() == 1, "kirmizi isik yakan kaybeder");
    }
    std::printf("[3] Debriyajsiz vites -> dis citirtisi, vites girmez\n");
    {
        DragRace r(5, 5, TreeType::Sportsman, 42, false);
        ScriptedPlayer p; p.useClutchForShift = false;
        run(r, p, 30.0);
        CHECK(r.lane(0).sim->powertrain().gear() == 1, "vites 1'de kaldi");
    }
    std::printf("[4] Pro agac: yesil amberden 0.400 s sonra\n");
    {
        DragRace r(5, 5, TreeType::Pro, 7, false);
        PlayerControls pc; pc.clutch = 1.0;
        double amberAt = -1, greenAt = -1;
        for (int i = 0; i < 240 * 8 && greenAt < 0; ++i) {
            r.advance(1.0 / 240.0, pc);
            if (amberAt < 0 && (r.treeLights() & 7) == 7) amberAt = r.clock();
            if (greenAt < 0 && (r.treeLights() & 8)) greenAt = r.clock();
        }
        std::printf("    amber %.4f yesil %.4f fark %.4f\n", amberAt, greenAt, greenAt - amberAt);
        CHECK(std::fabs(greenAt - amberAt - 0.4) < 0.006, "pro agac 0.400 s (+-1 kare)");
    }
    std::printf("[5] Determinizm: ayni tohum ayni sonuc\n");
    {
        DragRace a(227, 186, TreeType::Sportsman, 99, false), b(227, 186, TreeType::Sportsman, 99, false);
        ScriptedPlayer pa, pb;
        run(a, pa); run(b, pb);
        printSlip("A", a.lane(0).slip); printSlip("B", b.lane(0).slip);
        CHECK(a.lane(0).slip.quarter == b.lane(0).slip.quarter && a.lane(1).slip.quarter == b.lane(1).slip.quarter,
              "iki kosu birebir ayni");
    }
    std::printf("[6] Sanziman tipleri: DCT, dogbox, tork konvertoru, H-desen\n");
    for (int id : {55, 23, 36, 186}) {   // DCT (AWD), dogbox (FWD), tork konvertoru (RWD), H-desen (RWD)
        const VehicleDef* v = findVehicle(id);
        DragRace r(id, id, TreeType::Sportsman, 5, false);
        ScriptedPlayer p;
        run(r, p);
        std::printf("    #%d %s (%s)\n", id, v->fullName().c_str(), gearboxName(gearboxTable()[v->gearbox].type));
        printSlip("SEN", r.lane(0).slip); printSlip("RAKIP", r.lane(1).slip);
        CHECK(r.lane(0).slip.finished && !r.lane(0).slip.redLight, "oyuncu temiz bitirdi");
        CHECK(r.lane(1).slip.finished, "yapay zeka bitirdi");
    }
    std::printf(failures ? "\nSONUC: %d test KALDI\n" : "\nSONUC: tum testler gecti\n", failures);
    return failures ? 1 : 0;
}
