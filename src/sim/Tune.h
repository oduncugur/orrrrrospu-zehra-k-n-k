// ZEHRA KINIK - Arac parca / ayar durumu (kariyerde kaydedilir, fizikte uygulanir)
// Her alan fizikte gercek bir parametreyi degistirir; bkz. VehicleSim kurucusu. Tamsayi alanlar PartTables.h
// tablolarinin indeksidir (0 = stok). Eski (v1) seviyeler tablolarin ilk satirlaridir.
#pragma once
#include "sim/PowertrainCore.h"

#include <string>

namespace zk {

enum class TireType { Street = 0, SemiSlick = 1, DragSlick = 2 };
enum class DiffType { Open = 0, OneAndHalfWay = 1, TwoWay = 2, Spool = 3 };   // 4.. : diffTable() satirlari

struct Tune {
    // ---- v1 alanlari (YZ parca setleri bunlari kullanir) ----
    TireType tires = TireType::Street;   // lastik sinifi (pist hazirligi, YZ); tireSel kesin lastigi secer
    double   psi = 0.0;           // 0 = lastik tipinin varsayilani
    int      clutch = 0;          // clutchTable
    int      axles = 0;           // axleTable
    DiffType diff = DiffType::Open;   // diffTable indeksi
    double   finalDrive = 0.0;    // 0 = fabrika (v1 kayitlari: mutlak oran); finalSel > 0 ise tablodan
    int      weight = 0;          // weightTable
    int      intake = 0;          // intakeTable
    int      exhaust = 0;         // exhaustTable
    int      ecu = 0;             // ecuTable
    int      turbo = 0;           // turboTable
    bool     drySump = false;     // oilTable'dan (oil) turetilir
    bool     absKit = false, tcKit = false;   // elecTable'dan (elec) turetilir
    FuelType fuel = FuelType::Race100;        // fuelTable'dan (fuelSel) turetilir

    // ---- v2 alanlari (modifiye atolyesi) ----
    int tireSel = 0, rims = 0, finalSel = 0, gearSwap = 0, engineSwap = 0;
    int cam = 0, valve = 0, flywheel = 0, superch = 0, intercooler = 0, fuelSys = 0, nitrous = 0;
    int head = 0, piston = 0, rod = 0, crank = 0, bearing = 0, gasket = 0;      // motor ici
    int turbine = 0, wastegate = 0, boostCtl = 0, gbStrength = 0, cooling = 0;
    int oil = 0, fuelSel = 0, elec = 0, susp = 0, brakes = 0, aero = 0;
    int launchRpm = 0;                        // 2-step kalkis devri (0: otomatik, redline x 0.55)
    // Yakit sistemi (ayri parcalar; sinir = en zayif halka): pompa, enjektor, hat / regulator. Eski fuelSys yalniz eski kayit.
    int fuelPump = 0, injector = 0, fuelLine = 0;
    // ECU donanimi (yazilim yuvasi + modul seviye siniri) ve yazilim modulleri. Eski ecu alani: rakip paketleri / eski kayit.
    int ecuHw = 0, swMap = 0, swRev = 0, swLaunch = 0, swFlat = 0, swAntiLag = 0, swFlex = 0, swKnock = 0;
    // Bilesen parcalari (ayni anda takilabilen gercek parcalar; etkiler carpilir / toplanir)
    int throttleBody = 0, intakeMani = 0, header = 0, catalyst = 0, meth = 0, headStud = 0, mainSupport = 0;
    int brakeDisc = 0, brakeCaliper = 0, weightBody = 0, weightGlass = 0, weightChassis = 0;
    int aeroFront = 0, aeroSide = 0, aeroUnder = 0, fan = 0, coolMisc = 0, oilCooler = 0, oilPump = 0;
    int partsVer = 2;                         // parca tablolari surumu (eski kayit < 2: tek liste secimleri bilesenlere cevrilir)
    int totalWeightKg() const;                // ic + kaporta + cam + sasi hafifletmesi
    // Atolye (ozel uretim) degerleri; 0 = uretilmedi
    double custTurboMm = 0, custTurboAr = 0, custCamDeg = 0, custDisp = 0, custFinal = 0, custWingN = 0;
    double custGear[8] = {0, 0, 0, 0, 0, 0, 0, 0};   // vites basina fabrika oranina carpan (0 = fabrika)
    // Bilesen yipranmasi (hurdalik araclari; 0 = yeni, 1 = bitik): motor kompresyonu, lastik, fren, suspansiyon,
    // kaporta/pas (agirlik + surtunme), elektrik (guc kaybi, ABS/TC calismaz). Restorasyonla duzelir.
    double wearEngine = 0, wearTires = 0, wearBrakes = 0, wearSusp = 0, wearBody = 0, wearElec = 0;

    static int weightKg(int level);
    // Onbellek / kayit anahtari: tum alanlar
    std::string signature() const;
};

} // namespace zk
