// ZEHRA KINIK - Acik yol oturumu: oyuncu + (yarista) YZ rakip + trafik + carpisma + yaris kurallari.
// Ekrandan bagimsiz (headless test edilir); RoadScreen yalniz girdi ve cizim yapar.
#pragma once
#include "game/FlowScore.h"
#include "game/RoadCar.h"
#include "game/RoadPath.h"
#include "game/RunField.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace zk {

// v0: istenen hiz; uid: her yeniden doguste yeni (skor takibi icin kimlik)
// Trafik aracinin yari boyu (m): tir 8.25, kamyon 4.8, otomobil 2.2
inline double trafficHalfLen(int carId) { return carId == 9001 ? 8.25 : carId == 9002 ? 4.8 : 2.2; }
struct TrafficCar { int carId; double s, lane, v, v0; bool oncoming; bool braking = false; int uid = 0; int li = 0; };   // li: serit no (lane: yanal konum)

class RoadSession {
public:
    // Free: serbest surus; Race: YZ rakiple yol yarisi; Flow: otoban akisi (sureli skor: yakin gecis, hiz, apex)
    // Karma: duz bolumler drag gorunumunde, virajli bolumler 3B; rakipli, trafiksiz (RoadPath::karma)
    // Chase: polis kovalamacasi (polis arkadan baslar; 400 m acilip 4 s tut ya da 5 km'yi bitir = kactin;
    // polis dibindeyken yavaslamak / temas yakalanma gostergesini doldurur)
    // Marathon (THE RUN): uzun hibrit etap (duzluk drag gorunumu + viraj bloklari, uzun yokus / inisler), 10-200 rakip
    // (RunField: tarz + yakit modeli), gercekci depo; etap gercek mesafeyi temsil eder (yakit sikistirmasi). Benzinlikte dur, doldur.
    enum class Mode { Free, Race, Flow, Karma, Chase, Marathon };
    static constexpr double kFlowTime = 120.0;       // akis modu suresi (s)
    enum class Phase { Countdown, Run, Finished };
    enum class Kind { Highway, Touge };               // sehirlerarasi (genis viraj) / dag yolu (dar, keskin)
    // The Run: surulen uzunluk etabin gercek mesafesiyle orantili (km basina 140 m: 110 km ~15 km, 130 km ~18 km, 480 km 60 km)
    static double runDrivenM(double realKm) { return std::clamp(realKm * 140.0, 10000.0, 60000.0); }
    double raceLength() const {                       // m
        if (mode_ == Mode::Karma) return road_.length() - kStartS - 300.0;
        if (mode_ == Mode::Chase) return 5000.0;
        if (mode_ == Mode::Marathon) return runLen_;
        return kind_ == Kind::Touge ? 3000.0 : 4000.0;
    }
    bool hasRival() const { return mode_ == Mode::Race || mode_ == Mode::Karma; }
    // The Run alani: setRunField cagrilmazsa kurucuda 19 rastgele rakip; realKm: etabin temsil ettigi gercek mesafe
    void setRunField(const std::vector<RunEntrant>& field, double realKm);
    const RunField& runField() const { return run_; }
    // Karma / The Run duz bolumu (yandan 2B drag gorunumu): carpisma yok, direksiyon yok (serit otomatik)
    bool dragPart() const { return (mode_ == Mode::Karma || mode_ == Mode::Marathon) && player_ && !road_.curvyAt(player_->s()); }
    int runPosition() const { return run_.playerPosition(player_->s(), finishT_[0] > 0, finishT_[0]); }
    int runCount() const { return (int)run_.runners().size() + 1; }
    double realKm() const { return realKm_; }
    double compression() const { return realKm_ * 1000.0 / raceLength(); }
    static constexpr double kStationGap = 3000.0;    // benzinlik araligi (surulen m)
    // Benzinlik (maraton): istasyon baslangiclari (yol s'si), alan boyu, dolum hizi, depo / tuketim
    static constexpr double kStationLen = 70.0, kRefuelLps = 2.0, kMarathonTank = 8.0, kMarathonBurn = 4.0;
    std::vector<double> stations() const;
    double nextStation(double s) const;            // s'den sonraki ilk benzinlige kalan (m; yoksa -1)
    bool inStation(double s) const;
    // Benzinlik: 3 pompa (istasyon basindan 26.25 / 35 / 43.75 m), yoldan 6.5 m icerde; yakit yalniz pompanin yaninda
    // (yol tarafi 2 m) durunca. Pompaya > 30 km/h carpan istasyonu patlatir (kullanilamaz).
    static double pumpOffset(int k) { return 26.25 + 8.75 * k; }
    double pumpLat(double s) const { return -(road_.halfWidthAt(s) + 6.5); }
    double pumpSlotLat(double s) const { return pumpLat(s) + 2.0; }
    bool stationDestroyed(int idx) const { return idx >= 0 && idx < (int)stationDead_.size() && stationDead_[idx]; }
    bool takeExplosion(double& s, double& lat) { if (explS_ < 0) return false; s = explS_; lat = explLat_; explS_ = -1; return true; }
    bool refueling(int car) const { return refuel_[car]; }   // 0 oyuncu, 1 rakip
    // Yakit stratejisi (rakip; oyuncu otopilotu / testler): mola gerekiyorsa kontrolleri doldurur ve true doner
    bool pitControls(int car, double pace, RoadControls& out);
    static constexpr double kStartS = 20.0, kRunStartS = 950.0;
    double startS() const { return startS_; }
    static constexpr double kLane = 1.8;             // serit merkezi (sag: -1.8, karsi: +1.8); dag yolunda lane()
    double lane() const { return kind_ == Kind::Touge ? 1.5 : kLane; }
    double rightLane(double s) const { return road_.laneOffset(s, false, 0); }   // en sag gidis seridi (yanal)

    RoadSession(Mode mode, int playerCar, const Tune* playerTune, int rivalCar, const Tune* rivalTune, uint32_t seed,
                Kind kind = Kind::Highway, double runRealKm = 300.0);   // runRealKm: The Run etabinin gercek km
    Kind kind() const { return kind_; }

    void update(double dt, const RoadControls& player);

    Mode  mode() const { return mode_; }
    // Yol bolgesi (s'ye gore, 350 m bloklar): 0 kir, 1 sehir, 2 tunel. Cizim ve carpisma ayni bolgeyi kullanir.
    int zoneAt(double s) const;
    // Yol kenarindaki sert engelin yol merkezinden uzakligi (m): tunel duvari, dag / otoban bariyeri, sehir binalari;
    // -1: engel yok (acik arazi)
    double wallAt(double s, int side = 0) const;   // side: +1 sol / -1 sag (kirsal cit yalniz bazi kesimlerde, tek tarafta)
    Phase phase() const { return phase_; }
    double countdown() const { return countdown_; }
    // Karma: drag agaci (geri sayimin son 1.5 s'si 3 amber, sonra yesil); tepki = yesilden kalkisa (-1: henuz yok)
    int    treeLights() const;                        // bit: 1,2,4 amber; 8 yesil
    double reaction() const { return reaction_; }
    double rivalReaction() const { return rivalReact_; }
    // Odul carpani: karma uzun yol (8-10 km) -> mesafeyle orantili (4 km yol yarisi = 1)
    double prizeScale() const { return mode_ == Mode::Karma ? std::clamp(raceLength() / 4000.0, 1.0, 2.5) : mode_ == Mode::Marathon ? 3.0 : 1.0; }
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
    // Kovalamaca: yakalanma 0..1, kacis ilerlemesi 0..1 (400 m ustunde gecen sure / 4 s)
    double bustLevel() const { return bust_; }
    double escapeProgress() const { return std::clamp(escapeT_ / kEscapeHold, 0.0, 1.0); }
    static constexpr double kEscapeGap = 400.0, kEscapeHold = 4.0;
    double flowTimeLeft() const { return std::max(0.0, kFlowTime - raceT_); }

    std::vector<std::string> drainMessages() { auto m = std::move(msgs_); msgs_.clear(); return m; }
    bool takeCrash() { const bool c = crashEv_; crashEv_ = false; return c; }
    void setRivalPace(double p) { rivalPace_ = p; }   // YZ viraj temposu (yanal g payi; varsayilan 0.55)
    // Yagmur: tum araclarin tutusu %72, rakip temkinli (tempo x0.85)
    void setRain(bool on) { rain_ = on; const double g = on ? 0.72 : 1.0; player_->gripMul = g; if (rival_) rival_->gripMul = g; }
    bool rain() const { return rain_; }

private:
    void wallContact(RoadCar& car);              // arac duvara girdiyse geri itilir, yanal hiz soner (sekme)
    bool wallHit_ = false;
    std::vector<bool> stationDead_;
    double explS_ = -1, explLat_ = 0;
    void pumpContact(RoadCar& car, bool isPlayer);
    double runLen_ = 18000.0;
    // Kinematik arac (trafik / The Run rakibi) ile carpisma: kutle + hiz + temas noktasi -> impuls (sekme e, surtunme mu).
    // Donus: kinematik aracin yeni ileri hizi (m/s); rel: carpisma hizi (m/s)
    double impactKinematic(RoadCar& car, double ox, double oy, double opsi, double ov, double omass, double halfL, double halfW, double& rel);
    void spawnTraffic(TrafficCar& t, double fromS);
    void collide(RoadCar& car, bool isPlayer);
    RoadControls rivalControls();
    RoadControls chaseControls();
    void updateChase(double dt);

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
    double rivalPace_ = 0.50;
    bool rain_ = false;
    double reaction_ = -1.0, rivalReact_ = 0.0;      // karma tepki sureleri (rakip: 0.15-0.35 s, tohumdan)
    std::vector<std::string> msgs_;
    bool crashEv_ = false, touching_ = false;
    double bust_ = 0, escapeT_ = 0, contactKick_ = 0;
    bool refuel_[2] = {false, false};
    double pitAt_[2] = {-1, -1};                     // benzinlik durma noktasi (s; -1: mola yok)
    double refilled_[2] = {0, 0};                    // alinan toplam yakit (tuketim tahmini)
    bool lowFuelMsg_ = false;
    RunField run_;
    double realKm_ = 300.0;
    void fuelStep(double dt);
    double rnd();
};

} // namespace zk
