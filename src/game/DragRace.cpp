#include "DragRace.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <mutex>

namespace zk {

namespace {
constexpr double kRollout = 0.178;     // stage isigini terk etme (~7 inc)
constexpr double kClutchHold = 0.60;   // bu degerin ustunde debriyaj basili: arac frende tutulur
constexpr double k60 = 18.288, k330 = 100.584, k660 = 201.168, k1000 = 304.8;

std::string fmt(const char* f, double a) { char b[96]; std::snprintf(b, sizeof b, f, a); return b; }

// Pist hazirligi (VHT/yapiskan zemin): sokak lastigi en cok kazanir, slick en az. Yalniz oyundaki drag pisti;
// zehra_sim (Faz 1 referansi, regresyon) duz asfalt kalir. Kalibrasyon: gercek stok ET'ler (devir notu).
double trackPrep(const Tune* t) {
    const TireType tires = t ? t->tires : TireType::DragSlick;
    return tires == TireType::Street ? 1.35 : tires == TireType::SemiSlick ? 1.20 : 1.10;
}
} // namespace

DragRace::DragRace(int playerCarId, int opponentCarId, TreeType tree, uint32_t seed, bool withBurnout,
                   const Tune* playerTune, const Tune* opponentTune)
    : tree_(tree), phase_(withBurnout ? RacePhase::Burnout : RacePhase::Staging), rng_(seed * 2654435761u + 7u) {
    lanes_.resize(2);
    const int ids[2] = {playerCarId, opponentCarId};
    const Tune* tunes[2] = {playerTune, opponentTune};
    for (int i = 0; i < 2; ++i) {
        LaneState& L = lanes_[i];
        L.car = findVehicle(ids[i]);
        VehicleSimConfig cfg;
        cfg.car = L.car;
        if (tunes[i]) { tunes_[i] = *tunes[i]; cfg.tune = &tunes_[i]; }
        cfg.road = "drag";
        cfg.fuelLiters = 8.0;
        L.sim = std::make_unique<VehicleSim>(cfg);
        L.sim->setSurfaceMu(trackPrep(cfg.tune));
        L.sim->setTractionControl(false);                              // drag: TC kapali (oyuncu istege bagli acar)
        L.sim->powertrain().setGear(1);
        L.sim->powertrain().setClutchPedal(1.0);
    }
    // Rakibin kalkis planini arka planda hesapla (masaustu ~0.8 s; stage + agac en az ~2 s surer). Tune kopyasi
    // verilir: yaris nesnesi erken silinse de thread gecerli veri okur.
    {
        const VehicleDef* car = lanes_[1].car;
        const bool hasTune = opponentTune != nullptr;
        const Tune t = hasTune ? *opponentTune : Tune{};
        lanes_[1].plan = std::async(std::launch::async, [car, hasTune, t] { return planLaunch(car, hasTune ? &t : nullptr); }).share();
    }
    lanes_[1].aiReaction = 0.06 + (rnd() % 1000) / 1000.0 * 0.20;   // .060 - .260
    treeDelay_ = 0.8 + (rnd() % 1000) / 1000.0 * 1.2;
}

uint32_t DragRace::rnd() { rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5; return rng_; }

double DragRace::greenTime() const {
    if (treeStart_ < 0) return 1e9;
    return treeStart_ + (tree_ == TreeType::Pro ? 0.4 : 1.5);
}

int DragRace::treeLights() const {
    int m = 0;
    if (treeStart_ >= 0 && clock_ >= treeStart_) {
        const double t = clock_ - treeStart_;
        if (tree_ == TreeType::Pro) { if (t < 0.4) m |= 7; }
        else {
            if (t >= 0.0 && t < 1.5) m |= 1;
            if (t >= 0.5 && t < 1.5) m |= 2;
            if (t >= 1.0 && t < 1.5) m |= 4;
        }
        if (clock_ >= greenTime()) m |= 8;
    }
    if (lanes_[0].slip.redLight) m |= 16;
    if (lanes_[1].slip.redLight) m |= 32;
    return m;
}

void DragRace::skipBurnout() {
    if (phase_ == RacePhase::Burnout) { phase_ = RacePhase::Staging; phaseT_ = 0.0; events_.push_back("Burnout bitti - stage'e gec"); }
}

void DragRace::advance(double realDt, const PlayerControls& pc) {
    if (phase_ == RacePhase::Finished && lanes_[0].sim->speed() < 0.1 && lanes_[1].sim->speed() < 0.1) return;
    LaneState& P = lanes_[0];
    PowertrainCore& pt = P.sim->powertrain();
    const Gearbox box = P.sim->gearboxType();
    // ---- vites istekleri (kare basina bir kez; otopilotta oyuncu girdisi yok sayilir) ----
    if (autopilot_) { P.grind = false; }
    else if (box == Gearbox::HPattern && pc.requestedGear >= 0 && pc.requestedGear != pt.gear()) {
        const int g = std::min(pc.requestedGear, pt.gearCount());
        if (pc.clutch >= 0.40 || g == 0) { pt.setGear(g); P.grind = false; }   // %40 debriyaj yeter; bosa almak debriyajsiz
        else if (!P.grind) { P.grind = true; events_.push_back("DISLI CITIRTISI! Vites icin debriyaja bas"); }
    }
    if (pc.requestedGear < 0 || pc.requestedGear == pt.gear()) P.grind = false;
    if (!autopilot_ && (box == Gearbox::Dogbox || box == Gearbox::DCT) && pc.paddle != 0 && prevPaddle_ == 0) {
        const int g = std::clamp(pt.gear() + pc.paddle, 1, pt.gearCount());
        if (g != pt.gear()) { pt.setGear(g); P.shiftT = 0.0; }
    }
    prevPaddle_ = pc.paddle;

    acc_ += std::min(realDt, 0.1);
    while (acc_ >= kStep) {
        stepPhysics(pc);
        clock_ += kStep; phaseT_ += kStep;
        acc_ -= kStep;
    }
}

void DragRace::playerDrive(LaneState& L, const PlayerControls& pc, VehicleInputs& in) {
    PowertrainCore& pt = L.sim->powertrain();
    const Gearbox box = L.sim->gearboxType();
    double throttle = pc.throttle, clutch = pc.clutch;
    const bool beforeLeave = !L.left;

    // Otomatik debriyaj: DCT (launch control) ve tork konvertorlu otomatik
    if (box == Gearbox::DCT || box == Gearbox::TorqueConverter) {
        const bool holding = pc.brake > 0.5 && beforeLeave;
        if (box == Gearbox::TorqueConverter) {
            // Tork konvertoru (PowertrainCore): frende beklerken N + devir stall'da (brake-torque karsiligi,
            // defaultLaunchRpm); fren birakilinca akiskan kavrama — ani kilitlenme/darbe yok
            clutch = holding ? 1.0 : 0.0;
            if (!holding && pt.gear() < pt.gearCount() && pt.rpm() > L.sim->shiftRpm() && L.shiftT < 0) {
                pt.setGear(pt.gear() + 1); L.shiftT = 0.0;
            }
        } else {
            // DCT launch control: fren birakilinca kavrama ~150 ms'de rampa ile kapanir
            if (holding) L.autoRelease = -1.0;
            else if (L.autoRelease < 0.0) L.autoRelease = clock_;
            clutch = holding ? 1.0 : std::max(0.0, 1.0 - (clock_ - L.autoRelease) / 0.15);
        }
        in.held = holding;
    } else {
        in.held = clutch > kClutchHold && beforeLeave && phase_ != RacePhase::Run;
    }
    // Stage/agac: oyuncu debriyaja (DCT/otomatikte frene) ilk kez basana kadar arac frende tutulur ve motor
    // bosta kalir; boylece stage'e girerken rolantide surunup haksiz kirmizi isik yakilmaz.
    if ((phase_ == RacePhase::Staging || phase_ == RacePhase::Tree) && beforeLeave) {
        const bool autoBox = box == Gearbox::DCT || box == Gearbox::TorqueConverter;
        if (autoBox ? pc.brake > 0.5 : pc.clutch > kClutchHold) L.armed = true;
        if (!L.armed) { clutch = 1.0; in.held = true; }
    }
    if (phase_ == RacePhase::Burnout) in.held = true;          // line-lock: on frenler kilitli
    in.brake = pc.brake * (in.held ? 0.0 : 1.0);

    // Vites gecisi: dogbox 35 ms ateslemesi kesme, DCT 60 ms ortusme
    L.cutIgnition = false;
    if (L.shiftT >= 0.0) {
        const double dur = box == Gearbox::Dogbox ? 0.035 : box == Gearbox::DCT ? 0.060 : 0.30;
        if (box == Gearbox::Dogbox) { throttle = 0.0; L.cutIgnition = true; }
        L.shiftT += kStep;
        if (L.shiftT > dur) L.shiftT = -1.0;
    }
    pt.setTwoStep(pc.twoStep && beforeLeave && phase_ != RacePhase::Burnout, L.sim->defaultLaunchRpm());
    if (L.slip.finished) { throttle = 0.0; clutch = 1.0; in.brake = std::max(in.brake, 0.45); }   // parasut + fren
    pt.setThrottle(throttle);
    pt.setClutchPedal(clutch);
}

void DragRace::setOpponentHandicap(double k) {
    LaneState& L = lanes_[1];
    L.aiSlow = std::max(1.0, k);                                          // 1.6: otomatik debriyaj cezasiyla ayni (RoadCar slowClutch)
    if (k > 1.0) L.aiReaction = 0.06 + (k - 1.0) * 0.25 + (rnd() % 1000) / 1000.0 * 0.15;   // 1.6: .21-.36, patron 1.05: .07-.22
}

void DragRace::aiDrive(LaneState& L, VehicleInputs& in) {
    PowertrainCore& pt = L.sim->powertrain();
    const Gearbox box = L.sim->gearboxType();
    const double go = greenTime() + L.aiReaction;
    if (L.slip.finished || L.slip.broke) {
        pt.setThrottle(0.0); pt.setClutchPedal(1.0); in.brake = 0.45;
        return;
    }
    // Kalkis plani agac fazinin basinda uygulanir (sabit simulasyon ani: yaris deterministik kalir; plan hazir
    // degilse burada beklenir). Stage'de gaz kapali, plan gerekmez.
    if (L.aiLaunchRpm < 0.0 && phase_ != RacePhase::Burnout && phase_ != RacePhase::Staging) {
        const LaunchPlan p = L.plan.valid() ? L.plan.get() : planLaunch(L.car, L.sim->config().tune);
        L.aiLaunchRpm = p.rpm; L.aiRelease = p.release;
    }
    if (clock_ < go || phase_ == RacePhase::Burnout || phase_ == RacePhase::Staging) {
        pt.setGear(1);
        // Debriyaj basili (otomatikte N) + 2-step (plandaki kalkis devri; otomatikte stall devri)
        pt.setClutchPedal(1.0);
        // Agacta ve yesilden sonra tepki suresince tam gaz (eskiden tepki suresinde faz Run oldugu icin gaz
        // birakiliyordu: YZ yarim gazla kalkip bogulabiliyordu)
        pt.setThrottle(phase_ == RacePhase::Tree || phase_ == RacePhase::Run ? 1.0 : 0.0);
        pt.setTwoStep(true, L.aiLaunchRpm > 0.0 ? L.aiLaunchRpm : L.sim->defaultLaunchRpm());
        in.held = true;
        return;
    }
    (void)box;
    aiRun(L, clock_ - go);
}

// Kalkis sonrasi YZ surusu (yaris ve kalkis plani denemeleri ortak). t: kalkistan beri (s)
void DragRace::aiRun(LaneState& L, double t) {
    PowertrainCore& pt = L.sim->powertrain();
    const Gearbox box = L.sim->gearboxType();
    const bool tc = box == Gearbox::TorqueConverter;
    pt.setTwoStep(false, 0.0);
    // Kalkis: manuel/dogbox plandaki surede debriyaj birakma (pedal 1 -> 0; kavrama bolgesi bunun ~%30'u),
    // DCT launch control 150 ms rampa (oyuncuyla ayni), otomatik konvertor
    double clutch = tc ? 0.0 : std::max(0.0, 1.0 - t / (box == Gearbox::DCT ? 0.15 : L.aiRelease * L.aiSlow));
    const double kap = 0.5 * (L.sim->wheel(L.sim->drivenLeft()).kappa() + L.sim->wheel(L.sim->drivenRight()).kappa());
    if (t > 0.12) L.aiFoot = std::clamp(L.aiFoot + kStep * 8.0 * (0.12 - kap), pt.rpm() < 5500.0 ? 1.0 : 0.35, 1.0);
    double thr = L.aiFoot;
    L.cutIgnition = false;
    if (tc) {
        // Otomatik: vites aninda, en az 0.30 s arayla (oyuncu ile ayni)
        if (L.shiftT < 0.0 && pt.rpm() > L.sim->shiftRpm() && pt.gear() < pt.gearCount()) { pt.setGear(pt.gear() + 1); L.shiftT = 0.0; }
        if (L.shiftT >= 0.0) { L.shiftT += kStep; if (L.shiftT > 0.30) L.shiftT = -1.0; }
        pt.setClutchPedal(clutch);
        pt.setThrottle(thr);
        return;
    }
    // Aks hissi: kalkis/vites sonrasi aks gerilmesi kopma sinirinin %85'ini asarsa debriyaji ~80 ms kaydir,
    // gazi hafiflet (deneyimli surucunun "feather"i). Olculdu: stok guclu araclarda aks kirilmasini azaltir.
    {
        const DrivetrainFailure& f = L.sim->failure();
        const double ratio = std::max(f.shearMPa(0), f.shearMPa(1)) / f.axle().tauUltMPa;
        if (ratio > 0.85) L.aiFeatherT = 0.08;
        if (L.aiFeatherT > 0.0) {
            L.aiFeatherT -= kStep;
            clutch = std::max(clutch, 0.45);                              // isirma bolgesinde kaydir
            thr = std::min(thr, 0.7);
        }
    }
    if (L.shiftT < 0.0 && pt.rpm() > L.sim->shiftRpm() && pt.gear() < pt.gearCount()) L.shiftT = 0.0;
    if (L.shiftT >= 0.0 && (box == Gearbox::Dogbox || box == Gearbox::DCT)) {
        if (L.shiftT == 0.0) pt.setGear(pt.gear() + 1);
        const double dur = box == Gearbox::Dogbox ? 0.035 : 0.060;
        if (box == Gearbox::Dogbox) { thr = 0.0; L.cutIgnition = true; }
        L.shiftT += kStep;
        if (L.shiftT > dur) L.shiftT = -1.0;
    } else if (L.shiftT >= 0.0) {                                       // H-desen: ~270 ms (hata payiyla x aiSlow)
        const double k = L.aiSlow;
        if (L.shiftT < 0.10 * k)      { clutch = 1.0; thr = 0.0; }
        else if (L.shiftT < 0.17 * k) { clutch = 1.0; thr = 0.3; if (L.shiftT - kStep < 0.10 * k) pt.setGear(pt.gear() + 1); }
        else if (L.shiftT < 0.27 * k) { clutch = 1.0 - (L.shiftT - 0.17 * k) / (0.10 * k); }
        else L.shiftT = -1.0;
        if (L.shiftT >= 0.0) L.shiftT += kStep;
    }
    pt.setClutchPedal(clutch);
    pt.setThrottle(thr);
}

// Kalkis plani: kalkis devri x debriyaj birakma suresi adaylarini ayni fizikle 60 ft'e kadar dener, en hizlisini
// secer (aks gerilmesi kopmanin %92'sini asan aday elenir). Deterministik; arac+parca basina onbellek.
// Neden: geri beslemeli gaz/debriyaj kontrolu dusuk hizda lastik gecikmesi (~0.15 s) yuzunden salinir; usta
// pilot da kalkisi ezbere ayarlar. Olculdu: eski 120 ms dump sokak lastiginde 60 ft ~3.2 s veriyordu.
namespace {
// Plan / tahmin onbellegi anahtari: arac + kalkisi etkileyen tum parcalar
std::string tuneKey(const VehicleDef* car, const Tune* tune) {
    char key[160];
    const Tune t = tune ? *tune : Tune{};
    std::snprintf(key, sizeof key, "%d|%d|%d|%.1f|%d|%d|%d|%.3f|%d|%d|%d|%d|%d|%d|%d", car->id, tune ? 1 : 0, (int)t.tires, t.psi,
                  t.clutch, t.axles, (int)t.diff, t.finalDrive, t.weight, t.intake, t.exhaust, t.ecu, t.turbo, t.drySump ? 1 : 0, (int)t.fuel);
    return key;
}
} // namespace

DragRace::LaunchPlan DragRace::planLaunch(const VehicleDef* car, const Tune* tune) {
    static std::map<std::string, LaunchPlan> cache;
    static std::mutex cacheLock;                                       // plan arka plan thread'inde de hesaplanir
    const std::string key = tuneKey(car, tune);
    const Tune t = tune ? *tune : Tune{};
    {
        std::lock_guard<std::mutex> g(cacheLock);
        if (auto it = cache.find(key); it != cache.end()) return it->second;
    }

    auto trial = [&](double rpm, double release, double holdS) {
        LaneState L;
        VehicleSimConfig cfg; cfg.car = car; cfg.tune = tune ? &t : nullptr; cfg.road = "drag"; cfg.fuelLiters = 8.0;
        L.sim = std::make_unique<VehicleSim>(cfg);
        L.sim->setSurfaceMu(trackPrep(cfg.tune));
        L.sim->setTractionControl(false);
        L.car = car; L.aiLaunchRpm = rpm; L.aiRelease = release;
        PowertrainCore& pt = L.sim->powertrain();
        pt.setGear(1); pt.setClutchPedal(1.0); pt.setThrottle(1.0); pt.setTwoStep(true, rpm);
        VehicleInputs hold; hold.held = true;
        for (int i = 0; i < (int)(holdS / kStep); ++i) L.sim->step(kStep, hold);   // agac: 2-step'te bekle
        double tt = 0.0;
        const double bogRpm = std::max(1.5 * L.sim->engineSpec().idleRpm, 0.2 * L.sim->engineSpec().redlineRpm);
        while (L.sim->distance() < k60 && tt < 8.0) {
            aiRun(L, tt);
            L.sim->step(kStep, VehicleInputs{});
            tt += kStep;
            const DrivetrainFailure& f = L.sim->failure();
            if (f.snapped(0) || f.snapped(1) || f.peakShearMPa() > 0.92 * f.axle().tauUltMPa) return 1e9;
            // Bogulma payi: stop eden ya da bogulma devrine (1.5 x rolanti, en az %20 redline) dusen aday elenir;
            // denemede kil payi toparlanan kalkis yarista ufak farkla stop ediyordu (olculdu: RX-7 slick)
            if (pt.stalled() || (tt > 0.05 && pt.rpm() < bogRpm)) return 1e9;
        }
        return tt;
    };
    LaunchPlan best{0.0, 0.12, 1e9};
    VehicleSimConfig probeCfg; probeCfg.car = car; probeCfg.tune = tune ? &t : nullptr;
    const VehicleSim probe(probeCfg);
    if (probe.gearboxType() == Gearbox::TorqueConverter || probe.gearboxType() == Gearbox::DCT) {
        best = {probe.defaultLaunchRpm(), 0.12, 0.0};                   // otomatik/DCT: kalkis sanzimanda
    } else {
        const double red = probe.engineSpec().redlineRpm, idle = probe.engineSpec().idleRpm;
        for (double f : {0.35, 0.50, 0.65})
            for (double rel : {0.12, 0.6}) {
                const double rpm = std::max(idle + 1000.0, f * red);
                // 2-step kesmesi devri salindirir; yesil salinimin herhangi bir aninda gelebilir -> 2 evre, en kotusu
                double tt = 0.0;
                // Bekleme >= 1.5 s: yaristaki gibi suspansiyon/arac oturmus olmali (0.6 s'de oturmamisti: denemede
                // gecen plan yarista stop ediyordu)
                for (double hold : {1.50, 1.5137}) { tt = std::max(tt, trial(rpm, rel, hold)); if (tt >= 1e9) break; }
                if (tt < best.t60) best = {rpm, rel, tt};
            }
        if (best.t60 >= 1e9) best = {std::max(idle + 1000.0, 0.50 * red), 0.6, 1e9};   // hepsi riskli: orta devir, yavas birakma
    }
    std::lock_guard<std::mutex> g(cacheLock);
    cache[key] = best;
    return best;
}

// Parca onizlemesi: YZ kalkis plani ile tek seritte 1/4 mil (agac/reaksiyon yok, ET stage isigindan).
// Yarisla ayni fizik ve pist hazirligi; bekleme kisa (0.3 s) -> tahmin. Pahali (~1 s): arka planda cagirin.
QuarterEstimate DragRace::estimateQuarter(const VehicleDef* car, const Tune* tune) {
    static std::map<std::string, QuarterEstimate> cache;
    static std::mutex cacheLock;
    const std::string key = tuneKey(car, tune);
    {
        std::lock_guard<std::mutex> g(cacheLock);
        if (auto it = cache.find(key); it != cache.end()) return it->second;
    }
    const LaunchPlan plan = planLaunch(car, tune);
    const Tune t = tune ? *tune : Tune{};
    LaneState L;
    VehicleSimConfig cfg; cfg.car = car; cfg.tune = tune ? &t : nullptr; cfg.road = "drag"; cfg.fuelLiters = 8.0;
    L.sim = std::make_unique<VehicleSim>(cfg);
    L.sim->setSurfaceMu(trackPrep(cfg.tune));
    L.sim->setTractionControl(false);
    L.car = car; L.aiLaunchRpm = plan.rpm; L.aiRelease = plan.release;
    PowertrainCore& pt = L.sim->powertrain();
    pt.setGear(1); pt.setClutchPedal(1.0); pt.setThrottle(1.0); pt.setTwoStep(true, std::max(plan.rpm, 1.0));
    VehicleInputs hold; hold.held = true;
    for (int i = 0; i < (int)(0.3 / kStep); ++i) L.sim->step(kStep, hold);
    QuarterEstimate e;
    double tt = 0.0, t0 = -1.0;
    while (tt < 40.0) {
        aiRun(L, tt);
        L.sim->step(kStep, VehicleInputs{});
        tt += kStep;
        const double d = L.sim->distance();
        if (t0 < 0 && d > kRollout) t0 = tt;
        if (e.sixtyFt < 0 && d >= k60) e.sixtyFt = tt - t0;
        if (d >= kQuarterMile) { e.quarter = tt - t0; e.trapKmh = L.sim->speed() * 3.6; break; }
        const DrivetrainFailure& f = L.sim->failure();
        if (f.snapped(0) || f.snapped(1)) { e.broke = true; break; }
        if (pt.stalled()) break;
    }
    std::lock_guard<std::mutex> g(cacheLock);
    cache[key] = e;
    return e;
}

void DragRace::timing(LaneState& L, int idx) {
    const double d = L.sim->distance(), v = L.sim->speed();
    const char* who = idx == 0 ? "SEN" : "RAKIP";
    if (!L.left && d > kRollout) {
        L.left = true; L.leaveTime = clock_;
        L.slip.reaction = clock_ - greenTime();
        if (clock_ < greenTime()) {
            L.slip.redLight = true;
            events_.push_back(std::string(who) + ": KIRMIZI ISIK! (" + fmt("%+.3f", L.slip.reaction) + ")");
        }
    }
    if (!L.left || L.slip.finished) return;
    const double et = clock_ - L.leaveTime;
    if (L.slip.sixtyFt < 0 && d >= k60) L.slip.sixtyFt = et;
    if (L.slip.t330 < 0 && d >= k330) L.slip.t330 = et;
    if (L.slip.eighth < 0 && d >= k660) { L.slip.eighth = et; L.slip.eighthKmh = v * 3.6; }
    if (L.slip.t1000 < 0 && d >= k1000) L.slip.t1000 = et;
    if (d >= kQuarterMile) {
        L.slip.quarter = et; L.slip.trapKmh = v * 3.6; L.slip.finished = true;
        events_.push_back(std::string(who) + ": BITIS " + fmt("%.3f s", et) + " @ " + fmt("%.1f km/h", v * 3.6));
    }
}

void DragRace::stepPhysics(const PlayerControls& pc) {
    // ---- faz gecisleri ----
    if (phase_ == RacePhase::Burnout && (phaseT_ > 20.0 || (autopilot_ && phaseT_ > 1.5))) skipBurnout();
    if (phase_ == RacePhase::Staging && phaseT_ > 1.0) {
        for (auto& L : lanes_) L.staged = true;
        events_.push_back("PRE-STAGE / STAGE - Agac basliyor");
        phase_ = RacePhase::Tree; phaseT_ = 0.0;
        treeStart_ = clock_ + treeDelay_;
    }
    if (phase_ == RacePhase::Tree && clock_ >= greenTime()) {
        phase_ = RacePhase::Run; phaseT_ = 0.0;
        events_.push_back("YESIL!");
    }

    for (int i = 0; i < 2; ++i) {
        LaneState& L = lanes_[i];
        VehicleInputs in;
        if (i == 0 && !autopilot_) playerDrive(L, pc, in); else aiDrive(L, in);
        L.sim->step(kStep, in);
        timing(L, i);
        if (!L.slip.broke && (L.sim->failure().snapped(0) || L.sim->failure().snapped(1))) {
            L.slip.broke = true; L.slip.note = "AKS KIRILDI";
            events_.push_back(std::string(i == 0 ? "SEN" : "RAKIP") + ": AKS KIRILDI!");
        }
        if (!L.slip.stalled && L.sim->powertrain().stalled() && !L.slip.finished) {
            L.slip.stalled = true;
            events_.push_back(std::string(i == 0 ? "SEN" : "RAKIP") + ": MOTOR STOP ETTI");
            if (i == 0 && phase_ != RacePhase::Run) { L.sim->powertrain().restart(); L.slip.stalled = false; }  // agacta: mars
        }
    }
    for (auto& e : lanes_[0].sim->powertrain().drainEvents()) (void)e;   // VTEC vb. olaylar HUD'da gosterilir
    for (auto& e : lanes_[1].sim->powertrain().drainEvents()) (void)e;

    // ---- kazanan ----
    if (phase_ == RacePhase::Run && winner_ < 0) {
        const LaneState &A = lanes_[0], &B = lanes_[1];
        auto out = [](const LaneState& L) { return L.slip.redLight || L.slip.broke || (L.slip.stalled && !L.slip.finished); };
        if (A.slip.redLight && B.slip.redLight) winner_ = A.leaveTime > B.leaveTime ? 0 : 1;   // once yakan kaybeder
        else if (A.slip.redLight) winner_ = 1;
        else if (B.slip.redLight) winner_ = 0;
        else if (A.slip.finished && !B.slip.finished) winner_ = 0;
        else if (B.slip.finished && !A.slip.finished) winner_ = 1;
        else if (A.slip.finished && B.slip.finished) winner_ = 0;   // ayni adimda: oyuncu lehine
        else if (out(A) && out(B)) winner_ = 1;
        else if (out(A) && !out(B) && B.left) winner_ = 1;
        else if (out(B) && !out(A) && A.left) winner_ = 0;
        if (winner_ >= 0) events_.push_back(winner_ == 0 ? "KAZANDIN!" : "KAYBETTIN");
    }
    const bool doneA = lanes_[0].slip.finished || lanes_[0].slip.broke || lanes_[0].slip.stalled;
    const bool doneB = lanes_[1].slip.finished || lanes_[1].slip.broke || lanes_[1].slip.stalled;
    if (phase_ == RacePhase::Run && ((doneA && doneB) || clock_ - greenTime() > 40.0)) {
        phase_ = RacePhase::Finished; phaseT_ = 0.0;
        if (winner_ < 0) winner_ = lanes_[0].sim->distance() >= lanes_[1].sim->distance() ? 0 : 1;
    }
}

} // namespace zk
