// ZEHRA KINIK - Acik yol oturumu: oyuncu + (yarista) YZ rakip + trafik + carpisma + yaris kurallari.
// Ekrandan bagimsiz (headless test edilir); RoadScreen yalniz girdi ve cizim yapar.
#pragma once
#include "game/RoadCar.h"
#include "game/RoadPath.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace zk {

struct TrafficCar { int carId; double s, lane, v; bool oncoming; };

class RoadSession {
public:
    enum class Mode { Free, Race };
    enum class Phase { Countdown, Run, Finished };
    static constexpr double kRaceLength = 4000.0;    // m
    static constexpr double kLane = 1.8;             // serit merkezi (sag: -1.8, karsi: +1.8)

    RoadSession(Mode mode, int playerCar, const Tune* playerTune, int rivalCar, const Tune* rivalTune, uint32_t seed);

    void update(double dt, const RoadControls& player);

    Mode  mode() const { return mode_; }
    Phase phase() const { return phase_; }
    double countdown() const { return countdown_; }
    double raceTime() const { return raceT_; }
    const RoadPath& road() const { return road_; }
    RoadCar& player() { return *player_; }
    const RoadCar& player() const { return *player_; }
    RoadCar* rival() { return rival_.get(); }
    const std::vector<TrafficCar>& traffic() const { return traffic_; }
    int  playerCarId() const { return playerCar_; }
    int  rivalCarId() const { return rivalCar_; }
    // Trafik aracinin dunya konumu/yonu
    void trafficPose(const TrafficCar& t, double& x, double& y, double& psi) const;

    // Sonuc (yarista)
    bool   playerWon() const { return winner_ == 0; }
    double playerTime() const { return finishT_[0]; }
    double rivalTime() const { return finishT_[1]; }
    double gapMeters() const { return rival_ ? player_->s() - rival_->s() : 0.0; }   // + onde
    int    collisions() const { return collisions_; }

    std::vector<std::string> drainMessages() { auto m = std::move(msgs_); msgs_.clear(); return m; }
    bool takeCrash() { const bool c = crashEv_; crashEv_ = false; return c; }

private:
    void spawnTraffic(TrafficCar& t, double fromS);
    void collide(RoadCar& car, bool isPlayer);
    RoadControls rivalControls();

    Mode mode_;
    Phase phase_ = Phase::Run;
    RoadPath road_;
    std::unique_ptr<RoadCar> player_, rival_;
    int playerCar_, rivalCar_;
    std::vector<TrafficCar> traffic_;
    uint32_t rng_;
    double countdown_ = 0, raceT_ = 0, startS_ = 0;
    double finishT_[2] = {0, 0};
    int winner_ = -1, collisions_ = 0;
    double rivalLane_ = -kLane;
    std::vector<std::string> msgs_;
    bool crashEv_ = false, touching_ = false;
    double rnd();
};

} // namespace zk
