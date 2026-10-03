#include "RunField.h"

#include "garage/VehicleCatalog.h"
#include "sim/PartTables.h"
#include "sim/VehicleSim.h"

#include <algorithm>
#include <cmath>

namespace zk {

namespace {
uint32_t mix(uint32_t x) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x; }
const char* const kFirst[] = {"KARA", "DELI", "SESSIZ", "GECE", "TOZLU", "YAMAN", "KURT", "ATES", "BUZ", "FIRTINA", "ZEHIR", "COBAN",
                              "DUMAN", "KARTAL", "TILKI", "BOZ", "AKREP", "KASIRGA", "YILDIZ", "PAS"};
const char* const kLast[] = {"MEHMET", "AYSE", "CAN", "SELIN", "MURAT", "DENIZ", "EMRE", "BURAK", "ELIF", "KEMAL", "ZEYNEP", "ONUR",
                             "ECE", "TOLGA", "NUR", "SINAN", "IREM", "HAKAN", "DERYA", "UMUT"};
// Tarz: yakit (BSFC) carpani, seyir hizi orani (azami hiza gore), en buyuk gaz
struct StyleK { double bsfc, cruise, thr; };
const StyleK kStyle[StyleCount] = {{1.05, 0.88, 0.90}, {1.32, 1.00, 1.00}, {0.86, 0.70, 0.62}, {1.00, 0.86, 0.90}, {1.38, 1.00, 1.00}};
}

const char* runStyleName(int style) {
    static const char* const n[StyleCount] = {"DENGELI", "DIP GAZ", "EKO", "STOK", "CANAVAR"};
    return n[std::clamp(style, 0, StyleCount - 1)];
}

double RunField::tankFor(int carId) {
    const VehicleDef* v = findVehicle(carId);
    if (!v) return 50.0;
    double t = 22.0 + 0.026 * v->massKg;                                // ~1000 kg: 48 L, ~1600: 64 L, ~2200: 79 L
    if (v->body == Body::Super) t += 10.0;
    if (v->body == Body::SUV || v->body == Body::Pickup || v->body == Body::Van) t += 8.0;
    return std::clamp(std::round(t / 5.0) * 5.0, 40.0, 90.0);
}

void RunField::init(const std::vector<RunEntrant>& field, const RoadPath& road, double startS, double compression, uint32_t seed, int playerSlot) {
    comp_ = std::max(1.0, compression); startS_ = startS;
    r_.clear();
    const int lanes = road.lanesFwd(startS);
    for (size_t i = 0; i < field.size(); ++i) {
        const RunEntrant& e = field[i];
        const VehicleDef* v = findVehicle(e.carId);
        if (!v) continue;
        Runner R;
        R.carId = e.carId; R.style = std::clamp(e.style, 0, StyleCount - 1);
        const uint32_t h = mix(seed * 7919u + (uint32_t)i * 104729u);
        R.name = std::string(kFirst[h % 20]) + " " + kLast[(h >> 8) % 20];
        VehicleSimConfig cfg; cfg.car = v; cfg.tune = &e.tune;
        const VehicleSim sim(cfg);
        double hp = 0;
        for (auto* c : {&sim.engineSpec().lowCam, &sim.engineSpec().highCam})
            for (auto& p : *c) if (p.first <= sim.engineSpec().redlineRpm) hp = std::max(hp, p.second * p.first / 7120.9);
        R.powerW = hp * 745.7 * 0.86;                                   // tekerdeki guc (aktarma kaybi)
        R.massKg = sim.baseMassKg() + 40.0;
        R.cdA = sim.cdA();
        const TireOpt& T = tireTable()[std::clamp(e.tune.tireSel > 0 ? e.tune.tireSel : (int)e.tune.tires, 0, (int)tireTable().size() - 1)];
        R.mu = (e.tune.tires == TireType::DragSlick ? 1.05 : e.tune.tires == TireType::SemiSlick ? 1.12 : 1.0) * T.grip;
        R.vmax = std::cbrt(R.powerW / (0.5 * 1.2 * R.cdA + 1e-6));        // P = 1/2 rho CdA v^3 (yuvarlanma ihmal)
        const StyleK& k = kStyle[R.style];
        R.bsfcMul = k.bsfc; R.cruise = k.cruise; R.thrMax = k.thr;
        R.tankL = R.fuelL = tankFor(e.carId);
        // Grid: seritler boyunca 9 m arayla sira (oyuncu ayrica yerlestirilir)
        const int slot = (int)i + (playerSlot >= 0 && (int)i >= playerSlot ? 1 : 0);   // oyuncunun yeri atlanir
        const int row = slot / std::max(1, lanes), col = slot % std::max(1, lanes);
        R.s = startS - 12.0 - row * 9.0;
        R.li = col;
        R.lane = road.laneOffset(std::max(0.0, R.s), false, R.li);
        r_.push_back(R);
    }
}

void RunField::update(double dt, const RoadPath& road, const std::vector<double>& stations, double stationLen, double refuelLps,
                      double goalS, double raceT, double playerS, double playerLat, double playerV) {
    // Ondeki engeller icin s sirali indeks (her karede; 200 arac icin ucuz)
    std::vector<int> ord(r_.size());
    for (size_t i = 0; i < r_.size(); ++i) ord[i] = (int)i;
    std::sort(ord.begin(), ord.end(), [&](int a, int b) { return r_[a].s < r_[b].s; });
    std::vector<int> rank(r_.size());
    for (size_t k = 0; k < ord.size(); ++k) rank[ord[k]] = (int)k;
    auto nextStationS = [&](double s) { for (double st : stations) if (st + stationLen > s) return st; return -1.0; };
    for (size_t i = 0; i < r_.size(); ++i) {
        Runner& R = r_[i];
        if (R.finished) { R.v = std::max(0.0, R.v - 4.0 * dt); R.s += R.v * dt; continue; }
        const int nf = road.lanesFwd(R.s);
        R.li = std::min(R.li, nf - 1);
        const RoadPoint here = road.at(R.s);
        // ---- benzinlik karari (istasyona 400 m kala) ----
        const double st = nextStationS(R.s);
        if (R.pitS < 0 && st > 0 && st - R.s < 400.0 && st - R.s > 0.0) {
            const double perM = (R.usedL + 1e-6) / std::max(300.0, R.s - startS_);
            double after = goalS - R.s;
            for (double s2 : stations) if (s2 > st + 10.0) { after = s2 - R.s; break; }
            const bool need = R.fuelL < perM * after * 1.12;
            const bool habit = R.style == StyleFlatOut && R.fuelL < 0.6 * R.tankL;   // dip gaz: her benzinlik
            if (need || habit) { R.pitS = st + stationLen * 0.5; R.li = 0; }
        }
        // ---- hedef hiz: viraj (tutus), tarz seyri, onde engel ----
        double vT = R.vmax * R.cruise;
        if (R.style == StyleEco) vT = std::min(vT, 36.0);                // eko: ~130 km/h
        const double aLat = R.mu * 9.81 * 0.68, aBrk = 5.0 * std::min(1.0, R.mu);   // insan payi: lastigin sinirinda surmez
        for (double d = 0; d < R.v * R.v / (2 * aBrk) + 40.0; d += 10.0) {
            const double kk = std::max(std::fabs(road.at(R.s + d).curvature), 1e-4);
            vT = std::min(vT, std::sqrt(aLat / kk + 2 * aBrk * d));
        }
        if (R.pitS > 0) {
            const double dist = R.pitS - R.s;
            vT = std::min(vT, dist > 0 ? std::sqrt(2.0 * 4.0 * dist) : 0.0);
            if (dist < 2.0 || (R.v < 0.5 && dist < 25.0)) {
                R.v = 0.0; R.refueling = true;
                R.fuelL = std::min(R.tankL, R.fuelL + refuelLps * dt);
                if (R.fuelL >= R.tankL - 0.05) { R.pitS = -1; R.refueling = false; ++R.pits; }
                continue;
            }
        }
        R.refueling = false;
        // Onde ayni seritte yavas arac: sollama (bos yan serit) ya da takip
        double leadV = -1;
        for (size_t k = rank[i] + 1; k < ord.size() && k < (size_t)rank[i] + 12; ++k) {
            const Runner& O = r_[ord[k]];
            if (O.s - R.s > 30.0 + R.v) break;
            if (O.li == R.li && O.v < R.v) { leadV = O.v; break; }
        }
        if (playerS > R.s && playerS - R.s < 25.0 + R.v && std::fabs(playerLat - R.lane) < 2.0 && playerV < R.v) leadV = leadV < 0 ? playerV : std::min(leadV, playerV);
        if (leadV >= 0 && R.pitS < 0) {
            int best = -1;
            for (int cand : {R.li + 1, R.li - 1}) {
                if (cand < 0 || cand >= nf) continue;
                bool free = true;
                for (size_t k = 0; k < ord.size(); ++k) {
                    const Runner& O = r_[ord[k]];
                    if (&O != &R && O.li == cand && std::fabs(O.s - R.s) < 14.0) { free = false; break; }
                }
                if (free && std::fabs(playerLat - road.laneOffset(R.s, false, cand)) < 2.0 && std::fabs(playerS - R.s) < 12.0) free = false;
                if (free) { best = cand; break; }
            }
            if (best >= 0) R.li = best; else vT = std::min(vT, leadV);
        } else if (leadV < 0 && R.li > 0 && (i + (size_t)raceT) % 7 == 0) R.li -= 1;   // ara sira saga don
        // ---- boylamsal: guc / tutus siniri, direnc, yokus ----
        const double V = std::max(R.v, 3.0);
        const double drag = 0.5 * 1.2 * R.cdA * R.v * R.v / R.massKg, roll = 0.012 * 9.81, gg = 9.81 * here.grade;
        double a, pUse = 0;
        if (R.dry || R.fuelL <= 0.0) { R.dry = true; a = -drag - roll - gg; }
        else if (R.v < vT - 0.3) {
            const double aPow = R.powerW * R.thrMax / (R.massKg * V), aTr = R.mu * 9.81 * 0.55;
            const double aDrive = std::min(aPow, aTr);
            a = aDrive - drag - roll - gg;
            pUse = R.massKg * aDrive * R.v;
        } else if (R.v > vT + 0.3) a = -std::min(aBrk, (R.v - vT) * 2.0) - drag;
        else { a = 0; pUse = std::max(0.0, R.massKg * (drag + roll + gg) * R.v); }   // seyir: direnci karsila
        R.v = std::max(0.0, R.v + a * dt);
        R.s += R.v * dt;
        // Yakit: tekerdeki guc / aktarma -> motor; BSFC ~0.26 kg/kWh x tarz; rolanti; etap sikistirmasi
        const double kgps = (pUse / 0.86 / 1000.0) * 0.26 * R.bsfcMul / 3600.0 + 0.00025;
        const double dl = kgps / 0.745 * dt * comp_;
        R.fuelL = std::max(0.0, R.fuelL - dl); R.usedL += dl;
        // Yanal: hedef seride yumusak gecis; gorsel direksiyon ve teker
        const double target = road.laneOffset(R.s, false, R.li);
        const double dl2 = std::clamp(target - R.lane, -2.2 * dt, 2.2 * dt);
        R.lane += dl2;
        R.steer = (float)std::clamp(dl2 / std::max(dt, 1e-3) * 0.05 + here.curvature * 2.6, -0.5, 0.5);
        R.spin += std::min(R.v * dt / 0.31, 0.55);                        // gorsel donus (karede sinirli)
        if (R.s >= goalS) { R.finished = true; R.finishT = raceT; }
    }
}

int RunField::playerPosition(double playerS, bool playerFinished, double playerT) const {
    int pos = 1;
    for (const Runner& R : r_) {
        if (playerFinished) { if (R.finished && R.finishT < playerT) ++pos; }
        else if (R.finished || R.s > playerS) ++pos;
    }
    return pos;
}

} // namespace zk
