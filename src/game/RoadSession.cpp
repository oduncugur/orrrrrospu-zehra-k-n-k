#include "RoadSession.h"
#include "game/Contact.h"
#include "garage/VehicleCatalog.h"

#include <algorithm>
#include <cmath>

namespace zk {

double RoadSession::rnd() { rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5; return (rng_ & 0xFFFFFF) / double(0x1000000); }

RoadSession::RoadSession(Mode mode, int playerCar, const Tune* playerTune, int rivalCar, const Tune* rivalTune, uint32_t seed, Kind kind)
    : mode_(mode), kind_(kind),
      road_(20250930u + (mode != Mode::Free ? seed % 7 : 0) + (kind == Kind::Touge ? 1000u : 0u), 20000.0,
            kind == Kind::Touge ? 28.0 : 90.0, kind == Kind::Touge ? 3.0 : 3.6, kind == Kind::Touge ? 0.09 : 0.05),
      playerCar_(playerCar), rivalCar_(rivalCar), rng_(seed ? seed : 1u) {
    startS_ = kStartS;
    rivalLane_ = -lane();
    player_ = std::make_unique<RoadCar>(findVehicle(playerCar), playerTune, road_, startS_, -lane());
    if (mode == Mode::Race) {
        // Rakip yan seritte, ayni cizgide
        rival_ = std::make_unique<RoadCar>(findVehicle(rivalCar), rivalTune, road_, startS_, +lane());
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
    const int nTraffic = kind == Kind::Touge ? 5 : mode == Mode::Flow ? 24 : 14;   // dag yolunda trafik seyrek
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
RoadControls RoadSession::rivalControls() {
    RoadCar& r = *rival_;
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
    return r.aiControls(rivalLane_, 0.55, cap);
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
    rival_->update(dt, phase_ == Phase::Run || finishT_[1] <= 0 ? rivalControls() : RoadControls{0, 0, 0.4});
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
        }
        touching_ = c.touching;
    }
    if (phase_ == Phase::Run || phase_ == Phase::Finished) raceT_ += (phase_ == Phase::Run) ? dt : 0.0;
    const double goal = startS_ + raceLength();
    if (finishT_[0] <= 0 && player_->s() >= goal) { finishT_[0] = raceT_; if (winner_ < 0) winner_ = 0; }
    if (finishT_[1] <= 0 && rival_->s() >= goal) { finishT_[1] = raceT_; if (winner_ < 0) winner_ = 1; }
    if (phase_ == Phase::Run && finishT_[0] > 0) phase_ = Phase::Finished;   // oyuncu bitirince sonuc
    if (phase_ == Phase::Run && finishT_[1] > 0 && raceT_ > finishT_[1] + 30.0) { phase_ = Phase::Finished; }   // rakip bitti, 30 s sonra kaybettin
}

} // namespace zk
