#include "RoadSession.h"
#include "game/Contact.h"
#include "garage/VehicleCatalog.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace zk {

double RoadSession::rnd() { rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5; return (rng_ & 0xFFFFFF) / double(0x1000000); }

RoadSession::RoadSession(Mode mode, int playerCar, const Tune* playerTune, int rivalCar, const Tune* rivalTune, uint32_t seed, Kind kind)
    : mode_(mode), kind_(kind),
      road_(mode == Mode::Karma ? RoadPath::karma(seed, 0.04)
            : RoadPath(20250930u + (mode != Mode::Free ? seed % 7 : 0) + (kind == Kind::Touge ? 1000u : 0u), 20000.0,
                       kind == Kind::Touge ? 28.0 : 90.0, kind == Kind::Touge ? 3.0 : 3.6, kind == Kind::Touge ? 0.09 : 0.05)),
      playerCar_(playerCar), rivalCar_(rivalCar), rng_(seed ? seed : 1u) {
    // Yol genisligi cesitliligi: otoban / sehirlerarasi 2x1 ile 2x2 genislik arasi, dag yolu dar, karma orta
    if (mode == Mode::Karma) road_.setWidthRange(seed, 3.4, 5.4);
    else if (kind == Kind::Touge) road_.setWidthRange(seed + 17u, 2.7, 3.5);
    else road_.setWidthRange(seed + 31u, 3.3, 6.2);
    startS_ = mode == Mode::Chase ? 90.0 : kStartS;
    rivalLane_ = -lane();
    player_ = std::make_unique<RoadCar>(findVehicle(playerCar), playerTune, road_, startS_, -lane());
    if (mode == Mode::Chase) {
        // Polis 55 m arkada, karsi seritte baslar (kalkista carpismasin); tam debriyaj, keskin viraj temposu
        rival_ = std::make_unique<RoadCar>(findVehicle(rivalCar), rivalTune, road_, startS_ - 55.0, +lane());
        rivalLane_ = +lane();
        rivalPace_ = 0.64;
        const EngineSpec& e = rival_->sim().engineSpec();
        rival_->launchRpm = std::max(e.idleRpm + 800.0, 0.30 * e.redlineRpm);   // kalkis: drag devri
        phase_ = Phase::Countdown; countdown_ = 3.0;
        msgs_.push_back("POLIS! KAC!");
    }
    if (mode == Mode::Race || mode == Mode::Karma || mode == Mode::Marathon) {
        // Rakip yan seritte, ayni cizgide
        rival_ = std::make_unique<RoadCar>(findVehicle(rivalCar), rivalTune, road_, startS_, +lane());
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
    if (mode == Mode::Marathon) {                                        // kucuk depo: en az bir benzinlik molasi
        // Tuketim carpani guce gore normallesir (yakit ~ guc): her arac bir depoyla ~7 km gider, en az bir mola sart
        for (RoadCar* c : {player_.get(), rival_.get()}) {
            double hp = 0;
            for (auto* cv : {&c->sim().engineSpec().lowCam, &c->sim().engineSpec().highCam})
                for (auto& p : *cv) if (p.first <= c->sim().engineSpec().redlineRpm) hp = std::max(hp, p.second * p.first / 7120.9);
            c->sim().setFuelSystem(kMarathonTank, kMarathonBurn * 141.0 / std::max(60.0, hp));
        }
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
    const int nTraffic = mode == Mode::Karma ? 0 : kind == Kind::Touge ? 5 : mode == Mode::Flow ? 24 : mode == Mode::Chase ? 18 : 14;   // karma: kapali yol
    const double spacing = mode == Mode::Flow ? 70.0 : 170.0;
    for (int i = 0; i < nTraffic; ++i) {
        TrafficCar t{};
        int id;
        do { id = 1 + (int)(rnd() * cat.size()); } while (!cat[id - 1].streetLegal);
        t.carId = id;
        spawnTraffic(t, startS_ + 150.0 + i * spacing);
        traffic_.push_back(t);
    }
}

void RoadSession::spawnTraffic(TrafficCar& t, double fromS) {
    t.uid = nextUid_++;
    t.oncoming = rnd() < 0.45;
    t.lane = t.oncoming ? +lane() : -lane();
    t.v0 = t.oncoming ? 18.0 + 8.0 * rnd() : 14.0 + 8.0 * rnd();  // 50-95 km/h
    if (kind_ == Kind::Touge) t.v0 *= 0.6;                          // dag yolunda yavas
    t.v = t.v0;
    t.s = std::clamp(fromS + 60.0 * rnd(), 0.0, road_.length() - 10.0);
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
void RoadSession::collide(RoadCar& car, bool isPlayer) {
    const double cx = car.sim().posX(), cy = car.sim().posY();
    for (TrafficCar& t : traffic_) {
        if (std::fabs(t.s - car.s()) > 10.0) continue;
        double x, y, psi; trafficPose(t, x, y, psi);
        const double dx = cx - x, dy = cy - y, c = std::cos(psi), s = std::sin(psi);
        const double lon = dx * c + dy * s, lat = -dx * s + dy * c;
        if (std::fabs(lon) < 4.3 && std::fabs(lat) < 1.75) {
            const double rel = t.oncoming ? car.sim().speed() + t.v : std::max(0.0, car.sim().speed() - t.v);
            car.bump(std::clamp(1.0 - rel / 45.0, 0.15, 0.85));
            spawnTraffic(t, car.s() + 400.0);
            if (isPlayer) {
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
    return {startS_ + 4500.0, startS_ + 9000.0, startS_ + 13500.0};
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
        if (!c || !inStation(c->s()) || c->sim().speed() > 1.5 || c->lateral() > 0.0) continue;
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
    out = r.aiControls(-lane(), pace, dist > 0 ? std::sqrt(2.0 * 3.5 * dist) : 0.0);
    if (dist < 3.0 || (inStation(s) && r.sim().speed() < 1.0)) { out.throttle = 0.0; out.brake = 1.0; }
    return true;
}

RoadControls RoadSession::rivalControls() {
    RoadCar& r = *rival_;
    const double pace = rivalPace_ * (rain_ ? 0.85 : 1.0);
    if (mode_ == Mode::Marathon) {
        RoadControls c;
        if (pitControls(1, pace, c)) { rivalLane_ = -lane(); return c; }
    }
    if (mode_ == Mode::Karma) return r.aiControls(+lane(), pace + 0.05);   // kapali yol: kendi (sol) seridinde kalir
    const double s = r.s(), v = r.sim().speed();
    double cap = 1e9, blockV = -1;
    for (const TrafficCar& t : traffic_)
        if (!t.oncoming && t.s > s && t.s - s < 25.0 + 1.2 * v && t.v < v) blockV = std::max(blockV, t.v);
    // Oyuncu da engeldir: ayni seritte ve ondeyse (sollamak icin serit degistirir)
    {
        const double ds = player_->s() - s, pv = player_->sim().speed();
        if (ds > 0 && ds < 20.0 + 1.0 * v && std::fabs(player_->lateral() - rivalLane_) < 2.0 && pv < v) blockV = std::max(blockV, pv);
    }
    bool oncomingClose = false;
    for (const TrafficCar& t : traffic_)
        if (t.oncoming && t.s > s - 15.0 && t.s - s < 60.0 + 3.0 * v) oncomingClose = true;
    if (rivalLane_ < 0 && blockV >= 0) {
        if (!oncomingClose) rivalLane_ = +lane(); else cap = blockV;
    } else if (rivalLane_ > 0) {
        bool rightClear = true;
        for (const TrafficCar& t : traffic_) if (!t.oncoming && std::fabs(t.s - s) < 18.0) rightClear = false;
        if (std::fabs(player_->s() - s) < 12.0 && player_->lateral() < 0.0) rightClear = false;
        if (rightClear || oncomingClose) rivalLane_ = -lane();
    }
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
    if (blocked(target)) target = blocked(-lane()) ? +lane() : -lane();
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
        auto consider = [&](double s, double lat, double v, bool sameDirMover) {
            if (std::fabs(lat - t.lane) > 2.2) return;
            const double g = (s - t.s) * dir - 4.5;
            if (g > -2.0 && g < gap) { gap = std::max(g, 0.1); vLead = sameDirMover ? v : 0.0; }
        };
        for (const TrafficCar& o : traffic_) if (&o != &t && o.oncoming == t.oncoming) consider(o.s, o.lane, o.v, true);
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
    if (mode_ == Mode::Marathon) {
        fuelStep(dt);
        static const double kWarn = 1.0;
        if (player_->sim().fuelLiters() < kWarn && !lowFuelMsg_) { lowFuelMsg_ = true; msgs_.push_back("YAKIT AZ! BENZINLIGE GIR"); }
        if (player_->sim().fuelLiters() > 2.0) lowFuelMsg_ = false;
    }
    if (player_->takeRecovered()) msgs_.push_back("ARAC YOLA ALINDI");
    if (player_->takeStalled()) msgs_.push_back("MOTOR STOP ETTI");
    collide(*player_, true);
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
    collide(*rival_, false);
    // Oyuncu-rakip temasi: yonlu kutu cakismasi + kutle/atalet impulsu (Contact.h)
    {
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
