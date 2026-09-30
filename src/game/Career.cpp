#include "Career.h"
#include "sim/VehicleSim.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace zk {

// ------------------------------------------------------------------ parca katalogu
const char* partCatName(PartCat c) {
    static const char* n[] = {"LASTIK", "DEBRIYAJ", "AKS", "DIFERANSIYEL", "SON DISLI", "HAFIFLETME",
                              "EMME", "EGZOZ", "ECU", "TURBO KITI", "KARTER", "YAKIT"};
    return n[(int)c];
}

const std::vector<PartOption>& partOptions(PartCat c) {
    static const std::vector<PartOption> t[(int)PartCat::Count] = {
        {{"SOKAK", 0}, {"YARI-SLICK", 1200}, {"DRAG SLICK", 2800}},
        {{"STOK", 0}, {"STAGE 1", 900}, {"STAGE 2", 1800}, {"STAGE 3 METAL", 3200}},
        {{"STOK", 0}, {"KROM-MOLY", 1500}, {"YARIS 300M", 3800}},
        {{"ACIK", 0}, {"1.5-WAY LSD", 1400}, {"2-WAY LSD", 1700}, {"KAYNAK SPOOL", 300}},
        {{"FABRIKA", 0}, {"KISA +%15", 700}, {"UZUN -%10", 700}},
        {{"STOK", 0}, {"-40 KG", 600}, {"-85 KG", 1800}, {"-140 KG", 4000}},
        {{"STOK", 0}, {"SPOR FILTRE", 350}, {"SOGUK HAVA", 900}},
        {{"STOK", 0}, {"SPOR", 500}, {"DUZ BORU", 1200}},
        {{"STOK", 0}, {"REMAP", 1100}},
        {{"YOK / FABRIKA", 0}, {"KUCUK KIT", 4500}, {"BUYUK KIT", 9000}},
        {{"ISLAK", 0}, {"KURU KARTER", 3500}},
        {{"100 OKTAN", 0}, {"95 OKTAN", 0}, {"E85 + SISTEM", 1500}},
    };
    return t[(int)c];
}

int partLevel(const Tune& t, PartCat c, const VehicleDef& v) {
    switch (c) {
    case PartCat::Tires: return (int)t.tires;
    case PartCat::Clutch: return t.clutch;
    case PartCat::Axles: return t.axles;
    case PartCat::Diff: return (int)t.diff;
    case PartCat::FinalDrive: {
        if (t.finalDrive <= 0.0) return 0;
        return t.finalDrive > buildGearbox(v).finalDrive ? 1 : 2;
    }
    case PartCat::Weight: return t.weight;
    case PartCat::Intake: return t.intake;
    case PartCat::Exhaust: return t.exhaust;
    case PartCat::Ecu: return t.ecu;
    case PartCat::Turbo: return t.turbo;
    case PartCat::DrySump: return t.drySump ? 1 : 0;
    case PartCat::Fuel: return t.fuel == FuelType::Pump95 ? 1 : t.fuel == FuelType::E85 ? 2 : 0;
    default: return 0;
    }
}

void setPartLevel(Tune& t, PartCat c, int l, const VehicleDef& v) {
    l = std::clamp(l, 0, (int)partOptions(c).size() - 1);
    switch (c) {
    case PartCat::Tires: t.tires = (TireType)l; t.psi = 0; break;
    case PartCat::Clutch: t.clutch = l; break;
    case PartCat::Axles: t.axles = l; break;
    case PartCat::Diff: t.diff = (DiffType)l; break;
    case PartCat::FinalDrive: {
        const double fd = buildGearbox(v).finalDrive;
        t.finalDrive = l == 0 ? 0.0 : l == 1 ? fd * 1.15 : fd * 0.90;
        break;
    }
    case PartCat::Weight: t.weight = l; break;
    case PartCat::Intake: t.intake = l; break;
    case PartCat::Exhaust: t.exhaust = l; break;
    case PartCat::Ecu: t.ecu = l; break;
    case PartCat::Turbo: t.turbo = l; break;
    case PartCat::DrySump: t.drySump = l == 1; break;
    case PartCat::Fuel: t.fuel = l == 1 ? FuelType::Pump95 : l == 2 ? FuelType::E85 : FuelType::Race100; break;
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
    const double scale = std::clamp(0.6 + carPrice(v) / 40000.0, 0.6, 4.0);   // pahali arabanin parcasi pahali
    return (int)(std::round(o[level].basePrice * scale / 10.0) * 10.0);
}

bool partAvailable(PartCat c, int level, const VehicleDef& v, std::string* why) {
    const EngineDef& e = engineTable()[v.engine];
    if (c == PartCat::Turbo && level > 0 && e.induction == Induction::Supercharger) {
        if (why) *why = "KOMPRESORLU MOTORA TURBO YOK";
        return false;
    }
    return true;
}

int sellPrice(const OwnedCar& c) {
    return (int)(std::round((0.65 * carPrice(*findVehicle(c.carId)) + 0.4 * c.paidParts) / 10.0) * 10.0);
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

int racePrize(const VehicleDef& opponent, bool won) {
    return won ? (int)(std::round((250.0 + carPrice(opponent) * 0.04) / 10.0) * 10.0) : 0;
}

Opponent pickOpponent(int playerCarId, const Tune& playerTune, uint32_t seed) {
    const VehicleDef& pv = *findVehicle(playerCarId);
    const double target = performanceIndex(pv, playerTune);
    Tune presets[3];
    presets[1].intake = 1; presets[1].exhaust = 1; presets[1].tires = TireType::SemiSlick; presets[1].diff = DiffType::OneAndHalfWay;
    presets[2].intake = 2; presets[2].exhaust = 2; presets[2].ecu = 1; presets[2].tires = TireType::DragSlick;
    presets[2].clutch = 1; presets[2].axles = 1; presets[2].diff = DiffType::OneAndHalfWay;
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

std::string tuneSummary(const Tune& t) {
    std::string s;
    auto add = [&](const char* p) { if (!s.empty()) s += ' '; s += p; };
    if (t.tires == TireType::DragSlick) add("SLICK"); else if (t.tires == TireType::SemiSlick) add("YARI-SLICK"); else add("SOKAK");
    if (t.clutch) add(t.clutch == 1 ? "ST1" : t.clutch == 2 ? "ST2" : "ST3");
    if (t.axles) add(t.axles == 1 ? "KM-AKS" : "300M");
    if (t.diff != DiffType::Open) add(t.diff == DiffType::OneAndHalfWay ? "1.5W" : t.diff == DiffType::TwoWay ? "2W" : "SPOOL");
    if (t.turbo) add(t.turbo == 1 ? "TURBO-K" : "TURBO-B");
    if (t.ecu) add("ECU");
    if (t.intake || t.exhaust) add("EMME/EGZ");
    if (t.weight) add("HAFIF");
    if (t.drySump) add("KURU-KRT");
    if (t.fuel == FuelType::E85) add("E85");
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
    if (!partAvailable(c, level, v, why)) return false;
    const int p = partPrice(c, level, v);
    if (money < p) { if (why) *why = "PARA YETMIYOR"; return false; }
    money -= p;
    oc.paidParts += p;
    setPartLevel(oc.tune, c, level, v);
    return true;
}

void Career::recordRace(const VehicleDef& opponent, bool won, double et, long* prizeOut) {
    // Ayni rakibe ust uste galibiyet: odul %25 azalir (en az %25)
    if (opponent.id != lastOppId) { lastOppId = opponent.id; sameOppWins = 0; }
    const double decay = won ? std::max(0.25, 1.0 - 0.25 * sameOppWins) : 1.0;
    const long prize = (long)(racePrize(opponent, won) * decay);
    if (won) ++sameOppWins;
    money += prize; earnings += prize;
    ++races; if (won) ++wins;
    OwnedCar& oc = car();
    ++oc.races; if (won) ++oc.wins;
    if (et > 0 && (oc.bestEt <= 0 || et < oc.bestEt)) oc.bestEt = et;
    if (prizeOut) *prizeOut = prize;
}

// ------------------------------------------------------------------ kayit
namespace {
uint32_t fnv1a(const std::string& s) {
    uint32_t h = 2166136261u;
    for (unsigned char ch : s) { h ^= ch; h *= 16777619u; }
    return h;
}
} // namespace

void Career::recordDamage(bool axleBroke, double bearingDamage, bool bearingSpun) {
    OwnedCar& c = car();
    c.axleBroken = c.axleBroken || axleBroke;
    c.engineWear = bearingSpun ? 1.0 : std::clamp(c.engineWear + std::max(0.0, bearingDamage), 0.0, 1.0);
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

long Career::repairCost() const {
    const OwnedCar& c = car();
    const VehicleDef& v = *findVehicle(c.carId);
    long cost = 0;
    // Aks: takili seviyenin parca fiyati (stok aks icin 1. seviye fiyatinin yarisi) + iscilik
    if (c.axleBroken) cost += std::max(150L, (long)(c.tune.axles > 0 ? partPrice(PartCat::Axles, c.tune.axles, v)
                                                                       : partPrice(PartCat::Axles, 1, v) / 2)) + 120;
    // Motor: yatak degisimi; tam sarmada (1.0) krank taslama + revizyon ~ arac fiyatinin %18'i
    if (c.engineWear > 0.02) cost += (long)(250 + carPrice(v) * 0.18 * c.engineWear);
    return cost;
}

bool Career::repairCurrent(std::string* why) {
    const long cost = repairCost();
    if (cost <= 0) { if (why) *why = "HASAR YOK"; return false; }
    if (money < cost) { if (why) *why = "PARA YETMIYOR"; return false; }
    money -= cost;
    car().axleBroken = false; car().engineWear = 0.0;
    return true;
}

std::string Career::serialize() const {
    std::ostringstream o;
    o << "ZEHRAKINIK_KAYIT " << kVersion << "\n";
    o << "money=" << money << "\ncurrent=" << current << "\nraces=" << races << "\nwins=" << wins
      << "\nearnings=" << earnings << "\ntreePro=" << (treePro ? 1 : 0) << "\nstreak=" << lastOppId << ";" << sameOppWins << "\nflow=" << bestFlow << "\n";
    char buf[256];
    for (const OwnedCar& c : cars) {
        const Tune& t = c.tune;
        std::snprintf(buf, sizeof buf, "car=%d;%d;%d;%.3f;%d;%d;%.2f;%d;%d;%d;%.4f;%d;%d;%d;%d;%d;%d;%d\n", c.carId, c.races,
                      c.wins, c.bestEt, c.paidParts, (int)t.tires, t.psi, t.clutch, t.axles, (int)t.diff, t.finalDrive,
                      t.weight, t.intake, t.exhaust, t.ecu, t.turbo, t.drySump ? 1 : 0, (int)t.fuel);
        o << buf;
        if (c.damaged()) { std::snprintf(buf, sizeof buf, "dmg=%d;%.4f\n", c.axleBroken ? 1 : 0, c.engineWear); o << buf; }
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
        else if (k == "streak") {
            if (std::sscanf(v.c_str(), "%d;%d", &c.lastOppId, &c.sameOppWins) != 2) return false;
            c.sameOppWins = std::clamp(c.sameOppWins, 0, 100);
        }
        else if (k == "dmg" && !c.cars.empty()) {          // onceki araca ait hasar
            int ax = 0; double ew = 0;
            if (std::sscanf(v.c_str(), "%d;%lf", &ax, &ew) != 2) return false;
            c.cars.back().axleBroken = ax != 0;
            c.cars.back().engineWear = std::clamp(ew, 0.0, 1.0);
        }
        else if (k == "car") {
            OwnedCar oc; int tires, diff, dry, fuel;
            if (std::sscanf(v.c_str(), "%d;%d;%d;%lf;%d;%d;%lf;%d;%d;%d;%lf;%d;%d;%d;%d;%d;%d;%d", &oc.carId, &oc.races, &oc.wins,
                            &oc.bestEt, &oc.paidParts, &tires, &oc.tune.psi, &oc.tune.clutch, &oc.tune.axles, &diff,
                            &oc.tune.finalDrive, &oc.tune.weight, &oc.tune.intake, &oc.tune.exhaust, &oc.tune.ecu,
                            &oc.tune.turbo, &dry, &fuel) != 18) return false;
            if (oc.carId < 1 || oc.carId > nCars) return false;
            // Aralik denetimi: disaridan degistirilmis kayitta bile fizik gecerli degerler alsin
            oc.tune.tires = (TireType)std::clamp(tires, 0, 2);
            oc.tune.diff = (DiffType)std::clamp(diff, 0, 3);
            oc.tune.clutch = std::clamp(oc.tune.clutch, 0, 3);
            oc.tune.axles = std::clamp(oc.tune.axles, 0, 2);
            oc.tune.weight = std::clamp(oc.tune.weight, 0, 3);
            oc.tune.intake = std::clamp(oc.tune.intake, 0, 2);
            oc.tune.exhaust = std::clamp(oc.tune.exhaust, 0, 2);
            oc.tune.ecu = std::clamp(oc.tune.ecu, 0, 1);
            oc.tune.turbo = std::clamp(oc.tune.turbo, 0, 2);
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
