#include "VehicleCatalog.h"
#include <algorithm>
#include <iterator>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace zk {

static constexpr double kPi = 3.14159265358979323846;

// Tablo kisaltmalari (.inc dosyalari icin)
using L = Layout; using In = Induction; using GB = Gearbox; using B = Body; using D = Drive;

const std::vector<EngineDef>& engineTable() {
    static const std::vector<EngineDef> t = {
#include "EngineTable.inc"
    };
    return t;
}

const std::vector<GearboxDef>& gearboxTable() {
    static const std::vector<GearboxDef> t = {
#include "GearboxTable.inc"
    };
    return t;
}

int findEngine(const char* code) {
    const auto& t = engineTable();
    for (size_t i = 0; i < t.size(); ++i) if (!std::strcmp(t[i].code, code)) return (int)i;
    return -1;
}
int findGearbox(const char* code) {
    const auto& t = gearboxTable();
    for (size_t i = 0; i < t.size(); ++i) if (!std::strcmp(t[i].code, code)) return (int)i;
    return -1;
}

namespace {

struct Row {
    const char* brand; const char* model; const char* ref; int year; Body body; Drive drive;
    double mass, L, W, H, wb, fw; const char* engine; const char* gearbox; bool race;
};

unsigned paintFor(int id) {
    static const unsigned pal[] = {0xC81E1E, 0xF2F2F2, 0x1A1A1A, 0x1E4FC8, 0xF5C518, 0x2E8B3A, 0xE86A10,
                                   0x7A1FA2, 0x9AA3AD, 0x0FA3B1, 0x8B0000, 0xD4AF37, 0x3B5F2E, 0xFF5F9E};
    return pal[(id * 7 + id / 5) % (sizeof(pal) / sizeof(pal[0]))];
}

#include "SafetyTable.inc"

std::vector<VehicleDef> buildCatalog() {
#define V(br, mo, rf, yr, bo, dr, m, l, w, h, wb, fw, en, gb) Row{br, mo, rf, yr, bo, dr, m, l, w, h, wb, fw, en, gb, false}
#define R(br, mo, rf, yr, bo, dr, m, l, w, h, wb, fw, en, gb) Row{br, mo, rf, yr, bo, dr, m, l, w, h, wb, fw, en, gb, true}
    static const Row rows[] = {
#include "VehicleTable.inc"
    };
#undef V
#undef R
    std::vector<VehicleDef> out;
    int id = 1;
    for (const Row& r : rows) {
        VehicleDef d;
        d.id = id; d.brand = r.brand; d.model = r.model; d.ref = r.ref; d.year = r.year;
        d.body = r.body; d.drive = r.drive; d.massKg = r.mass; d.lengthM = r.L; d.widthM = r.W;
        d.heightM = r.H; d.wheelbaseM = r.wb; d.frontWeight = r.fw;
        d.engine = d.head = findEngine(r.engine);
        d.gearbox = findGearbox(r.gearbox);
        if (d.engine < 0 || d.gearbox < 0) {
            std::fprintf(stderr, "Katalog hatasi: arac %d (%s) motor '%s' / sanziman '%s' bulunamadi\n", id,
                         r.ref, r.engine, r.gearbox);
            std::abort();
        }
        d.streetLegal = !r.race;
        d.exhaust = r.race ? Exhaust::StraightPipe : Exhaust::Stock;
        d.wing = r.race || r.body == Body::Super;
        d.hoodScoop = engineTable()[d.engine].induction == Induction::Supercharger;
        d.widebody = r.race;
        d.rideHeightM = (r.body == Body::SUV || r.body == Body::Pickup || r.body == Body::Van) ? 0.22
                      : r.race ? 0.08 : 0.13;
        d.paintRGB = paintFor(id);
        d.abs = std::find(std::begin(kFactoryAbs), std::end(kFactoryAbs), id) != std::end(kFactoryAbs) && !r.race;
        d.tc = std::find(std::begin(kFactoryTc), std::end(kFactoryTc), id) != std::end(kFactoryTc) && !r.race;
        out.push_back(d);
        ++id;
    }
    return out;
}

} // namespace

const std::vector<VehicleDef>& vehicleCatalog() {
    static const std::vector<VehicleDef> c = buildCatalog();
    return c;
}

const VehicleDef* findVehicle(int id) {
    const auto& c = vehicleCatalog();
    return (id >= 1 && id <= (int)c.size()) ? &c[id - 1] : nullptr;
}

const char* layoutName(Layout l) {
    switch (l) {
    case Layout::I3: return "I3"; case Layout::I4: return "I4"; case Layout::I5: return "I5";
    case Layout::I6: return "I6"; case Layout::V6_60: return "V6"; case Layout::V6_90Odd: return "V6 oddfire";
    case Layout::V8Cross: return "V8 crossplane"; case Layout::V8Flat: return "V8 flatplane";
    case Layout::V10: return "V10"; case Layout::V12: return "V12"; case Layout::Boxer4: return "Boxer4";
    case Layout::Boxer6: return "Boxer6"; case Layout::Rotary2: return "2-Rotor"; case Layout::Rotary3: return "3-Rotor";
    case Layout::Rotary4: return "4-Rotor";
    }
    return "?";
}
const char* inductionName(Induction i) {
    switch (i) { case Induction::NA: return "NA"; case Induction::ITB: return "ITB"; case Induction::Turbo: return "Turbo";
    case Induction::TwinTurbo: return "TwinTurbo"; case Induction::Supercharger: return "Kompresor"; }
    return "?";
}
bool engineIsDiesel(const EngineDef& e) {
    const std::string r = e.ref;
    for (const char* k : {"TDI", "TDCi", "dCi", "CDI", "CRDi", "D-4D", "HDi", "JTD", "Multijet", "SDI", "Diesel", "dizel"})
        if (r.find(k) != std::string::npos) return true;
    return false;
}
std::string engineDisplayName(const EngineDef& e) {
    static const char* lay[] = {"SIRALI 3", "SIRALI 4", "SIRALI 5", "SIRALI 6", "V6", "V6", "V8", "V8 DUZ KRANK", "V10", "V12",
                                "BOXER 4", "BOXER 6", "2 ROTORLU WANKEL", "3 ROTORLU WANKEL", "4 ROTORLU WANKEL"};
    const bool rotary = e.layout == Layout::Rotary2 || e.layout == Layout::Rotary3 || e.layout == Layout::Rotary4;
    char b[96];
    std::snprintf(b, sizeof b, "%.1f %s", e.displacementL, lay[(int)e.layout]);
    std::string s = b;
    if (!rotary && e.valvesPerCyl > 0) { std::snprintf(b, sizeof b, " %dV", e.cylinders * e.valvesPerCyl); s += b; }
    if (e.variableCam) s += " DEGISKEN KAM";
    switch (e.induction) {
    case Induction::NA: break;
    case Induction::ITB: s += " TEK GAZ KELEBEKLI"; break;
    case Induction::Turbo: s += " TURBO"; break;
    case Induction::TwinTurbo: s += " CIFT TURBO"; break;
    case Induction::Supercharger: s += " KOMPRESORLU"; break;
    }
    s += engineIsDiesel(e) ? " DIZEL" : " BENZIN";
    return s;
}
const char* driveName(Drive d) { return d == Drive::FWD ? "FWD" : d == Drive::RWD ? "RWD" : "AWD"; }
const char* bodyName(Body b) {
    static const char* n[] = {"Kei", "Hatch", "Sedan", "Coupe", "Wagon", "Roadster", "Muscle", "Super", "SUV",
                              "Pickup", "Van", "Rally"};
    return n[(int)b];
}
const char* gearboxName(Gearbox g) {
    switch (g) { case Gearbox::HPattern: return "H-Manuel"; case Gearbox::TorqueConverter: return "TC-Otomatik";
    case Gearbox::Dogbox: return "Dogbox"; case Gearbox::DCT: return "DCT"; }
    return "?";
}

// ---------------------------------------------------------------------------------------------
// Tork egrisi: gercek tepe tork (Nm @ rpm) ve tepe guc (HP @ rpm) noktalarindan gecen egri.
// T_P = HP * 7120.9 / rpm  (HP -> N*m)
// ---------------------------------------------------------------------------------------------
static TorqueCurve synthCurve(const EngineDef& e, bool lowCam) {
    const double Tpk = e.torqueNm, rT = e.torqueRpm;
    const double rP = std::max(e.powerRpm, rT + 250.0);
    const double TP = std::min(Tpk, e.powerHp * 7120.9 / rP);
    const bool forced = e.induction == Induction::Turbo || e.induction == Induction::TwinTurbo;
    const bool rotary = e.layout == Layout::Rotary2 || e.layout == Layout::Rotary3 || e.layout == Layout::Rotary4;
    const double redline = e.redline;
    TorqueCurve c;
    for (double r = 750; r <= redline + 500; r += 250) {
        double T;
        if (r <= rT) {
            // Alt devir: turbo spool (boost oncesi ~%45), atmosferikte ~%70'ten yukselis
            const double x = (r - 750.0) / std::max(rT - 750.0, 1.0);
            const double base = forced ? 0.42 : rotary ? 0.75 : 0.68;
            const double s = forced ? x * x * (3 - 2 * x) : std::sqrt(std::max(0.0, x));
            T = Tpk * (base + (1.0 - base) * s);
        } else if (r <= rP) {
            const double x = (r - rT) / (rP - rT);
            T = Tpk + (TP - Tpk) * x;              // tork tepesi -> guc tepesi dogrusal
        } else {
            const double x = (r - rP) / std::max(redline - rP + 300.0, 300.0);
            T = TP * (1.0 - 0.18 * x - 0.25 * x * x); // redline sonrasi nefessizlik
        }
        if (lowCam && e.variableCam) {
            // Dusuk kam: kam gecis devrinin %85'inde tepe yapar, ustunde hizla duser
            const double sw = e.camSwitchRpm;
            if (r < sw) T *= (r < 0.6 * sw ? 1.04 : 1.0);
            else T *= std::max(0.55, 0.92 - 0.00018 * (r - sw));
        }
        c.push_back({r, std::max(15.0, T)});
    }
    return c;
}

EngineSpec buildEngineSpec(const VehicleDef& v) { return buildEngineSpecFor(v.engine); }

EngineSpec buildEngineSpecFor(int engineIdx) {
    const EngineDef& e = engineTable()[engineIdx];
    EngineSpec s;
    s.name = engineDisplayName(e);
    s.cylinders = e.cylinders;
    const double perCylCc = e.displacementL * 1000.0 / std::max(1, e.cylinders);
    s.boreMm = s.strokeMm = std::cbrt(perCylCc * 4.0 / kPi) * 10.0;
    // Tork egrisi gercek veriden geldigi icin CR referans 11.5'e (verim carpani 1.0) ayarlanir
    const double area = kPi / 4.0 * (s.boreMm / 10) * (s.boreMm / 10);
    s.gasketMm = 0.70;
    s.chamberCc = perCylCc / (11.5 - 1.0) - area * 0.07;
    s.inertia = 0.025 * e.displacementL + 0.07;
    s.redlineRpm = e.redline;
    s.idleRpm = (e.layout == Layout::V8Cross && e.displacementL > 5.0) ? 700 : 850;
    s.valvetrain = Valvetrain::V16;   // gercek egri zaten subap mimarisini icerir
    s.stagedValves = false;
    s.intake = e.induction == Induction::ITB ? IntakeType::ITB : IntakeType::Plenum;
    s.fuel = FuelType::Race100;
    s.lowCam = synthCurve(e, true);
    s.highCam = e.variableCam ? synthCurve(e, false) : s.lowCam;
    s.vtecRpm = e.variableCam ? e.camSwitchRpm : 1e9;
    if (!e.variableCam) s.lowCam = synthCurve(e, false), s.highCam = s.lowCam;
    return s;
}

GearboxSpec buildGearbox(const VehicleDef& v) { return buildGearboxFor(v.gearbox); }

GearboxSpec buildGearboxFor(int gearboxIdx) {
    const GearboxDef& g = gearboxTable()[gearboxIdx];
    GearboxSpec s;
    s.ratios = g.ratios;
    s.finalDrive = g.finalDrive;
    s.efficiency = g.type == Gearbox::TorqueConverter ? 0.88 : g.type == Gearbox::Dogbox ? 0.95 : 0.93;
    return s;
}

double peakPowerHp(const VehicleDef& v) {
    const EngineSpec s = buildEngineSpec(v);
    double best = 0.0;
    for (const auto* c : {&s.lowCam, &s.highCam})
        for (auto& [rpm, T] : *c)
            if (rpm <= s.redlineRpm) best = std::max(best, T * rpm * 2.0 * kPi / 60.0 / 745.7);
    return best;
}

} // namespace zk
