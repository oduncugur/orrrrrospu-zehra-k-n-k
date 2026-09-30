#include "RoadSession.h"
#include "garage/VehicleCatalog.h"

#include <algorithm>
#include <cmath>

namespace zk {

double RoadSession::rnd() { rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5; return (rng_ & 0xFFFFFF) / double(0x1000000); }

RoadSession::RoadSession(Mode mode, int playerCar, const Tune* playerTune, int rivalCar, const Tune* rivalTune, uint32_t seed)
    : mode_(mode), road_(20250930u + (mode == Mode::Race ? seed % 7 : 0), 20000.0, 90.0),
      playerCar_(playerCar), rivalCar_(rivalCar), rng_(seed ? seed : 1u) {
    startS_ = 20.0;
    player_ = std::make_unique<RoadCar>(findVehicle(playerCar), playerTune, road_, startS_, -kLane);
    if (mode == Mode::Race) {
        // Rakip yan seritte, ayni cizgide
        rival_ = std::make_unique<RoadCar>(findVehicle(rivalCar), rivalTune, road_, startS_, +kLane);
        phase_ = Phase::Countdown; countdown_ = 3.0;
    }
    // Trafik: yol boyunca seyrek araclar (sag seritte yavas, karsi seritte gelen)
    const auto& cat = vehicleCatalog();
    for (int i = 0; i < 14; ++i) {
        TrafficCar t{};
        int id;
        do { id = 1 + (int)(rnd() * cat.size()); } while (!cat[id - 1].streetLegal);
        t.carId = id;
        spawnTraffic(t, startS_ + 150.0 + i * 170.0);
        traffic_.push_back(t);
    }
}

void RoadSession::spawnTraffic(TrafficCar& t, double fromS) {
    t.oncoming = rnd() < 0.45;
    t.lane = t.oncoming ? +kLane : -kLane;
    t.v = t.oncoming ? 18.0 + 8.0 * rnd() : 14.0 + 8.0 * rnd();   // 50-95 km/h
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
            if (isPlayer) { ++collisions_; crashEv_ = true; msgs_.push_back(rel > 20 ? "AGIR CARPISMA!" : "CARPISMA"); }
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
        if (!oncomingClose) rivalLane_ = +kLane; else cap = blockV;
    } else if (rivalLane_ > 0) {
        bool rightClear = true;
        for (const TrafficCar& t : traffic_) if (!t.oncoming && std::fabs(t.s - s) < 18.0) rightClear = false;
        if (std::fabs(player_->s() - s) < 12.0 && player_->lateral() < 0.0) rightClear = false;
        if (rightClear || oncomingClose) rivalLane_ = -kLane;
    }
    return r.aiControls(rivalLane_, 0.55, cap);
}

void RoadSession::update(double dt, const RoadControls& in) {
    dt = std::min(dt, 0.05);
    // Trafik hareketi + geri donusum (oyuncunun etrafinda ~1.5 km pencere)
    for (TrafficCar& t : traffic_) {
        t.s += (t.oncoming ? -t.v : t.v) * dt;
        const double ref = player_->s();
        if (t.s < ref - 250.0 || t.s > ref + 2600.0 || t.s < 5.0 || t.s > road_.length() - 20.0) spawnTraffic(t, ref + 900.0 + 1200.0 * rnd());
    }
    if (phase_ == Phase::Countdown) {
        countdown_ -= dt;
        RoadControls hold = in; hold.brake = 1.0; hold.steer = 0.0;      // isik yesil olana kadar fren
        player_->update(dt, hold);
        RoadControls rh; rh.brake = 1.0; rh.throttle = 0.3;
        rival_->update(dt, rh);
        if (countdown_ <= 0.0) { phase_ = Phase::Run; msgs_.push_back("YESIL!"); }
        return;
    }
    player_->update(dt, in);
    if (player_->takeRecovered()) msgs_.push_back("ARAC YOLA ALINDI");
    if (player_->takeStalled()) msgs_.push_back("MOTOR STOP ETTI");
    collide(*player_, true);
    if (mode_ == Mode::Free) {
        if (player_->s() > road_.length() - 80.0) { player_->recover(road_.length() - 100.0); msgs_.push_back("YOL SONU - BASA DONULDU"); }
        return;
    }
    rival_->update(dt, phase_ == Phase::Run || finishT_[1] <= 0 ? rivalControls() : RoadControls{0, 0, 0.4});
    rival_->takeRecovered(); rival_->takeStalled();
    collide(*rival_, false);
    // Oyuncu-rakip temasi: govdeler ic ice girerse yana itilir, ikisi de biraz hiz kaybeder
    {
        const double dx = rival_->sim().posX() - player_->sim().posX(), dy = rival_->sim().posY() - player_->sim().posY();
        const double h = player_->sim().heading(), c = std::cos(h), sn = std::sin(h);
        const double lon = dx * c + dy * sn, lat = -dx * sn + dy * c;
        if (std::fabs(lon) < 4.2 && std::fabs(lat) < 1.8) {
            const double push = (1.85 - std::fabs(lat)) * 0.5 * (lat >= 0 ? 1.0 : -1.0);
            player_->nudge(-push * -sn, -push * c);
            rival_->nudge(push * -sn, push * c);
            if (!touching_) { player_->bump(0.97); rival_->bump(0.97); msgs_.push_back("TEMAS!"); crashEv_ = true; }
            touching_ = true;
        } else touching_ = false;
    }
    if (phase_ == Phase::Run || phase_ == Phase::Finished) raceT_ += (phase_ == Phase::Run) ? dt : 0.0;
    const double goal = startS_ + kRaceLength;
    if (finishT_[0] <= 0 && player_->s() >= goal) { finishT_[0] = raceT_; if (winner_ < 0) winner_ = 0; }
    if (finishT_[1] <= 0 && rival_->s() >= goal) { finishT_[1] = raceT_; if (winner_ < 0) winner_ = 1; }
    if (phase_ == Phase::Run && finishT_[0] > 0) phase_ = Phase::Finished;   // oyuncu bitirince sonuc
    if (phase_ == Phase::Run && finishT_[1] > 0 && raceT_ > finishT_[1] + 30.0) { phase_ = Phase::Finished; }   // rakip bitti, 30 s sonra kaybettin
}

} // namespace zk
