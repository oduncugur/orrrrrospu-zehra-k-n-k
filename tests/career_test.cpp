// Kariyer / ekonomi / kayit testleri
#include "game/Career.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <string>

using namespace zk;

static int failures = 0;
#define CHECK(cond, msg) do { if (cond) std::printf("  GECTI  %s\n", msg); else { std::printf("  KALDI  %s\n", msg); ++failures; } } while (0)

static bool sameTune(const Tune& a, const Tune& b) {
    return a.tires == b.tires && a.psi == b.psi && a.clutch == b.clutch && a.axles == b.axles && a.diff == b.diff &&
           std::abs(a.finalDrive - b.finalDrive) < 1e-4 && a.weight == b.weight && a.intake == b.intake &&
           a.exhaust == b.exhaust && a.ecu == b.ecu && a.turbo == b.turbo && a.drySump == b.drySump && a.fuel == b.fuel;
}

int main() {
    std::printf("[1] Yeni oyun\n");
    Career c = Career::newGame();
    CHECK(c.money == 6000 && c.cars.size() == 1 && c.car().carId == 217, "6000 $, tek arac (#217)");

    std::printf("[2] Fiyatlar\n");
    const int sahin = carPrice(*findVehicle(217)), civic = carPrice(*findVehicle(5)), mustang = carPrice(*findVehicle(227)),
              testa = carPrice(*findVehicle(185)), mcf1 = carPrice(*findVehicle(313));
    std::printf("    Sahin %d | Civic Tip-R %d | Mustang GT %d | Testarossa %d | McLaren F1 %d\n", sahin, civic, mustang, testa, mcf1);
    CHECK(sahin < 6000, "baslangic araci baslangic parasindan ucuz");
    CHECK(sahin < civic && civic < mustang && mustang < testa && testa < mcf1, "fiyat sirasi mantikli");
    CHECK(partPrice(PartCat::Turbo, 2, *findVehicle(313)) > partPrice(PartCat::Turbo, 2, *findVehicle(217)), "pahali arabanin parcasi pahali");

    std::printf("[3] Satin alma\n");
    std::string why;
    CHECK(!c.buyCar(227, &why) && why == "PARA YETMIYOR", "para yetmezse Mustang alinamaz");
    c.money = 100000;
    CHECK(c.buyCar(227) && c.cars.size() == 2 && c.current == 1 && c.money == 100000 - mustang, "Mustang alindi, secili");

    std::printf("[4] Parca\n");
    const long before = c.money;
    const int slickPrice = partPrice(PartCat::Tires, 2, *findVehicle(227));
    CHECK(c.buyPart(PartCat::Tires, 2) && c.money == before - slickPrice && c.car().tune.tires == TireType::DragSlick, "slick takildi, para dustu");
    CHECK(!c.buyPart(PartCat::Tires, 2, &why) && why == "ZATEN TAKILI", "ayni parca tekrar alinmaz");
    c.buyPart(PartCat::FinalDrive, 1); c.buyPart(PartCat::Turbo, 1); c.buyPart(PartCat::Diff, 1); c.buyPart(PartCat::Fuel, 2);
    CHECK(c.car().tune.finalDrive > 3.73 * 1.1 && c.car().tune.fuel == FuelType::E85, "son disli ve E85 dogru uygulandi");
    // Kompresorlu motor (Hellcat #280 civari): turbo kiti yok
    int scCar = 0;
    for (const auto& v : vehicleCatalog()) if (engineTable()[v.engine].induction == Induction::Supercharger) { scCar = v.id; break; }
    CHECK(scCar && !partAvailable(PartCat::Turbo, 1, *findVehicle(scCar)), "kompresorlu motora turbo kiti yok");

    std::printf("[5] Kayit gidis-donus\n");
    c.recordRace(*findVehicle(5), true, 12.345);
    c.treePro = true;
    const std::string s = c.serialize();
    Career d;
    const bool ok = Career::parse(s, d);
    bool same = ok && d.money == c.money && d.current == c.current && d.races == c.races && d.wins == c.wins &&
                d.earnings == c.earnings && d.treePro == c.treePro && d.cars.size() == c.cars.size();
    for (size_t i = 0; same && i < c.cars.size(); ++i)
        same = d.cars[i].carId == c.cars[i].carId && sameTune(d.cars[i].tune, c.cars[i].tune) &&
               d.cars[i].paidParts == c.cars[i].paidParts && std::abs(d.cars[i].bestEt - c.cars[i].bestEt) < 1e-3;
    CHECK(same, "tum alanlar birebir geri okundu");

    std::printf("[6] Bozuk / yarim kayit\n");
    std::string bad = s; bad[bad.find("money=") + 6] ^= 1;
    CHECK(!Career::parse(bad, d), "tek bit degismis kayit reddedildi (sağlama toplamı)");
    CHECK(!Career::parse(s.substr(0, s.size() / 2), d), "yarim kayit reddedildi");
    CHECK(!Career::parse("", d), "bos kayit reddedildi");
    const Career fresh = Career::loadOrNew("/nonexistent/zk_kayit.txt");
    CHECK(fresh.cars.size() == 1 && fresh.money == 6000, "dosya yoksa yeni oyun");

    std::printf("[7] Dosyaya kaydet / yukle\n");
    const std::string path = "/tmp/zk_career_test.sav";
    CHECK(c.save(path), "kaydedildi");
    const Career e = Career::loadOrNew(path);
    CHECK(e.money == c.money && e.cars.size() == c.cars.size() && e.car().carId == c.car().carId, "dosyadan ayni kariyer");

    std::printf("[8] Aralik disi degerler kirpilir\n");
    {
        std::string body = "ZEHRAKINIK_KAYIT 1\nmoney=-50\ncurrent=9\ncar=5;0;0;0;0;7;99;9;9;9;99;9;9;9;9;9;1;9\n";
        // gecerli saglama toplami ile
        uint32_t h = 2166136261u; for (unsigned char ch : body) { h ^= ch; h *= 16777619u; }
        char cs[32]; std::snprintf(cs, sizeof cs, "checksum=%08x\n", h);
        Career f;
        const bool okf = Career::parse(body + cs, f);
        const Tune& t = okf ? f.car().tune : Tune{};
        CHECK(okf && f.money == 0 && f.current == 0 && (int)t.tires == 2 && t.clutch == 3 && t.turbo == 2 && t.psi == 45.0,
              "negatif para 0, seviyeler en yuksek gecerli degere kirpildi");
    }

    std::printf("[9] Yaris odulu\n");
    {
        Career g = Career::newGame();
        long prize = 0;
        g.recordRace(*findVehicle(227), true, 14.2, &prize);
        CHECK(prize > 0 && g.money == 6000 + prize && g.wins == 1 && g.car().bestEt == 14.2, "galibiyet parasi ve en iyi ET");
        g.recordRace(*findVehicle(227), false, 13.9, &prize);
        CHECK(prize == 0 && g.races == 2 && g.car().bestEt == 13.9, "maglubiyette para yok, daha iyi ET kaydedildi");
    }

    std::printf("[10] Performans endeksi\n");
    {
        Tune stock, built; built.tires = TireType::DragSlick; built.turbo = 1; built.weight = 2;
        const double a = performanceIndex(*findVehicle(5), stock), b = performanceIndex(*findVehicle(5), built);
        std::printf("    Civic stok %.1f | yapimli %.1f | Mustang stok %.1f\n", a, b, performanceIndex(*findVehicle(227), stock));
        CHECK(b > a * 1.3, "yapimli arac endeksi belirgin yuksek");
    }

    std::printf("[11] Satis\n");
    {
        Career h = Career::newGame();
        CHECK(!h.sellCurrent(&why) && why == "SON ARACINI SATAMAZSIN", "son arac satilamaz");
        h.money = 50000; h.buyCar(5);
        const long m = h.money;
        CHECK(h.sellCurrent() && h.money > m && h.cars.size() == 1, "ikinci arac satildi, para geldi");
        // Fiyat dokumu: arac + parca - hasar; hasar fiyati dusurur, alt sinir govdenin %25'i
        OwnedCar oc; oc.carId = 5; oc.paidParts = 3000;
        const SaleQuote clean = saleQuote(oc);
        CHECK(clean.damage == 0 && clean.total == clean.car + clean.parts && clean.parts == 1200, "hasarsiz: arac + %40 parca");
        oc.axleBroken = true; oc.engineWear = 0.5;
        const SaleQuote dmg = saleQuote(oc);
        std::printf("    Civic: temiz $%d | hasarli $%d (hasar $%d)\n", clean.total, dmg.total, dmg.damage);
        CHECK(dmg.damage > 0 && dmg.total < clean.total && dmg.total == sellPrice(oc), "hasar satis fiyatindan duser");
        oc.engineWear = 1.0; oc.paidParts = 0;
        CHECK(saleQuote(oc).total >= saleQuote(oc).car / 4 - 10, "hurda alt siniri");
    }
    std::printf("[H] Hasar ve tamir\n");
    {
        Career c = Career::newGame();
        c.recordDamage(true, 0.4, false);
        CHECK(c.car().damaged() && !c.car().raceable(), "kirik aks: yarisamaz");
        const long cost = c.repairCost();
        std::printf("    tamir bedeli $%ld\n", cost);
        CHECK(cost > 0, "tamir ucretli");
        Career d; CHECK(Career::parse(c.serialize(), d) && d.car().axleBroken && std::fabs(d.car().engineWear - 0.4) < 1e-3, "hasar kayitta korunur");
        c.money = cost - 1; std::string why;
        CHECK(!c.repairCurrent(&why) && c.car().axleBroken, "para yetmezse tamir yok");
        c.money = cost + 10;
        CHECK(c.repairCurrent(&why) && !c.car().damaged() && c.money == 10, "tamir: hasar sifir, para dustu");
        c.recordDamage(false, 0.1, true);
        CHECK(!c.car().raceable() && c.car().engineWear == 1.0, "sarmis yatak: motor revizyonu gerekir");
    }
    std::printf("[I] Para kasma engeli + bozuk kayit\n");
    {
        Career c = Career::newGame();
        const VehicleDef& opp = *findVehicle(227);
        long p1 = 0, p2 = 0, p3 = 0, p4 = 0;
        c.recordRace(opp, true, 13.0, &p1); c.recordRace(opp, true, 13.0, &p2); c.recordRace(opp, true, 13.0, &p3);
        c.recordRace(*findVehicle(5), true, 13.0, &p4);
        std::printf("    ayni rakip odulleri: %ld %ld %ld\n", p1, p2, p3);
        CHECK(p2 < p1 && p3 < p2 && p3 >= p1 / 4, "ayni rakibe ust uste galibiyet odulu azalir");
        CHECK(p4 == racePrize(*findVehicle(5), true), "yeni rakipte odul tam");
        Career d; CHECK(Career::parse(c.serialize(), d) && d.lastOppId == 5 && d.sameOppWins == 1, "seri kayitta korunur");
        const std::string path = "/tmp/zk_bozuk_test.zks";
        { std::FILE* f = std::fopen(path.c_str(), "wb"); std::fputs("cop veri", f); std::fclose(f); }
        bool corrupt = false;
        Career e = Career::loadOrNew(path, &corrupt);
        std::FILE* bk = std::fopen((path + ".bozuk").c_str(), "rb");
        CHECK(corrupt && bk && e.money == Career::newGame().money, "bozuk kayit: bildirilir, yedeklenir, yeni oyun");
        if (bk) std::fclose(bk);
        std::remove((path + ".bozuk").c_str());
    }
    std::printf(failures ? "\nSONUC: %d test KALDI\n" : "\nSONUC: tum testler gecti\n", failures);
    return failures ? 1 : 0;
}
