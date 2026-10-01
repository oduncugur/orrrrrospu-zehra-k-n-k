// Modifiye sistemi: kategori kapsami, parca fizigi (turbo, nitro, swap, dayanim, isi), atolye, kayit
#include "game/Career.h"
#include "game/RoadCar.h"
#include "garage/VehicleCatalog.h"
#include "sim/PartTables.h"
#include "sim/VehicleSim.h"
#include <cmath>
#include <cstdio>

using namespace zk;
static int failures = 0;
#define CHECK(cond, msg) do { if (cond) std::printf("  GECTI  %s\n", msg); else { std::printf("  KALDI  %s\n", msg); ++failures; } } while (0)

static double peakHp(const VehicleDef& v, const Tune& t) {
    VehicleSimConfig c; c.car = &v; c.tune = &t;
    const VehicleSim s(c);
    double hp = 0;
    for (auto* cv : {&s.engineSpec().lowCam, &s.engineSpec().highCam})
        for (auto& p : *cv) if (p.first <= s.engineSpec().redlineRpm) hp = std::max(hp, p.second * p.first / 7120.9);
    return hp;
}

// Tam gaz surus: acik yolda (uzun otoban) oyunun surus yardimi (otomatik debriyaj + vites), direksiyon YZ, gaz tam
static RoadCar* runWot(const VehicleDef& v, const Tune& t, double seconds) {
    static const RoadPath road(7u, 20000.0, 400.0, 3.6, 0.0);
    RoadCar* c = new RoadCar(&v, &t, road, 20.0, -1.8);
    for (double tt = 0; tt < seconds; tt += 1.0 / 60.0) {
        RoadControls rc = c->aiControls(-1.8, 0.8);
        rc.throttle = 1.0; rc.brake = 0.0;
        c->update(1.0 / 60.0, rc);
    }
    return c;
}

int main() {
    std::printf("[1] Kategori kapsami (en az 10 secenek; lastik ve turbo 30; motor swap tum motorlar)\n");
    {
        int few = 0;
        for (int i = 0; i < (int)PartCat::Count; ++i) if (partOptions((PartCat)i).size() < 10) { ++few; std::printf("    %s: %zu\n", partCatName((PartCat)i), partOptions((PartCat)i).size()); }
        std::printf("    %d kategori, lastik %zu, turbo %zu, sanziman %zu, motor swap %zu\n", (int)PartCat::Count,
                    partOptions(PartCat::Tires).size(), partOptions(PartCat::Turbo).size(), partOptions(PartCat::Gearbox).size(), partOptions(PartCat::EngineSwap).size());
        CHECK(few == 0, "her kategori en az 10 secenek");
        CHECK(partOptions(PartCat::Tires).size() >= 30 && partOptions(PartCat::Turbo).size() >= 30 && partOptions(PartCat::Gearbox).size() >= 30,
              "lastik / turbo / sanziman 30 secenek");
        CHECK(partOptions(PartCat::EngineSwap).size() == engineTable().size() + 1, "motor swap: katalogdaki tum motorlar");
        for (int g = 1; g < (int)gearTable().size(); ++g)
            if (gearTable()[g].code && findGearbox(gearTable()[g].code) < 0) { std::printf("    eksik sanziman %s\n", gearTable()[g].code); ++failures; }
    }
    const VehicleDef& sahin = *findVehicle(217);
    std::printf("[2] Parca fizigi\n");
    {
        Tune stock, small, big; small.turbo = 5; big.turbo = 12; big.fuelSys = 6;
        const double h0 = peakHp(sahin, stock), h1 = peakHp(sahin, small), h2 = peakHp(sahin, big);
        Tune bigNoFuel = big; bigNoFuel.fuelSys = 0;
        const double h3 = peakHp(sahin, bigNoFuel);
        std::printf("    Sahin: stok %.0f HP, GT2560 %.0f, GT4088+yakit %.0f, GT4088 stok yakit %.0f\n", h0, h1, h2, h3);
        CHECK(h1 > h0 * 1.3 && h2 > h1 * 1.4, "buyuk turbo daha cok guc");
        CHECK(h3 < h2 * 0.8, "stok yakit sistemi buyuk turboyu kisar (yakit siniri)");
        Tune cam; cam.cam = 8; cam.valve = 6;
        VehicleSimConfig a; a.car = &sahin; a.tune = &stock; const VehicleSim sa(a);
        VehicleSimConfig b; b.car = &sahin; b.tune = &cam; const VehicleSim sb(b);
        CHECK(sb.engineSpec().redlineRpm > sa.engineSpec().redlineRpm + 1000, "yaris kami + titanyum supap devir sinirini yukseltir");
        Tune swap; swap.engineSwap = (int)swapEngines().size();                  // en guclu motor
        const double hs = peakHp(sahin, swap);
        std::printf("    Sahin + en guclu motor swap: %.0f HP, kutle farki %+.0f kg\n", hs, swapMassDelta(sahin, swap));
        CHECK(hs > 600 && swapMassDelta(sahin, swap) > 50, "motor swap: takili motorun gucu ve agirligi");
        Tune nos; nos.nitrous = 5; nos.tires = TireType::DragSlick; nos.axles = 9; nos.tireSel = 25; nos.clutch = 9; stock.clutch = 9; stock.tires = TireType::DragSlick; stock.axles = 9; stock.tireSel = 25;
        RoadCar* n0 = runWot(sahin, stock, 8.0); RoadCar* n1 = runWot(sahin, nos, 8.0);
        std::printf("    8 s sonunda hiz: nitrosuz %.1f, nitrolu %.1f m/s\n", n0->sim().speed(), n1->sim().speed());
        CHECK(n1->sim().hasNitrous() && n1->sim().speed() > n0->sim().speed() * 1.08, "nitro tam gazda hizlandirir");
    }
    std::printf("[3] Dayanim ve isi\n");
    {
        Tune over; over.turbo = 14; over.fuelSys = 9; over.axles = 9; over.clutch = 0; over.gbStrength = 9; over.tires = TireType::DragSlick;                            // GT45, stok ic aksam
        const Durability d0 = durability(sahin, &over), dStock = durability(sahin, nullptr);
        RoadCar* sc = runWot(sahin, over, 20.0); const VehicleSim& s = sc->sim();
        std::printf("    debug: GT45 tepe %.0f HP, hiz %.1f, rpm %.0f, tork %.0f, aks kirik %d\n", peakHp(sahin, over), s.speed(), s.powertrain().rpm(), s.powertrain().engineTorque(), s.failure().snapped(0) ? 1 : 0);
        std::printf("    Sahin GT45 stok ic aksam: dayanim %.0f Nm, motor zorlanma %.2f, patladi %d\n", d0.engineNm, s.engineStress(), s.engineBlown());
        CHECK(s.engineBlown(), "stok ic aksam asiri boosta dayanmaz (motor patlar)");
        Tune built = over; built.piston = 9; built.rod = 9; built.crank = 4; built.bearing = 9; built.gasket = 7; built.gbStrength = 9;
        const Durability d1 = durability(sahin, &built);
        CHECK(d1.engineNm > d0.engineNm * 1.8 && d1.gearboxNm > dStock.gearboxNm * 2.5, "dovme ic aksam + guclendirilmis sanziman dayanimi artirir");
        Tune hot; hot.turbo = 9; hot.fuelSys = 5; hot.piston = 6; hot.rod = 4; hot.crank = 4; hot.axles = 9; hot.clutch = 9; hot.gbStrength = 9;
        const double T0 = 88.0;
        RoadCar* hc = runWot(sahin, hot, 150.0); const VehicleSim& h = hc->sim();       // uzun tam yuk
        std::printf("    debug: v %.1f m/s rpm %.0f vites %d tork %.0f\n", h.speed(), h.powertrain().rpm(), h.powertrain().gear(), h.powertrain().engineTorque());
        std::printf("    Sahin GT3076, stok sogutma, 150 s tam yuk: su %.0f -> %.0f C\n", T0, h.coolantC());
        Tune cool = hot; cool.cooling = 9;
        RoadCar* hc2 = runWot(sahin, cool, 150.0); const VehicleSim& h2 = hc2->sim();
        std::printf("    ayni arac yaris sogutma: su %.0f C\n", h2.coolantC());
        CHECK(h.coolantC() > T0 + 5 && h2.coolantC() < h.coolantC() - 5, "sogutma yukseltmesi motoru serin tutar");
    }
    std::printf("[3b] Vuruntu: oktan / boost / ECU korumasi\n");
    {
        Tune k95; k95.turbo = 2; k95.fuelSel = 1; k95.clutch = 9; k95.axles = 9; k95.tireSel = 25; k95.tires = TireType::DragSlick;
        k95.piston = 9; k95.rod = 9; k95.crank = 4; k95.bearing = 9; k95.gbStrength = 9;
        Tune k116 = k95; k116.fuelSel = 7;
        Tune stock;
        RoadCar* a = runWot(sahin, k95, 10.0); RoadCar* b = runWot(sahin, k116, 10.0); RoadCar* c = runWot(sahin, stock, 3.0);
        std::printf("    95 oktan: istenen %.1f, vuruntu %d, %.1f m/s | 116 oktan: istenen %.1f, %.1f m/s | stok istenen %.1f / %.0f\n",
                    a->sim().octaneRequired(), a->sim().knocking() ? 1 : 0, a->sim().speed(), b->sim().octaneRequired(), b->sim().speed(),
                    c->sim().octaneRequired(), c->sim().octane());
        CHECK(a->sim().octaneRequired() > 95.0 && b->sim().octaneRequired() < 116.0, "buyuk turbo 95 oktana fazla, 116 yeter");
        CHECK(c->sim().octaneRequired() < 95.0, "stok motor pompa benzinine razi");
        CHECK(b->sim().speed() > a->sim().speed() + 0.3, "vuruntuda ECU avansi ceker: dusuk oktan yavas");
        Tune sa = k95; sa.ecu = 4;                       // standalone: vuruntu korumasi yok
        Tune sb = k116; sb.ecu = 4;
        RoadCar* d = runWot(sahin, sa, 12.0); RoadCar* e = runWot(sahin, sb, 12.0);
        std::printf("    standalone stres: 95 oktan %.3f, 116 oktan %.3f\n", d->sim().engineStress(), e->sim().engineStress());
        CHECK(d->sim().engineStress() > e->sim().engineStress() + 0.02, "korumasiz ECU + dusuk oktan motoru dover");
        delete a; delete b; delete c; delete d; delete e;
    }
    std::printf("[3c] Lastik asinmasi: patinaj lastigi yer, kariyere yazilir\n");
    {
        Tune spin; spin.turbo = 2; spin.fuelSel = 7; spin.clutch = 9; spin.axles = 9;   // sokak lastigi + cok guc: patinaj
        Tune grip = spin; grip.tires = TireType::DragSlick; grip.tireSel = 25;
        RoadCar* a = runWot(sahin, spin, 6.0); RoadCar* b = runWot(sahin, grip, 6.0);
        std::printf("    6 s tam gaz: sokak lastigi asinma %.4f, slick %.4f\n", a->sim().tireWearGained(), b->sim().tireWearGained());
        CHECK(a->sim().tireWearGained() > 0.004 && a->sim().tireWearGained() > b->sim().tireWearGained() * 1.5, "patinaj lastigi asindirir");
        Career c = Career::newGame();
        c.recordDamage(false, 0.0, false, false, 0.0, 0.05);
        c.recordDamage(false, 0.0, false, false, 0.0, 0.05);
        CHECK(std::fabs(c.car().tune.wearTires - 0.10) < 1e-9, "asinma birikir (yarislar arasi)");
        delete a; delete b;
    }
    std::printf("[4] Atolye ve kayit\n");
    {
        Career c = Career::newGame();
        c.money = 1000000;
        std::string why;
        CHECK(!c.buyPart(PartCat::Turbo, kCustomTurbo, &why) && why == "ATOLYEDE URET", "ozel turbo once uretilmeli");
        c.car().tune.custTurboMm = 72; c.car().tune.custTurboAr = 0.9; c.car().tune.fuelSys = 6;
        CHECK(c.buyPart(PartCat::Turbo, kCustomTurbo, &why), "uretilen ozel turbo takilir");
        CHECK(peakHp(sahin, c.car().tune) > peakHp(sahin, Tune{}) * 1.5, "72 mm ozel turbo guc katar");
        c.buyPart(PartCat::EngineSwap, 200); c.buyPart(PartCat::Nitrous, 4); c.buyPart(PartCat::Tires, 22); c.buyPart(PartCat::Gearbox, 9);
        c.car().gearboxBroken = true;
        Career d;
        const bool ok = Career::parse(c.serialize(), d);
        const Tune &a = c.car().tune, &b = ok ? d.car().tune : Tune{};
        CHECK(ok && a.signature() == b.signature() && d.car().gearboxBroken, "v2 parcalar + atolye degerleri + sanziman hasari kayitta korunur");
        CHECK(!d.car().raceable() && d.repairCost() > 600, "kirik sanziman: yarisamaz, tamir ucretli");
    }
    std::printf("[5] Hurdalik ve restorasyon\n");
    {
        Career c = Career::newGame();
        c.money = 200000;
        const auto offers = junkyardOffers(5);
        bool cheap = offers.size() == 6;
        for (const JunkCar& j : offers) cheap = cheap && j.price <= carPrice(*findVehicle(j.carId)) * 0.36;
        CHECK(cheap, "6 hurda arac, fiyat <= degerin %36'si");
        const JunkCar j = offers[0];
        Tune fresh;
        const double hpJunk = peakHp(*findVehicle(j.carId), j.tune), hpNew = peakHp(*findVehicle(j.carId), fresh);
        std::printf("    hurda #%d: guc %.0f / yeni %.0f HP, fiyat %d, deger %d\n", j.carId, hpJunk, hpNew, j.price, carPrice(*findVehicle(j.carId)));
        CHECK(c.buyJunk(j) && hpJunk < hpNew * 0.9, "hurda alindi; yipranmis motor guc kaybeder");
        const int before = sellPrice(c.car());
        int steps = 0;
        for (int k = 0; k < kRestoreParts; ++k) while (restoreStepCost(c.car(), k) > 0 && steps < 60) { c.restoreStep(k); ++steps; }
        const int after = sellPrice(c.car());
        std::printf("    tam restorasyon: %d adim, satis degeri %d -> %d\n", steps, before, after);
        CHECK(steps > kRestoreParts, "tam toparlamak birden cok adim ister");
        CHECK(c.car().raceable() && after > before * 2, "restore edilen arac yarisir ve degerlenir");
        Career d;
        CHECK(Career::parse(c.serialize(), d) && d.car().fromJunk && d.car().tune.signature() == c.car().tune.signature(), "hurda araci kayitta korunur");
    }
    std::printf(failures ? "\nSONUC: %d test KALDI\n" : "\nSONUC: tum testler gecti\n", failures);
    return failures ? 1 : 0;
}
