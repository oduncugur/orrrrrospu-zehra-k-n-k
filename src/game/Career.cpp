#include "Career.h"
#include "game/Achievements.h"
#include "sim/PartTables.h"
#include "sim/VehicleSim.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace zk {

// ------------------------------------------------------------------ parca katalogu (PartTables)
namespace {
template <class T> std::vector<PartOption> opts(const std::vector<T>& t) {
    std::vector<PartOption> o;
    for (const auto& r : t) o.push_back({r.name, r.price});
    return o;
}
template <class T> int clampRow(const std::vector<T>& t, int i) { return std::clamp(i, 0, (int)t.size() - 1); }
} // namespace

const char* partTabName(int tab) {
    static const char* n[kPartTabs] = {"MOTOR", "EMME", "TURBO", "AKTARMA", "SASI", "KAPORTA", "ECU"};
    return n[std::clamp(tab, 0, kPartTabs - 1)];
}
int partTab(PartCat c) {
    const int i = (int)c;
    return i <= (int)PartCat::EngineSwap ? 0 : i <= (int)PartCat::Exhaust ? 1 : i <= (int)PartCat::Fuel ? 2 : i <= (int)PartCat::Axles ? 3
         : i <= (int)PartCat::BrakeCaliper ? 4 : i <= (int)PartCat::AeroUnder ? 5 : 6;
}
int customOption(PartCat c) {
    switch (c) {
    case PartCat::Turbo: return kCustomTurbo;
    case PartCat::Cam: return kCustomCam;
    case PartCat::Crank: return kCustomCrank;
    case PartCat::FinalDrive: return kCustomFinal;
    case PartCat::Gearbox: return kCustomGear;
    case PartCat::Aero: return kCustomAero;
    default: return -1;
    }
}

int adasFactory(const VehicleDef& v) { return v.year >= 2020 ? 3 : v.year >= 2014 ? 2 : v.year >= 1998 ? 1 : 0; }
int adasLevel(const VehicleDef& v, const Tune& t) { return std::max(adasFactory(v), std::clamp(t.adas, 0, 4)); }
const char* adasName(int l) {
    static const char* n[5] = {"YOK", "HIZ SABITLEYICI", "ADAPTIF HIZ SABITLEYICI", "SERIT TAKIP + ADAPTIF", "OTONOM SURUS"};
    return n[std::clamp(l, 0, 4)];
}

const char* partCatName(PartCat c) {
    static const char* n[(int)PartCat::Count] = {
        "SILINDIR KAPAGI", "SUPAP + YAY", "KAM MILI", "PISTON", "BIYEL", "KRANK", "YATAK", "ANA YATAK DESTEGI", "KAPAK CONTASI",
        "KAPAK SAPLAMASI", "VOLAN", "MOTOR SWAP",
        "HAVA FILTRESI / EMME", "GAZ KELEBEGI", "EMME MANIFOLDU", "EGZOZ MANIFOLDU", "KATALIZOR", "EGZOZ HATTI / SUSTURUCU",
        "TURBO", "TURBIN", "WASTEGATE", "BOOST KONTROL", "KOMPRESOR", "INTERCOOLER", "SU / METANOL", "NITRO (NOS)",
        "YAKIT POMPASI", "ENJEKTOR", "YAKIT HATTI / REGULATOR", "YAKIT",
        "DEBRIYAJ", "SANZIMAN SWAP", "SANZIMAN GUCLENDIRME", "SON DISLI", "DIFERANSIYEL", "AKS",
        "LASTIK", "YOL LASTIGI (2. TAKIM)", "JANT", "SUSPANSIYON", "FREN BALATASI", "FREN DISKI", "KALIPER",
        "IC HAFIFLETME", "KAPORTA HAFIFLETME", "CAM", "SASI HAFIFLETME", "ON AERO", "YAN AERO", "ARKA KANAT / SPOILER", "ALT AERO",
        "ECU", "ELEKTRONIK", "RADYATOR", "FAN", "TERMOSTAT / KATKI", "KARTER", "YAG SOGUTUCU", "YAG POMPASI", "SURUS YARDIMI"};
    return n[(int)c];
}

// Motor swap = motorun kendi bedeli + sabit iscilik. Motor bedeli: o motorla gelen en ucuz aracin fiyatinin yarisi
// (katalogda yoksa 30 $/HP). Iscilik her motorda ayni.
constexpr int kSwapLabor = 2500;
static int swapPrice(int engine) {
    int best = 0;
    for (const VehicleDef& v : vehicleCatalog()) if (v.engine == engine) { const int p = carPrice(v); if (best == 0 || p < best) best = p; }
    const int value = best > 0 ? (int)(0.5 * best) : (int)(30.0 * engineTable()[engine].powerHp);
    return (value + kSwapLabor) / 10 * 10;
}

const std::vector<PartOption>& partOptions(PartCat c) {
    static std::vector<PartOption> t[(int)PartCat::Count];
    static bool init = false;
    if (!init) {
        init = true;
        t[(int)PartCat::Head] = opts(headTable());          t[(int)PartCat::Valve] = opts(valveTable());
        t[(int)PartCat::Cam] = opts(camTable());            t[(int)PartCat::Piston] = opts(pistonTable());
        t[(int)PartCat::Rod] = opts(rodTable());            t[(int)PartCat::Crank] = opts(crankTable());
        t[(int)PartCat::Bearing] = opts(bearingTable());    t[(int)PartCat::Gasket] = opts(gasketTable());
        t[(int)PartCat::Flywheel] = opts(flywheelTable());
        t[(int)PartCat::MainSupport] = opts(mainSupportTable()); t[(int)PartCat::HeadStud] = opts(headStudTable());
        t[(int)PartCat::Throttle] = opts(throttleTable());  t[(int)PartCat::IntakeMani] = opts(intakeManiTable());
        t[(int)PartCat::Header] = opts(headerTable());      t[(int)PartCat::Catalyst] = opts(catalystTable());
        t[(int)PartCat::Meth] = opts(methTable());
        t[(int)PartCat::BrakeDisc] = opts(brakeDiscTable()); t[(int)PartCat::BrakeCaliper] = opts(brakeCaliperTable());
        t[(int)PartCat::WeightBody] = opts(weightBodyTable()); t[(int)PartCat::WeightGlass] = opts(weightGlassTable());
        t[(int)PartCat::WeightChassis] = opts(weightChassisTable());
        t[(int)PartCat::AeroFront] = opts(aeroFrontTable()); t[(int)PartCat::AeroSide] = opts(aeroSideTable());
        t[(int)PartCat::AeroUnder] = opts(aeroUnderTable());
        t[(int)PartCat::Fan] = opts(fanTable());            t[(int)PartCat::CoolMisc] = opts(coolMiscTable());
        t[(int)PartCat::OilCooler] = opts(oilCoolerTable()); t[(int)PartCat::OilPump] = opts(oilPumpTable());
        t[(int)PartCat::Adas] = {{"YOK", 0}, {"HIZ SABITLEYICI", 450}, {"ADAPTIF HIZ SABITLEYICI", 1600}, {"SERIT TAKIP + ADAPTIF", 2900},
                                 {"OTONOM SURUS", 6500}};
        {   // Motor swap: fabrika + tum motorlar (guce gore); ad: kod + duzen + guc
            static std::vector<std::string> names;
            names.reserve(swapEngines().size() + 1);
            names.push_back("FABRIKA MOTORU");
            for (int e : swapEngines()) {
                const EngineDef& d = engineTable()[e];
                char b[96]; std::snprintf(b, sizeof b, "%s %.0fHP", engineDisplayName(d).c_str(), d.powerHp);
                names.push_back(b);
            }
            for (size_t i = 0; i < names.size(); ++i)
                t[(int)PartCat::EngineSwap].push_back({names[i].c_str(), i == 0 ? 0 : swapPrice(swapEngines()[i - 1])});
        }
        t[(int)PartCat::Intake] = opts(intakeTable());      t[(int)PartCat::Exhaust] = opts(exhaustTable());
        t[(int)PartCat::RoadTires] = opts(tireTable()); t[(int)PartCat::RoadTires][0] = {"YOK (YOLDA ANA LASTIK)", 0};
        t[(int)PartCat::Turbo] = opts(turboTable());        t[(int)PartCat::Turbine] = opts(turbineTable());
        t[(int)PartCat::Wastegate] = opts(wastegateTable()); t[(int)PartCat::BoostCtl] = opts(boostCtlTable());
        t[(int)PartCat::Supercharger] = opts(superTable()); t[(int)PartCat::Intercooler] = opts(intercoolerTable());
        t[(int)PartCat::Nitrous] = opts(nosTable());        t[(int)PartCat::FuelPump] = opts(fuelPumpTable());
        t[(int)PartCat::Injector] = opts(injectorTable());  t[(int)PartCat::FuelLine] = opts(fuelLineTable());
        t[(int)PartCat::Fuel] = opts(fuelTable());          t[(int)PartCat::Clutch] = opts(clutchTable());
        t[(int)PartCat::Gearbox] = opts(gearTable());       t[(int)PartCat::GbStrength] = opts(gbStrengthTable());
        t[(int)PartCat::FinalDrive] = opts(finalTable());   t[(int)PartCat::Diff] = opts(diffTable());
        t[(int)PartCat::Axles] = opts(axleTable());         t[(int)PartCat::Tires] = opts(tireTable());
        t[(int)PartCat::Rims] = opts(rimTable());           t[(int)PartCat::Suspension] = opts(suspTable());
        t[(int)PartCat::Brakes] = opts(brakeTable());       t[(int)PartCat::Weight] = opts(weightTable());
        t[(int)PartCat::Aero] = opts(aeroTable());          t[(int)PartCat::Ecu] = opts(ecuHwTable());
        t[(int)PartCat::Electronics] = opts(elecTable());   t[(int)PartCat::Cooling] = opts(coolingTable());
        t[(int)PartCat::DrySump] = opts(oilTable());
    }
    return t[(int)c];
}

int partLevel(const Tune& t, PartCat c, const VehicleDef& v) {
    switch (c) {
    case PartCat::Head: return t.head;           case PartCat::Valve: return t.valve;
    case PartCat::Cam: return t.cam;             case PartCat::Piston: return t.piston;
    case PartCat::Rod: return t.rod;             case PartCat::Crank: return t.crank;
    case PartCat::Bearing: return t.bearing;     case PartCat::Gasket: return t.gasket;
    case PartCat::Flywheel: return t.flywheel;   case PartCat::EngineSwap: return t.engineSwap;
    case PartCat::MainSupport: return t.mainSupport; case PartCat::HeadStud: return t.headStud;
    case PartCat::Throttle: return t.throttleBody; case PartCat::IntakeMani: return t.intakeMani;
    case PartCat::Header: return t.header;       case PartCat::Catalyst: return t.catalyst;
    case PartCat::Meth: return t.meth;
    case PartCat::BrakeDisc: return t.brakeDisc; case PartCat::BrakeCaliper: return t.brakeCaliper;
    case PartCat::WeightBody: return t.weightBody; case PartCat::WeightGlass: return t.weightGlass;
    case PartCat::WeightChassis: return t.weightChassis;
    case PartCat::AeroFront: return t.aeroFront; case PartCat::AeroSide: return t.aeroSide; case PartCat::AeroUnder: return t.aeroUnder;
    case PartCat::Fan: return t.fan;             case PartCat::CoolMisc: return t.coolMisc;
    case PartCat::OilCooler: return t.oilCooler; case PartCat::OilPump: return t.oilPump;
    case PartCat::Adas: return t.adas;
    case PartCat::Intake: return t.intake;       case PartCat::Exhaust: return t.exhaust;
    case PartCat::Turbo: return t.turbo;         case PartCat::Turbine: return t.turbine;
    case PartCat::Wastegate: return t.wastegate; case PartCat::BoostCtl: return t.boostCtl;
    case PartCat::Supercharger: return t.superch; case PartCat::Intercooler: return t.intercooler;
    case PartCat::Nitrous: return t.nitrous;     case PartCat::FuelPump: return t.fuelPump;
    case PartCat::Injector: return t.injector;   case PartCat::FuelLine: return t.fuelLine;
    case PartCat::Fuel: return t.fuelSel > 0 ? t.fuelSel : t.fuel == FuelType::Pump95 ? 1 : t.fuel == FuelType::E85 ? 2 : 0;
    case PartCat::Clutch: return t.clutch;       case PartCat::Gearbox: return t.gearSwap;
    case PartCat::GbStrength: return t.gbStrength;
    case PartCat::FinalDrive: {
        if (t.finalSel > 0) return t.finalSel;
        if (t.finalDrive <= 0.0) return 0;
        return t.finalDrive > buildGearbox(v).finalDrive ? 1 : 2;           // v1 kaydi (mutlak oran)
    }
    case PartCat::Diff: return (int)t.diff;      case PartCat::Axles: return t.axles;
    case PartCat::Tires: return t.tireSel > 0 ? t.tireSel : (int)t.tires;
    case PartCat::RoadTires: return t.roadTire;
    case PartCat::Rims: return t.rims;           case PartCat::Suspension: return t.susp;
    case PartCat::Brakes: return t.brakes;       case PartCat::Weight: return t.weight;
    case PartCat::Aero: return t.aero;           case PartCat::Ecu: return t.ecuHw;
    case PartCat::Electronics: return t.elec > 0 ? t.elec : t.tcKit ? 2 : t.absKit ? 1 : 0;
    case PartCat::Cooling: return t.cooling;
    case PartCat::DrySump: return t.oil > 0 ? t.oil : t.drySump ? 1 : 0;
    default: return 0;
    }
}

void setPartLevel(Tune& t, PartCat c, int l, const VehicleDef& v) {
    l = std::clamp(l, 0, (int)partOptions(c).size() - 1);
    (void)v;
    switch (c) {
    case PartCat::Head: t.head = l; break;           case PartCat::Valve: t.valve = l; break;
    case PartCat::Cam: t.cam = l; break;             case PartCat::Piston: t.piston = l; break;
    case PartCat::Rod: t.rod = l; break;             case PartCat::Crank: t.crank = l; break;
    case PartCat::Bearing: t.bearing = l; break;     case PartCat::Gasket: t.gasket = l; break;
    case PartCat::Flywheel: t.flywheel = l; break;   case PartCat::EngineSwap: t.engineSwap = l; break;
    case PartCat::MainSupport: t.mainSupport = l; break; case PartCat::HeadStud: t.headStud = l; break;
    case PartCat::Throttle: t.throttleBody = l; break; case PartCat::IntakeMani: t.intakeMani = l; break;
    case PartCat::Header: t.header = l; break;       case PartCat::Catalyst: t.catalyst = l; break;
    case PartCat::Meth: t.meth = l; break;
    case PartCat::BrakeDisc: t.brakeDisc = l; break; case PartCat::BrakeCaliper: t.brakeCaliper = l; break;
    case PartCat::WeightBody: t.weightBody = l; break; case PartCat::WeightGlass: t.weightGlass = l; break;
    case PartCat::WeightChassis: t.weightChassis = l; break;
    case PartCat::AeroFront: t.aeroFront = l; break; case PartCat::AeroSide: t.aeroSide = l; break; case PartCat::AeroUnder: t.aeroUnder = l; break;
    case PartCat::Fan: t.fan = l; break;             case PartCat::CoolMisc: t.coolMisc = l; break;
    case PartCat::OilCooler: t.oilCooler = l; break; case PartCat::OilPump: t.oilPump = l; break;
    case PartCat::Adas: t.adas = l; break;
    case PartCat::Intake: t.intake = l; break;       case PartCat::Exhaust: t.exhaust = l; break;
    case PartCat::Turbo: t.turbo = l; break;         case PartCat::Turbine: t.turbine = l; break;
    case PartCat::Wastegate: t.wastegate = l; break; case PartCat::BoostCtl: t.boostCtl = l; break;
    case PartCat::Supercharger: t.superch = l; break; case PartCat::Intercooler: t.intercooler = l; break;
    case PartCat::Nitrous: t.nitrous = l; break;
    case PartCat::FuelPump: t.fuelPump = l; t.fuelSys = 0; break;
    case PartCat::Injector: t.injector = l; t.fuelSys = 0; break;
    case PartCat::FuelLine: t.fuelLine = l; t.fuelSys = 0; break;
    case PartCat::Fuel: t.fuelSel = l; t.fuel = (FuelType)fuelTable()[l].type; break;
    case PartCat::Clutch: t.clutch = l; break;       case PartCat::Gearbox: t.gearSwap = l; break;
    case PartCat::GbStrength: t.gbStrength = l; break;
    case PartCat::FinalDrive: t.finalSel = l; t.finalDrive = 0.0; break;
    case PartCat::Diff: t.diff = (DiffType)l; break; case PartCat::Axles: t.axles = l; break;
    case PartCat::Tires: t.tireSel = l; t.tires = (TireType)tireTable()[l].type; t.psi = 0; break;
    case PartCat::RoadTires: t.roadTire = l; break;
    case PartCat::Rims: t.rims = l; break;           case PartCat::Suspension: t.susp = l; break;
    case PartCat::Brakes: t.brakes = l; break;       case PartCat::Weight: t.weight = l; break;
    case PartCat::Aero: t.aero = l; break;
    case PartCat::Ecu: t.ecuHw = l; t.ecu = 0; clampEcuSoftware(t); break;   // yeni ECU: yazilim sigdirilir
    case PartCat::Electronics: t.elec = l; t.absKit = elecTable()[l].abs; t.tcKit = elecTable()[l].tc; break;
    case PartCat::Cooling: t.cooling = l; break;
    case PartCat::DrySump: t.oil = l; t.drySump = oilTable()[l].dry; break;
    default: break;
    }
}
int carPrice(const VehicleDef& v) {
    const double hp = peakPowerHp(v);
    double bodyF = 1.0;
    switch (v.body) {
    case Body::Super: bodyF = 2.4; break;   case Body::Muscle: bodyF = 1.2; break;
    case Body::SUV: case Body::Pickup: case Body::Van: bodyF = 0.9; break;
    case Body::Kei: bodyF = 0.7; break;     case Body::Roadster: bodyF = 1.1; break;
    default: break;
    }
    const double ageF = v.year < 1975 ? 1.35 : v.year < 1995 ? 0.85 : v.year < 2010 ? 1.0 : 1.3;
    double p = 1500.0 + 11.0 * std::pow(hp, 1.25) * bodyF * ageF * (v.streetLegal ? 1.0 : 1.4);
    return (int)(std::round(p / 100.0) * 100.0);
}

double typicalKm(const VehicleDef& v) { return std::clamp(16000.0 * std::max(0, kGameYear - v.year) + 6000.0, 6000.0, 260000.0); }
// km egrisi: f(km) = (1 + km / 150000)^-0.9 -> 30 bin 0.85, 300 bin 0.37; ortalamaya oranla
double kmValueMul(const VehicleDef& v, double km) {
    auto f = [](double k) { return std::pow(1.0 + std::max(0.0, k) / 150000.0, -0.9); };
    return std::clamp(f(km) / f(typicalKm(v)), 0.4, 1.5);
}
double OwnedCar::odo() const { return km >= 0 ? km : typicalKm(*findVehicle(carId)); }

const char* faultName(int f) {
    switch (f) { case 1: return "MOTOR YORGUN"; case 2: return "SANZIMAN KIRIK"; case 3: return "AKS KIRIK"; default: return "YOK"; }
}

OwnedCar listingCar(const UsedListing& l) {
    OwnedCar oc; oc.carId = l.carId; oc.km = l.km;
    const VehicleDef& v = *findVehicle(l.carId);
    Tune& t = oc.tune;
    const double worn = (100 - l.cond) / 100.0;                       // kondisyon: parca yipranmasi
    t.wearEngine = 0.25 * worn; t.wearTires = 0.8 * worn; t.wearBrakes = 0.6 * worn; t.wearSusp = 0.5 * worn; t.wearBody = 0.5 * worn;
    if (l.mods > 0) {                                                 // modifiyeli: emme / egzoz / ECU / suspansiyon / jant
        const PartCat cats[] = {PartCat::Intake, PartCat::Exhaust, PartCat::Ecu, PartCat::Suspension, PartCat::Header, PartCat::Rims};
        for (int i = 0; i < 2 + 2 * l.mods && i < 6; ++i) {
            const int lv = std::min(l.mods, (int)partOptions(cats[i]).size() - 1);
            if (lv > 0 && partAvailable(cats[i], lv, v, nullptr, &t)) { setPartLevel(t, cats[i], lv, v); oc.paidParts += partPrice(cats[i], lv, v); }
        }
    }
    if (l.fault == 1) oc.engineWear = 0.45;
    if (l.fault == 2) oc.gearboxBroken = true;
    if (l.fault == 3) oc.axleBroken = true;
    return oc;
}

UsedListing usedListing(int carId, int week, int k) {
    const VehicleDef& v = *findVehicle(carId);
    uint32_t h = (uint32_t)carId * 2654435761u ^ (uint32_t)(week + 7) * 2246822519u ^ (uint32_t)(k + 1) * 3266489917u;
    auto u = [&]() { h ^= h >> 15; h *= 0x2c1b3c6du; h ^= h >> 12; h *= 0x297a2d39u; h ^= h >> 15; return (h & 0xFFFFFF) / 16777216.0; };
    UsedListing l; l.carId = carId;
    const double ty = typicalKm(v);
    l.km = std::round(ty * (0.3 + 1.4 * u()) / 100.0) * 100.0;          // ortalamanin %30 - %170'i
    if (k == 0 && soldNew(v)) l.km = std::round(ty * (0.15 + 0.4 * u()) / 100.0) * 100.0;   // genc arac: ilk ilan az km
    l.cond = (int)std::clamp(100.0 - l.km / 6000.0 - 25.0 * u(), 35.0, 100.0);
    const double r = u();
    l.mods = r < 0.25 ? 1 + (int)(u() * 3) : 0;                        // %25 modifiyeli
    const double fr = u();
    l.fault = fr < 0.12 ? 1 : fr < 0.18 ? 2 : fr < 0.22 ? 3 : 0;      // %22 arizali
    const OwnedCar oc = listingCar(l);
    const double base = carPrice(v) * kmValueMul(v, l.km) * (0.7 + 0.3 * l.cond / 100.0) * (soldNew(v) ? 0.82 : 1.0);
    const double p = base + 0.5 * oc.paidParts - 0.9 * repairCostFor(oc);
    l.price = (long)(std::round(std::max(0.25 * carPrice(v), p) / 50.0) * 50.0);
    return l;
}

int partPrice(PartCat c, int level, const VehicleDef& v) {
    const auto& o = partOptions(c);
    if (level < 0 || level >= (int)o.size()) return 0;
    if (c == PartCat::EngineSwap) return (int)(std::round(o[level].basePrice / 10.0) * 10.0);   // motor fiyati aractan bagimsiz
    const double scale = std::clamp(0.6 + carPrice(v) / 40000.0, 0.6, 4.0);   // pahali arabanin parcasi pahali
    return (int)(std::round(o[level].basePrice * scale / 10.0) * 10.0);
}

bool partAvailable(PartCat c, int level, const VehicleDef& v, std::string* why, const Tune* t) {
    auto no = [&](const char* m) { if (why) *why = m; return false; };
    if (c == PartCat::Adas && level > 0 && level <= adasFactory(v)) return no("FABRIKADA VAR");
    if (c == PartCat::Electronics && level > 0) {
        // Fabrikada olan sistem tekrar takilmaz; olmayana ancak ECU yukseltmesiyle eklenir
        if (level == 1 && v.abs) return no("FABRIKADA VAR");
        if (level == 2 && v.abs && v.tc) return no("FABRIKADA VAR");
        if (t && t->ecu < 1 && t->ecuHw < 1) return no("ONCE ECU GEREKLI");
    }
    const EngineDef& e = engineTable()[effectiveEngine(v, t)];
    const bool turboEngine = e.induction == Induction::Turbo || e.induction == Induction::TwinTurbo;
    if (c == PartCat::Turbo && level > 0 && e.induction == Induction::Supercharger) return no("KOMPRESORLU MOTORA TURBO YOK");
    if ((c == PartCat::Turbine || c == PartCat::Wastegate || c == PartCat::BoostCtl) && level > 0 && t && t->turbo == 0 && !turboEngine)
        return no(e.induction == Induction::Supercharger ? "KOMPRESORDE TURBO PARCASI YOK" : "ONCE TURBO GEREKLI");
    if (c == PartCat::Intercooler && level > 0 && t && t->turbo == 0 && t->superch == 0 && !turboEngine && e.induction != Induction::Supercharger)
        return no("ASIRI BESLEME YOK");
    if (c == PartCat::EngineSwap && level > 0 && swapEngines()[level - 1] == v.engine) return no("FABRIKA MOTORU");
    if (c == PartCat::RoadTires && level > 0 && tireTable()[level].type == (int)TireType::DragSlick) return no("DRAG LASTIGI YOLA OLMAZ");
    // Atolye satirlari: once ozel parca uretilmeli (PARCA URET)
    if (level > 0 && level == customOption(c) && t) {
        const bool made = c == PartCat::Turbo ? t->custTurboMm > 0 : c == PartCat::Cam ? t->custCamDeg > 0 : c == PartCat::Crank ? t->custDisp > 0
                        : c == PartCat::FinalDrive ? t->custFinal > 0 : c == PartCat::Aero ? t->custWingN > 0
                        : c == PartCat::Gearbox ? (t->custGear[0] > 0 || t->custGear[1] > 0) : false;
        if (!made) return no("ATOLYEDE URET");
    }
    return true;
}

// Satis: arac %65 + parcalar %40 - hasar (tamir bedeli); hasarli arac en az govde degerinin %25'ine gider
SaleQuote saleQuote(const OwnedCar& c) {
    auto r10 = [](double x) { return (int)(std::round(x / 10.0) * 10.0); };
    SaleQuote q;
    const VehicleDef& qv = *findVehicle(c.carId);
    q.car = r10(0.65 * carPrice(qv) * kmValueMul(qv, c.odo()));       // kilometreye gore
    q.parts = r10(0.4 * c.paidParts);
    q.damage = r10((double)repairCostFor(c));
    {   // Yipranmis bilesenler (hurdalik araci): restorasyon bedelinin %80'i dusulur
        double w = 0;
        for (int k = 2; k < kRestoreParts; ++k) w += restoreStepCost(c, k) * (restoreDamage(c, k) > 0.15 ? 1.6 : 1.0);
        if (c.tune.wearEngine > 0.02) w += restoreStepCost(c, 0) * 0.5;
        q.damage += r10(w * 0.8);
    }
    q.total = std::max(r10(0.25 * q.car), q.car + q.parts - q.damage);
    return q;
}

int sellPrice(const OwnedCar& c) { return saleQuote(c).total; }

double peakHpOf(const VehicleDef& v, const Tune& t) {
    VehicleSimConfig cfg; cfg.car = &v; cfg.tune = &t;
    const VehicleSim s(cfg);
    double hp = 0;
    for (auto* c : {&s.engineSpec().lowCam, &s.engineSpec().highCam})
        for (auto& p : *c) if (p.first <= s.engineSpec().redlineRpm) hp = std::max(hp, p.second * p.first * 2 * 3.14159265 / 60 / 745.7);
    return hp;
}

double performanceIndex(const VehicleDef& v, const Tune& t) {
    VehicleSimConfig cfg; cfg.car = &v; cfg.tune = &t;
    const VehicleSim s(cfg);
    double hp = 0;
    for (auto* c : {&s.engineSpec().lowCam, &s.engineSpec().highCam})
        for (auto& p : *c) if (p.first <= s.engineSpec().redlineRpm) hp = std::max(hp, p.second * p.first * 2 * 3.14159265 / 60 / 745.7);
    const double tireF = t.tires == TireType::DragSlick ? 1.0 : t.tires == TireType::SemiSlick ? 0.93 : 0.85;
    const double driveF = v.drive == Drive::AWD ? 1.08 : v.drive == Drive::FWD ? 0.94 : 1.0;
    return hp / s.baseMassKg() * 1000.0 * tireF * driveF;
}

namespace {
struct EtRow { int id; double e0, e1, e2; };
const EtRow kEtTable[] = {
#include "EtTable.inc"
    {0, -1, -1, -1}
};
const EtRow* etRow(int id) {
    static std::vector<const EtRow*> byId;
    if (byId.empty()) {
        byId.assign(vehicleCatalog().size() + 2, nullptr);
        for (const EtRow& r : kEtTable) if (r.id > 0 && r.id < (int)byId.size()) byId[r.id] = &r;
    }
    return id > 0 && id < (int)byId.size() ? byId[id] : nullptr;
}
} // namespace

double tableEt(int carId, int preset) {
    const EtRow* r = etRow(carId);
    if (!r) return -1.0;
    return preset == 0 ? r->e0 : preset == 1 ? r->e1 : r->e2;
}

double estimatedEt(const VehicleDef& v, const Tune& t) {
    const double idx = performanceIndex(v, t);
    // Lastik tipine gore en yakin YZ seti once, sonra digerleri (gecerli satir bulunana kadar)
    const int first = t.tires == TireType::DragSlick ? 2 : t.tires == TireType::SemiSlick ? 1 : 0;
    for (int k : {first, 1, 0, 2}) {
        const double e = tableEt(v.id, k);
        if (e > 0) return e * std::cbrt(performanceIndex(v, opponentPreset(k)) / std::max(idx, 1.0));
    }
    return 5.825 * std::cbrt(1000.0 * 2.2046 / 1.0139 / std::max(idx, 1.0)) * 1.12;   // lb/hp formulu + sokak payi
}

double prizeDifficulty(double gap) { return std::clamp(1.0 + 0.6 * gap, 0.5, 1.8); }

int racePrize(const VehicleDef& opponent, bool won) {
    return won ? (int)(std::round((250.0 + carPrice(opponent) * 0.04) / 10.0) * 10.0) : 0;
}

Tune opponentPreset(int i) {
    Tune t;
    // Rakip paketleri bilesen parcalarini da kullanir (ET tablosu bu paketlerle olculur: zehra_ettable gen)
    if (i == 1) {                                                   // SOKAK: emme / egzoz, kelebek, katalizor, fren, hafif ic
        t.intake = 1; t.exhaust = 1; t.tires = TireType::SemiSlick; t.diff = DiffType::OneAndHalfWay;
        t.throttleBody = 2; t.catalyst = 1; t.brakes = 1; t.brakeDisc = 1; t.weight = 1;
        t.ecu = 1;                                                  // emme / egzoz kazanci icin stage 1 harita
    }
    if (i == 2) {                                                   // DRAG: ECU + yazilim, header, yakit, hafifletme, dusuk surtunme
        t.intake = 2; t.exhaust = 2; t.tires = TireType::DragSlick;
        t.clutch = 1; t.axles = 1; t.diff = DiffType::OneAndHalfWay;
        t.ecuHw = 4; t.swMap = 2; t.swLaunch = 1; t.header = 1; t.throttleBody = 4; t.catalyst = 4;
        t.fuelPump = 2; t.injector = 2; t.weight = 3; t.weightBody = 3; t.aero = 8;
    }
    return t;
}

Opponent pickOpponent(int playerCarId, const Tune& playerTune, uint32_t seed) {
    const VehicleDef& pv = *findVehicle(playerCarId);
    const double target = performanceIndex(pv, playerTune);
    Tune presets[kOpponentPresets];
    for (int i = 0; i < kOpponentPresets; ++i) presets[i] = opponentPreset(i);
    std::vector<Opponent> all;
    for (const auto& v : vehicleCatalog()) {
        if (v.id == playerCarId || !v.streetLegal) continue;
        for (const Tune& t : presets) all.push_back({v.id, t, performanceIndex(v, t)});
    }
    std::vector<const Opponent*> pool;
    for (double band : {0.06, 0.12, 0.25, 1.0}) {                 // yeterli aday yoksa bandi genislet
        pool.clear();
        for (const Opponent& o : all) if (std::fabs(o.index / target - 1.0) <= band) pool.push_back(&o);
        if (pool.size() >= 6) break;
    }
    if (pool.empty()) return {playerCarId, Tune{}, target};
    uint32_t x = seed * 2654435761u + 0x9E3779B9u; x ^= x >> 15;
    return *pool[x % pool.size()];
}

// Dengeli eslesme: endeks yerine tahmini 1/4 mil (vites, kalkis, cekis dahil). Hedef = oyuncu ET'si + 0.20 s (YZ kusursuz
// kalkar/vites atar; insan payi) - 0.10 s x form
// (kazandikca rakipler hizlanir, kaybettikce yavaslar); bant 0.15 s'den genisler; ayni rakip ust uste gelmez.
void Career::tourRefresh() {
    const int w = todayIndex() / 7;
    if (w != tourWeek) { tourWeek = w; tourRound = 0; tourOut = false; }
}
// ---------------- Sokak ----------------
namespace {
uint32_t mix32(uint32_t x) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x; }
long r50(double v) { return (long)std::lround(v / 50.0) * 50; }
}
int Career::meetSlot() const { return clockSlot >= 0 ? clockSlot : (int)(std::time(nullptr) / (3 * 3600)); }
long Career::meetStake() const {
    const VehicleDef& v = *findVehicle(car().carId);
    return std::clamp(r50(carPrice(v) * 0.08 + 300.0 * (1 + leagueUnlocked())), 500L, 30000L);
}
Opponent Career::meetOpponent() const { return pickOpponentFor((uint32_t)meetSlot() * 7919u + 13u, -0.05); }
bool Career::meetAvailable(std::string* why) const {
    auto no = [&](const char* m) { if (why) *why = m; return false; };
    if (meetDone == meetSlot()) return no("BU GECE YARISTIN");
    if (!car().raceable()) return no(car().jobHp ? "MUSTERI ARACIYLA OLMAZ" : "ARAC HASARLI");
    if (money < meetStake()) return no("BAHSE PARA YETMIYOR");
    return true;
}
bool Career::meetPinkAvailable(std::string* why) const {
    auto no = [&](const char* m) { if (why) *why = m; return false; };
    if (cars.size() < 2) return no("PINK SLIP: EN AZ 2 ARAC");
    if (garageFull()) return no("PINK SLIP: GARAJ DOLU");
    return true;
}
bool Career::meetStart(std::string* why) {
    if (meetPink) {
        if (meetDone == meetSlot()) { if (why) *why = "BU GECE YARISTIN"; return false; }
        if (!car().raceable()) { if (why) *why = "ARAC HASARLI"; return false; }
        if (!meetPinkAvailable(why)) return false;
        meetDone = meetSlot();
        return true;
    }
    if (!meetAvailable(why)) return false;
    money -= meetStake(); meetDone = meetSlot();
    return true;
}
long Career::recordMeet(bool won, long* fine) {
    const long stake = meetStake();
    ++races;
    long net = 0;
    meetPinkWon = 0;
    if (meetPink) {                                                    // pink slip: para yok, araba el degistirir
        meetPink = false;
        if (won) {
            ++wins; rep += 6;
            const Opponent o = meetOpponent();
            OwnedCar oc; oc.carId = o.carId; oc.tune = o.tune;
            cars.push_back(oc); meetPinkWon = o.carId;
            dailyAdd(TaskType::WinDrag, 1); dailyAdd(TaskType::WinAny, 1);
        } else if (cars.size() > 1) {
            meetPinkWon = -car().carId;
            cars.erase(cars.begin() + current);
            current = std::min(current, (int)cars.size() - 1);
        }
        loanRace();
        return 0;
    }
    if (won) { ++wins; net = 2 * stake; money += net; earnings += stake; rep += 2; dailyAdd(TaskType::WinDrag, 1); dailyAdd(TaskType::WinAny, 1); }
    const bool raid = (mix32((uint32_t)meetSlot() * 977u + 5u) % 1000u) < (uint32_t)(kRaidChance * 1000.0);
    const long f = raid ? std::min(money, stake / 2) : 0;
    money -= f;
    if (fine) *fine = f;
    loanRace();
    return net;
}

// ------------------------------------------------------------------ tefeci
long Career::loanLimit() const { return 4000L + 6000L * leagueUnlocked() * (1 + leagueUnlocked()) / 2; }
bool Career::borrow(long amount, std::string* why) {
    amount = std::max(0L, amount) / 10 * 10;
    if (amount <= 0) return false;
    if (loan + amount > loanLimit()) { if (why) *why = "TEFECI: LIMIT DOLU"; return false; }
    if (loan == 0) loanRaces = 0;
    loan += amount; money += amount;
    return true;
}
long Career::repay(long amount) {
    const long p = std::min({std::max(0L, amount), loan, money});
    loan -= p; money -= p;
    if (loan == 0) loanRaces = 0;
    return p;
}
void Career::loanRace() {
    if (loan <= 0) return;
    loan = (long)std::ceil(loan * 1.05 / 10.0) * 10;                   // yaris basina %5
    if (++loanRaces < 12) return;
    // Vade doldu: once para, yetmezse secili araba (en az bir arac kalir); kalan borc silinir (tefeci araciyla gider)
    const long cash = std::min(money, loan);
    money -= cash; loan -= cash;
    char b[96];
    if (loan > 0 && cars.size() > 1) {
        std::snprintf(b, sizeof b, "TEFECI ARACINI ALDI: %s", findVehicle(car().carId)->fullName().c_str());
        cars.erase(cars.begin() + current);
        current = std::min(current, (int)cars.size() - 1);
        loan = 0;
    } else if (loan > 0) {
        std::snprintf(b, sizeof b, "TEFECI PARANI ALDI, BORC SURUYOR: $%ld", loan);
        loanRaces = 6;                                                     // yeni vade (6 yaris)
    } else std::snprintf(b, sizeof b, "TEFECI BORCU PARANDAN KESTI: -$%ld", cash);
    if (loan == 0) loanRaces = 0;
    loanNote = b;
}

// ------------------------------------------------------------------ arac gosterisi
double Career::showScore(const OwnedCar& c) const {
    const Tune& t = c.tune;
    double s = 0;
    if (c.paint >= 0) s += 18;                                             // ozel boya
    s += 6.0 * std::clamp(c.finish, 0, 3);                                 // metalik / mat / sedef
    if (c.stripe > 0) s += 8;
    if (c.rimCol >= 0) s += 5;
    s += 3.0 * t.rims + 2.0 * t.susp;                                      // jant, basiklik
    s += t.aero > 0 ? 10 + t.aero : 0;
    s += 4.0 * ((t.aeroFront > 0) + (t.aeroSide > 0) + (t.aeroUnder > 0));
    s += 0.08 * peakHpOf(*findVehicle(c.carId), t);
    s -= 40.0 * std::clamp(t.wearBody, 0.0, 1.0);                         // pas / gocuk
    return std::max(0.0, s);
}
std::vector<Career::DynoEntry> Career::showField() const {
    static const char* const kNames[9] = {"KROM KAAN", "BASIK BARIS", "NEON NAZ", "JANTCI OZAN", "BOYACI SELO",
                                          "STANCE SERAP", "CILALI CEM", "VITRIN VEDAT", "KANAT KORAY"};
    std::vector<DynoEntry> f;
    const uint32_t w = (uint32_t)(todayIndex() / 7);
    const double me = showScore(car());
    for (int k = 0; k < 9; ++k) {
        const Opponent o = pickOpponentFor(w * 7727u + (uint32_t)k * 104729u + 11u, 0.0);
        const double j = (mix32(w * 31u + (uint32_t)k * 977u) % 1000u) / 1000.0;
        f.push_back({kNames[k], o.carId, std::max(5.0, (20.0 + me) * (0.55 + 0.09 * k) + 8.0 * (j - 0.5)), false});
    }
    return f;
}
bool Career::showAvailable(std::string* why) const {
    auto no = [&](const char* m) { if (why) *why = m; return false; };
    if (showWeek == todayIndex() / 7) return no("BU HAFTA KATILDIN");
    if (car().jobHp) return no("MUSTERI ARACIYLA OLMAZ");
    if (money < dynoEntryFee()) return no("GIRIS UCRETI YETMIYOR");
    return true;
}
bool Career::showEnter(int* place, long* prize, std::vector<DynoEntry>* board, std::string* why) {
    if (!showAvailable(why)) return false;
    const long fee = dynoEntryFee();
    money -= fee; showWeek = todayIndex() / 7;
    std::vector<DynoEntry> f = showField();
    f.push_back({"SEN", car().carId, showScore(car()), true});
    std::stable_sort(f.begin(), f.end(), [](const DynoEntry& a, const DynoEntry& b) { return a.hp > b.hp; });
    int p = 1;
    while (!f[p - 1].player) ++p;
    static const double kMul[3] = {5.0, 2.5, 1.2};
    const long pr = p <= 3 ? r50(fee * kMul[p - 1]) : 0;
    money += pr; earnings += pr;
    rep += p == 1 ? 6 : p <= 3 ? 2 : 0;
    if (place) *place = p;
    if (prize) *prize = pr;
    if (board) *board = f;
    return true;
}

long Career::dynoEntryFee() const {
    return std::clamp(r50(carPrice(*findVehicle(car().carId)) * 0.02 + 150.0 * (1 + leagueUnlocked())), 200L, 6000L);
}
std::vector<Career::DynoEntry> Career::dynoField() const {
    static const char* const kNames[9] = {"TURBO TAYFUN", "ATOLYE KEMAL", "SESSIZ DENIZ", "CAKAR ERCAN", "KISA KOLLU CAN",
                                          "ROT BALANS UGUR", "GAZCI GIZEM", "DUZCE MURAT", "ESKI TOPRAK HALUK"};
    std::vector<DynoEntry> f;
    const uint32_t w = (uint32_t)(todayIndex() / 7);
    for (int k = 0; k < 9; ++k) {
        const Opponent o = pickOpponentFor(w * 104729u + (uint32_t)k * 7919u + 3u, -0.30 + 0.07 * k);
        f.push_back({kNames[k], o.carId, peakHpOf(*findVehicle(o.carId), o.tune), false});
    }
    return f;
}
bool Career::dynoAvailable(std::string* why) const {
    auto no = [&](const char* m) { if (why) *why = m; return false; };
    if (dynoWeek == todayIndex() / 7) return no("BU HAFTA KATILDIN");
    if (car().jobHp) return no("MUSTERI ARACIYLA OLMAZ");
    if (money < dynoEntryFee()) return no("GIRISE PARA YETMIYOR");
    return true;
}
bool Career::dynoEnter(int* place, long* prize, std::vector<DynoEntry>* board, std::string* why) {
    if (!dynoAvailable(why)) return false;
    const long fee = dynoEntryFee();
    money -= fee; dynoWeek = todayIndex() / 7;
    std::vector<DynoEntry> f = dynoField();
    f.push_back({"SEN", car().carId, peakHpOf(*findVehicle(car().carId), car().tune), true});
    std::stable_sort(f.begin(), f.end(), [](const DynoEntry& a, const DynoEntry& b) { return a.hp > b.hp; });
    int p = 1;
    while (!f[p - 1].player) ++p;
    static const double kMul[3] = {5.0, 2.5, 1.2};
    const long pr = p <= 3 ? r50(fee * kMul[p - 1]) : 0;
    money += pr; earnings += pr;
    if (p == 1) rep += 3;
    if (place) *place = p;
    if (prize) *prize = pr;
    if (board) *board = f;
    return true;
}

Career::JobOffer Career::jobOffer(int k) const {
    // Bugunun teklifleri: ucuz sokak araclari; hedef fabrika gucunun %25-%60 ustu; odeme beygir basina ~45 $ + iscilik
    static std::vector<int> pool;
    if (pool.empty())
        for (const auto& v : vehicleCatalog()) if (v.streetLegal && carPrice(v) <= 30000) pool.push_back(v.id);
    const uint32_t x = mix32((uint32_t)jobSeq[std::clamp(k, 0, 2)] * 2654435761u + (uint32_t)k * 40503u + 17u);   // kabul edilince yenilenir
    const int id = pool[x % pool.size()];
    const VehicleDef& v = *findVehicle(id);
    const double stock = peakHpOf(v, Tune{});
    const double mul = 1.25 + 0.12 * k + 0.01 * (int)((x >> 12) % 11u);
    const int target = (int)std::lround(stock * mul / 5.0) * 5;
    const long reward = r50((700.0 + 48.0 * (target - stock)) * (1.0 + 0.10 * k));
    return {id, target, reward, (int)std::lround(stock)};
}
bool Career::jobTaken(int) const { return false; }                     // teklif kabulde yenilenir (gunluk bekleme yok)
bool Career::acceptJob(int k, std::string* why) {
    auto no = [&](const char* m) { if (why) *why = m; return false; };
    if (k < 0 || k > 2) return no("GECERSIZ");
    if (jobTaken(k)) return no("ALINDI");
    if (garageFull()) return no("GARAJ DOLU");
    const JobOffer o = jobOffer(k);
    OwnedCar oc; oc.carId = o.carId; oc.jobHp = o.targetHp; oc.jobReward = o.reward;
    cars.push_back(oc);
    current = (int)cars.size() - 1;
    ++jobSeq[k];                                                        // o slota yeni teklif
    return true;
}
bool Career::deliverJob(long* paid, std::string* why) {
    OwnedCar& oc = cars[current];
    if (oc.jobHp <= 0) { if (why) *why = "MUSTERI ARACI DEGIL"; return false; }
    const double hp = peakHpOf(*findVehicle(oc.carId), oc.tune);
    if (hp + 0.5 < oc.jobHp) { if (why) *why = "HEDEF GUCE ULASILMADI"; return false; }
    const long r = oc.jobReward;
    money += r; earnings += r; rep += 1;
    if (paid) *paid = r;
    cars.erase(cars.begin() + current);
    current = std::min(current, (int)cars.size() - 1);
    return true;
}
bool Career::cancelJob() {
    if (cars[current].jobHp <= 0) return false;
    cars.erase(cars.begin() + current);
    current = std::min(current, (int)cars.size() - 1);
    return true;
}

int gaugeStylePrice(int style) { static const int p[4] = {0, 400, 650, 950}; return p[std::clamp(style, 0, 3)]; }
bool Career::selectGauge(int style, std::string* why) {
    if (style < 0 || style > 3) return false;
    OwnedCar& oc = car();
    if (style > 0 && !((oc.gaugeOwned >> style) & 1)) {
        const int p = gaugeStylePrice(style);
        if (money < p) { if (why) *why = "PARA YETMIYOR"; return false; }
        money -= p; oc.gaugeOwned |= 1 << style;
    }
    oc.gauge = style;
    return true;
}
bool Career::toggleBoostGauge(std::string* why) {
    OwnedCar& oc = car();
    if (!((oc.gaugeOwned >> 8) & 1)) {
        if (money < kBoostGaugePrice) { if (why) *why = "PARA YETMIYOR"; return false; }
        money -= kBoostGaugePrice; oc.gaugeOwned |= 1 << 8; oc.boostGauge = true;
        return true;
    }
    oc.boostGauge = !oc.boostGauge;
    return true;
}

long Career::recordRun(int position, int count, double realKm) {
    ++races;
    const double base = 30.0 * realKm * (1 + leagueUnlocked() * 0.5);
    const double k = position == 1 ? 1.0 : position == 2 ? 0.6 : position == 3 ? 0.38 : std::max(0.04, 0.2 * (1.0 - (double)position / std::max(2, count)));
    const long p = (long)std::lround(base * k / 50.0) * 50;
    money += p; earnings += p;
    if (position == 1) { ++wins; rep += 4; dailyAdd(TaskType::WinRoad, 1); dailyAdd(TaskType::WinAny, 1); }
    else if (position <= 3) rep += 2;
    return p;
}

const char* Career::cityName(int c) {
    static const char* const n[kCities] = {"ISTANBUL", "IZMIT", "BURSA", "ESKISEHIR", "ANKARA", "KONYA", "KAPADOKYA", "NIGDE", "MERSIN", "ANTALYA"};
    return n[std::clamp(c, 0, kCities - 1)];
}
double Career::legKm(int from) { static const double km[kCities - 1] = {110, 130, 155, 235, 260, 230, 85, 205, 480}; return km[std::clamp(from, 0, kCities - 2)]; }
int Career::legField(int from) { static const int n[kCities - 1] = {10, 20, 30, 50, 70, 100, 120, 150, 200}; return n[std::clamp(from, 0, kCities - 2)]; }
double Career::travelKm(int to) const {
    double km = 0;
    for (int c = std::min(city, to); c < std::max(city, to); ++c) km += legKm(c);
    return km;
}
long Career::fastTravelPrice(int to) const { return (long)std::lround(travelKm(to) * 9.0 / 10.0) * 10; }   // ~9 $/km
bool Career::canTravel(int to, std::string* why) const {
    auto no = [&](const char* m) { if (why) *why = m; return false; };
    if (to < 0 || to >= kCities) return no("GECERSIZ");
    if (to == city) return no("ZATEN BURADASIN");
    if (cityLeague(to) > leagueUnlocked()) return no("KILITLI: ONCEKI PATRONU YEN");
    return true;
}
bool Career::fastTravel(int to, std::string* why) {
    if (!canTravel(to, why)) return false;
    const long p = fastTravelPrice(to);
    if (money < p) { if (why) *why = "PARA YETMIYOR"; return false; }
    money -= p;
    addKm(travelKm(to));                                               // hizli gecis: arac yolu surerek gider (km sayaci)
    city = to;
    return true;
}
std::vector<RunEntrant> Career::runField(int n, uint32_t seed) const {
    std::vector<RunEntrant> f;
    f.reserve(n);
    for (int i = 0; i < n; ++i) {
        const uint32_t h = (seed + (uint32_t)i * 2654435761u) * 2246822519u;
        const int roll = (int)(h >> 8) % 100;
        const int style = roll < 30 ? StyleBalanced : roll < 50 ? StyleFlatOut : roll < 70 ? StyleEco : roll < 85 ? StyleStock : StyleMonster;
        const double off = style == StyleMonster ? -1.2 : style == StyleStock ? 0.6 : -0.3 + 0.6 * ((h >> 3) % 100) / 100.0;
        Opponent o = pickOpponentFor(seed * 31u + (uint32_t)i * 977u + 5u, off);
        if (style == StyleStock) o.tune = Tune{};
        f.push_back({o.carId, o.tune, style});
    }
    return f;
}

long Career::tourEntry() const { return 400L * (1 + leagueUnlocked()); }
long Career::tourPrize() const { return 3000L * (1 + leagueUnlocked()) * (1 + leagueUnlocked()) / 2 + 2000L; }
bool Career::tourAvailable(std::string* why) const {
    auto no = [&](const char* m) { if (why) *why = m; return false; };
    if (tourOut) return no("BU HAFTA ELENDIN");
    if (tourRound >= kTourRounds) return no("BU HAFTA SAMPIYONSUN");
    if (!car().raceable()) return no(car().jobHp ? "MUSTERI ARACIYLA OLMAZ" : "ARAC HASARLI");
    if (tourRound == 0 && money < tourEntry()) return no("GIRIS UCRETI YETMIYOR");
    return true;
}
bool Career::tourStart(std::string* why) {
    tourRefresh();
    if (!tourAvailable(why)) return false;
    if (tourRound == 0) money -= tourEntry();
    return true;
}
long Career::recordTour(bool won) {
    ++races;
    if (!won) { tourOut = true; form = std::max(-3, form - 1); loanRace(); return 0; }
    ++wins; form = std::min(3, form + 1);
    dailyAdd(TaskType::WinDrag, 1); dailyAdd(TaskType::WinAny, 1);
    loanRace();
    if (++tourRound < kTourRounds) {                                   // ara tur: odul her turda buyur (%10, %20 ...)
        const long r = tourPrize() * tourRound / 10 / 10 * 10;
        money += r; earnings += r;
        return r;
    }
    const long p = tourPrize();
    money += p; earnings += p; rep += 40;
    dailyAdd(TaskType::Earn, p);
    return p;
}

Opponent Career::pickOpponentFor(uint32_t seed, double etOffset) const {
    const OwnedCar& oc = car();
    const VehicleDef& pv = *findVehicle(oc.carId);
    const double pe = estimatedEt(pv, oc.tune);
    uint32_t x = seed * 2654435761u + 0x9E3779B9u; x ^= x >> 15;
    const double jitter = ((x >> 8) % 1000) / 1000.0 * 0.16 - 0.08;
    const double target = pe + 0.05 - 0.10 * std::clamp(form, -3, 3) + jitter + etOffset;   // rakip yarista hata payli (DragRace)
    struct Cand { int id, preset; double et; };
    static std::vector<Cand> all;
    if (all.empty())
        for (const auto& v : vehicleCatalog()) {
            if (!v.streetLegal) continue;
            for (int k = 0; k < kOpponentPresets; ++k) if (const double e = tableEt(v.id, k); e > 0) all.push_back({v.id, k, e});
        }
    if (all.empty()) { Opponent o = pickOpponent(oc.carId, oc.tune, seed); o.playerEt = pe; return o; }   // tablo yok
    std::vector<const Cand*> pool;
    for (double band : {0.15, 0.3, 0.6, 1.2, 1e9}) {
        pool.clear();
        for (const Cand& c : all)
            if (c.id != oc.carId && c.id != lastOppId && std::fabs(c.et - target) <= band) pool.push_back(&c);
        if (pool.size() >= 6) break;
    }
    if (pool.empty()) return {oc.carId, Tune{}, performanceIndex(pv, oc.tune), pe, pe};
    const Cand& c = *pool[(x >> 3) % pool.size()];
    const Tune t = opponentPreset(c.preset);
    return {c.id, t, performanceIndex(*findVehicle(c.id), t), c.et, pe};
}

std::string tuneSummary(const Tune& t) {
    std::string s;
    auto add = [&](const std::string& p) { if (!s.empty()) s += ' '; s += p; };
    if (t.engineSwap > 0 && t.engineSwap <= (int)swapEngines().size()) add(std::string("SWAP:") + engineTable()[swapEngines()[t.engineSwap - 1]].code);
    if (t.tires == TireType::DragSlick) add("SLICK"); else if (t.tires == TireType::SemiSlick) add("YARI-SLICK"); else add("SOKAK");
    if (t.turbo) add(t.turbo == 1 ? "TURBO-K" : t.turbo == 2 ? "TURBO-B" : "TURBO");
    if (t.superch) add("KOMPRESOR");
    if (t.nitrous) add("NOS");
    if (t.clutch) add(t.clutch == 1 ? "ST1" : t.clutch == 2 ? "ST2" : t.clutch == 3 ? "ST3" : "DEBR+");
    if (t.axles) add(t.axles == 1 ? "KM-AKS" : t.axles == 2 ? "300M" : "AKS+");
    if (t.diff != DiffType::Open) add(t.diff == DiffType::OneAndHalfWay ? "1.5W" : t.diff == DiffType::TwoWay ? "2W" : t.diff == DiffType::Spool ? "SPOOL" : "LSD");
    if (t.ecu || ecuNewSystem(t)) add("ECU");
    if (t.intake || t.exhaust) add("EMME/EGZ");
    if (t.weight) add("HAFIF");
    if (t.drySump) add("KURU-KRT");
    if (t.fuel == FuelType::E85) add("E85");
    int more = 0;                                                  // diger parcalar
    for (int x : {t.cam, t.valve, t.head, t.piston, t.rod, t.crank, t.bearing, t.gasket, t.flywheel, t.turbine, t.wastegate,
                  t.boostCtl, t.intercooler, t.fuelSys, t.fuelPump, t.injector, t.fuelLine, t.gbStrength, t.throttleBody, t.intakeMani,
                  t.header, t.catalyst, t.meth, t.headStud, t.mainSupport, t.brakeDisc, t.brakeCaliper, t.weightBody, t.weightGlass,
                  t.weightChassis, t.aeroFront, t.aeroSide, t.aeroUnder, t.fan, t.coolMisc, t.oilCooler, t.oilPump, t.gearSwap, t.finalSel, t.rims, t.susp, t.brakes, t.aero, t.cooling})
        more += x > 0;
    if (more) add("+" + std::to_string(more) + " PARCA");
    return s;
}

// ------------------------------------------------------------------ kariyer islemleri
Career Career::newGame() {
    Career c;
    c.money = 6000;
    OwnedCar start; start.carId = 217;          // Bursa Sahin 1.6 (Tofas Sahin): mutevazi yerli baslangic
    c.cars.push_back(start);
    c.current = 0;
    return c;
}

long Career::slotPrice() const {
    double p = 4000.0;
    for (int i = kStartSlots; i < garageSlots; ++i) p *= 1.6;
    return (long)(p / 100.0) * 100;
}
bool Career::buySlot(std::string* why) {
    if (garageSlots >= kMaxSlots) { if (why) *why = "GARAJ EN BUYUK HALINDE"; return false; }
    const long p = slotPrice();
    if (money < p) { if (why) *why = "PARA YETMIYOR"; return false; }
    money -= p;
    ++garageSlots;
    return true;
}

bool Career::buyCar(int carId, std::string* why) {
    const VehicleDef* v = findVehicle(carId);
    if (!v) { if (why) *why = "ARAC YOK"; return false; }
    if (garageFull()) { if (why) *why = "GARAJ DOLU"; return false; }
    const int p = carPrice(*v);
    if (money < p) { if (why) *why = "PARA YETMIYOR"; return false; }
    money -= p;
    OwnedCar oc; oc.carId = carId; oc.km = 0.0;
    cars.push_back(oc);
    current = (int)cars.size() - 1;
    return true;
}

int Career::barnCar(int city) {
    // Eski (1995 oncesi) sokak araclarindan en degerli 10'u; sehir sirasiyla
    std::vector<std::pair<int, int>> v;
    for (const VehicleDef& d : vehicleCatalog()) if (d.streetLegal && d.year < 1995) v.push_back({carPrice(d), d.id});
    std::sort(v.rbegin(), v.rend());
    return v.empty() ? 1 : v[std::min((size_t)std::clamp(city, 0, kCities - 1), v.size() - 1)].second;
}

bool Career::claimBarn(int c, std::string* why) {
    if ((barnFound >> c) & 1u) { if (why) *why = "BU SEHRIN BULGUSU ALINDI"; return false; }
    if (garageFull()) { if (why) *why = "GARAJ DOLU"; return false; }
    OwnedCar oc; oc.carId = barnCar(c); oc.fromJunk = true; oc.engineWear = 0.55;
    oc.km = std::round(typicalKm(*findVehicle(oc.carId)) * 0.6 / 100.0) * 100.0;   // az km, uzun sure beklemis
    oc.tune.wearTires = 0.9; oc.tune.wearBrakes = 0.7; oc.tune.wearSusp = 0.6; oc.tune.wearBody = 0.5; oc.tune.wearElec = 0.6;
    cars.push_back(oc);
    current = (int)cars.size() - 1;
    barnFound |= 1u << c;
    return true;
}

bool Career::buyUsed(const UsedListing& l, std::string* why) {
    if (!findVehicle(l.carId)) { if (why) *why = "ARAC YOK"; return false; }
    if (garageFull()) { if (why) *why = "GARAJ DOLU"; return false; }
    if (money < l.price) { if (why) *why = "PARA YETMIYOR"; return false; }
    money -= l.price;
    cars.push_back(listingCar(l));
    current = (int)cars.size() - 1;
    return true;
}

bool Career::sellCurrent(std::string* why) {
    if (cars[current].jobHp > 0) { if (why) *why = "MUSTERI ARACI SATILAMAZ"; return false; }
    if (cars.size() <= 1) { if (why) *why = "SON ARACINI SATAMAZSIN"; return false; }
    money += sellPrice(cars[current]);
    cars.erase(cars.begin() + current);
    current = std::min(current, (int)cars.size() - 1);
    return true;
}

int ecuSwPrice(int sw, int level, const VehicleDef& v) {
    if (level < 1 || level > 80) return 0;
    const double scale = std::clamp(0.6 + carPrice(v) / 40000.0, 0.6, 4.0);
    const int base = ecuSwDef(sw).price[std::min(level - 1, 9)];
    return (int)(std::round((base > 0 ? base : ecuSwDef(sw).price[0]) * scale / 10.0) * 10.0);
}

bool Career::buyEcuSoftware(int sw, std::string* why) {
    OwnedCar& oc = car();
    const VehicleDef& v = *findVehicle(oc.carId);
    Tune& t = oc.tune;
    auto no = [&](const char* m) { if (why) *why = m; return false; };
    const int lv = ecuSwLevel(t, sw), mx = ecuSwMax(t, sw);
    if (mx <= 0) return no("ECU DESTEKLEMIYOR");
    if (lv >= mx) return no("ECU SINIRINDA");
    if (lv == 0 && ecuSlotsUsed(t) >= ecuHwTable()[std::clamp(t.ecuHw, 0, 9)].slots) return no("YAZILIM YUVASI DOLU");
    const int p = ecuSwPrice(sw, lv + 1, v);
    if (money < p) return no("PARA YETMIYOR");
    if (!ecuNewSystem(t) && t.ecu > 0) migrateTune(t);              // eski paket: once yeni sisteme cevrilir
    money -= p;
    oc.paidParts += p;
    t.ecu = 0;
    setEcuSwLevel(t, sw, lv + 1);
    dailyAdd(TaskType::BuyParts, 1);
    return true;
}
void Career::dropEcuSoftware(int sw) {
    Tune& t = car().tune;
    setEcuSwLevel(t, sw, ecuSwLevel(t, sw) - 1);
}

// Eski kayit: tek "yakit sistemi" secenegi -> pompa + enjektor; tek ECU paketi -> donanim + yazilim modulleri
void migrateTune(Tune& t) {
    if (t.fuelSys > 0 && t.fuelPump == 0 && t.injector == 0) {
        static const int pump[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}, inj[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        const int f = std::clamp(t.fuelSys, 0, 9);
        t.fuelPump = pump[f]; t.injector = inj[f]; t.fuelLine = f == 9 ? 9 : 0;
        t.fuelSys = 0;
    }
    if (t.partsVer < 2) {
        // Emme: 6 buyuk kelebek, 7 portlu manifold, 8 karbon airbox, 9 ITB
        switch (t.intake) {
        case 6: t.intake = 0; t.throttleBody = 4; break;
        case 7: t.intake = 0; t.intakeMani = 1; break;
        case 8: t.intake = 6; t.throttleBody = 1; break;
        case 9: t.intake = 0; t.intakeMani = 8; break;
        default: break;
        }
        // Egzoz: 5-7 header, 8 katalizor iptal, 9 titanyum (tam sistem)
        switch (t.exhaust) {
        case 5: t.exhaust = 0; t.header = 1; break;
        case 6: t.exhaust = 0; t.header = 2; break;
        case 7: t.exhaust = 0; t.header = 3; break;
        case 8: t.exhaust = 0; t.catalyst = 4; break;
        case 9: t.exhaust = 9; t.header = 3; break;
        default: break;
        }
        // Sogutma: 4-5 fan, 6-7 termostat / katki, 8-9 radyator + fan
        switch (t.cooling) {
        case 4: t.cooling = 0; t.fan = 1; break;
        case 5: t.cooling = 0; t.fan = 2; break;
        case 6: t.cooling = 0; t.coolMisc = 1; break;
        case 7: t.cooling = 0; t.coolMisc = 2; break;
        case 8: t.cooling = 5; t.fan = 2; break;
        case 9: t.cooling = 7; t.fan = 5; break;
        default: break;
        }
        {   // Fren: balata / disk / kaliper
            static const int pad[10] = {0, 1, 0, 2, 0, 0, 0, 5, 0, 6}, disc[10] = {0, 0, 1, 0, 0, 0, 6, 5, 7, 8}, cal[10] = {0, 0, 0, 0, 1, 2, 0, 2, 2, 4};
            const int b = std::clamp(t.brakes, 0, 9);
            t.brakes = pad[b]; t.brakeDisc = disc[b]; t.brakeCaliper = cal[b];
        }
        {   // Hafifletme: paket -> ic / kaporta / cam / sasi
            static const int in[12] = {0, 2, 3, 5, 1, 2, 3, 3, 5, 5, 5, 5}, bd[12] = {0, 1, 3, 3, 0, 0, 0, 4, 3, 5, 6, 6};
            static const int gl[12] = {0, 0, 0, 1, 0, 0, 0, 2, 4, 4, 4, 4}, ch[12] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 4};
            const int w = std::clamp(t.weight, 0, 11);
            t.weight = in[w]; t.weightBody = bd[w]; t.weightGlass = gl[w]; t.weightChassis = ch[w];
        }
        // Aero: 1 on lip, 2 yan etek, 7 difuzor ayri kategoriye
        if (t.aero == 1) { t.aero = 0; t.aeroFront = 1; }
        else if (t.aero == 2) { t.aero = 0; t.aeroSide = 1; }
        else if (t.aero == 7) { t.aero = 0; t.aeroUnder = 2; }
        // Karter: 3 yag sogutucu, 4 akumulator, 5 genis, 6 yuksek hacimli pompa, 7-9 kuru karter / yaris
        switch (t.oil) {
        case 3: t.oil = 0; t.oilCooler = 2; break;
        case 4: t.oil = 0; t.oilPump = 3; break;
        case 5: t.oil = 3; break;
        case 6: t.oil = 0; t.oilPump = 1; break;
        case 7: t.oil = 6; break;
        case 8: t.oil = 7; break;
        case 9: t.oil = 8; break;
        default: break;
        }
        // Intercooler: 7 metanol, 8 su + metanol, 9 CO2 sprey -> su / metanol
        if (t.intercooler >= 7) { t.meth = t.intercooler == 7 ? 2 : t.intercooler == 8 ? 3 : 5; t.intercooler = 0; }
        // Conta: 8 ARP saplama, 9 saplama + MLS
        if (t.gasket == 8) { t.gasket = 0; t.headStud = 3; }
        else if (t.gasket == 9) { t.gasket = 3; t.headStud = 1; }
        {   // Yatak: 5-9 ana yatak destegi
            static const int brg[10] = {0, 1, 2, 3, 4, 1, 1, 1, 2, 3}, sup[10] = {0, 0, 0, 0, 0, 2, 1, 3, 4, 5};
            const int b = std::clamp(t.bearing, 0, 9);
            t.bearing = brg[b]; t.mainSupport = sup[b];
        }
        t.partsVer = 2;
    }
    if (t.ecu > 0 && !ecuNewSystem(t)) {
        struct M { int hw, map, rev, lc, flat, al, flex; };
        static const M m[10] = {{0, 0, 0, 0, 0, 0, 0}, {0, 1, 0, 0, 0, 0, 0}, {4, 2, 0, 0, 0, 0, 0}, {5, 3, 0, 0, 0, 0, 0},
                                {6, 3, 1, 0, 0, 0, 0}, {4, 1, 2, 0, 0, 0, 0}, {2, 1, 0, 0, 0, 0, 1}, {6, 1, 0, 0, 0, 1, 0},
                                {4, 1, 0, 1, 1, 0, 0}, {8, 3, 2, 0, 0, 0, 0}};
        const M& x = m[std::clamp(t.ecu, 0, 9)];
        t.ecuHw = x.hw; t.swMap = x.map; t.swRev = x.rev * 5; t.swLaunch = x.lc; t.swFlat = x.flat; t.swAntiLag = x.al; t.swFlex = x.flex;
        t.ecu = 0;
        clampEcuSoftware(t);
    }
}

bool usedAvailable(PartCat c, int level) {
    if (partTab(c) == 3 || level <= 0 || level == customOption(c)) return false;   // aktarma: ikinci el yok
    return true;
}
int usedPrice(int newPrice) { return newPrice * 55 / 100 / 10 * 10; }
// Parca pazari: kategori fiyati haftadan haftaya %80-%120 arasi dalgalanir (%5 adim; ayni hafta herkes icin ayni)
double marketMul(PartCat c, int week) {
    if (week < 0) return 1.0;
    uint32_t x = (uint32_t)week * 2654435761u ^ ((uint32_t)c + 1u) * 40503u;
    x ^= x >> 13; x *= 0x5bd1e995u; x ^= x >> 15;
    return 0.80 + 0.05 * (int)(x % 9u);
}
int Career::marketWeek() const { return marketOff ? -1 : todayIndex() / 7; }
int Career::shopPrice(PartCat c, int level) const {
    const int p = partPrice(c, level, *findVehicle(car().carId));
    return p <= 0 ? p : (int)std::round(p * marketMul(c, marketWeek()) / 10.0) * 10;
}
void applyUsedWear(Tune& t, PartCat c) {
    auto add = [](double& w, double d) { w = std::max(w, d); };   // birikmez: ikinci el parca en fazla kendi yipranmasini getirir
    switch (c) {
    case PartCat::Tires: t.wearTires = std::max(t.wearTires, 0.40); return;          // yarim dis
    case PartCat::Brakes: case PartCat::BrakeDisc: case PartCat::BrakeCaliper: add(t.wearBrakes, 0.25); return;
    case PartCat::Suspension: add(t.wearSusp, 0.20); return;
    case PartCat::Rims: add(t.wearBody, 0.08); return;
    default: break;
    }
    switch (partTab(c)) {
    case 0: add(t.wearEngine, 0.06); break;                                       // motor ic aksami
    case 1: case 2: add(t.wearEngine, 0.04); break;                               // emme / egzoz, turbo / yakit
    case 5: add(t.wearBody, 0.08); break;                                         // kaporta: hafifletme / aero
    case 6: add(t.wearElec, 0.08); break;                                         // ecu / sogutma / yag
    default: break;
    }
}

// Kurulum profili: ayarlanabilir alanlar (lastik basinci, suspansiyon, LSD, NOS memesi, kalkis devri, dyno ECU ayari)
std::string setupString(const Tune& t) {
    char b[160];
    std::snprintf(b, sizeof b, "%.1f;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d", t.psi, t.setRide, t.setSpring, t.setDamp, t.setArbF, t.setArbR,
                  t.setPreload, t.setNos, t.launchRpm, t.ecuTiming, t.ecuAfr, t.ecuBoost, t.setRideR, t.setSpringR, t.setDampR);
    return b;
}
bool applySetupString(const std::string& s, Tune& t) {
    Tune n = t;
    const int got = std::sscanf(s.c_str(), "%lf;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d", &n.psi, &n.setRide, &n.setSpring, &n.setDamp, &n.setArbF, &n.setArbR,
                    &n.setPreload, &n.setNos, &n.launchRpm, &n.ecuTiming, &n.ecuAfr, &n.ecuBoost, &n.setRideR, &n.setSpringR, &n.setDampR);
    if (got != 12 && got != 15) return false;                          // eski profil: arka = on yok sayilir (0)
    n.psi = std::clamp(n.psi, 0.0, 45.0);
    n.ecuTiming = std::clamp(n.ecuTiming, -4, 6); n.ecuAfr = n.ecuAfr ? std::clamp(n.ecuAfr, 115, 135) : 0; n.ecuBoost = std::clamp(n.ecuBoost, -3, 5);
    n.launchRpm = std::clamp(n.launchRpm, 0, 12000);
    clampSetup(n);                                                     // o anki parcalarin izin verdigi kadar
    t = n;
    return true;
}
// NOS tupu: yarista harcanan kalir; dolum kit fiyatinin %12'si x bos kisim
void Career::recordNosUse(double leftFrac) { if (car().tune.nitrous > 0) car().nosFill = std::clamp(leftFrac, 0.0, 1.0); }
int Career::nosRefillPrice() const {
    const OwnedCar& oc = car();
    if (oc.tune.nitrous <= 0 || oc.nosFill >= 0.999) return 0;
    return std::max(10, (int)std::round(partPrice(PartCat::Nitrous, oc.tune.nitrous, *findVehicle(oc.carId)) * 0.12 * (1.0 - oc.nosFill) / 10.0) * 10);
}
bool Career::refillNos(std::string* why) {
    const int p = nosRefillPrice();
    if (p <= 0) { if (why) *why = "TUP DOLU"; return false; }
    if (money < p) { if (why) *why = "PARA YETMIYOR"; return false; }
    money -= p; car().nosFill = 1.0;
    return true;
}

void Career::saveProfile(int k) { if (k >= 0 && k < 3) car().profile[k] = setupString(car().tune); }
bool Career::loadProfile(int k) { return k >= 0 && k < 3 && !car().profile[k].empty() && applySetupString(car().profile[k], car().tune); }

bool Career::buyPart(PartCat c, int level, std::string* why, bool used, long* refund) {
    OwnedCar& oc = car();
    const VehicleDef& v = *findVehicle(oc.carId);
    if (refund) *refund = 0;
    if (level < 0 || level >= (int)partOptions(c).size()) { if (why) *why = "GECERSIZ"; return false; }
    if (partLevel(oc.tune, c, v) == level) { if (why) *why = "ZATEN TAKILI"; return false; }
    if (!partAvailable(c, level, v, why, &oc.tune)) return false;
    if (used && !usedAvailable(c, level)) { if (why) *why = "IKINCI EL YOK"; return false; }
    const int p = used ? usedPrice(shopPrice(c, level)) : shopPrice(c, level);   // haftalik pazar fiyati
    if (money < p) { if (why) *why = "PARA YETMIYOR"; return false; }
    const int old = partLevel(oc.tune, c, v);
    const int oldPrice = old > 0 && old != customOption(c) ? partPrice(c, old, v) : 0;
    money -= p;
    oc.paidParts += p;
    if (oldPrice > 0) {                                                       // sokulen parca satilir
        const long back = (long)oldPrice * 35 / 100 / 10 * 10;
        money += back;
        oc.paidParts = std::max(0, oc.paidParts - oldPrice);
        if (refund) *refund = back;
    }
    setPartLevel(oc.tune, c, level, v);
    clampSetup(oc.tune);                                               // parca degisti: gecersiz kurulum ayari sifirlanir
    if (c == PartCat::Nitrous) oc.nosFill = 1.0;                       // yeni kit dolu gelir
    if (c == PartCat::EngineSwap && level > 0) { oc.engineWear = 0.0; oc.tune.wearEngine = 0.0; }   // yeni motor: eski motorun hasari gider
    if (used) applyUsedWear(oc.tune, c);
    dailyAdd(TaskType::BuyParts, 1);
    return true;
}

// ------------------------------------------------------------------ ligler
int Career::leagueWins(int league) const {
    int n = 0;
    const auto& ev = leagueEvents();
    for (size_t i = 0; i < ev.size(); ++i) n += ev[i].league == league && eventWon((int)i);
    return n;
}

int Career::leagueUnlocked() const {
    const auto& ev = leagueEvents();
    int l = 0;
    for (int L = 0; L < kLeagues - 1; ++L) {
        bool boss = false;
        for (size_t i = 0; i < ev.size(); ++i)
            if (ev[i].league == L && ev[i].rival >= 0 && rivals()[ev[i].rival].boss && eventWon((int)i)) boss = true;
        if (!boss) break;
        l = L + 1;
    }
    return l;
}

bool Career::eventAvailable(int idx, std::string* why) const {
    auto no = [&](const std::string& m) { if (why) *why = m; return false; };
    const auto& ev = leagueEvents();
    if (idx < 0 || idx >= (int)ev.size()) return no("GECERSIZ");
    const EventDef& e = ev[idx];
    if (e.league > leagueUnlocked()) return no("ONCEKI LIGIN PATRONUNU YEN");
    const bool boss = e.rival >= 0 && rivals()[e.rival].boss;
    if (boss && leagueWins(e.league) < kBossUnlockWins) return no("PATRON ICIN 3 GALIBIYET GEREK");
    if (e.pink && cars.size() < 2) return no("PINK SLIP ICIN 2 ARAC GEREK");
    if (e.pink && garageFull()) return no("PINK SLIP ICIN GARAJDA YER GEREK");
    if (e.pink && eventWon(idx)) return no("ARABASINI ZATEN ALDIN");
    if (!car().raceable()) return no(car().jobHp ? "MUSTERI ARACIYLA OLMAZ" : "ARAC HASARLI - TAMIR");
    const double cap = leagueIndexCap(e.league);
    if (cap > 0 && performanceIndex(*findVehicle(car().carId), car().tune) > cap) {
        char b[48]; std::snprintf(b, sizeof b, "SINIF ASIMI (ENDEKS > %.0f)", cap);
        return no(b);
    }
    return true;
}

const std::vector<SponsorDef>& sponsors() {
    static const std::vector<SponsorDef> s = {
        // ad, en dusuk acik lig, galibiyet, yaris hakki, galibiyet primi, bonus, ceza
        {"SANAYI YAG", 0, 3, 5, 150, 1200, 600},
        {"KOSE MARKET", 0, 5, 8, 200, 2000, 1000},
        {"TURBO LASTIK", 1, 4, 6, 450, 5000, 2500},
        {"GECE RADYO", 1, 6, 9, 500, 7500, 3500},
        {"OTOBAN PETROL", 2, 5, 8, 1000, 14000, 7000},
        {"KARBON PARCA", 3, 5, 7, 1800, 26000, 13000},
        {"PIST ENERJI", 4, 6, 8, 3500, 60000, 30000},
    };
    return s;
}

int Career::sponsorOffer() const {
    if (sponsor >= 0) return -1;
    const auto& s = sponsors();
    int best = -1;                                                    // acik ligdeki en iyi, bitirilmemis sozlesme
    for (int i = 0; i < (int)s.size(); ++i)
        if (!(sponsorsDone >> i & 1) && s[i].league <= leagueUnlocked()) best = i;
    return best;
}

bool Career::signSponsor(int s) {
    if (s < 0 || s != sponsorOffer()) return false;
    sponsor = s; spWins = spRaces = 0;
    sponsorNote = std::string("SPONSOR: ") + sponsors()[s].name;
    return true;
}

void Career::sponsorRace(bool won) {
    if (sponsor < 0) return;
    const SponsorDef& d = sponsors()[sponsor];
    ++spRaces;
    if (won) { ++spWins; money += d.perWin; earnings += d.perWin; }
    char b[96];
    if (spWins >= d.wins) {
        money += d.bonus; earnings += d.bonus; sponsorsDone |= 1u << sponsor;
        std::snprintf(b, sizeof b, "%s SOZLESMESI TAMAM! +$%ld", d.name, d.bonus);
        sponsor = -1; sponsorNote = b;
    } else if (spRaces >= d.races) {
        money = std::max(0L, money - d.penalty);
        std::snprintf(b, sizeof b, "%s SOZLESMESI BOZULDU: -$%ld", d.name, d.penalty);
        sponsor = -1; sponsorNote = b;
    }
}

long Career::eventPrize(int idx, bool first) const {
    const auto& ev = leagueEvents();
    if (idx < 0 || idx >= (int)ev.size()) return 0;
    static const double kLeagueMul[kLeagues] = {1.0, 1.0, 1.0, 1.25, 1.5};   // ust ligde parca / arac pahali
    const long p = (long)std::lround(ev[idx].prize * kLeagueMul[std::clamp(ev[idx].league, 0, kLeagues - 1)] / 10.0) * 10;
    return first ? p : p * 4 / 10;
}

long Career::wagerFor(int idx, int step) const {
    static const double k[kWagerSteps] = {0.0, 0.5, 1.0, 2.0, 3.0};
    const long w = (long)(eventPrize(idx, true) * k[std::clamp(step, 0, kWagerSteps - 1)]) / 10 * 10;
    return std::min(w, money);                                         // parandan fazlasina oynanmaz
}

long Career::recordEvent(int idx, bool won, double et, long flowScore, int* pinkOut) {
    if (pinkOut) *pinkOut = 0;
    const auto& ev = leagueEvents();
    if (idx < 0 || idx >= (int)ev.size()) return 0;
    const EventDef& e = ev[idx];
    if (e.mode == EventMode::Flow) won = flowScore >= e.flowTarget;
    const bool first = !eventWon(idx);
    long prize = 0;
    ++races; ++car().races;
    if (won) {
        ++wins; ++car().wins;
        prize = eventPrize(idx, first);
        rep += first ? e.rep : std::max(1, e.rep * 3 / 10);
        if (idx < 64) eventWins |= (uint64_t)1 << idx; else eventWins2 |= (uint64_t)1 << (idx - 64);
        form = std::clamp(form + 1, -3, 3);
    } else {
        rep = std::max(0, rep - e.rep / 4);
        form = std::clamp(form - 1, -3, 3);
    }
    if (et > 0 && (car().bestEt <= 0 || et < car().bestEt)) car().bestEt = et;
    // Pink slip: kazanan rakibin arabasini (parcalariyla) alir; kaybeden kendi arabasini verir
    if (e.pink && e.rival >= 0) {
        const RivalDef& r = rivals()[e.rival];
        if (won) {
            OwnedCar oc; oc.carId = r.carId; oc.tune = opponentPreset(r.preset);
            cars.push_back(oc);
            if (pinkOut) *pinkOut = r.carId;
        } else if (cars.size() > 1) {
            if (pinkOut) *pinkOut = -car().carId;
            cars.erase(cars.begin() + current);
            current = std::min(current, (int)cars.size() - 1);
        }
    }
    money += prize; earnings += prize;
    // Bahis: kazanirsa bahis kadar ek, kaybederse bahis kadar kayip (pink slip'te bahis yok)
    long net = prize;
    if (wager > 0 && !e.pink) {
        const long w = wager;
        if (won) { money += w; earnings += w; net += w; }
        else { money -= std::min(w, money); net -= w; }
    }
    wager = 0;
    if (!e.pink) sponsorRace(won);
    loanRace();
    // Gunluk gorevler
    if (won) {
        dailyAdd(TaskType::WinAny, 1);
        dailyAdd(e.mode == EventMode::Drag ? TaskType::WinDrag : TaskType::WinRoad, 1);
    }
    if (et > 0) dailyAdd(TaskType::EtUnder, (long)std::round(et * 100.0));
    if (e.mode == EventMode::Flow) dailyAdd(TaskType::FlowScore, flowScore);
    if (net > 0) dailyAdd(TaskType::Earn, net);
    return net;
}

void Career::dailyRefresh() {
    const int d = todayIndex();
    if (d == dailyDay) return;
    dailyDay = d; dailyDone = 0;
    for (long& p : dailyProg) p = 0;
}

long Career::dailyAdd(TaskType t, long amount) {
    dailyRefresh();
    const auto tasks = dailyTasks(dailyDay, cars.empty() ? 15.0 : estimatedEt(*findVehicle(car().carId), car().tune));
    long paid = 0;
    for (int i = 0; i < 3; ++i) {
        if (tasks[i].type != t || ((dailyDone >> i) & 1)) continue;
        bool done;
        if (t == TaskType::EtUnder) { if (dailyProg[i] == 0 || amount < dailyProg[i]) dailyProg[i] = amount; done = dailyProg[i] <= tasks[i].target; }
        else if (t == TaskType::FlowScore) { dailyProg[i] = std::max(dailyProg[i], amount); done = dailyProg[i] >= tasks[i].target; }
        else { dailyProg[i] += amount; done = dailyProg[i] >= tasks[i].target; }
        if (done) { dailyDone |= 1 << i; money += tasks[i].reward; earnings += tasks[i].reward; rep += tasks[i].rep; paid += tasks[i].reward; }
    }
    return paid;
}

void Career::recordRace(const VehicleDef& opponent, bool won, double et, long* prizeOut, double prizeScale) {
    // Ayni rakibe ust uste galibiyet: odul %25 azalir (en az %25)
    if (opponent.id != lastOppId) { lastOppId = opponent.id; sameOppWins = 0; }
    const double decay = won ? std::max(0.25, 1.0 - 0.25 * sameOppWins) : 1.0;
    const long prize = (long)(racePrize(opponent, won) * decay * prizeScale);
    if (won) ++sameOppWins;
    form = std::clamp(form + (won ? 1 : -1), -3, 3);
    money += prize; earnings += prize;
    ++races; if (won) ++wins;
    OwnedCar& oc = car();
    ++oc.races; if (won) ++oc.wins;
    if (et > 0 && (oc.bestEt <= 0 || et < oc.bestEt)) oc.bestEt = et;
    if (prizeOut) *prizeOut = prize;
    if (won) { dailyAdd(TaskType::WinAny, 1); dailyAdd(et > 0 ? TaskType::WinDrag : TaskType::WinRoad, 1); rep += 2; }
    sponsorRace(won);
    loanRace();
    if (et > 0) dailyAdd(TaskType::EtUnder, (long)std::round(et * 100.0));
    if (prize > 0) dailyAdd(TaskType::Earn, prize);
}

// ------------------------------------------------------------------ kayit
namespace {
struct IntField { const char* key; int Tune::*f; int max; int min = 0; };
struct DblField { const char* key; double Tune::*f; double max; };
const IntField kIntFields[] = {
    {"tire", &Tune::tireSel, 29}, {"rim", &Tune::rims, 9}, {"fin", &Tune::finalSel, 11}, {"gbx", &Tune::gearSwap, 40},
    {"eng", &Tune::engineSwap, 600}, {"cam", &Tune::cam, 11}, {"val", &Tune::valve, 9}, {"fly", &Tune::flywheel, 9},
    {"sc", &Tune::superch, 9}, {"ic", &Tune::intercooler, 9}, {"fsy", &Tune::fuelSys, 9}, {"nos", &Tune::nitrous, 9},
    {"head", &Tune::head, 9}, {"pis", &Tune::piston, 9}, {"rod", &Tune::rod, 9}, {"crk", &Tune::crank, 9},
    {"brg", &Tune::bearing, 9}, {"gsk", &Tune::gasket, 9}, {"trb", &Tune::turbine, 9}, {"wg", &Tune::wastegate, 9},
    {"bc", &Tune::boostCtl, 9}, {"gbs", &Tune::gbStrength, 9}, {"cool", &Tune::cooling, 9}, {"oil", &Tune::oil, 9},
    {"fuel", &Tune::fuelSel, 9}, {"elx", &Tune::elec, 9}, {"sus", &Tune::susp, 9}, {"brk", &Tune::brakes, 9}, {"aero", &Tune::aero, 9},
    {"lrpm", &Tune::launchRpm, 12000}, {"rtir", &Tune::roadTire, 29}, {"adas", &Tune::adas, 4},
    {"fpmp", &Tune::fuelPump, 9}, {"inj", &Tune::injector, 9}, {"fln", &Tune::fuelLine, 9},
    {"ehw", &Tune::ecuHw, 9}, {"smap", &Tune::swMap, 3}, {"srv", &Tune::swRev, 80}, {"slc", &Tune::swLaunch, 1},
    {"sfs", &Tune::swFlat, 1}, {"sal", &Tune::swAntiLag, 1}, {"sflx", &Tune::swFlex, 1}, {"skn", &Tune::swKnock, 1}, {"stcu", &Tune::swTcu, 1},
    {"pv", &Tune::partsVer, 9}, {"thr", &Tune::throttleBody, 9}, {"imf", &Tune::intakeMani, 9}, {"hdr", &Tune::header, 9},
    {"cat", &Tune::catalyst, 5}, {"meth", &Tune::meth, 5}, {"stud", &Tune::headStud, 4}, {"msup", &Tune::mainSupport, 5},
    {"bdsc", &Tune::brakeDisc, 8}, {"bcal", &Tune::brakeCaliper, 5}, {"wbdy", &Tune::weightBody, 6}, {"wgls", &Tune::weightGlass, 4},
    {"wch", &Tune::weightChassis, 4}, {"aef", &Tune::aeroFront, 4}, {"aes", &Tune::aeroSide, 4}, {"aeu", &Tune::aeroUnder, 4},
    {"fan", &Tune::fan, 5}, {"cmsc", &Tune::coolMisc, 4}, {"oclr", &Tune::oilCooler, 4}, {"opmp", &Tune::oilPump, 4},
    {"etim", &Tune::ecuTiming, 6, -4}, {"eafr", &Tune::ecuAfr, 135, 0}, {"ebst", &Tune::ecuBoost, 5, -3},
    {"kri", &Tune::setRide, 20, -40}, {"ksp", &Tune::setSpring, 5, -5}, {"kdm", &Tune::setDamp, 5, -5}, {"karf", &Tune::setArbF, 3, -3},
    {"karr", &Tune::setArbR, 3, -3}, {"kpl", &Tune::setPreload, 5, -5}, {"knos", &Tune::setNos, 95},
    {"krr", &Tune::setRideR, 20, -40}, {"kspr", &Tune::setSpringR, 5, -5}, {"kdmr", &Tune::setDampR, 5, -5},
};
const DblField kDblFields[] = {
    {"ctmm", &Tune::custTurboMm, 100}, {"ctar", &Tune::custTurboAr, 1.4}, {"ccam", &Tune::custCamDeg, 330},
    {"cdisp", &Tune::custDisp, 0.4}, {"cfin", &Tune::custFinal, 7.5}, {"cwing", &Tune::custWingN, 1600},
    {"wEng", &Tune::wearEngine, 1}, {"wTir", &Tune::wearTires, 1}, {"wBrk", &Tune::wearBrakes, 1}, {"wSus", &Tune::wearSusp, 1},
    {"wBdy", &Tune::wearBody, 1}, {"wElx", &Tune::wearElec, 1},
};
} // namespace

std::string tuneV2String(const Tune& t) {
    std::string s;
    char b[48];
    for (const IntField& f : kIntFields) if (t.*(f.f)) { std::snprintf(b, sizeof b, "%s:%d,", f.key, t.*(f.f)); s += b; }
    for (const DblField& f : kDblFields) if (t.*(f.f) > 0) { std::snprintf(b, sizeof b, "%s:%.4f,", f.key, t.*(f.f)); s += b; }
    for (int i = 0; i < 8; ++i) if (t.custGear[i] > 0) { std::snprintf(b, sizeof b, "cg%d:%.4f,", i, t.custGear[i]); s += b; }
    if (!s.empty()) s.pop_back();
    return s;
}

void parseTuneV2(const std::string& v, Tune& t) {
    size_t pos = 0;
    while (pos < v.size()) {
        size_t end = v.find(',', pos); if (end == std::string::npos) end = v.size();
        const std::string item = v.substr(pos, end - pos);
        const size_t c = item.find(':');
        if (c != std::string::npos) {
            const std::string k = item.substr(0, c);
            const double x = std::atof(item.c_str() + c + 1);
            for (const IntField& f : kIntFields) if (k == f.key) t.*(f.f) = std::clamp((int)x, f.min, f.max);
            if (k == "srev") t.swRev = std::clamp((int)x * 5, 0, 80);           // eski kayit: 250 rpm adimlari -> 50 rpm
            for (const DblField& f : kDblFields) if (k == f.key) t.*(f.f) = std::clamp(x, 0.0, f.max);
            if (k.size() == 3 && k[0] == 'c' && k[1] == 'g' && k[2] >= '0' && k[2] <= '7') t.custGear[k[2] - '0'] = std::clamp(x, 0.0, 1.5);
        }
        pos = end + 1;
    }
    // Turetilmis alanlar (tablo satirindan)
    if (t.tireSel > 0) t.tires = (TireType)tireTable()[std::min(t.tireSel, (int)tireTable().size() - 1)].type;
    if (t.fuelSel > 0) t.fuel = (FuelType)fuelTable()[std::min(t.fuelSel, (int)fuelTable().size() - 1)].type;
    if (t.elec > 0) { t.absKit = elecTable()[std::min(t.elec, 9)].abs; t.tcKit = elecTable()[std::min(t.elec, 9)].tc; }
    if (t.oil > 0) t.drySump = oilTable()[std::min(t.oil, 9)].dry;
    if (t.engineSwap > (int)swapEngines().size()) t.engineSwap = 0;
}
namespace {
uint32_t fnv1a(const std::string& s) {
    uint32_t h = 2166136261u;
    for (unsigned char ch : s) { h ^= ch; h *= 16777619u; }
    return h;
}
} // namespace

void Career::recordDamage(bool axleBroke, double bearingDamage, bool bearingSpun, bool gearboxBroke, double engineStress, double tireWear) {
    OwnedCar& c = car();
    c.tune.wearTires = std::clamp(c.tune.wearTires + std::max(0.0, tireWear), 0.0, 1.0);   // patinaj / drift lastigi yer
    {   // Olagan asinma (her yaris): motor (asiri besleme / yuksek devir yazilimi hizlandirir), fren, suspansiyon, elektrik.
        // ~120 yarista motor %50 yipranir (guc -%20); restorasyon / bakim ekraninda yenilenir.
        const double boost = (c.tune.turbo > 0 || c.tune.superch > 0 || c.tune.nitrous > 0) ? 1.6 : 1.0;
        const double rev = 1.0 + 0.01 * c.tune.swRev;                   // +250 rpm basina %5 (50 rpm adim)
        auto add = [](double& w, double d) { w = std::clamp(w + d, 0.0, 1.0); };
        add(c.tune.wearEngine, 0.004 * boost * rev);
        add(c.tune.wearBrakes, 0.010);
        add(c.tune.wearSusp, 0.004);
        add(c.tune.wearElec, 0.002);
    }
    c.axleBroken = c.axleBroken || axleBroke;
    c.gearboxBroken = c.gearboxBroken || gearboxBroke;
    c.engineWear = bearingSpun || engineStress >= 1.0 ? 1.0
                 : std::clamp(c.engineWear + std::max(0.0, bearingDamage) + 0.5 * std::max(0.0, engineStress), 0.0, 1.0);
}

long Career::recordChase(bool escaped, int collisions) {
    if (escaped) {
        const long prize = collisions == 0 ? 1500 : 1000;
        money += prize; earnings += prize; ++chaseEscapes;
        dailyAdd(TaskType::Earn, prize);
        return prize;
    }
    const long fine = std::min(money, 400L);                              // ceza: parayi eksiye dusurmez
    money -= fine;
    return -fine;
}

long Career::recordFlow(long score, bool* newRecord) {
    score = std::max(0L, score);
    const bool rec = score > bestFlow && score > 0;
    long prize = std::min(score / 20, kFlowPrizeCap);
    if (rec) { prize += prize / 2; bestFlow = score; }
    prize = prize / 10 * 10;
    money += prize; earnings += prize;
    if (newRecord) *newRecord = rec;
    dailyAdd(TaskType::FlowScore, score);
    if (prize > 0) dailyAdd(TaskType::Earn, prize);
    return prize;
}

long Career::repairCost() const { return repairCostFor(car()); }

std::vector<int> Career::checkAchievements() {
    std::vector<int> got;
    const auto& a = achievements();
    for (int i = 0; i < (int)a.size() && i < 32; ++i) {
        if ((achieved >> i) & 1u) continue;
        if (!achievementMet(*this, i)) continue;
        achieved |= 1u << i;
        money += a[i].reward;                                          // odul kazanc sayilmaz (CEYREK MILYON zinciri olmasin)
        got.push_back(i);
    }
    return got;
}

long repairCostFor(const OwnedCar& c) {
    const VehicleDef& v = *findVehicle(c.carId);
    long cost = 0;
    // Aks: takili seviyenin parca fiyati (stok aks icin 1. seviye fiyatinin yarisi) + iscilik
    if (c.axleBroken) cost += std::max(150L, (long)(c.tune.axles > 0 ? partPrice(PartCat::Axles, c.tune.axles, v)
                                                                       : partPrice(PartCat::Axles, 1, v) / 2)) + 120;
    // Motor: yatak degisimi; tam sarmada (1.0) krank taslama + revizyon ~ arac fiyatinin %18'i
    if (c.engineWear > 0.02) cost += (long)(250 + carPrice(v) * 0.18 * c.engineWear);
    // Sanziman: disli seti + iscilik (guclendirme seviyesi fiyatina gore)
    if (c.gearboxBroken) cost += 600 + partPrice(PartCat::GbStrength, std::max(1, c.tune.gbStrength), v) / 2;
    return cost;
}

// ------------------------------------------------------------------ hurdalik + restorasyon
std::vector<JunkCar> junkyardOffers(uint32_t seed) {
    std::vector<JunkCar> out;
    uint32_t x = seed * 2654435761u + 0x1234567u;
    auto rnd = [&]() { x ^= x << 13; x ^= x >> 17; x ^= x << 5; return (x & 0xFFFFFF) / double(0x1000000); };
    const auto& cat = vehicleCatalog();
    while (out.size() < 6) {
        const VehicleDef& v = cat[(size_t)(rnd() * cat.size())];
        if (!v.streetLegal) continue;
        bool dup = false;
        for (const JunkCar& j : out) dup |= j.carId == v.id;
        if (dup) continue;
        JunkCar j;
        j.carId = v.id;
        // Harbi hurda: her bilesen kotu, cogu bitik; motor ya da sanziman cogu zaman olu
        auto bad = [&](double lo) { return std::min(1.0, lo + (1.0 - lo) * rnd()); };
        j.tune.wearEngine = bad(0.35); j.tune.wearTires = bad(0.55); j.tune.wearBrakes = bad(0.45);
        j.tune.wearSusp = bad(0.40); j.tune.wearBody = bad(0.30); j.tune.wearElec = bad(0.25);
        j.engineWear = rnd() < 0.55 ? 1.0 : bad(0.3);                 // %55 motor yatak sarmis
        j.axleBroken = rnd() < 0.35;
        j.gearboxBroken = rnd() < 0.40;
        const double cond = (j.tune.wearEngine + j.tune.wearTires + j.tune.wearBrakes + j.tune.wearSusp + j.tune.wearBody + j.tune.wearElec) / 6.0;
        const double f = 0.35 - 0.20 * cond - (j.engineWear >= 1.0 ? 0.04 : 0.0) - (j.gearboxBroken ? 0.02 : 0.0);
        j.price = (int)(std::round(carPrice(v) * std::max(0.08, f) / 10.0) * 10.0);
        out.push_back(j);
    }
    return out;
}

bool Career::buyJunk(const JunkCar& j, std::string* why) {
    if (garageFull()) { if (why) *why = "GARAJ DOLU"; return false; }
    if (money < j.price) { if (why) *why = "PARA YETMIYOR"; return false; }
    money -= j.price;
    OwnedCar oc; oc.carId = j.carId; oc.tune = j.tune;
    oc.km = std::round(typicalKm(*findVehicle(j.carId)) * 1.4 / 100.0) * 100.0;   // hurdalik araci: cok km
    oc.engineWear = j.engineWear; oc.axleBroken = j.axleBroken; oc.gearboxBroken = j.gearboxBroken;
    oc.fromJunk = true;
    cars.push_back(oc);
    current = (int)cars.size() - 1;
    return true;
}

const char* restoreName(int comp) {
    static const char* n[kRestoreParts] = {"MOTOR REVIZYONU", "SANZIMAN + AKS", "LASTIKLER", "FRENLER", "SUSPANSIYON",
                                           "KAPORTA + PAS", "ELEKTRIK TESISATI"};
    return n[std::clamp(comp, 0, kRestoreParts - 1)];
}
// Bilesenin bozukluk orani 0..1 (gosterge)
double restoreDamage(const OwnedCar& c, int comp) {
    switch (comp) {
    case 0: return std::max(c.engineWear, c.tune.wearEngine);
    case 1: return (c.gearboxBroken ? 0.6 : 0.0) + (c.axleBroken ? 0.4 : 0.0);
    case 2: return c.tune.wearTires;
    case 3: return c.tune.wearBrakes;
    case 4: return c.tune.wearSusp;
    case 5: return c.tune.wearBody;
    case 6: return c.tune.wearElec;
    default: return 0.0;
    }
}
// Bir adim (tek seferde yariya; lastik ve sanziman/aks tek seferde): fiyat arac degerine ve hasara gore
long restoreStepCost(const OwnedCar& c, int comp) {
    const VehicleDef& v = *findVehicle(c.carId);
    const double d = restoreDamage(c, comp), P = carPrice(v);
    if (d < 0.02) return 0;
    switch (comp) {
    case 0: return (long)(250 + P * (c.engineWear >= 1.0 ? 0.14 : 0.08) * d);
    case 1: return repairCostFor(c) > 0 ? (long)((c.gearboxBroken ? 600 + partPrice(PartCat::GbStrength, std::max(1, c.tune.gbStrength), v) / 2 : 0) +
                                                (c.axleBroken ? std::max(150L, (long)partPrice(PartCat::Axles, std::max(1, c.tune.axles), v) / 2) + 120 : 0)) : 0;
    case 2: return (long)(120 + partPrice(PartCat::Tires, std::max(1, partLevel(c.tune, PartCat::Tires, v)), v) * 0.6 * d + 80);
    case 3: return (long)(90 + P * 0.025 * d);
    case 4: return (long)(150 + P * 0.04 * d);
    case 5: return (long)(300 + P * 0.10 * d);
    case 6: return (long)(120 + P * 0.035 * d);
    default: return 0;
    }
}

bool Career::restoreStep(int comp, std::string* why) {
    OwnedCar& c = car();
    const long cost = restoreStepCost(c, comp);
    if (cost <= 0) { if (why) *why = "SAGLAM"; return false; }
    if (money < cost) { if (why) *why = "PARA YETMIYOR"; return false; }
    money -= cost;
    auto half = [](double& w) { w = w < 0.15 ? 0.0 : w * 0.5; };   // her adim yariya indirir (tam toparlamak icin ugras)
    switch (comp) {
    case 0: if (c.engineWear >= 1.0) c.engineWear = 0.3; else half(c.engineWear); half(c.tune.wearEngine); break;
    case 1: c.gearboxBroken = false; c.axleBroken = false; break;
    case 2: c.tune.wearTires = 0.0; break;                         // yeni lastik
    case 3: half(c.tune.wearBrakes); break;
    case 4: half(c.tune.wearSusp); break;
    case 5: half(c.tune.wearBody); break;
    case 6: half(c.tune.wearElec); break;
    }
    dailyAdd(TaskType::Restore, 1);
    return true;
}

bool Career::repairCurrent(std::string* why) {
    const long cost = repairCost();
    if (cost <= 0) { if (why) *why = "HASAR YOK"; return false; }
    if (money < cost) { if (why) *why = "PARA YETMIYOR"; return false; }
    money -= cost;
    car().axleBroken = false; car().engineWear = 0.0; car().gearboxBroken = false;
    return true;
}

std::string Career::serialize() const {
    std::ostringstream o;
    o << "ZEHRAKINIK_KAYIT " << kVersion << "\n";
    o << "money=" << money << "\ncurrent=" << current << "\nraces=" << races << "\nwins=" << wins
      << "\nearnings=" << earnings << "\ntreePro=" << (treePro ? 1 : 0) << "\nstreak=" << lastOppId << ";" << sameOppWins << "\nflow=" << bestFlow << "\nform=" << form << "\nrep=" << rep << "\nchase=" << chaseEscapes << "\nslots=" << garageSlots << "\ntour=" << tourWeek << ";" << tourRound << ";" << (tourOut ? 1 : 0) << "\nstreet=" << meetDone << ";" << dynoWeek << ";" << jobDay << ";" << jobMask << "\ncity2=" << city << "\nsponsor=" << sponsor << ";" << spWins << ";" << spRaces << ";" << sponsorsDone << "\nloan=" << loan << ";" << loanRaces << ";" << showWeek << "\njobs=" << jobSeq[0] << ";" << jobSeq[1] << ";" << jobSeq[2] << "\n";
    {
        char lb[200];
        std::snprintf(lb, sizeof lb, "evw=%llx\nevw2=%llx\nbarn=%x\ndaily=%d;%ld;%ld;%ld;%d\nach=%x\n", (unsigned long long)eventWins, (unsigned long long)eventWins2, (unsigned)barnFound, dailyDay, dailyProg[0], dailyProg[1], dailyProg[2], dailyDone, (unsigned)achieved);
        o << lb;
        // Acik dunya: radar dedektoru, toplananlar, kamera rekorlari (km/h), sehirlerarasi gecis rekorlari (s)
        o << "owr=" << (radarDetector ? 1 : 0) << ";" << std::hex << collected << std::dec << "\n";
        if (!camBest.empty()) { o << "cams="; for (size_t i = 0; i < camBest.size(); ++i) o << (i ? "," : "") << camBest[i]; o << "\n"; }
        if (!legBest.empty()) { o << "legs="; for (size_t i = 0; i < legBest.size(); ++i) o << (i ? "," : "") << legBest[i]; o << "\n"; }
    }
    char buf[256];
    for (const OwnedCar& c : cars) {
        const Tune& t = c.tune;
        std::snprintf(buf, sizeof buf, "car=%d;%d;%d;%.3f;%d;%d;%.2f;%d;%d;%d;%.4f;%d;%d;%d;%d;%d;%d;%d\n", c.carId, c.races,
                      c.wins, c.bestEt, c.paidParts, (int)t.tires, t.psi, t.clutch, t.axles, (int)t.diff, t.finalDrive,
                      t.weight, t.intake, t.exhaust, t.ecu, t.turbo, t.drySump ? 1 : 0, (int)t.fuel);
        o << buf;
        if (c.damaged()) { std::snprintf(buf, sizeof buf, "dmg=%d;%.4f\n", c.axleBroken ? 1 : 0, c.engineWear); o << buf; }
        if (c.gearboxBroken) o << "gbx=1\n";
        if (c.fromJunk) o << "junk=1\n";
        if (c.paint >= 0 || c.finish || c.stripe || c.rimCol >= 0) {
            std::snprintf(buf, sizeof buf, "look=%d;%d;%d;%d;%d\n", c.paint, c.finish, c.stripe, c.stripeCol, c.rimCol);
            o << buf;
        }
        o << "tun2=" << tuneV2String(t) << "\n";
        for (int k = 0; k < 3; ++k) if (!c.profile[k].empty()) o << "prof" << k << "=" << c.profile[k] << "\n";
        if (c.nosFill < 0.999) { std::snprintf(buf, sizeof buf, "nosf=%.3f\n", c.nosFill); o << buf; }
        if (c.km >= 0) { std::snprintf(buf, sizeof buf, "km=%.1f\n", c.km); o << buf; }
        if (c.fuelL >= 0) { std::snprintf(buf, sizeof buf, "fuel=%.2f\n", c.fuelL); o << buf; }
        if (c.jobHp > 0) { std::snprintf(buf, sizeof buf, "job=%d;%ld\n", c.jobHp, c.jobReward); o << buf; }
        if (c.gaugeOwned) { std::snprintf(buf, sizeof buf, "gauge=%d;%d;%d\n", c.gauge, c.gaugeOwned, c.boostGauge ? 1 : 0); o << buf; }
        if (t.absKit || t.tcKit) o << "elx=" << (t.absKit ? 1 : 0) << ";" << (t.tcKit ? 1 : 0) << "\n";   // ECU ile eklenen ABS / TC
    }
    const std::string body = o.str();
    char cs[32]; std::snprintf(cs, sizeof cs, "checksum=%08x\n", fnv1a(body));
    return body + cs;
}

bool Career::parse(const std::string& text, Career& out) {
    const size_t csPos = text.rfind("checksum=");
    if (csPos == std::string::npos) return false;
    const std::string body = text.substr(0, csPos);
    const uint32_t want = (uint32_t)std::strtoul(text.c_str() + csPos + 9, nullptr, 16);
    if (fnv1a(body) != want) return false;                       // yarim yazilmis / bozuk
    std::istringstream in(body);
    std::string line;
    if (!std::getline(in, line)) return false;
    int ver = 0;
    if (std::sscanf(line.c_str(), "ZEHRAKINIK_KAYIT %d", &ver) != 1 || ver < 1 || ver > kVersion) return false;
    Career c;
    const int nCars = (int)vehicleCatalog().size();
    while (std::getline(in, line)) {
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (k == "money") c.money = std::max(0L, std::atol(v.c_str()));
        else if (k == "current") c.current = std::atoi(v.c_str());
        else if (k == "races") c.races = std::max(0, std::atoi(v.c_str()));
        else if (k == "wins") c.wins = std::max(0, std::atoi(v.c_str()));
        else if (k == "earnings") c.earnings = std::max(0L, std::atol(v.c_str()));
        else if (k == "treePro") c.treePro = std::atoi(v.c_str()) != 0;
        else if (k == "flow") c.bestFlow = std::max(0L, std::atol(v.c_str()));
        else if (k == "chase") c.chaseEscapes = std::max(0, std::atoi(v.c_str()));
        else if (k == "jobs") std::sscanf(v.c_str(), "%d;%d;%d", &c.jobSeq[0], &c.jobSeq[1], &c.jobSeq[2]);
        else if (k == "loan") {
            long l = 0; int r = 0, w = -1;
            if (std::sscanf(v.c_str(), "%ld;%d;%d", &l, &r, &w) >= 2) { c.loan = std::max(0L, l); c.loanRaces = std::max(0, r); c.showWeek = w; }
        }
        else if (k == "sponsor") {
            int a = -1, w = 0, r = 0; unsigned long d = 0;
            if (std::sscanf(v.c_str(), "%d;%d;%d;%lu", &a, &w, &r, &d) == 4) {
                c.sponsor = a >= 0 && a < (int)sponsors().size() ? a : -1; c.spWins = std::max(0, w); c.spRaces = std::max(0, r);
                c.sponsorsDone = (uint32_t)d;
            }
        }
        else if (k == "tour") {
            int w = -1, r = 0, o = 0;
            if (std::sscanf(v.c_str(), "%d;%d;%d", &w, &r, &o) == 3) { c.tourWeek = w; c.tourRound = std::clamp(r, 0, (int)kTourRounds); c.tourOut = o != 0; }
        }
        else if (k == "city") c.city = std::clamp(std::atoi(v.c_str()) * 2, 0, kCities - 1);   // eski kayit: 5 sehir (lig) -> ligin ilk sehri
        else if (k == "city2") c.city = std::clamp(std::atoi(v.c_str()), 0, kCities - 1);
        else if (k == "slots") c.garageSlots = std::clamp(std::atoi(v.c_str()), (int)kStartSlots, (int)kMaxSlots);
        else if (k == "form") c.form = std::clamp(std::atoi(v.c_str()), -3, 3);
        else if (k == "rep") c.rep = std::max(0, std::atoi(v.c_str()));
        else if (k == "evw") c.eventWins = std::strtoull(v.c_str(), nullptr, 16);
        else if (k == "evw2") c.eventWins2 = std::strtoull(v.c_str(), nullptr, 16);
        else if (k == "barn") c.barnFound = (uint32_t)std::strtoul(v.c_str(), nullptr, 16);
        else if (k == "owr") { int rd = 0; unsigned long long col = 0; if (std::sscanf(v.c_str(), "%d;%llx", &rd, &col) == 2) { c.radarDetector = rd != 0; c.collected = col; } }
        else if (k == "cams") { c.camBest.clear(); for (size_t a = 0; a < v.size();) { c.camBest.push_back(std::atoi(v.c_str() + a)); const size_t b = v.find(',', a); if (b == std::string::npos) break; a = b + 1; } }
        else if (k == "legs") { c.legBest.clear(); for (size_t a = 0; a < v.size();) { c.legBest.push_back(std::atof(v.c_str() + a)); const size_t b = v.find(',', a); if (b == std::string::npos) break; a = b + 1; } }
        else if (k == "ach") c.achieved = (uint32_t)std::strtoul(v.c_str(), nullptr, 16);
        else if (k == "daily") std::sscanf(v.c_str(), "%d;%ld;%ld;%ld;%d", &c.dailyDay, &c.dailyProg[0], &c.dailyProg[1], &c.dailyProg[2], &c.dailyDone);
        else if (k == "streak") {
            if (std::sscanf(v.c_str(), "%d;%d", &c.lastOppId, &c.sameOppWins) != 2) return false;
            c.sameOppWins = std::clamp(c.sameOppWins, 0, 100);
        }
        else if (k == "elx" && !c.cars.empty()) {          // onceki araca ECU ile eklenen ABS / TC
            int a = 0, tc = 0;
            if (std::sscanf(v.c_str(), "%d;%d", &a, &tc) != 2) return false;
            c.cars.back().tune.absKit = a != 0; c.cars.back().tune.tcKit = tc != 0;
        }
        else if (k == "dmg" && !c.cars.empty()) {          // onceki araca ait hasar
            int ax = 0; double ew = 0;
            if (std::sscanf(v.c_str(), "%d;%lf", &ax, &ew) != 2) return false;
            c.cars.back().axleBroken = ax != 0;
            c.cars.back().engineWear = std::clamp(ew, 0.0, 1.0);
        }
        else if (k == "gbx" && !c.cars.empty()) c.cars.back().gearboxBroken = v == "1";
        else if (k == "junk" && !c.cars.empty()) c.cars.back().fromJunk = v == "1";
        else if (k == "look" && !c.cars.empty()) {
            OwnedCar& oc = c.cars.back();
            if (std::sscanf(v.c_str(), "%d;%d;%d;%d;%d", &oc.paint, &oc.finish, &oc.stripe, &oc.stripeCol, &oc.rimCol) != 5) return false;
        }
        else if (k == "tun2" && !c.cars.empty()) parseTuneV2(v, c.cars.back().tune);
        else if ((k == "prof0" || k == "prof1" || k == "prof2") && !c.cars.empty()) c.cars.back().profile[k[4] - '0'] = v;
        else if (k == "nosf" && !c.cars.empty()) c.cars.back().nosFill = std::clamp(std::atof(v.c_str()), 0.0, 1.0);
        else if (k == "km" && !c.cars.empty()) c.cars.back().km = std::max(0.0, std::atof(v.c_str()));
        else if (k == "fuel" && !c.cars.empty()) c.cars.back().fuelL = std::max(0.0, std::atof(v.c_str()));
        else if (k == "job" && !c.cars.empty()) { OwnedCar& oc = c.cars.back(); if (std::sscanf(v.c_str(), "%d;%ld", &oc.jobHp, &oc.jobReward) != 2) oc.jobHp = 0; }
        else if (k == "gauge" && !c.cars.empty()) {
            OwnedCar& oc = c.cars.back(); int bg = 0;
            if (std::sscanf(v.c_str(), "%d;%d;%d", &oc.gauge, &oc.gaugeOwned, &bg) == 3) { oc.gauge = std::clamp(oc.gauge, 0, 3); oc.boostGauge = bg != 0; }
        }
        else if (k == "street") std::sscanf(v.c_str(), "%d;%d;%d;%d", &c.meetDone, &c.dynoWeek, &c.jobDay, &c.jobMask);
        else if (k == "car") {
            OwnedCar oc; int tires, diff, dry, fuel;
            oc.tune.partsVer = 1;                                  // "pv" yoksa eski kayit: tek liste secimleri donusturulur
            if (std::sscanf(v.c_str(), "%d;%d;%d;%lf;%d;%d;%lf;%d;%d;%d;%lf;%d;%d;%d;%d;%d;%d;%d", &oc.carId, &oc.races, &oc.wins,
                            &oc.bestEt, &oc.paidParts, &tires, &oc.tune.psi, &oc.tune.clutch, &oc.tune.axles, &diff,
                            &oc.tune.finalDrive, &oc.tune.weight, &oc.tune.intake, &oc.tune.exhaust, &oc.tune.ecu,
                            &oc.tune.turbo, &dry, &fuel) != 18) return false;
            if (oc.carId < 1 || oc.carId > nCars) return false;
            // Aralik denetimi: disaridan degistirilmis kayitta bile fizik gecerli degerler alsin
            oc.tune.tires = (TireType)std::clamp(tires, 0, 2);
            oc.tune.diff = (DiffType)clampRow(diffTable(), diff);
            oc.tune.clutch = clampRow(clutchTable(), oc.tune.clutch);
            oc.tune.axles = clampRow(axleTable(), oc.tune.axles);
            oc.tune.weight = clampRow(weightTable(), oc.tune.weight);
            oc.tune.intake = clampRow(intakeTable(), oc.tune.intake);
            oc.tune.exhaust = clampRow(exhaustTable(), oc.tune.exhaust);
            oc.tune.ecu = clampRow(ecuTable(), oc.tune.ecu);
            oc.tune.turbo = clampRow(turboTable(), oc.tune.turbo);
            oc.tune.psi = std::clamp(oc.tune.psi, 0.0, 45.0);
            oc.tune.finalDrive = std::clamp(oc.tune.finalDrive, 0.0, 8.0);
            oc.tune.drySump = dry != 0;
            oc.tune.fuel = (FuelType)std::clamp(fuel, 0, 2);
            c.cars.push_back(oc);
        }
    }
    if (c.cars.empty()) return false;
    for (OwnedCar& oc : c.cars) migrateTune(oc.tune);              // eski yakit sistemi / ECU paketi -> yeni parcalar
    // Eski surum hatasi: hurda aracta motor swap'tan sonra eski motorun hasari kaliyordu (yeni motor hasarsiz)
    for (OwnedCar& oc : c.cars)
        if (oc.fromJunk && oc.tune.engineSwap > 0 && oc.engineWear >= 1.0) { oc.engineWear = 0.0; oc.tune.wearEngine = 0.0; }
    c.garageSlots = std::max(c.garageSlots, std::min((int)c.cars.size(), (int)kMaxSlots));   // eski kayit: arac sayisi kadar yuva
    c.current = std::clamp(c.current, 0, (int)c.cars.size() - 1);
    out = c;
    return true;
}

bool Career::save(const std::string& path) const {
    const std::string tmp = path + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f << serialize();
        f.flush();
        if (!f) return false;
    }
#ifdef _WIN32
    std::remove(path.c_str());
#endif
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}

Career Career::loadOrNew(const std::string& path, bool* corrupt) {
    if (corrupt) *corrupt = false;
    {
        std::ifstream f(path, std::ios::binary);
        if (!f) return newGame();
        std::stringstream ss; ss << f.rdbuf();
        Career c;
        if (parse(ss.str(), c)) return c;
    }
    std::rename(path.c_str(), (path + ".bozuk").c_str());      // kullanici/destek icin sakla, uzerine yazilmasin
    if (corrupt) *corrupt = true;
    return newGame();
}

} // namespace zk
