// ZEHRA KINIK - Acik yol oturumu: oyuncu + (yarista) YZ rakip + trafik + carpisma + yaris kurallari.
// Ekrandan bagimsiz (headless test edilir); RoadScreen yalniz girdi ve cizim yapar.
#pragma once
#include "game/FlowScore.h"
#include "game/RoadCar.h"
#include "game/RoadPath.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace zk {

// v0: istenen hiz; uid: her yeniden doguste yeni (skor takibi icin kimlik)
struct TrafficCar { int carId; double s, lane, v, v0; bool oncoming; bool braking = false; int uid = 0; };

class RoadSession {
public:
    // Free: serbest surus; Race: YZ rakiple yol yarisi; Flow: otoban akisi (sureli skor: yakin gecis, hiz, apex)
    // Karma: duz bolumler drag gorunumunde, virajli bolumler 3B; rakipli, trafiksiz (RoadPath::karma)
    enum class Mode { Free, Race, Flow, Karma };
    static constexpr double kFlowTime = 120.0;       // akis modu suresi (s)
    enum class Phase { Countdown, Run, Finished };
    enum class Kind { Highway, Touge };               // sehirlerarasi (genis viraj) / dag yolu (dar, keskin)
    double raceLength() const {                       // m
        if (mode_ == Mode::Karma) return road_.length() - kStartS - 300.0;
        return kind_ == Kind::Touge ? 3000.0 : 4000.0;
    }
    bool hasRival() const { return mode_ == Mode::Race || mode_ == Mode::Karma; }
    static constexpr double kStartS = 20.0;
    static constexpr double kLane = 1.8;             // serit merkezi (sag: -1.8, karsi: +1.8); dag yolunda lane()
    double lane() const { return kind_ == Kind::Touge ? 1.5 : kLane; }

    RoadSession(Mode mode, int playerCar, const Tune* playerTune, int rivalCar, const Tune* rivalTune, uint32_t seed,
                Kind kind = Kind::Highway);
    Kind kind() const { return kind_; }

    void update(double dt, const RoadControls& player);

    Mode  mode() const { return mode_; }
    Phase phase() const { return phase_; }
    double countdown() const { return countdown_; }
    // Karma: drag agaci (geri sayimin son 1.5 s'si 3 amber, sonra yesil); tepki = yesilden kalkisa (-1: henuz yok)
    int    treeLights() const;                        // bit: 1,2,4 amber; 8 yesil
    double reaction() const { return reaction_; }
    double rivalReaction() const { return rivalReact_; }
    // Odul carpani: karma uzun yol (8-10 km) -> mesafeyle orantili (4 km yol yarisi = 1)
    double prizeScale() const { return mode_ == Mode::Karma ? std::clamp(raceLength() / 4000.0, 1.0, 2.5) : 1.0; }
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
    // Akis modu
    const FlowScorer* flow() const { return flow_.get(); }
    double flowTimeLeft() const { return std::max(0.0, kFlowTime - raceT_); }

    std::vector<std::string> drainMessages() { auto m = std::move(msgs_); msgs_.clear(); return m; }
    bool takeCrash() { const bool c = crashEv_; crashEv_ = false; return c; }
    void setRivalPace(double p) { rivalPace_ = p; }   // YZ viraj temposu (yanal g payi; varsayilan 0.55)

private:
    void spawnTraffic(TrafficCar& t, double fromS);
    void collide(RoadCar& car, bool isPlayer);
    RoadControls rivalControls();

    Mode mode_;
    Kind kind_;
    Phase phase_ = Phase::Run;
    RoadPath road_;
    std::unique_ptr<RoadCar> player_, rival_;
    std::unique_ptr<FlowScorer> flow_;
    int nextUid_ = 1;
    int playerCar_, rivalCar_;
    std::vector<TrafficCar> traffic_;
    uint32_t rng_;
    double countdown_ = 0, raceT_ = 0, startS_ = 0;
    double finishT_[2] = {0, 0};
    int winner_ = -1, collisions_ = 0;
    double rivalLane_ = -kLane;
    double rivalPace_ = 0.55;
    double reaction_ = -1.0, rivalReact_ = 0.0;      // karma tepki sureleri (rakip: 0.15-0.35 s, tohumdan)
    std::vector<std::string> msgs_;
    bool crashEv_ = false, touching_ = false;
    double rnd();
};

} // namespace zk
