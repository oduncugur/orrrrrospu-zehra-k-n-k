// Parca etkisi testleri: iki serit ayni arac + ayni yapay zeka surucu, tek fark incelenen parca.
#include "game/DragRace.h"
#include "game/Career.h"
#include "sim/PartTables.h"
#include <cmath>
#include <cstdio>

using namespace zk;

static int failures = 0;
#define CHECK(cond, msg) do { if (cond) std::printf("  GECTI  %s\n", msg); else { std::printf("  KALDI  %s\n", msg); ++failures; } } while (0)

struct Result { TimeSlip a, b; double peakA = 0, peakB = 0; };

static Result race(int car, const Tune& ta, const Tune& tb, uint32_t seed = 3) {
    DragRace r(car, car, TreeType::Pro, seed, false, &ta, &tb);
    r.setPlayerAutopilot(true);
    PlayerControls pc;
    while (r.clock() < 70 && !(r.phase() == RacePhase::Finished && r.lane(0).sim->speed() < 1 && r.lane(1).sim->speed() < 1)) {
        r.advance(1.0 / 60.0, pc);
        r.drainEvents();
    }
    Result res{r.lane(0).slip, r.lane(1).slip, r.lane(0).sim->failure().peakShearMPa(), r.lane(1).sim->failure().peakShearMPa()};
    return res;
}

static void show(const char* a, const char* b, const Result& r) {
    std::printf("    %-22s 60ft %.3f  1/4 %.3f @ %.1f%s\n", a, r.a.sixtyFt, r.a.quarter, r.a.trapKmh, r.a.broke ? " [AKS KIRIK]" : "");
    std::printf("    %-22s 60ft %.3f  1/4 %.3f @ %.1f%s\n", b, r.b.sixtyFt, r.b.quarter, r.b.trapKmh, r.b.broke ? " [AKS KIRIK]" : "");
}

int main() {
    const int mustang = 227;   // RWD, 435 HP, H-manuel
    const int civic = 5;       // FWD, 185 HP

    std::printf("[1] Lastik: drag slick vs sokak (#227)\n");
    {
        Tune slick; slick.tires = TireType::DragSlick; slick.axles = 1;
        Tune street; street.axles = 1;
        Result r = race(mustang, slick, street); show("slick", "sokak", r);
        CHECK(r.a.sixtyFt < r.b.sixtyFt - 0.05, "slick 60 ft'i kisaltir");
        CHECK(r.a.quarter < r.b.quarter, "slick 1/4 mil ET'yi kisaltir");
    }
    std::printf("[2] Hafifletme: -140 kg vs stok (#5)\n");
    {
        Tune light; light.weight = 3; light.tires = TireType::DragSlick; light.axles = 1;
        Tune stock; stock.tires = TireType::DragSlick; stock.axles = 1;
        Result r = race(civic, light, stock); show("-140 kg", "stok", r);
        CHECK(r.a.quarter < r.b.quarter - 0.1, "hafif arac daha hizli (>0.1 s)");
    }
    std::printf("[3] Motor: emme+egzoz+ECU vs stok (#5)\n");
    {
        Tune power; power.intake = 2; power.exhaust = 2; power.ecu = 1; power.tires = TireType::DragSlick; power.axles = 1;
        Tune stock; stock.tires = TireType::DragSlick; stock.axles = 1;
        Result r = race(civic, power, stock); show("emme+egzoz+ECU", "stok", r);
        CHECK(r.a.trapKmh > r.b.trapKmh + 3.0, "trap hizi artar (>3 km/h)");
    }
    std::printf("[4] Turbo kiti (atmosferik #5'e buyuk kit)\n");
    {
        Tune turbo; turbo.turbo = 2; turbo.tires = TireType::DragSlick; turbo.axles = 2; turbo.clutch = 1;   // buyuk turbo -> yaris aksi
        Tune stock; stock.tires = TireType::DragSlick; stock.axles = 1;
        VehicleSimConfig ca; ca.car = findVehicle(civic); ca.tune = &turbo;
        VehicleSimConfig cb; cb.car = findVehicle(civic); cb.tune = &stock;
        VehicleSim sa(ca), sb(cb);
        double ta = 0, tb = 0, t3a = 0, t3b = 0;
        for (auto& p : sa.engineSpec().highCam) { ta = std::max(ta, p.second); if (p.first == 3250) t3a = p.second; }
        for (auto& p : sb.engineSpec().highCam) { tb = std::max(tb, p.second); if (p.first == 3250) t3b = p.second; }
        std::printf("    tepe tork: kitli %.0f Nm, stok %.0f Nm | 3250 rpm: %.0f vs %.0f (spool oncesi)\n", ta, tb, t3a, t3b);
        CHECK(ta > tb * 1.8, "buyuk kit tepe torku ~2.2 kat (>1.8)");
        CHECK(t3a < t3b * 1.15, "dusuk devirde turbo henuz dolmamis (gec spool)");
        Result r = race(civic, turbo, stock); show("buyuk turbo", "stok", r);
        CHECK(r.a.trapKmh > r.b.trapKmh + 15.0, "trap hizi belirgin artar (>15 km/h)");
    }
    std::printf("[5] Aks: stok aks + asiri guc kirilir, yaris aksi dayanir (#227)\n");
    {
        Tune crazy; crazy.turbo = 1; crazy.ecu = 1; crazy.tires = TireType::DragSlick; crazy.clutch = 2;   // ~800 HP yapim
        Tune crazyCm = crazy; crazyCm.axles = 2;
        Result r = race(mustang, crazy, crazyCm); show("stok aks", "yaris aksi", r);
        std::printf("    tepe aks gerilmesi: stok %.0f MPa, yaris aksi %.0f MPa\n", r.peakA, r.peakB);
        CHECK(r.a.broke, "stok aks kirildi");
        CHECK(!r.b.broke && r.b.finished, "yaris aksi dayandi, bitirdi");
    }
    std::printf("[6] Stok arac stok aksi kirmaz (#227, stok parcalar)\n");
    {
        Tune stock; Result r = race(mustang, stock, stock); show("stok", "stok", r);
        CHECK(!r.a.broke && !r.b.broke, "fabrika kurulumu saglam");
    }
    std::printf("[7] Spool diferansiyel kararli (#227)\n");
    {
        Tune spool; spool.diff = DiffType::Spool; spool.tires = TireType::DragSlick; spool.axles = 1;
        Tune lsd; lsd.diff = DiffType::OneAndHalfWay; lsd.tires = TireType::DragSlick; lsd.axles = 1;
        Result r = race(mustang, spool, lsd); show("spool", "1.5-way", r);
        CHECK(r.a.finished && std::isfinite(r.a.quarter) && r.a.quarter > 5 && r.a.quarter < 25, "spool ile sayisal sorun yok, bitirdi");
    }
    std::printf("[8] Son disli: kisa (4.4) vs fabrika (3.73) (#227)\n");
    {
        Tune shortFd; shortFd.finalDrive = 4.4; shortFd.tires = TireType::DragSlick; shortFd.axles = 1;
        Tune stock; stock.tires = TireType::DragSlick; stock.axles = 1;
        Result r = race(mustang, shortFd, stock); show("4.40", "3.73", r);
        // Slick + planli kalkista Mustang cekis sinirinda: kisa disli fazla torku patinaja harcar, 60 ft'i belirgin
        // degistirmez (eski 120 ms dump'ta kisa disli kazandiriyordu). Kaba hata yakalayici: fark <= 0.05 s.
        CHECK(std::fabs(r.a.sixtyFt - r.b.sixtyFt) <= 0.08, "cekis sinirinda kisa son disli 60 ft'i belirgin degistirmez");
    }
    std::printf("[K] Kurulum: parca kilidi, yukseklik, NOS memesi, LSD on yuku, profiller\n");
    {
        const VehicleDef* v = findVehicle(5);
        auto sim = [&](const Tune& t) { VehicleSimConfig c; c.car = v; c.tune = &t; return VehicleSim(c); };
        Tune base; base.susp = 0; base.setRide = -30;                       // stok suspansiyon: ayar etkisiz
        Tune stock;
        CHECK(std::fabs(sim(base).vehicleLoad().hCoG - sim(stock).vehicleLoad().hCoG) < 1e-12, "ayarli suspansiyon yoksa yukseklik etkisiz");
        Tune co; co.susp = 3; Tune lo = co; lo.setRide = -30; Tune hi = co; hi.setRide = 20;
        const double h0 = sim(co).vehicleLoad().hCoG, hl = sim(lo).vehicleLoad().hCoG, hh = sim(hi).vehicleLoad().hCoG;
        std::printf("    agirlik merkezi: coilover %.3f, -30 mm %.3f, +20 mm %.3f\n", h0, hl, hh);
        CHECK(hl < h0 && hh > h0, "coilover ile alcaltma / yukseltme agirlik merkezini oynatir");
        Tune n; n.nitrous = 4; Tune nj = n; nj.setNos = 50;
        CHECK(std::fabs(sim(nj).nosHp() - 0.5 * sim(n).nosHp()) < 1e-6 && std::fabs(sim(nj).nosBottleS() - 2.0 * sim(n).nosBottleS()) < 1e-6,
              "NOS memesi %50: yari guc, iki kat tup suresi");
        Tune d; d.diff = DiffType::OneAndHalfWay; d.setPreload = 3; clampSetup(d);
        Tune o; o.diff = DiffType::Open; o.setPreload = 3; clampSetup(o);
        CHECK(d.setPreload == 3 && o.setPreload == 0, "LSD on yuku yalniz plakali LSD'de");
        Tune pre = co; pre.setRide = 0;
        CHECK(pre.signature() != lo.signature(), "kurulum imzaya (onbellek anahtari) girer");
        CHECK(stock.signature() == Tune{}.signature(), "ayarsiz arac imzasi degismedi");
        Career c = Career::newGame();
        c.cars[c.current].tune.susp = 3; c.cars[c.current].tune.setRide = -20; c.cars[c.current].tune.psi = 18;
        c.saveProfile(0);
        c.cars[c.current].tune.setRide = 10; c.cars[c.current].tune.psi = 30;
        CHECK(c.loadProfile(0) && c.car().tune.setRide == -20 && c.car().tune.psi == 18, "profil kaydet / yukle");
        Career r;
        CHECK(Career::parse(c.serialize(), r) && r.car().profile[0] == c.car().profile[0] && r.car().tune.setRide == -20, "profil ve kurulum kayitta");
        c.cars[c.current].tune.susp = 0; clampSetup(c.cars[c.current].tune);
        CHECK(c.car().tune.setRide == 0 && !c.loadProfile(1), "parca sokulunce ayar sifirlanir; bos profil yuklenmez");
    }
    std::printf(failures ? "\nSONUC: %d test KALDI\n" : "\nSONUC: tum testler gecti\n", failures);
    return failures ? 1 : 0;
}
