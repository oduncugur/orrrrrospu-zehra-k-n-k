// ZEHRA KINIK - Drag yarisi mantigi (platformdan bagimsiz, deterministik)
//
// Akis: BURNOUT (istege bagli) -> STAGING -> TREE -> RUN -> FINISHED
//  * Agac: Sportsman (3 amber .500 aralikla, yesil .500 sonra) ya da Pro (3 amber birlikte, yesil .400 sonra).
//  * Debriyaj pedali ısırma noktasinin ustunde (basili) iken arac frende tutulur (hill-hold / line-lock).
//    Yesilden once debriyaj birakilip arac stage isigini terk ederse KIRMIZI ISIK.
//  * Zamanlama NHRA gibi: reaksiyon = arac stage isigini terk etme ani - yesil. ET, stage isigindan itibaren.
//  * Kazanan: bitis cizgisine once ulasan (reaksiyon + ET). Kirmizi isik yakan kaybeder (ikisi de yakarsa once yakan).
//  * Rakip ayni fizik (VehicleSim) ile yapay zeka surucu tarafindan surulur.
#pragma once
#include "sim/VehicleSim.h"

#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <vector>

namespace zk {

enum class TreeType { Sportsman, Pro };
enum class RacePhase { Burnout, Staging, Tree, Run, Finished };

struct PlayerControls {
    double throttle = 0.0;      // 0..1
    double clutch = 1.0;        // 0 = birakili (kavrali) .. 1 = tam basili
    double brake = 0.0;         // 0..1
    int    requestedGear = -1;  // H-desen: hedef vites (0 = bos), -1 = degisiklik yok
    int    paddle = 0;          // +1 yukari / -1 asagi (DCT, dogbox)
    bool   twoStep = true;      // agacta devir sinirlayici (launch control)
};

struct TimeSlip {
    double reaction = 0, sixtyFt = -1, t330 = -1, eighth = -1, eighthKmh = 0, t1000 = -1, quarter = -1, trapKmh = 0;
    bool   redLight = false, broke = false, stalled = false, finished = false;
    std::string note;
};

// YZ kalkis plani: kalkis devri + debriyaj birakma suresi (t60: denemedeki 60 ft suresi, s)
struct LaunchPlan { double rpm, release, t60; };
// 1/4 mil tahmini (parca onizlemesi): -1 = ulasilamadi (aks kirildi / stop)
struct QuarterEstimate { double sixtyFt = -1, quarter = -1, trapKmh = 0; bool broke = false; };

struct LaneState {
    std::unique_ptr<VehicleSim> sim;
    const VehicleDef* car = nullptr;
    TimeSlip slip;
    bool staged = false, left = false;   // stage isigini terk etti mi
    bool armed = false;                  // stage'de debriyaja (DCT/otomatikte frene) basti: kalkis kontrolu oyuncuda
    bool burnArmed = false;
    double flatRpm = -1;                 // flat shift: debriyaja basildigi andaki devir (ECU tutar)              // burnout'ta gaza / debriyaja dokundu (oncesinde debriyaj basili: motor bogulmaz)
    double leaveTime = -1;               // yaris saatinde (s)
    // Vites gecisi durumu
    double shiftT = -1; int shiftTarget = 0;
    bool   cutIgnition = false;          // dogbox/devir kesici: ses icin
    // Yapay zeka
    double aiFoot = 1.0, aiReaction = 0.15, aiFeatherT = 0.0;
    double aiLaunchRpm = -1.0, aiRelease = 0.12;   // kalkis plani (-1: henuz planlanmadi)
    double aiSlow = 1.0;                            // hata payi: debriyaj birakma ve H-desen vites suresi carpani
    std::shared_future<LaunchPlan> plan;            // arka planda hesaplanan plan (rakip seridi)
    double autoRelease = -1;             // DCT launch: fren birakma ani (debriyaj rampasi icin)
    bool   grind = false;                // H-desende debriyajsiz vites denemesi (dis citirtisi)
};

class DragRace {
public:
    static constexpr double kQuarterMile = 402.336;
    static constexpr double kStep = 5e-5;   // 20 kHz (olculdu: 10 us referansina gore teker titresimi +%8)

    DragRace(int playerCarId, int opponentCarId, TreeType tree, uint32_t seed, bool withBurnout,
             const Tune* playerTune = nullptr, const Tune* opponentTune = nullptr);

    // Gercek zamanli ilerleme (platform her karede cagirir). Fizik sabit adimla ilerler.
    void advance(double realDt, const PlayerControls& pc);

    RacePhase phase() const { return phase_; }
    double clock() const { return clock_; }                 // yaris saati (s)
    double treeTime() const { return clock_ - treeStart_; }
    int    treeLights() const;                               // bit maskesi: 1,2,4 amberler; 8 yesil; 16 kirmizi (oyuncu); 32 kirmizi (rakip)
    const LaneState& lane(int i) const { return lanes_[i]; }  // 0 = oyuncu, 1 = rakip
    int winner() const { return winner_; }                   // -1 belirsiz, 0 oyuncu, 1 rakip
    std::vector<std::string> drainEvents() { auto v = std::move(events_); events_.clear(); return v; }
    void skipBurnout();                                      // burnout'tan stage'e gec
    void setPlayerAutopilot(bool on) { autopilot_ = on; }
    void setPlayerTractionControl(bool on) { lanes_[0].sim->setTractionControl(on); }   // aracta TC varsa    // oyuncu serdini yapay zeka surer (tanitim / test)
    TreeType tree() const { return tree_; }
    // Rakibe insan hata payi (kariyer): otomatik debriyajli oyuncu gibi gec tepki, yavas debriyaj ve vites.
    // Boylece manuel debriyaji iyi kullanan oyuncu avantajli (kullanici istegi). Plan / ET tablosu etkilenmez.
    void setOpponentHandicap(bool on) { setOpponentHandicap(on ? 1.6 : 1.0); }
    void setOpponentHandicap(double k);           // 1.0 kusursuz .. 1.6 otomatik debriyajli oyuncu (patronlar 1.05-1.3)

    using LaunchPlan = zk::LaunchPlan;
    static LaunchPlan planLaunch(const VehicleDef* car, const Tune* tune);
    void setPlayerLaunchRpm(int rpm) { tunes_[0].launchRpm = rpm; }   // 2-step (sim bu Tune'u okur)
    static QuarterEstimate estimateQuarter(const VehicleDef* car, const Tune* tune);   // pahali: arka planda

private:
    void stepPhysics(const PlayerControls& pc);
    void playerDrive(LaneState& L, const PlayerControls& pc, VehicleInputs& in);
    void aiDrive(LaneState& L, VehicleInputs& in);
    static void aiRun(LaneState& L, double t);
    void timing(LaneState& L, int idx);
    double greenTime() const;
    uint32_t rnd();

    std::vector<LaneState> lanes_;
    Tune tunes_[2];                      // parca durumu (VehicleSim isaretci tutar; omur yaris kadar)
    TreeType tree_;
    RacePhase phase_;
    double clock_ = 0.0, acc_ = 0.0, phaseT_ = 0.0, treeStart_ = -1.0, treeDelay_ = 1.0;
    int winner_ = -1;
    int prevPaddle_ = 0;
    bool autopilot_ = false;
    uint32_t rng_;
    std::vector<std::string> events_;
};

} // namespace zk
