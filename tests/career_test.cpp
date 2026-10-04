// Kariyer / ekonomi / kayit testleri
#include "game/Career.h"
#include "game/SaveCode.h"
#include "game/Achievements.h"
#include "game/DragRace.h"
#include "sim/VehicleSim.h"
#include "garage/VehicleCatalog.h"
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
    c.marketOff = true;                                   // sabit liste fiyati (haftalik pazar kapali)
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
    { VehicleSimConfig vc; vc.car = findVehicle(227); vc.tune = &c.car().tune; const VehicleSim vs(vc);
      CHECK(vs.gearboxSpec().finalDrive > 3.73 * 1.1 && c.car().tune.fuel == FuelType::E85, "son disli ve E85 dogru uygulandi"); }
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
        CHECK(okf && f.money == 0 && f.current == 0 && (int)t.tires == 2 && t.clutch == 9 && t.turbo == 9 && t.psi == 45.0,
              "negatif para 0, seviyeler en yuksek gecerli degere kirpildi");
    }

    std::printf("[9] Yaris odulu\n");
    {
        Career g = Career::newGame();
        g.dailyDay = todayIndex(); g.dailyDone = 7;                  // gunluk gorev odulu karismasin
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

    std::printf("[R] Rakip dengesi: ET tablosu guncel, eslesme ET'ye gore, form zorlugu, odul zorluk carpani\n");
    {
        // Tablo fizikle uyumlu mu (fizik/YZ degisince zehra_ettable gen ile yenileyin)
        for (int id : {5, 227, 269}) {
            const Tune t = opponentPreset(1);
            const QuarterEstimate e = DragRace::estimateQuarter(findVehicle(id), &t);
            char m[96]; std::snprintf(m, sizeof m, "#%d tablo %.3f = tahmin %.3f (EtTable.inc guncel)", id, tableEt(id, 1), e.quarter);
            CHECK(std::fabs(tableEt(id, 1) - e.quarter) < 0.02, m);
        }
        Career c = Career::newGame();
        const double pe = estimatedEt(*findVehicle(c.car().carId), c.car().tune);
        double sum0 = 0, sum3 = 0; int far = 0;
        for (uint32_t s = 1; s <= 30; ++s) {
            c.form = 0; const Opponent a = c.pickOpponentFor(s);
            c.form = 3; const Opponent b = c.pickOpponentFor(s);
            sum0 += a.estEt; sum3 += b.estEt;
            far += std::fabs(a.estEt - (pe + 0.2)) > 0.4;
        }
        std::printf("    Sahin tahmini %.2f s | rakip ort. form 0: %.2f s, form +3: %.2f s\n", pe, sum0 / 30, sum3 / 30);
        CHECK(far == 0, "rakipler oyuncu ET'sine yakin (+0.2 s insan payi, 0.4 s icinde)");
        CHECK(sum3 < sum0 - 0.15 * 30, "kazandikca rakipler hizlanir");
        c.form = 0; c.lastOppId = 0;
        const Opponent o = c.pickOpponentFor(7);
        c.lastOppId = o.carId;
        bool repeat = false;
        for (uint32_t s = 1; s <= 20; ++s) repeat |= c.pickOpponentFor(s).carId == o.carId;
        CHECK(!repeat, "ayni rakip ust uste gelmez");
        c.form = 0;
        c.recordRace(*findVehicle(5), true, 0.0); c.recordRace(*findVehicle(5), true, 0.0);
        CHECK(c.form == 2, "galibiyet formu artirir");
        Career d; CHECK(Career::parse(c.serialize(), d) && d.form == 2, "form kayitta korunur");
        CHECK(prizeDifficulty(0.5) > 1.2 && prizeDifficulty(-0.5) < 0.8 && prizeDifficulty(5) == 1.8, "zor rakip daha cok odul");
    }
    std::printf("[L] Ligler: patron kilidi, lig acilmasi, pink slip, gunluk gorev, kayit\n");
    {
        Career c = Career::newGame();
        c.money = 100000;
        const auto& ev = leagueEvents();
        int boss0 = -1, pink1 = -1, ev1 = -1;
        for (int i = 0; i < (int)ev.size(); ++i) {
            if (ev[i].league == 0 && ev[i].rival >= 0 && rivals()[ev[i].rival].boss) boss0 = i;
            if (ev[i].league == 1 && ev[i].pink) pink1 = i;
            if (ev[i].league == 1 && ev1 < 0) ev1 = i;
        }
        std::string why;
        CHECK(!c.eventAvailable(boss0, &why) && why.find("3 GALIBIYET") != std::string::npos, "patron icin once 3 galibiyet");
        CHECK(!c.eventAvailable(ev1, &why) && c.leagueUnlocked() == 0, "sehir ligi kilitli");
        long got = 0;
        for (int i = 0; i < 3; ++i) got += c.recordEvent(i, true, 15.0, 0);
        CHECK(got == ev[0].prize + ev[1].prize + ev[2].prize && c.rep > 0, "ilk galibiyetler tam odul + un");
        const long again = c.recordEvent(0, true, 15.0, 0);
        CHECK(again == ev[0].prize * 4 / 10, "tekrar galibiyet %40 odul");
        CHECK(c.eventAvailable(boss0, &why), "3 galibiyetten sonra patron acik");
        c.recordEvent(boss0, true, 15.0, 0);
        CHECK(c.leagueUnlocked() == 1, "patron yenilince sehir ligi acilir");
        // Pink slip: 2 arac gerekir; kazaninca rakibin arabasi gelir, kaybedince secili arac gider
        CHECK(!c.eventAvailable(pink1, &why) && why.find("2 ARAC") != std::string::npos, "pink slip 2 arac ister");
        c.buyCar(217);
        const size_t n0 = c.cars.size();
        int pinkCar = 0;
        c.recordEvent(pink1, true, 15.0, 0, &pinkCar);
        CHECK(c.cars.size() == n0 + 1 && pinkCar == rivals()[ev[pink1].rival].carId, "pink slip kazanildi: rakibin arabasi garajda");
        Career d2 = c; d2.eventWins = 0;
        d2.recordEvent(pink1, false, 0.0, 0, &pinkCar);
        CHECK(d2.cars.size() == c.cars.size() - 1 && pinkCar < 0, "pink slip kaybedildi: araba gitti");
        // Gunluk gorevler: odul bir kez verilir
        c.dailyDay = -1; c.dailyRefresh();                         // onceki yarislarin ilerlemesi sifirlansin
        const auto tasks = dailyTasks(c.dailyDay, 15.0);
        const long m0 = c.money;
        for (int k = 0; k < 10; ++k) { c.dailyAdd(TaskType::WinAny, 1); c.dailyAdd(TaskType::WinDrag, 1); c.dailyAdd(TaskType::WinRoad, 1);
            c.dailyAdd(TaskType::Earn, 1000); c.dailyAdd(TaskType::Restore, 1); c.dailyAdd(TaskType::BuyParts, 1);
            c.dailyAdd(TaskType::FlowScore, 99999); c.dailyAdd(TaskType::EtUnder, 100); }
        const long paid = c.money - m0;
        CHECK(c.dailyDone == 7 && paid == tasks[0].reward + tasks[1].reward + tasks[2].reward, "uc gunluk gorev tamamlandi, odul bir kez");
        Career d;
        CHECK(Career::parse(c.serialize(), d) && d.rep == c.rep && d.eventWins == c.eventWins && d.dailyDone == 7 && d.leagueUnlocked() == 1,
              "un, etkinlikler ve gorevler kayitta korunur");
    }
    std::printf("[G] Gorunum (boyahane) kayitta korunur\n");
    {
        Career c = Career::newGame();
        c.cars[0].paint = 11; c.cars[0].finish = 2; c.cars[0].stripe = 3; c.cars[0].stripeCol = 7; c.cars[0].rimCol = 15;
        Career d;
        CHECK(Career::parse(c.serialize(), d) && d.cars[0].paint == 11 && d.cars[0].finish == 2 && d.cars[0].stripe == 3
              && d.cars[0].stripeCol == 7 && d.cars[0].rimCol == 15, "boya / cila / serit / jant kayitta");
        Career e;
        CHECK(Career::parse(Career::newGame().serialize(), e) && e.cars[0].paint == -1 && e.cars[0].rimCol == -1, "fabrika gorunum varsayilan");
    }
    std::printf("[A] Basarimlar: bir kez odul, kayitta korunur\n");
    {
        Career c = Career::newGame();
        CHECK(c.checkAchievements().empty(), "yeni oyunda basarim yok");
        c.wins = 1; c.cars[0].paint = 3;
        const long m0 = c.money;
        const auto got = c.checkAchievements();
        CHECK(got.size() == 2 && c.money == m0 + achievements()[0].reward + achievements()[19].reward, "ILK KAN + BOYACI odulu");
        CHECK(c.checkAchievements().empty() && c.money == m0 + achievements()[0].reward + achievements()[19].reward, "ikinci kez odul yok");
        c.cars[0].bestEt = 12.5;
        CHECK(c.checkAchievements().size() == 1 && ((c.achieved >> 6) & 1u), "13 saniye alti");
        Career d;
        CHECK(Career::parse(c.serialize(), d) && d.achieved == c.achieved, "basarimlar kayitta");
    }
    std::printf("[U] Ikinci el parca + eski parca satisi\n");
    {
        Career c = Career::newGame();
        c.marketOff = true;                                   // sabit liste fiyati (haftalik pazar kapali)
        c.money = 100000; c.dailyDay = todayIndex(); c.dailyDone = 7;   // gunluk gorev odulu karismasin
        const VehicleDef& v = *findVehicle(c.car().carId);
        const int p3 = partPrice(PartCat::Brakes, 3, v), p4 = partPrice(PartCat::Brakes, 4, v);
        long back = -1; std::string why;
        long m0 = c.money;
        CHECK(c.buyPart(PartCat::Brakes, 3, &why, true, &back) && c.money == m0 - usedPrice(p3) && back == 0, "ikinci el fren %55");
        CHECK(std::fabs(c.car().tune.wearBrakes - 0.25) < 1e-9, "ikinci el fren yipranmis gelir");
        m0 = c.money;
        CHECK(c.buyPart(PartCat::Brakes, 4, &why, false, &back) && back == (long)p3 * 35 / 100 / 10 * 10 && c.money == m0 - p4 + back,
              "yeni parca: eskisi %35'e satilir");
        CHECK(!c.buyPart(PartCat::Gearbox, 2, &why, true) && why == "IKINCI EL YOK", "aktarma parcasi ikinci el yok");
        CHECK(c.buyPart(PartCat::Tires, 5, &why, true) && c.car().tune.wearTires >= 0.40, "ikinci el lastik yarim dis");
    }
    std::printf("[K] Kayit kodu: kariyer -> kod -> ayni kariyer; bozuk kod reddedilir\n");
    {
        Career c = Career::newGame();
        c.money = 123456; c.wins = 7; c.buyCar(227); c.cars[0].tune.ecuHw = 4; c.cars[0].tune.swRev = 2;
        const std::string code = makeSaveCode(c.serialize());
        std::string text; Career d;
        std::printf("    kod uzunlugu %zu karakter\n", code.size());
        CHECK(code.rfind("ZK1-", 0) == 0 && readSaveCode(code, text) && Career::parse(text, d) && d.serialize() == c.serialize(),
              "kod ayni kariyeri geri verir");
        std::string spaced = code; spaced.insert(40, "\n "); spaced = "  " + spaced + "\n";
        CHECK(readSaveCode(spaced, text) && Career::parse(text, d) && d.money == c.money, "bosluk / satir sonu eklenmis kod da okunur");
        std::string bad = code; bad[bad.size() / 2] = bad[bad.size() / 2] == 'A' ? 'B' : 'A';
        CHECK(!(readSaveCode(bad, text) && Career::parse(text, d)), "bozuk kod reddedilir (checksum)");
        CHECK(!readSaveCode("merhaba", text), "kod olmayan metin reddedilir");
    }
    std::printf("[Y] Garaj yuvalari\n");
    {
        Career c = Career::newGame();
        c.money = 10000000; c.dailyDay = todayIndex(); c.dailyDone = 7;
        std::string why;
        for (int id : {5, 227, 78}) c.buyCar(id);
        CHECK(c.cars.size() == 4 && c.garageFull() && !c.buyCar(269, &why) && why == "GARAJ DOLU", "4 yuva dolunca arac alinmaz");
        const long p1 = c.slotPrice();
        CHECK(c.buySlot() && c.garageSlots == 5 && c.slotPrice() > p1 && c.buyCar(269), "yuva alinca arac girer, sonraki yuva pahali");
        while (c.garageSlots < Career::kMaxSlots) c.buySlot();
        CHECK(!c.buySlot(&why) && c.garageSlots == Career::kMaxSlots, "en fazla 12 yuva");
        Career d;
        CHECK(Career::parse(c.serialize(), d) && d.garageSlots == Career::kMaxSlots, "yuva sayisi kayitta");
    }
    std::printf("[T] Haftalik turnuva: 3 tur, elenme, odul bir kez, yeni hafta sifirlar\n");
    {
        Career c = Career::newGame();
        c.money = 100000; c.dailyDay = todayIndex(); c.dailyDone = 7;
        const long m0 = c.money;
        CHECK(c.tourStart() && c.money == m0 - c.tourEntry(), "ilk turda giris ucreti");
        CHECK(c.recordTour(true) == 0 && c.tourStart() && c.money == m0 - c.tourEntry(), "2. tur ucretsiz");
        c.recordTour(true);
        const long m1 = c.money, prize = c.tourPrize();
        CHECK(c.tourStart() && c.recordTour(true) == prize && c.money == m1 + prize && c.tourRound == Career::kTourRounds, "3 tur = odul");
        std::string why;
        CHECK(!c.tourAvailable(&why) && why == "BU HAFTA SAMPIYONSUN", "ayni hafta tekrar yok");
        Career e = Career::newGame(); e.money = 100000;
        e.tourStart(); e.recordTour(false);
        CHECK(e.tourOut && !e.tourAvailable(&why) && why == "BU HAFTA ELENDIN", "kaybeden elenir");
        e.tourWeek -= 1; e.tourRefresh();
        CHECK(!e.tourOut && e.tourRound == 0 && e.tourAvailable(), "yeni hafta sifirlanir");
        Career d;
        CHECK(Career::parse(c.serialize(), d) && d.tourRound == c.tourRound && d.tourWeek == c.tourWeek, "turnuva durumu kayitta");
    }
    std::printf("[P] Parca pazari: haftalik fiyat %80-%120, dukkan fiyati odenir\n");
    {
        Career c = Career::newGame(); c.money = 1000000;
        bool differ = false, inRange = true;
        for (int w = 0; w < 30; ++w) {
            const double m = marketMul(PartCat::Turbo, w);
            inRange = inRange && m >= 0.799 && m <= 1.201;
            differ = differ || std::fabs(m - marketMul(PartCat::Turbo, 0)) > 1e-9;
        }
        CHECK(inRange && differ, "carpan aralikta ve haftadan haftaya degisir");
        const long m0 = c.money; const int sp = c.shopPrice(PartCat::Intake, 2);
        CHECK(c.buyPart(PartCat::Intake, 2) && c.money == m0 - sp, "satin alma pazar fiyatindan");
        c.marketOff = true;
        CHECK(c.shopPrice(PartCat::Intake, 3) == partPrice(PartCat::Intake, 3, *findVehicle(c.car().carId)), "pazar kapali: liste fiyati");
    }
    std::printf("[S] Sokak: gece bulusmasi, dyno yarismasi, musteri isleri\n");
    {
        Career c = Career::newGame(); c.money = 200000; c.clockSlot = 12345;
        const long stake = c.meetStake(), m0 = c.money;
        CHECK(stake >= 500 && c.meetAvailable() && c.meetStart() && c.money == m0 - stake, "bulusma: bahis masaya konur");
        std::string why;
        CHECK(!c.meetAvailable(&why) && why == "BU GECE YARISTIN", "ayni dilimde ikinci bulusma yok");
        long fine = -1;
        const long net = c.recordMeet(true, &fine);
        CHECK(net == 2 * stake && c.money == m0 + stake - fine && fine >= 0 && fine <= stake / 2, "kazanan bahsi iki kat alir (baskinda ceza)");
        int raids = 0;
        for (int sl = 0; sl < 1000; ++sl) { Career r = Career::newGame(); r.money = 100000; r.clockSlot = sl; r.meetStart(); long f = 0; r.recordMeet(false, &f); raids += f > 0; }
        std::printf("    1000 bulusmada polis baskini: %d\n", raids);
        CHECK(raids > 120 && raids < 240, "polis baskini ~%18");
        Career d = Career::newGame(); d.money = 100000;
        int place = 0; long prize = 0; std::vector<Career::DynoEntry> board;
        const long d0 = d.money, fee = d.dynoEntryFee();
        CHECK(d.dynoEnter(&place, &prize, &board) && board.size() == 10 && place >= 1 && place <= 10 && board[place - 1].player, "dyno: 10 arac, oyuncu siralamada");
        CHECK(d.money == d0 - fee + prize && (place <= 3) == (prize > 0), "giris ucreti alinir, ilk uce odul");
        bool sorted = true; for (size_t i = 1; i < board.size(); ++i) sorted = sorted && board[i - 1].hp >= board[i].hp;
        CHECK(sorted && !d.dynoAvailable(&why) && why == "BU HAFTA KATILDIN", "siralama guce gore; haftada bir");
        Career j = Career::newGame(); j.money = 100000;
        const Career::JobOffer o = j.jobOffer(0);
        CHECK(o.targetHp > o.stockHp && o.reward > 0, "musteri teklifi: hedef fabrikadan yuksek, odeme var");
        const size_t n0 = j.cars.size();
        CHECK(j.acceptJob(0) && j.cars.size() == n0 + 1 && j.car().jobHp == o.targetHp && !j.car().raceable(), "isi al: arac garaja gelir, yarisamaz");
        CHECK(!j.sellCurrent(&why) && why == "MUSTERI ARACI SATILAMAZ" && !j.acceptJob(0), "musteri araci satilamaz, ayni teklif tekrar alinmaz");
        long paid = 0;
        CHECK(!j.deliverJob(&paid, &why) && why == "HEDEF GUCE ULASILMADI", "hedefe ulasmadan teslim yok");
        Career k2;
        CHECK(Career::parse(j.serialize(), k2) && k2.car().jobHp == o.targetHp && k2.car().jobReward == o.reward && k2.jobTaken(0), "musteri isi kayitta");
        j.cars[j.current].tune.engineSwap = 0;
        for (int sw = 1; sw < 300 && peakHpOf(*findVehicle(j.car().carId), j.car().tune) < o.targetHp; ++sw) j.cars[j.current].tune.engineSwap = sw;   // guclu motor
        const long jm = j.money;
        const bool reached = peakHpOf(*findVehicle(j.car().carId), j.car().tune) >= o.targetHp;
        CHECK(reached && j.deliverJob(&paid) && paid == o.reward && j.money == jm + o.reward && j.cars.size() == n0, "hedefe ulasinca teslim: odeme, arac gider");
        CHECK(j.acceptJob(1) && j.cancelJob() && j.cars.size() == n0, "iptal: arac geri verilir");
    }
    std::printf("[G] Kadran: satin al, ucretsiz degistir, turbo gostergesi, kayit\n");
    {
        Career c = Career::newGame(); c.money = 5000;
        const long m0 = c.money;
        CHECK(c.selectGauge(1) && c.car().gauge == 1 && c.money == m0 - gaugeStylePrice(1), "analog kadran satin alinir");
        CHECK(c.selectGauge(0) && c.selectGauge(1) && c.money == m0 - gaugeStylePrice(1), "alinmis kadrana donus ucretsiz");
        CHECK(c.toggleBoostGauge() && c.car().boostGauge && c.money == m0 - gaugeStylePrice(1) - kBoostGaugePrice, "turbo gostergesi alinir");
        CHECK(c.toggleBoostGauge() && !c.car().boostGauge && c.money == m0 - gaugeStylePrice(1) - kBoostGaugePrice, "turbo gostergesi ucretsiz kapanir");
        Career r;
        CHECK(Career::parse(c.serialize(), r) && r.car().gauge == 1 && r.car().gaugeOwned == c.car().gaugeOwned, "kadran kayitta");
        Career poor = Career::newGame(); poor.money = 100; std::string why;
        CHECK(!poor.selectGauge(3, &why) && why == "PARA YETMIYOR" && poor.car().gauge == 0, "para yoksa takilmaz");
    }
    std::printf("[C] Sehirler: seyahat kilidi, hizli gecis, kayit, etap alani\n");
    {
        Career c = Career::newGame(); c.money = 100000;
        std::string why;
        CHECK(c.city == 0 && !c.canTravel(1, &why) && why == "KILITLI: ONCEKI PATRONU YEN", "kilitli sehre gidilmez");
        CHECK(!c.canTravel(0, &why) && why == "ZATEN BURADASIN", "bulunulan sehir");
        for (int i = 0; i < (int)leagueEvents().size(); ++i)            // tum ligleri ac (patronlari kazanmis say)
            if (leagueEvents()[i].rival >= 0 && rivals()[leagueEvents()[i].rival].boss) c.recordEvent(i, true, 12.0, 0, nullptr);
        CHECK(c.leagueUnlocked() >= 2 && c.canTravel(2), "acik lig sehrine gidilir");
        const long p2 = c.fastTravelPrice(2), m0 = c.money;
        CHECK(std::fabs(c.travelKm(2) - (Career::legKm(0) + Career::legKm(1))) < 1e-9 && p2 > 0, "iki sehir: iki ayak toplam km");
        CHECK(c.fastTravel(2) && c.city == 2 && c.money == m0 - p2, "hizli gecis: para dusulur, sehir degisir");
        Career r;
        CHECK(Career::parse(c.serialize(), r) && r.city == 2, "sehir kayitta");
        const auto f = c.runField(50, 7u);
        int st[StyleCount] = {0};
        for (const RunEntrant& e : f) ++st[e.style];
        int kinds = 0; for (int k = 0; k < StyleCount; ++k) kinds += st[k] > 0;
        std::printf("    50 arac: dengeli %d, dip gaz %d, eko %d, stok %d, canavar %d\n", st[0], st[1], st[2], st[3], st[4]);
        CHECK(f.size() == 50 && kinds == StyleCount, "etap alani: 50 arac, 5 tarzin hepsi var");
    }
    std::printf("[W] Bahis: kazanirsa +bahis, kaybederse -bahis; paradan fazla oynanmaz; ust lig odul carpani\n");
    {
        Career c = Career::newGame(); c.money = 5000;
        const long p0 = c.eventPrize(0, true);
        CHECK(c.wagerFor(0, 0) == 0 && c.wagerFor(0, 2) == p0 && c.wagerFor(0, 4) == 3 * p0, "bahis kademeleri 0 / 1x / 3x");
        c.wager = c.wagerFor(0, 2);
        const long m0 = c.money, net = c.recordEvent(0, true, 15.0, 0);
        CHECK(net == p0 + p0 && c.money >= m0 + 2 * p0 && c.wager == 0, "kazanilan bahis: odul + bahis");
        c.wager = c.wagerFor(1, 2);
        const long w = c.wager, m1 = c.money, net2 = c.recordEvent(1, false, 16.0, 0);
        CHECK(net2 == -w && c.money <= m1 - w + 2000, "kaybedilen bahis paradan duser");
        c.money = 100;
        CHECK(c.wagerFor(0, 4) == 100, "bahis paradan fazla olamaz");
        int l4 = -1;
        for (int i = 0; i < (int)leagueEvents().size(); ++i) if (leagueEvents()[i].league == 4 && leagueEvents()[i].prize > 0) { l4 = i; break; }
        CHECK(l4 >= 0 && c.eventPrize(l4, true) == leagueEvents()[l4].prize * 3 / 2, "pist ligi odulu x1.5");
    }
    std::printf("[S] Sponsor: teklif, prim, bonus, ceza, kayit; olagan asinma\n");
    {
        Career c = Career::newGame(); c.money = 5000;
        const int o = c.sponsorOffer();
        CHECK(o >= 0 && c.signSponsor(o) && c.sponsor == o && c.sponsorOffer() == -1, "sponsor teklifi kabul edilir");
        const SponsorDef& d = sponsors()[o];
        const long m0 = c.money;
        for (int i = 0; i < d.wins; ++i) c.sponsorRace(true);
        CHECK(c.sponsor == -1 && (c.sponsorsDone >> o & 1) && c.money == m0 + d.wins * d.perWin + d.bonus, "sozlesme tamam: prim + bonus");
        const int o2 = c.sponsorOffer();
        CHECK(o2 >= 0 && o2 != o && c.signSponsor(o2), "yeni teklif");
        const long m1 = c.money;
        for (int i = 0; i < sponsors()[o2].races; ++i) c.sponsorRace(false);
        CHECK(c.sponsor == -1 && c.money == std::max(0L, m1 - sponsors()[o2].penalty), "tutturamazsa ceza");
        c.signSponsor(c.sponsorOffer()); c.sponsorRace(true);
        Career r;
        CHECK(Career::parse(c.serialize(), r) && r.sponsor == c.sponsor && r.spWins == 1 && r.sponsorsDone == c.sponsorsDone, "sponsor kayitta");
        const double w0 = c.car().tune.wearEngine;
        c.recordDamage(false, 0, false, false, 0, 0);
        CHECK(c.car().tune.wearEngine > w0 && c.car().tune.wearBrakes > 0, "her yarista olagan asinma");
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
