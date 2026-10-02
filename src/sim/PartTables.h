// ZEHRA KINIK - Modifiye parca tablolari (fizik etkileri + ad + taban fiyat). Tek kaynak: hem fizik (VehicleSim)
// hem dukkan (Career / PartsScreen) bu tablolari okur. Her tablonun ilk satirlari eski (v1) seviyelerle birebir
// aynidir: eski kayitlar ve YZ parca setleri (EtTable.inc) degismez. "OZEL" satirlari atolye (Tune::cust*) degerlerini kullanir.
#pragma once
#include "sim/PowertrainCore.h"

#include <vector>

namespace zk {

struct Tune;
struct VehicleDef;

// Tork egrisi bicimi: dusuk devir (<= %40 redline) ve yuksek devir (>= %75) carpanlari, arasi dogrusal
struct ShapeOpt   { const char* name; int price; double low, high; };
struct IntakeOpt  { const char* name; int price; double low, high; };
struct CamOpt     { const char* name; int price; double low, high, redline; };              // redline: ek devir
struct ValveOpt   { const char* name; int price; double redline; };
struct HeadOpt    { const char* name; int price; double low, high, redline; };             // silindir kapagi (akis)
struct PistonOpt  { const char* name; int price; double mul, strength; };                   // sikistirma etkisi, dayanim
struct RodOpt     { const char* name; int price; double strength, inertia; };
struct CrankOpt   { const char* name; int price; double disp, strength, inertia, redline; };  // stroker: hacim carpani
struct BearingOpt { const char* name; int price; double strength; };
struct GasketOpt  { const char* name; int price; double mm, strength; };                    // conta kalinligi (CR), boost dayanimi
struct TurbineOpt { const char* name; int price; double spool, top; };                      // spool kaymasi (redline orani), tepe boost carpani
struct GateOpt    { const char* name; int price; double mul; };                             // wastegate: hedef boost tutturma
struct BoostCtlOpt{ const char* name; int price; double extra; };                           // boost kontrolcusu: ek boost orani
struct GbStrOpt   { const char* name; int price; double mul; };                             // sanziman dayanim carpani
struct CoolOpt    { const char* name; int price; double cap, lowSpeed; };                   // sogutma kapasitesi, dusuk hizda hava akisi
struct FlyOpt     { const char* name; int price; double inertia; };
struct TurboOpt   { const char* name; int price; double bar, spoolLo, spoolHi; };          // spool: redline orani
struct SuperOpt   { const char* name; int price; double bar; int type; };                  // 0 roots, 1 twin-screw, 2 santrifuj
struct IcOpt      { const char* name; int price; double eff; };                            // eklenen boost etkinligi
struct FuelSysOpt { const char* name; int price; double cap; };                            // guc siniri: fabrika HP x cap
struct NosOpt     { const char* name; int price; double hp, bottleS; };
struct EcuOpt     { const char* name; int price; double na, forced, redline, spool; };      // tork carpani (atm / asiri besleme)
struct FuelOpt    { const char* name; int price; int type; double mul; };                  // type: FuelType
struct OilOpt     { const char* name; int price; bool dry; double liters; };
struct ClutchOpt  { const char* name; int price; double mul; };
struct AxleOpt    { const char* name; int price; int spec; double dia; };                  // spec 0 stok, 1 krom-moly, 2 yaris
struct DiffOpt    { const char* name; int price; double preload, plate, rampA, rampD; };
struct FinalOpt   { const char* name; int price; double mul; };
struct GearOpt    { const char* name; int price; const char* code; double strengthNm; };   // nullptr: stok / ozel; giris tork dayanimi
struct WeightOpt  { const char* name; int price; double kg; };
struct TireOpt    { const char* name; int price; int type; double grip, wheelKg; };        // type: TireType
struct RimOpt     { const char* name; int price; double wheelKg; };
struct SuspOpt    { const char* name; int price; double lowerMm, ride; };
struct BrakeOpt   { const char* name; int price; double mul; };
struct AeroOpt    { const char* name; int price; double cd, downforce; };                  // downforce: N @ 100 km/h
struct ElecOpt    { const char* name; int price; bool abs, tc; double tcSlip, absSlip; };

const std::vector<IntakeOpt>&  intakeTable();
const std::vector<ShapeOpt>&   exhaustTable();
const std::vector<CamOpt>&     camTable();
const std::vector<ValveOpt>&   valveTable();
const std::vector<HeadOpt>&    headTable();
const std::vector<PistonOpt>&  pistonTable();
const std::vector<RodOpt>&     rodTable();
const std::vector<CrankOpt>&   crankTable();
const std::vector<BearingOpt>& bearingTable();
const std::vector<GasketOpt>&  gasketTable();
const std::vector<TurbineOpt>& turbineTable();
const std::vector<GateOpt>&    wastegateTable();
const std::vector<BoostCtlOpt>& boostCtlTable();
const std::vector<GbStrOpt>&   gbStrengthTable();
const std::vector<CoolOpt>&    coolingTable();
const std::vector<FlyOpt>&     flywheelTable();
const std::vector<TurboOpt>&   turboTable();
const std::vector<SuperOpt>&   superTable();
const std::vector<IcOpt>&      intercoolerTable();
const std::vector<FuelSysOpt>& fuelSysTable();
const std::vector<NosOpt>&     nosTable();
const std::vector<EcuOpt>&     ecuTable();
// Bilesen tablolari (ayni anda takilabilen parcalar): gaz kelebegi, emme manifoldu, egzoz manifoldu, katalizor,
// su-metanol, kapak saplamasi, ana yatak destegi, fren diski / kaliper, kaporta / cam / sasi hafifletme, on / yan / alt
// aero, fan, termostat-katki, yag sogutucu, yag pompasi
const std::vector<IntakeOpt>&  throttleTable();
const std::vector<IntakeOpt>&  intakeManiTable();
const std::vector<ShapeOpt>&   headerTable();
const std::vector<ShapeOpt>&   catalystTable();
const std::vector<IcOpt>&      methTable();
const std::vector<BearingOpt>& headStudTable();
const std::vector<BearingOpt>& mainSupportTable();
const std::vector<BrakeOpt>&   brakeDiscTable();
const std::vector<BrakeOpt>&   brakeCaliperTable();
const std::vector<WeightOpt>&  weightBodyTable();
const std::vector<WeightOpt>&  weightGlassTable();
const std::vector<WeightOpt>&  weightChassisTable();
const std::vector<AeroOpt>&    aeroFrontTable();
const std::vector<AeroOpt>&    aeroSideTable();
const std::vector<AeroOpt>&    aeroUnderTable();
const std::vector<CoolOpt>&    fanTable();
const std::vector<CoolOpt>&    coolMiscTable();
const std::vector<CoolOpt>&    oilCoolerTable();
const std::vector<OilOpt>&     oilPumpTable();
double coolingCapMul(const Tune& t);                 // radyator x fan x termostat-katki x yag sogutucu
double coolingLowSpeed(const Tune& t);               // dusuk hizda hava akisi (radyator / fan)
// Yakit sistemi parcalari (FuelSysOpt.cap: fabrika HP x cap). Sinir = min(pompa, enjektor) x hat carpani
const std::vector<FuelSysOpt>& fuelPumpTable();
const std::vector<FuelSysOpt>& injectorTable();
const std::vector<FuelSysOpt>& fuelLineTable();
double fuelCap(const Tune& t);
// ECU donanimi: yazilim yuvasi ve modul seviye sinirlari
struct EcuHwOpt { const char* name; int price; int slots; int maxLv[7]; bool knockBuiltin; };
const std::vector<EcuHwOpt>& ecuHwTable();
// Yazilim modulleri (sira: harita, devir, launch, flat shift, anti-lag, flex fuel, vuruntu kontrol)
enum EcuSw { SwMap = 0, SwRev, SwLaunch, SwFlat, SwAntiLag, SwFlex, SwKnock, SwCount };
struct EcuSwDef { const char* name; const char* desc; int price[10]; };
const EcuSwDef& ecuSwDef(int sw);
int  ecuSwLevel(const Tune& t, int sw);
void setEcuSwLevel(Tune& t, int sw, int lv);
int  ecuSwMax(const Tune& t, int sw);               // takili ECU'nun izin verdigi en yuksek seviye
int  ecuSlotsUsed(const Tune& t);
void clampEcuSoftware(Tune& t);                      // ECU degisince yazilim yeni donanima sigdirilir
bool ecuNewSystem(const Tune& t);                    // donanim / yazilim kullaniliyor (eski ecu alani yerine)
EcuOpt effectiveEcu(const Tune& t, bool* knockSensor = nullptr);
bool launchControlAvailable(const Tune& t);          // ayarlanabilir 2-step (launch yazilimi ya da eski ECU paketi)
const std::vector<FuelOpt>&    fuelTable();
const std::vector<OilOpt>&     oilTable();
const std::vector<ClutchOpt>&  clutchTable();
const std::vector<AxleOpt>&    axleTable();
const std::vector<DiffOpt>&    diffTable();
const std::vector<FinalOpt>&   finalTable();
const std::vector<GearOpt>&    gearTable();
const std::vector<WeightOpt>&  weightTable();
const std::vector<TireOpt>&    tireTable();
const std::vector<RimOpt>&     rimTable();
const std::vector<SuspOpt>&    suspTable();
const std::vector<BrakeOpt>&   brakeTable();
const std::vector<AeroOpt>&    aeroTable();
const std::vector<ElecOpt>&    elecTable();

// Ozel uretim (atolye) secenek indeksleri: bu satirlar Tune::cust* degerlerini kullanir
constexpr int kCustomTurbo = 29, kCustomCam = 11, kCustomCrank = 9, kCustomFinal = 11, kCustomGear = 30, kCustomAero = 9;
// Atolye ozel parca fiziği (turbo kompresor capi mm -> boost/spool, kam derecesi -> egri, hacim artisi, kanat)
TurboOpt customTurbo(const Tune& t);
CamOpt   customCam(const Tune& t);
AeroOpt  customAero(const Tune& t);

// Motor swap listesi: 0 = fabrika motoru, sonra tum motorlar guce gore sirali (engineTable indeksleri)
const std::vector<int>& swapEngines();
// Aracin efektif motor / sanziman indeksleri (swap varsa)
int effectiveEngine(const VehicleDef& v, const Tune* t);
int effectiveGearbox(const VehicleDef& v, const Tune* t);
// Motor swap agirlik farki (kg): hacim ve silindir sayisindan
double swapMassDelta(const VehicleDef& v, const Tune& t);

// Dayanim (N*m): motor (piston/biyel/krank en zayif halka x yatak x [asiri beslemede] conta), sanziman giris torku.
// Sinirin ustunde calisma hasar biriktirir (VehicleSim): motor patlar / sanziman kirilir. Fabrika parcalari fabrika
// torkunun %30 / %90 fazlasini kaldirir.
struct Durability { double engineNm, gearboxNm; };
Durability durability(const VehicleDef& v, const Tune* t);

} // namespace zk
