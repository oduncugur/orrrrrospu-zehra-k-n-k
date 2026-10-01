#include "Career.h"
#include "sim/PartTables.h"
#include "sim/VehicleSim.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
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
    static const char* n[kPartTabs] = {"MOTOR", "BESLEME", "AKTARMA", "SASI", "ECU+SOGUTMA"};
    return n[std::clamp(tab, 0, kPartTabs - 1)];
}
int partTab(PartCat c) {
    const int i = (int)c;
    return i <= (int)PartCat::EngineSwap ? 0 : i <= (int)PartCat::Fuel ? 1 : i <= (int)PartCat::Axles ? 2 : i <= (int)PartCat::Aero ? 3 : 4;
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

const char* partCatName(PartCat c) {
    static const char* n[(int)PartCat::Count] = {
        "SILINDIR KAPAGI", "SUPAP + YAY", "KAM MILI", "PISTON", "BIYEL", "KRANK", "YATAK", "KAPAK CONTASI", "VOLAN", "MOTOR SWAP",
        "EMME", "EGZOZ", "TURBO", "TURBIN", "WASTEGATE", "BOOST KONTROL", "KOMPRESOR", "INTERCOOLER", "NITRO (NOS)", "YAKIT SISTEMI", "YAKIT",
        "DEBRIYAJ", "SANZIMAN SWAP", "SANZIMAN GUCLENDIRME", "SON DISLI", "DIFERANSIYEL", "AKS",
        "LASTIK", "JANT", "SUSPANSIYON", "FREN", "HAFIFLETME", "AERO",
        "ECU", "ELEKTRONIK", "SOGUTMA", "YAG / KARTER"};
    return n[(int)c];
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
        {   // Motor swap: fabrika + tum motorlar (guce gore); ad: kod + duzen + guc
            static std::vector<std::string> names;
            names.reserve(swapEngines().size() + 1);
            names.push_back("FABRIKA MOTORU");
            for (int e : swapEngines()) {
                const EngineDef& d = engineTable()[e];
                char b[64]; std::snprintf(b, sizeof b, "%s %s %.1fL %.0fHP", d.code, layoutName(d.layout), d.displacementL, d.powerHp);
                names.push_back(b);
            }
            for (size_t i = 0; i < names.size(); ++i)
                t[(int)PartCat::EngineSwap].push_back({names[i].c_str(), i == 0 ? 0 : (int)(1500 + engineTable()[swapEngines()[i - 1]].powerHp * 22)});
        }
        t[(int)PartCat::Intake] = opts(intakeTable());      t[(int)PartCat::Exhaust] = opts(exhaustTable());
        t[(int)PartCat::Turbo] = opts(turboTable());        t[(int)PartCat::Turbine] = opts(turbineTable());
        t[(int)PartCat::Wastegate] = opts(wastegateTable()); t[(int)PartCat::BoostCtl] = opts(boostCtlTable());
        t[(int)PartCat::Supercharger] = opts(superTable()); t[(int)PartCat::Intercooler] = opts(intercoolerTable());
        t[(int)PartCat::Nitrous] = opts(nosTable());        t[(int)PartCat::FuelSys] = opts(fuelSysTable());
        t[(int)PartCat::Fuel] = opts(fuelTable());          t[(int)PartCat::Clutch] = opts(clutchTable());
        t[(int)PartCat::Gearbox] = opts(gearTable());       t[(int)PartCat::GbStrength] = opts(gbStrengthTable());
        t[(int)PartCat::FinalDrive] = opts(finalTable());   t[(int)PartCat::Diff] = opts(diffTable());
        t[(int)PartCat::Axles] = opts(axleTable());         t[(int)PartCat::Tires] = opts(tireTable());
        t[(int)PartCat::Rims] = opts(rimTable());           t[(int)PartCat::Suspension] = opts(suspTable());
        t[(int)PartCat::Brakes] = opts(brakeTable());       t[(int)PartCat::Weight] = opts(weightTable());
        t[(int)PartCat::Aero] = opts(aeroTable());          t[(int)PartCat::Ecu] = opts(ecuTable());
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
    case PartCat::Intake: return t.intake;       case PartCat::Exhaust: return t.exhaust;
    case PartCat::Turbo: return t.turbo;         case PartCat::Turbine: return t.turbine;
    case PartCat::Wastegate: return t.wastegate; case PartCat::BoostCtl: return t.boostCtl;
    case PartCat::Supercharger: return t.superch; case PartCat::Intercooler: return t.intercooler;
    case PartCat::Nitrous: return t.nitrous;     case PartCat::FuelSys: return t.fuelSys;
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
    case PartCat::Rims: return t.rims;           case PartCat::Suspension: return t.susp;
    case PartCat::Brakes: return t.brakes;       case PartCat::Weight: return t.weight;
    case PartCat::Aero: return t.aero;           case PartCat::Ecu: return t.ecu;
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
    case PartCat::Intake: t.intake = l; break;       case PartCat::Exhaust: t.exhaust = l; break;
    case PartCat::Turbo: t.turbo = l; break;         case PartCat::Turbine: t.turbine = l; break;
    case PartCat::Wastegate: t.wastegate = l; break; case PartCat::BoostCtl: t.boostCtl = l; break;
    case PartCat::Supercharger: t.superch = l; break; case PartCat::Intercooler: t.intercooler = l; break;
    case PartCat::Nitrous: t.nitrous = l; break;     case PartCat::FuelSys: t.fuelSys = l; break;
    case PartCat::Fuel: t.fuelSel = l; t.fuel = (FuelType)fuelTable()[l].type; break;
    case PartCat::Clutch: t.clutch = l; break;       case PartCat::Gearbox: t.gearSwap = l; break;
    case PartCat::GbStrength: t.gbStrength = l; break;
    case PartCat::FinalDrive: t.finalSel = l; t.finalDrive = 0.0; break;
    case PartCat::Diff: t.diff = (DiffType)l; break; case PartCat::Axles: t.axles = l; break;
    case PartCat::Tires: t.tireSel = l; t.tires = (TireType)tireTable()[l].type; t.psi = 0; break;
    case PartCat::Rims: t.rims = l; break;           case PartCat::Suspension: t.susp = l; break;
    case PartCat::Brakes: t.brakes = l; break;       case PartCat::Weight: t.weight = l; break;
    case PartCat::Aero: t.aero = l; break;           case PartCat::Ecu: t.ecu = l; break;
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

int partPrice(PartCat c, int level, const VehicleDef& v) {
    const auto& o = partOptions(c);
    if (level < 0 || level >= (int)o.size()) return 0;
    if (c == PartCat::EngineSwap) return (int)(std::round(o[level].basePrice / 10.0) * 10.0);   // motor fiyati aractan bagimsiz
    const double scale = std::clamp(0.6 + carPrice(v) / 40000.0, 0.6, 4.0);   // pahali arabanin parcasi pahali
    return (int)(std::round(o[level].basePrice * scale / 10.0) * 10.0);
}

bool partAvailable(PartCat c, int level, const VehicleDef& v, std::string* why, const Tune* t) {
    auto no = [&](const char* m) { if (why) *why = m; return false; };
    if (c == PartCat::Electronics && level > 0) {
        // Fabrikada olan sistem tekrar takilmaz; olmayana ancak ECU yukseltmesiyle eklenir
        if (level == 1 && v.abs) return no("FABRIKADA VAR");
        if (level == 2 && v.abs && v.tc) return no("FABRIKADA VAR");
        if (t && t->ecu < 1) return no("ONCE ECU GEREKLI");
    }
    const EngineDef& e = engineTable()[effectiveEngine(v, t)];
    const bool turboEngine = e.induction == Induction::Turbo || e.induction == Induction::TwinTurbo;
    if (c == PartCat::Turbo && level > 0 && e.induction == Induction::Supercharger) return no("KOMPRESORLU MOTORA TURBO YOK");
    if ((c == PartCat::Turbine || c == PartCat::Wastegate || c == PartCat::BoostCtl) && level > 0 && t && t->turbo == 0 && !turboEngine)
        return no("ONCE TURBO GEREKLI");
    if (c == PartCat::Intercooler && level > 0 && t && t->turbo == 0 && t->superch == 0 && !turboEngine && e.induction != Induction::Supercharger)
        return no("ASIRI BESLEME YOK");
    if (c == PartCat::Supercharger && level > 0 && e.induction == Induction::Supercharger) return no("FABRIKADA KOMPRESOR VAR");
    if (c == PartCat::EngineSwap && level > 0 && swapEngines()[level - 1] == v.engine) return no("FABRIKA MOTORU");
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
    q.car = r10(0.65 * carPrice(*findVehicle(c.carId)));
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
    if (i == 1) { t.intake = 1; t.exhaust = 1; t.tires = TireType::SemiSlick; t.diff = DiffType::OneAndHalfWay; }
    if (i == 2) {
        t.intake = 2; t.exhaust = 2; t.ecu = 1; t.tires = TireType::DragSlick;
        t.clutch = 1; t.axles = 1; t.diff = DiffType::OneAndHalfWay;
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
Opponent Career::pickOpponentFor(uint32_t seed) const {
    const OwnedCar& oc = car();
    const VehicleDef& pv = *findVehicle(oc.carId);
    const double pe = estimatedEt(pv, oc.tune);
    uint32_t x = seed * 2654435761u + 0x9E3779B9u; x ^= x >> 15;
    const double jitter = ((x >> 8) % 1000) / 1000.0 * 0.16 - 0.08;
    const double target = pe + 0.05 - 0.10 * std::clamp(form, -3, 3) + jitter;   // rakip yarista hata payli (DragRace)
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
    if (t.ecu) add("ECU");
    if (t.intake || t.exhaust) add("EMME/EGZ");
    if (t.weight) add("HAFIF");
    if (t.drySump) add("KURU-KRT");
    if (t.fuel == FuelType::E85) add("E85");
    int more = 0;                                                  // diger parcalar
    for (int x : {t.cam, t.valve, t.head, t.piston, t.rod, t.crank, t.bearing, t.gasket, t.flywheel, t.turbine, t.wastegate,
                  t.boostCtl, t.intercooler, t.fuelSys, t.gbStrength, t.gearSwap, t.finalSel, t.rims, t.susp, t.brakes, t.aero, t.cooling})
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

bool Career::buyCar(int carId, std::string* why) {
    const VehicleDef* v = findVehicle(carId);
    if (!v) { if (why) *why = "ARAC YOK"; return false; }
    const int p = carPrice(*v);
    if (money < p) { if (why) *why = "PARA YETMIYOR"; return false; }
    money -= p;
    OwnedCar oc; oc.carId = carId;
    cars.push_back(oc);
    current = (int)cars.size() - 1;
    return true;
}

bool Career::sellCurrent(std::string* why) {
    if (cars.size() <= 1) { if (why) *why = "SON ARACINI SATAMAZSIN"; return false; }
    money += sellPrice(cars[current]);
    cars.erase(cars.begin() + current);
    current = std::min(current, (int)cars.size() - 1);
    return true;
}

bool Career::buyPart(PartCat c, int level, std::string* why) {
    OwnedCar& oc = car();
    const VehicleDef& v = *findVehicle(oc.carId);
    if (level < 0 || level >= (int)partOptions(c).size()) { if (why) *why = "GECERSIZ"; return false; }
    if (partLevel(oc.tune, c, v) == level) { if (why) *why = "ZATEN TAKILI"; return false; }
    if (!partAvailable(c, level, v, why, &oc.tune)) return false;
    const int p = partPrice(c, level, v);
    if (money < p) { if (why) *why = "PARA YETMIYOR"; return false; }
    money -= p;
    oc.paidParts += p;
    setPartLevel(oc.tune, c, level, v);
    return true;
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
}

// ------------------------------------------------------------------ kayit
namespace {
struct IntField { const char* key; int Tune::*f; int max; };
struct DblField { const char* key; double Tune::*f; double max; };
const IntField kIntFields[] = {
    {"tire", &Tune::tireSel, 29}, {"rim", &Tune::rims, 9}, {"fin", &Tune::finalSel, 11}, {"gbx", &Tune::gearSwap, 30},
    {"eng", &Tune::engineSwap, 400}, {"cam", &Tune::cam, 11}, {"val", &Tune::valve, 9}, {"fly", &Tune::flywheel, 9},
    {"sc", &Tune::superch, 9}, {"ic", &Tune::intercooler, 9}, {"fsy", &Tune::fuelSys, 9}, {"nos", &Tune::nitrous, 9},
    {"head", &Tune::head, 9}, {"pis", &Tune::piston, 9}, {"rod", &Tune::rod, 9}, {"crk", &Tune::crank, 9},
    {"brg", &Tune::bearing, 9}, {"gsk", &Tune::gasket, 9}, {"trb", &Tune::turbine, 9}, {"wg", &Tune::wastegate, 9},
    {"bc", &Tune::boostCtl, 9}, {"gbs", &Tune::gbStrength, 9}, {"cool", &Tune::cooling, 9}, {"oil", &Tune::oil, 9},
    {"fuel", &Tune::fuelSel, 9}, {"elx", &Tune::elec, 9}, {"sus", &Tune::susp, 9}, {"brk", &Tune::brakes, 9}, {"aero", &Tune::aero, 9},
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
            for (const IntField& f : kIntFields) if (k == f.key) t.*(f.f) = std::clamp((int)x, 0, f.max);
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

void Career::recordDamage(bool axleBroke, double bearingDamage, bool bearingSpun, bool gearboxBroke, double engineStress) {
    OwnedCar& c = car();
    c.axleBroken = c.axleBroken || axleBroke;
    c.gearboxBroken = c.gearboxBroken || gearboxBroke;
    c.engineWear = bearingSpun || engineStress >= 1.0 ? 1.0
                 : std::clamp(c.engineWear + std::max(0.0, bearingDamage) + 0.5 * std::max(0.0, engineStress), 0.0, 1.0);
}

long Career::recordFlow(long score, bool* newRecord) {
    score = std::max(0L, score);
    const bool rec = score > bestFlow && score > 0;
    long prize = std::min(score / 20, kFlowPrizeCap);
    if (rec) { prize += prize / 2; bestFlow = score; }
    prize = prize / 10 * 10;
    money += prize; earnings += prize;
    if (newRecord) *newRecord = rec;
    return prize;
}

long Career::repairCost() const { return repairCostFor(car()); }

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
    if (money < j.price) { if (why) *why = "PARA YETMIYOR"; return false; }
    money -= j.price;
    OwnedCar oc; oc.carId = j.carId; oc.tune = j.tune;
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
      << "\nearnings=" << earnings << "\ntreePro=" << (treePro ? 1 : 0) << "\nstreak=" << lastOppId << ";" << sameOppWins << "\nflow=" << bestFlow << "\nform=" << form << "\n";
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
        o << "tun2=" << tuneV2String(t) << "\n";
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
        else if (k == "form") c.form = std::clamp(std::atoi(v.c_str()), -3, 3);
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
        else if (k == "tun2" && !c.cars.empty()) parseTuneV2(v, c.cars.back().tune);
        else if (k == "car") {
            OwnedCar oc; int tires, diff, dry, fuel;
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
