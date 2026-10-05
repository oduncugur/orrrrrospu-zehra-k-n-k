#include "RoadSession.h"
#include "game/Contact.h"
#include "garage/VehicleCatalog.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace zk {

double RoadSession::rnd() { rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5; return (rng_ & 0xFFFFFF) / double(0x1000000); }

RoadSession::RoadSession(Mode mode, int playerCar, const Tune* playerTune, int rivalCar, const Tune* rivalTune, uint32_t seed, Kind kind, double runRealKm)
    : mode_(mode), kind_(kind),
      road_(mode == Mode::Karma ? RoadPath::karma(seed, 0.04) : mode == Mode::Marathon ? RoadPath::run(seed, runDrivenM(runRealKm) + kRunStartS, 0.07)
            : RoadPath(20250930u + (mode != Mode::Free ? seed % 7 : 0) + (kind == Kind::Touge ? 1000u : 0u), 20000.0,
                       kind == Kind::Touge ? 28.0 : 90.0, kind == Kind::Touge ? 3.0 : 3.6, kind == Kind::Touge ? 0.09 : 0.05)),
      playerCar_(playerCar), rivalCar_(rivalCar), rng_(seed ? seed : 1u) {
    runLen_ = runDrivenM(runRealKm);
    // Yol genisligi cesitliligi: otoban / sehirlerarasi 2x1 ile 2x2 genislik arasi, dag yolu dar, karma orta
    // Serit duzeni: bolum bolum degisir (1+1, 2+2, 4+4, tek yon 3 / 4 ...); dag yolu dar 1+1; karma / maraton kapali yol (tek yon)
    if (mode == Mode::Karma || mode == Mode::Marathon) road_.setLaneProgram(seed, RoadPath::LanesClosed);
    else if (kind == Kind::Touge) road_.setLaneProgram(seed + 17u, RoadPath::LanesMountain);
    else road_.setLaneProgram(seed + 31u, RoadPath::LanesHighway);
    startS_ = mode == Mode::Chase ? 90.0 : mode == Mode::Marathon ? kRunStartS : kStartS;   // The Run: 200 araclik grid sigar
    rivalLane_ = rightLane(startS_);
    player_ = std::make_unique<RoadCar>(findVehicle(playerCar), playerTune, road_, startS_, rightLane(startS_));
    if (mode == Mode::Chase) {
        // Polis 55 m arkada, karsi seritte baslar (kalkista carpismasin); tam debriyaj, keskin viraj temposu
        const double pl = road_.lanesFwd(startS_) >= 2 ? road_.laneOffset(startS_, false, 1) : road_.laneOffset(startS_, true, road_.lanesBack(startS_) - 1);   // oyuncunun yan seridi
        rival_ = std::make_unique<RoadCar>(findVehicle(rivalCar), rivalTune, road_, startS_ - 55.0, pl);
        rivalLane_ = pl;
        rivalPace_ = 0.58;
        const EngineSpec& e = rival_->sim().engineSpec();
        rival_->launchRpm = std::max(e.idleRpm + 800.0, 0.30 * e.redlineRpm);   // kalkis: drag devri
        phase_ = Phase::Countdown; countdown_ = 3.0;
        msgs_.push_back("POLIS! KAC!");
    }
    if (mode == Mode::Race || mode == Mode::Karma) {
        // Rakip yan seritte, ayni cizgide
        // Yan seritte: gidis yonunde ikinci serit varsa orada, yoksa karsi seritte
        rivalLane_ = road_.lanesFwd(startS_) >= 2 ? road_.laneOffset(startS_, false, 1) : road_.laneOffset(startS_, true, 0);
        rival_ = std::make_unique<RoadCar>(findVehicle(rivalCar), rivalTune, road_, startS_, rivalLane_);
        rival_->slowClutch = true;                   // otomatik debriyajli oyuncu gibi gec kavrar (hata payi)
        phase_ = Phase::Countdown; countdown_ = 3.0;
        if (mode == Mode::Karma) {
            // Drag kalkisi: kalkis devri kirmizi cizginin %30'u (acik yolda olculdu: 0/30/40/50/60% arasinda en iyi
            // 400 m; Aygir S5 GT -1.25 s), insan gibi tepki suresi
            const EngineSpec& e = rival_->sim().engineSpec();
            rival_->launchRpm = std::max(e.idleRpm + 800.0, 0.30 * e.redlineRpm);
            rivalReact_ = 0.15 + 0.20 * rnd();
        }
    }
    if (mode == Mode::Marathon) {                                        // varsayilan alan: 19 rastgele rakip, karisik tarz
        std::vector<RunEntrant> f;
        const auto& cat = vehicleCatalog();
        for (int i = 0; i < 19; ++i) {
            int id;
            do { id = 1 + (int)(rnd() * cat.size()); } while (!cat[id - 1].streetLegal);
            f.push_back({id, Tune{}, (int)(rnd() * StyleCount) % StyleCount});
        }
        setRunField(f, runRealKm);
        phase_ = Phase::Countdown; countdown_ = 3.0;
    }
    if (mode == Mode::Flow) {
        // Viraj tepeleri (R < 600 m) + oyuncu govde yari genisligi
        std::vector<double> ss, ks;
        for (const RoadPoint& p : road_.points()) { ss.push_back(p.s); ks.push_back(p.curvature); }
        flow_ = std::make_unique<FlowScorer>(findApexes(ss, ks, 600.0), 0.5 * findVehicle(playerCar)->widthM);
        phase_ = Phase::Countdown; countdown_ = 3.0;
    }
    // Trafik: yol boyunca araclar (sag seritte yavas, karsi seritte gelen). Akis modunda yogun.
    const auto& cat = vehicleCatalog();
    // Sehirlerarasi (The Run / uzun yol): seyrek trafik, yaklasik yarisi tir / kamyon. Serbest otoban: dortte bir agir vasita.
    const bool intercity = mode == Mode::Marathon;
    const int nTraffic = mode == Mode::Karma ? 0 : kind == Kind::Touge ? 5 : mode == Mode::Flow ? 24 : mode == Mode::Chase ? 18 : intercity ? 9 : 14;   // karma: kapali yol
    const double spacing = mode == Mode::Flow ? 70.0 : intercity ? 320.0 : 170.0;
    const double truckShare = kind == Kind::Touge || mode == Mode::Flow ? 0.0 : intercity ? 0.5 : mode == Mode::Free ? 0.25 : 0.12;
    for (int i = 0; i < nTraffic; ++i) {
        TrafficCar t{};
        int id;
        if (rnd() < truckShare) id = rnd() < 0.6 ? kTruckId0 : kTruckId0 + 1;
        else do { id = 1 + (int)(rnd() * cat.size()); } while (!cat[id - 1].streetLegal);
        t.carId = id;
        spawnTraffic(t, startS_ + 150.0 + i * spacing);
        traffic_.push_back(t);
    }
}

void RoadSession::spawnTraffic(TrafficCar& t, double fromS) {
    t.uid = nextUid_++;
    t.s = std::clamp(fromS + 60.0 * rnd(), 0.0, road_.length() - 10.0);
    t.oncoming = road_.lanesBack(t.s) > 0 && rnd() < 0.45;             // tek yonde gelen trafik yok
    const int n = t.oncoming ? road_.lanesBack(t.s) : road_.lanesFwd(t.s);
    t.li = std::min(n - 1, (int)(rnd() * n * 0.999));
    if (!t.oncoming && n >= 3 && t.li == n - 1 && rnd() < 0.5) t.li = 0;   // sol serit (sollama) seyrek
    t.lane = road_.laneOffset(t.s, t.oncoming, t.li);
    t.v0 = t.oncoming ? 18.0 + 8.0 * rnd() : 14.0 + 8.0 * rnd();  // 50-95 km/h
    if (kind_ == Kind::Touge) t.v0 *= 0.6;                          // dag yolunda yavas
    t.v0 *= 1.0 + 0.08 * t.li;                                         // sol seritler hizli
    if (isTruckId(t.carId)) {                                          // agir vasita: sag serit, 70-85 km/h (yokusta yavas)
        if (!t.oncoming) { t.li = 0; t.lane = road_.laneOffset(t.s, false, 0); }
        t.v0 = (19.5 + 4.0 * rnd()) * (kind_ == Kind::Touge ? 0.6 : 1.0);
    }
    t.v = t.v0;
}

int RoadSession::treeLights() const {
    if (mode_ != Mode::Karma) return 0;
    if (phase_ != Phase::Countdown) return raceT_ < 1.0 ? 8 : 0;           // yesil 1 s yanik kalir
    int m = 0;
    if (countdown_ <= 1.5) m |= 1;
    if (countdown_ <= 1.0) m |= 2;
    if (countdown_ <= 0.5) m |= 4;
    return m;
}

void RoadSession::trafficPose(const TrafficCar& t, double& x, double& y, double& psi) const {
    const RoadPoint p = road_.at(t.s);
    x = p.x - t.lane * std::sin(p.heading);
    y = p.y + t.lane * std::cos(p.heading);
    psi = p.heading + (t.oncoming ? 3.14159265358979 : 0.0);
}

// Arac-trafik carpismasi: trafik aracinin kutusu (4.4 x 1.8 m) icinde mi? Hiz kaybi + trafik araci savrulur (yeniden dogar)
int RoadSession::zoneAt(double s) const {
    auto hashf = [](int i) { unsigned x = (unsigned)i * 2654435761u; x ^= x >> 13; x *= 0x5bd1e995u; x ^= x >> 15; return (x & 0xFFFF) / 65535.0f; };
    const bool mtn = kind_ == Kind::Touge;
    const int block = (int)(s / 350.0);
    if (block < 1) return 0;                                           // baslangic acik alanda
    const float h = hashf(block * 13 + (int)raceLength());
    if (mtn) return h < 0.18f ? 2 : 0;
    if ((mode_ == Mode::Karma || mode_ == Mode::Marathon) && road_.curvyAt(s)) return 0;
    return h < 0.32f ? 1 : h < 0.40f ? 2 : 0;
}

double RoadSession::wallAt(double s, int side) const {
    const double hw = road_.halfWidthAt(s);
    const int z = zoneAt(s);
    if (side < 0 && inStation(s)) return hw + 13.5;                   // benzinlik sahasi (sag): icine girilir
    if (z == 2) return hw + 1.2;                                       // tunel duvari
    for (double k = 2.0; k <= 160.0; k += 4.0)                         // tunel yaklasma yamaci (dag yanlarda yukselir)
        if (zoneAt(s + k) == 2 || zoneAt(s - k) == 2) return hw + 2.5;
    if (kind_ == Kind::Touge) return hw + 1.2;                        // dag: celik bariyer / kaya
    if (z == 1) return hw + 4.8;                                       // sehir: bina cephesi
    if (hw > 4.6) return hw + 2.0;                                     // otoban bariyeri
    if (side != 0) {                                                   // kirsal ahsap cit (cizimle ayni kesimler: 80 m bloklar)
        auto hashf = [](int k) { unsigned x = (unsigned)k * 2654435761u; x ^= x >> 13; x *= 0x5bd1e995u; x ^= x >> 15; return (x & 0xFFFF) / 65535.0f; };
        const int i = (int)(s / RoadPath::kStep);
        if (hashf((i / 40) * 3 + (side > 0)) >= 0.5f) return hw + 6.0;
    }
    return hw + 14.0;                                                  // acik arazi: yoldan fazla uzaklasilmaz (geri isinlanma yok)
}

void RoadSession::wallContact(RoadCar& car) {
    const double wall = wallAt(car.s(), car.lateral() > 0 ? 1 : -1);
    if (wall < 0) return;
    const double half = 0.95, lat = car.lateral();
    const double pen = std::fabs(lat) + half - wall;
    if (pen <= 0) return;
    VehicleSim& sm = car.sim();
    const RoadPoint p = road_.at(car.s());
    const double sg = lat > 0 ? 1.0 : -1.0;
    const double nx = -std::sin(p.heading) * sg, ny = std::cos(p.heading) * sg;   // yol disina dogru
    sm.nudge(-nx * pen, -ny * pen);                                    // duvarin icine geri
    double vx, vy; sm.worldVelocity(vx, vy);
    const double vn = vx * nx + vy * ny;
    if (vn > 0) {                                                      // duvara dogru hiz: soner (az sekme) + surtunme
        const double m = sm.mass(), j = m * vn * 1.3;
        sm.applyImpulse(-nx * j, -ny * j, 0, 0);
        const double tx = -ny, ty = nx, vt = vx * tx + vy * ty;
        sm.applyImpulse(-tx * m * vt * 0.12, -ty * m * vt * 0.12, 0, 0);   // surtunme: hiz kaybi
        if (&car == player_.get() && vn > 4.0 && !wallHit_) { msgs_.push_back(vn > 9.0 ? "DUVARA CARPTIN!" : "DUVAR!"); crashEv_ = true; }
    }
    if (&car == player_.get()) wallHit_ = vn > 4.0;
}

double RoadSession::impactKinematic(RoadCar& car, double ox, double oy, double opsi, double ov, double omass,
                                    double halfL, double halfW, double& rel) {
    rel = 0;
    VehicleSim& sm = car.sim();
    const double cx = sm.posX(), cy = sm.posY(), c = std::cos(opsi), s = std::sin(opsi);
    const double dx = cx - ox, dy = cy - oy;
    const double lon = dx * c + dy * s, lat = -dx * s + dy * c;
    const double carHalfL = 2.2, carHalfW = 0.9;
    const double penLon = halfL + carHalfL - std::fabs(lon), penLat = halfW + carHalfW - std::fabs(lat);
    if (penLon <= 0 || penLat <= 0) return ov;
    // Temas normali: en az girilen eksen (kinematik aracin yerel ekseni), arac tarafina dogru
    double nx, ny, pen;
    if (penLon < penLat) { const double sg = lon > 0 ? 1.0 : -1.0; nx = c * sg; ny = s * sg; pen = penLon; }
    else { const double sg = lat > 0 ? 1.0 : -1.0; nx = -s * sg; ny = c * sg; pen = penLat; }
    sm.nudge(nx * pen, ny * pen);                                      // ayir
    double vx, vy; sm.worldVelocity(vx, vy);
    const double ovx = c * ov, ovy = s * ov;
    const double vn = (vx - ovx) * nx + (vy - ovy) * ny;              // < 0: yaklasiyor
    if (vn >= 0) return ov;
    rel = -vn;
    // Temas noktasi: aracin merkezinden normal yonunde yari genislik kadar (yandan vuruslar yaw uretir)
    const double m = sm.mass(), e = 0.25, mu = 0.45;
    const double rx = -nx * carHalfW, ry = -ny * carHalfW;
    const double j = -(1.0 + e) * vn / (1.0 / m + 1.0 / omass);
    sm.applyImpulse(nx * j, ny * j, rx, ry);
    // Surtunme (teget): goreli kayma hizini azaltir, |jt| <= mu j
    const double tx = -ny, ty = nx, vt = (vx - ovx) * tx + (vy - ovy) * ty;
    const double jt = std::clamp(-vt / (1.0 / m + 1.0 / omass), -mu * j, mu * j);
    sm.applyImpulse(tx * jt, ty * jt, rx, ry);
    // Kinematik arac: ileri ekseni boyunca momentum degisimi (ters isaretli impuls)
    const double dvo = (-(nx * j + tx * jt) * c - (ny * j + ty * jt) * s) / omass;
    return ov + dvo;
}

void RoadSession::pumpContact(RoadCar& car, bool isPlayer) {
    const auto st = stations();
    if (stationDead_.size() != st.size()) stationDead_.assign(st.size(), false);
    for (size_t k = 0; k < st.size(); ++k) {
        if (stationDead_[k] || car.s() < st[k] - 5.0 || car.s() > st[k] + kStationLen + 5.0) continue;
        for (int p = 0; p < 3; ++p) {
            const double ps = st[k] + pumpOffset(p), lat = pumpLat(ps);
            const RoadPoint q = road_.at(ps);
            const double px = q.x - lat * std::sin(q.heading), py = q.y + lat * std::cos(q.heading);
            double rel = 0;
            impactKinematic(car, px, py, q.heading, 0.0, 4000.0, 0.6, 0.45, rel);
            if (rel > 30.0 / 3.6) {                                      // > 30 km/h: istasyon patlar
                stationDead_[k] = true; explS_ = ps; explLat_ = lat;
                car.sim().scaleVelocity(0.35);
                if (isPlayer) { msgs_.push_back("BENZINLIK PATLADI!"); crashEv_ = true; ++collisions_; }
                return;
            } else if (rel > 2.0 && isPlayer) { msgs_.push_back("POMPAYA CARPTIN!"); crashEv_ = true; }
        }
    }
}

void RoadSession::collide(RoadCar& car, bool isPlayer) {
    const double cx = car.sim().posX(), cy = car.sim().posY();
    for (TrafficCar& t : traffic_) {
        const double hl = trafficHalfLen(t.carId), hwid = isTruckId(t.carId) ? 1.28 : 0.9;
        if (std::fabs(t.s - car.s()) > hl + 8.0) continue;
        double x, y, psi; trafficPose(t, x, y, psi);
        const double dx = cx - x, dy = cy - y, c = std::cos(psi), s = std::sin(psi);
        const double lon = dx * c + dy * s, lat = -dx * s + dy * c;
        if (std::fabs(lon) < hl + 2.1 && std::fabs(lat) < hwid + 0.85) {
            double rel = 0;
            const double mt = isTruckId(t.carId) ? findVehicle(t.carId)->massKg
                                                 : 1150.0 + 350.0 * ((t.uid * 2654435761u >> 20) % 3);   // 1150-1850 kg
            const double nv = impactKinematic(car, x, y, psi, t.v, mt, hl, hwid, rel);
            if (rel < 0.5) continue;                                     // ayriliyorlar: temas yok
            t.v = std::max(0.0, std::fabs(nv));                            // trafik araci yavaslar / itilir
            if (rel > 6.0) spawnTraffic(t, car.s() + 400.0);              // sert carpisma: trafik araci yoldan cekilir
            if (isPlayer && rel > 1.5) {
                ++collisions_; crashEv_ = true;
                if (flow_) flow_->crash();                          // skor cezasi + kombo sifir (mesaji skor verir)
                else msgs_.push_back(rel > 20 ? "AGIR CARPISMA!" : "CARPISMA");
            }
        }
    }
}

// YZ rakip: sag seritte yavas trafik varsa karsi serit bossa sollar, degilse arkasinda bekler
std::vector<double> RoadSession::stations() const {
    if (mode_ != Mode::Marathon) return {};
    std::vector<double> v;
    for (double st = startS_ + kStationGap; st < startS_ + raceLength() - 1000.0; st += kStationGap) v.push_back(st);
    return v;
}
void RoadSession::setRunField(const std::vector<RunEntrant>& field, double realKm) {
    realKm_ = std::max(20.0, realKm);
    // Oyuncu gridin arka ucte birinde (gecerek ilerler); o sira bos birakilir; depo gercekci, ayni sikistirma
    const int lanes = std::max(1, road_.lanesFwd(startS_)), n = (int)field.size();
    const int slot = std::max(0, n * 2 / 3);
    run_.init(field, road_, startS_, compression(), rng_, slot);
    const double ps = startS_ - 12.0 - (slot / lanes) * 9.0;
    player_->recoverAt(std::max(2.0, ps), road_.laneOffset(std::max(2.0, ps), false, slot % lanes));
    player_->sim().setFuelSystem(RunField::tankFor(playerCar_), compression());
}
double RoadSession::nextStation(double s) const {
    for (double st : stations()) if (st + kStationLen > s) return std::max(0.0, st - s);
    return -1.0;
}
bool RoadSession::inStation(double s) const {
    for (double st : stations()) if (s >= st && s <= st + kStationLen) return true;
    return false;
}
// Benzinlik: istasyon alaninda, sag seritte (yola gore saga), neredeyse durmus arac depo dolana dek yakit alir
void RoadSession::fuelStep(double dt) {
    RoadCar* cars[2] = {player_.get(), rival_.get()};
    for (int i = 0; i < 2; ++i) {
        refuel_[i] = false;
        RoadCar* c = cars[i];
        if (!c || !inStation(c->s()) || c->sim().speed() > 1.5) continue;
        bool atPump = false;
        const auto st = stations();
        for (size_t k = 0; k < st.size(); ++k) {
            if (stationDestroyed((int)k) || c->s() < st[k] || c->s() > st[k] + kStationLen) continue;
            for (int p = 0; p < 3; ++p)
                if (std::fabs(c->s() - (st[k] + pumpOffset(p))) < 4.0 && std::fabs(c->lateral() - pumpSlotLat(c->s())) < 1.8) atPump = true;
        }
        if (!atPump) continue;
        if (c->sim().fuelLiters() < c->sim().tankLiters() - 0.01) { c->sim().addFuel(kRefuelLps * dt); refuel_[i] = true; refilled_[i] += kRefuelLps * dt; }
    }
}

// Yakit stratejisi (rakip ve oyuncu otopilotu): ortalama tuketimle bir sonraki benzinlige / bitise yetmeyecekse bu
// benzinlikte dur: durma noktasi karar aninda sabitlenir (istasyon ortasi), sag seritte yavaslar, depo dolunca devam.
bool RoadSession::pitControls(int car, double pace, RoadControls& out) {
    RoadCar& r = car == 0 ? *player_ : *rival_;
    const double s = r.s();
    const double perM = s > startS_ + 300.0 ? std::max(1e-5, (r.sim().tankLiters() - r.sim().fuelLiters() + refilled_[car]) / (s - startS_)) : 1.0 / 7000.0;
    const double toNext = nextStation(s);
    if (pitAt_[car] < 0 && toNext >= 0 && toNext < 400.0 && !inStation(s)) {
        double after = startS_ + raceLength() - s;                          // bu istasyondan sonra gerekli yol
        for (double st : stations()) if (st > s + toNext + 10.0) { after = st - s; break; }
        if (r.sim().fuelLiters() < perM * after * 1.15) pitAt_[car] = s + toNext + kStationLen * 0.5;
    }
    if (pitAt_[car] < 0) return false;
    if (r.sim().fuelLiters() >= r.sim().tankLiters() - 0.05 || s > pitAt_[car] + kStationLen) { pitAt_[car] = -1; return false; }   // dolu / gecti
    const double dist = pitAt_[car] - s;
    out = r.aiControls(dist < 70.0 ? pumpSlotLat(s) : rightLane(s), pace, dist > 0 ? std::sqrt(2.0 * 3.5 * dist) : 0.0);   // orta pompanin yanina
    if (dist < 3.0 || (inStation(s) && r.sim().speed() < 1.0)) { out.throttle = 0.0; out.brake = 1.0; }
    return true;
}

RoadControls RoadSession::rivalControls() {
    RoadCar& r = *rival_;
    const double pace = rivalPace_ * (rain_ ? 0.85 : 1.0);
    const double s = r.s(), v = r.sim().speed();
    if (mode_ == Mode::Karma) return r.aiControls(road_.laneOffset(s, false, std::min(1, road_.lanesFwd(s) - 1)), pace + 0.05);   // kapali yol: sol serit
    // Seritler: gidis 0..nf-1 (0 en sag), varsa karsi seridin ilki (sollama). Bulundugu seritte onde yavas arac
    // (trafik / oyuncu) varsa: once soldaki gidis seridi, o da doluysa ve karsi bos ise karsi serit; olmazsa takip.
    // Engel yoksa ve sagdaki serit bossa saga doner (sag serit kurali).
    const int nf = road_.lanesFwd(s), nb = road_.lanesBack(s);
    auto laneAhead = [&](double off, bool sameDir) {                     // en yavas ondeki hiz (-1: bos)
        double bv = -1;
        for (const TrafficCar& t : traffic_) {
            if (t.oncoming == sameDir || std::fabs(t.lane - off) > 2.0) continue;
            if (sameDir ? (t.s > s && t.s - s < 25.0 + 1.2 * v && t.v < v) : (t.s > s - 15.0 && t.s - s < 60.0 + 3.0 * v)) bv = bv < 0 ? t.v : std::min(bv, t.v);
        }
        const double ds = player_->s() - s, pv = player_->sim().speed();
        if (sameDir && ds > 0 && ds < 20.0 + 1.0 * v && std::fabs(player_->lateral() - off) < 2.0 && pv < v) bv = bv < 0 ? pv : std::min(bv, pv);
        return bv;
    };
    auto sideClear = [&](double off) {
        for (const TrafficCar& t : traffic_) if (std::fabs(t.lane - off) < 2.0 && std::fabs(t.s - s) < 16.0) return false;
        if (std::fabs(player_->s() - s) < 12.0 && std::fabs(player_->lateral() - off) < 2.0) return false;
        return true;
    };
    int cur = -1;                                                       // bulundugu gidis seridi (-1: karsi seritte)
    {
        double best = 1e9;
        for (int k = 0; k < nf; ++k) { const double d = std::fabs(rivalLane_ - road_.laneOffset(s, false, k)); if (d < best) { best = d; cur = k; } }
        if (nb > 0 && std::fabs(rivalLane_ - road_.laneOffset(s, true, 0)) < best) cur = -1;
    }
    double cap = 1e9;
    const double oncomingOff = nb > 0 ? road_.laneOffset(s, true, 0) : 0.0;
    const bool oncomingClose = nb > 0 && laneAhead(oncomingOff, false) >= 0;
    if (cur >= 0) {
        const double here = road_.laneOffset(s, false, cur), block = laneAhead(here, true);
        if (block >= 0) {
            if (cur + 1 < nf && laneAhead(road_.laneOffset(s, false, cur + 1), true) < 0 && sideClear(road_.laneOffset(s, false, cur + 1))) cur += 1;
            else if (cur == nf - 1 && nb > 0 && !oncomingClose) cur = -1;
            else cap = block;
        } else if (cur > 0 && laneAhead(road_.laneOffset(s, false, cur - 1), true) < 0 && sideClear(road_.laneOffset(s, false, cur - 1))) cur -= 1;
    } else if (nb == 0 || oncomingClose || sideClear(road_.laneOffset(s, false, nf - 1))) cur = nf - 1;   // karsidan don
    rivalLane_ = cur >= 0 ? road_.laneOffset(s, false, cur) : oncomingOff;
    return r.aiControls(rivalLane_, pace, cap);
}

// Polis: oyuncunun seridini izler (yolda kalarak), onunde trafik varsa diger seride gecer; uzaksa tam tempo
RoadControls RoadSession::chaseControls() {
    RoadCar& r = *rival_;
    const double s = r.s(), v = r.sim().speed();
    const double hw = road_.halfWidthAt(s);
    // Hiz kazanana kadar kendi seridinde kalir (dururken serit degistirmek kalkisi bogar)
    const double near = player_->s() - s;
    double target = v < 15.0 && near > 45.0 ? rivalLane_ : std::clamp(player_->lateral(), -hw + 1.1, hw - 1.1);
    auto blocked = [&](double lat) {
        for (const TrafficCar& t : traffic_)
            if (std::fabs(t.lane - lat) < 2.2 && t.s > s - 3.0 && t.s - s < 18.0 + 1.1 * v) return true;
        return false;
    };
    if (blocked(target)) {                                             // dolu: bos bir gidis seridi, o da yoksa karsi serit
        double alt = target;
        for (int k = 0; k < road_.lanesFwd(s); ++k) if (!blocked(road_.laneOffset(s, false, k))) { alt = road_.laneOffset(s, false, k); break; }
        if (alt == target && road_.lanesBack(s) > 0) alt = road_.laneOffset(s, true, 0);
        target = alt;
    }
    rivalLane_ += std::clamp(target - rivalLane_, -2.5 * 0.05, 2.5 * 0.05) * 4.0;   // yumusak serit degisimi
    const double gap = player_->s() - s;
    const double pace = (gap > 120.0 ? 0.74 : rivalPace_) * (rain_ ? 0.88 : 1.0);
    return r.aiControls(rivalLane_, pace);
}

void RoadSession::updateChase(double dt) {
    if (phase_ != Phase::Run) return;
    const double gap = player_->s() - rival_->s();
    const double latD = std::fabs(player_->lateral() - rival_->lateral());
    const double pv = player_->sim().speed();
    // Yakalanma: polis dibinde (12 m) ve oyuncu yavas -> dolar; temas ani artis; uzaklasinca azalir
    if (gap < 12.0 && gap > -6.0 && latD < 3.5) bust_ += dt * (pv < 6.0 ? 0.45 : pv < 14.0 ? 0.18 : 0.06);
    else if (gap > 25.0) bust_ -= dt * 0.12;
    if (contactKick_ > 0) { bust_ += std::min(0.35, 0.06 + contactKick_ * 0.03); contactKick_ = 0; }
    bust_ = std::clamp(bust_, 0.0, 1.0);
    escapeT_ = gap > kEscapeGap ? escapeT_ + dt : std::max(0.0, escapeT_ - dt * 2.0);
    const bool end = player_->s() >= startS_ + raceLength();
    if (bust_ >= 1.0) { winner_ = 1; phase_ = Phase::Finished; finishT_[0] = raceT_; msgs_.push_back("YAKALANDIN!"); }
    else if (escapeT_ >= kEscapeHold || end) { winner_ = 0; phase_ = Phase::Finished; finishT_[0] = raceT_; msgs_.push_back("KACTIN!"); }
}

void RoadSession::update(double dt, const RoadControls& in) {
    dt = std::min(dt, 0.05);
    // Trafik hareketi + geri donusum (oyuncunun etrafinda ~1.5 km pencere)
    // Trafik takip modeli (IDM): ondeki araca (trafik, oyuncu, rakip) gore yavaslar/fren yapar
    for (TrafficCar& t : traffic_) {
        const double dir = t.oncoming ? -1.0 : 1.0;
        double gap = 1e9, vLead = t.v0;
        double otherHl = 2.2;
        auto consider = [&](double s, double lat, double v, bool sameDirMover) {
            if (std::fabs(lat - t.lane) > 2.2) return;
            const double g = (s - t.s) * dir - 2.3 - trafficHalfLen(t.carId) - (otherHl - 2.2);
            if (g > -2.0 && g < gap) { gap = std::max(g, 0.1); vLead = sameDirMover ? v : 0.0; }
        };
        for (const TrafficCar& o : traffic_) if (&o != &t && o.oncoming == t.oncoming) { otherHl = trafficHalfLen(o.carId); consider(o.s, o.lane, o.v, true); }
        otherHl = 2.2;
        consider(player_->s(), player_->lateral(), player_->sim().speed(), !t.oncoming);
        if (rival_) consider(rival_->s(), rival_->lateral(), rival_->sim().speed(), !t.oncoming);
        const double a = 1.5, bComf = 3.0, dv = t.v - vLead;
        const double sStar = 4.0 + t.v * 1.4 + t.v * dv / (2.0 * std::sqrt(a * bComf));
        double acc = a * (1.0 - std::pow(t.v / t.v0, 4) - (gap < 1e8 ? std::pow(std::max(sStar, 0.0) / gap, 2) : 0.0));
        acc = std::max(acc, -8.0);
        t.braking = acc < -1.0;
        t.v = std::max(0.0, t.v + acc * dt);
    }
    // Geri donusum: akis modunda araclar oyuncunun daha yakininda yeniden dogar (surekli aksiyon)
    const double near = mode_ == Mode::Flow ? 350.0 : 900.0, spread = mode_ == Mode::Flow ? 700.0 : 1200.0;
    const double far = mode_ == Mode::Flow ? 1400.0 : 2600.0;
    for (TrafficCar& t : traffic_) {
        t.s += (t.oncoming ? -t.v : t.v) * dt;
        const int n = t.oncoming ? road_.lanesBack(t.s) : road_.lanesFwd(t.s);
        if (n == 0) { spawnTraffic(t, player_->s() + near + spread * rnd()); continue; }   // tek yona girdi
        t.li = std::min(t.li, n - 1);                                   // serit bitti: birles
        const double target = road_.laneOffset(t.s, t.oncoming, t.li);
        t.lane += std::clamp(target - t.lane, -1.6 * dt, 1.6 * dt);
        const double ref = player_->s();
        if (t.s < ref - 250.0 || t.s > ref + far || t.s < 5.0 || t.s > road_.length() - 20.0) spawnTraffic(t, ref + near + spread * rnd());
    }
    if (phase_ == Phase::Countdown) {
        countdown_ -= dt;
        RoadControls hold = in; hold.brake = 1.0; hold.steer = 0.0;      // isik yesil olana kadar fren
        player_->update(dt, hold);
        if (rival_) { RoadControls rh; rh.brake = 1.0; rh.throttle = 0.3; rival_->update(dt, rh); }
        if (countdown_ <= 0.0) { phase_ = Phase::Run; msgs_.push_back("YESIL!"); }
        return;
    }
    if (mode_ == Mode::Flow && phase_ == Phase::Finished) {          // sure bitti: yavasla, skor sabit
        player_->update(dt, RoadControls{in.steer, 0.0, 0.5});
        return;
    }
    player_->update(dt, in);
    wallContact(*player_);
    pumpContact(*player_, true);
    if (mode_ == Mode::Marathon) {
        fuelStep(dt);
        static const double kWarn = 1.0;
        if (player_->sim().fuelLiters() < kWarn && !lowFuelMsg_) { lowFuelMsg_ = true; msgs_.push_back("YAKIT AZ! BENZINLIGE GIR"); }
        if (player_->sim().fuelLiters() > 2.0) lowFuelMsg_ = false;
    }
    if (mode_ == Mode::Marathon) {
        if (player_->takeRecovered()) msgs_.push_back("ARAC YOLA ALINDI");
        player_->takeStalled();
        const double goal = startS_ + raceLength();
        if (phase_ == Phase::Run) raceT_ += dt;
        run_.update(dt, road_, stations(), kStationLen, kRefuelLps, goal, raceT_, player_->s(), player_->lateral(), player_->sim().speed());
        // Oyuncu - rakip temasi (kutu): oyuncu savrulur / yavaslar, rakip yavaslar
        const double cx = player_->sim().posX(), cy = player_->sim().posY();
        for (Runner& R : run_.runners()) {
            if (dragPart() || std::fabs(R.s - player_->s()) > 9.0) continue;   // 2B drag bolumu: temas yok
            const RoadPoint p = road_.at(R.s);
            const double x = p.x - R.lane * std::sin(p.heading), y = p.y + R.lane * std::cos(p.heading);
            const double dx = cx - x, dy = cy - y, c = std::cos(p.heading), sn = std::sin(p.heading);
            const double lon = dx * c + dy * sn, lat = -dx * sn + dy * c;
            if (std::fabs(lon) < 4.3 && std::fabs(lat) < 1.75) {
                double rel = 0;
                const double nv = impactKinematic(*player_, x, y, p.heading, R.v, std::max(900.0, R.massKg), 2.2, 0.9, rel);
                if (rel < 0.5) continue;
                R.v = std::max(0.0, nv); R.lane += lat > 0 ? -0.4 : 0.4;
                if (!touching_) { msgs_.push_back(rel > 6.0 ? "SERT TEMAS!" : "TEMAS!"); crashEv_ = true; ++collisions_; }
                touching_ = true;
            }
        }
        if (finishT_[0] <= 0 && player_->s() >= goal) {
            finishT_[0] = raceT_; phase_ = Phase::Finished;
            winner_ = runPosition() == 1 ? 0 : 1;
            char m[48]; std::snprintf(m, sizeof m, "BITIS: %d. / %d", runPosition(), runCount()); msgs_.push_back(m);
        }
        return;
    }
    if (player_->takeRecovered()) msgs_.push_back("ARAC YOLA ALINDI");
    if (player_->takeStalled()) msgs_.push_back("MOTOR STOP ETTI");
    if (!dragPart()) collide(*player_, true);
    if (mode_ == Mode::Flow) {
        std::vector<FlowScorer::Car> snap;
        snap.reserve(traffic_.size());
        for (const TrafficCar& t : traffic_) snap.push_back({t.uid, t.s, t.lane, t.oncoming});
        flow_->update(dt, player_->s(), player_->lateral(), player_->sim().speed(), player_->offRoad(), snap);
        for (auto& m : flow_->drainMessages()) msgs_.push_back(m);
        raceT_ += dt;
        if (raceT_ >= kFlowTime) { raceT_ = kFlowTime; phase_ = Phase::Finished; msgs_.push_back("SURE BITTI"); }
        if (player_->s() > road_.length() - 80.0) { player_->recover(road_.length() - 100.0); msgs_.push_back("YOL SONU - BASA DONULDU"); }
        return;
    }
    if (mode_ == Mode::Free) {
        if (player_->s() > road_.length() - 80.0) { player_->recover(road_.length() - 100.0); msgs_.push_back("YOL SONU - BASA DONULDU"); }
        return;
    }
    if (mode_ == Mode::Karma && raceT_ < rivalReact_) {                 // rakip tepki suresi: henuz yesili gormedi
        RoadControls rh; rh.brake = 1.0; rh.throttle = 0.3; rival_->update(dt, rh);
    } else if (mode_ == Mode::Chase) rival_->update(dt, phase_ == Phase::Run ? chaseControls() : RoadControls{0, 0, 0.6});
    else rival_->update(dt, phase_ == Phase::Run || finishT_[1] <= 0 ? rivalControls() : RoadControls{0, 0, 0.4});
    rival_->takeRecovered(); rival_->takeStalled();
    wallContact(*rival_);
    pumpContact(*rival_, false);
    collide(*rival_, false);
    // Oyuncu-rakip temasi: yonlu kutu cakismasi + kutle/atalet impulsu (Contact.h); 2B drag bolumunde yok
    if (dragPart()) touching_ = false;
    else {
        const VehicleDef* pv = findVehicle(playerCar_);
        const VehicleDef* rv = findVehicle(rivalCar_);
        const ContactResult c = resolveContact({&player_->sim(), 0.5 * pv->lengthM, 0.5 * pv->widthM},
                                               {&rival_->sim(), 0.5 * rv->lengthM, 0.5 * rv->widthM});
        if (c.touching && !touching_ && c.closingSpeed > 0.5) {
            msgs_.push_back(c.closingSpeed > 6.0 ? "SERT TEMAS!" : "TEMAS!");
            crashEv_ = true;
            if (mode_ == Mode::Chase) contactKick_ = c.closingSpeed;
        }
        touching_ = c.touching;
    }
    if (phase_ == Phase::Run || phase_ == Phase::Finished) raceT_ += (phase_ == Phase::Run) ? dt : 0.0;
    if (mode_ == Mode::Chase) { updateChase(dt); return; }
    if (mode_ == Mode::Karma && reaction_ < 0 && player_->s() > startS_ + 0.3) {       // tepki: arac ~30 cm ilerledi
        reaction_ = raceT_;
        char m[48]; std::snprintf(m, sizeof m, "TEPKI %.3f S", reaction_);
        msgs_.push_back(m);
    }
    const double goal = startS_ + raceLength();
    if (finishT_[0] <= 0 && player_->s() >= goal) { finishT_[0] = raceT_; if (winner_ < 0) winner_ = 0; }
    if (finishT_[1] <= 0 && rival_->s() >= goal) { finishT_[1] = raceT_; if (winner_ < 0) winner_ = 1; }
    if (phase_ == Phase::Run && finishT_[0] > 0) phase_ = Phase::Finished;   // oyuncu bitirince sonuc
    if (phase_ == Phase::Run && finishT_[1] > 0 && raceT_ > finishT_[1] + 30.0) { phase_ = Phase::Finished; }   // rakip bitti, 30 s sonra kaybettin
}

} // namespace zk
