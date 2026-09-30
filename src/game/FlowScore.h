// ZEHRA KINIK - Otoban akisi skoru (Faz 5 olcut 6): yakin gecis, hiz, karsi serit, viraj (apex); carpisma cezasi.
// Oturumdan bagimsiz saf mantik: her kare oyuncu durumu + trafik anlik goruntusu verilir (headless test edilir).
#pragma once
#include <string>
#include <utility>
#include <vector>

namespace zk {

class FlowScorer {
public:
    // Kurallar (birim: m, m/s, s)
    static constexpr double kNearGap = 1.0;       // yakin gecis: govdeler arasi yanal bosluk bundan az
    static constexpr double kMinNearV = 60 / 3.6; // yakin gecis icin en dusuk hiz
    static constexpr double kSpeedFloor = 90 / 3.6;   // bunun ustunde hiz puani akar
    static constexpr double kComboTime = 4.0;     // kombo penceresi
    static constexpr int    kMaxCombo = 5;
    static constexpr long   kCrashPenalty = 1000;
    static constexpr double kApexG = 0.45;        // apex puani icin en dusuk yanal ivme (g)

    struct Car { int uid; double s, lat; bool oncoming; };   // trafik anlik goruntusu (uid: yeniden doguste degisir)
    struct Stats { long score = 0; int nearMisses = 0, oncomingMisses = 0, apexes = 0, crashes = 0, bestCombo = 1;
                   double topSpeed = 0, oncomingTime = 0; };

    // apexes: (s, egrilik) viraj tepe noktalari; playerHalfWidth: oyuncu govde yari genisligi
    FlowScorer(std::vector<std::pair<double, double>> apexes, double playerHalfWidth);

    // Bir kare. lat: yol ekseninden sola + (karsi serit > 0). offRoad: asfalt disi
    void update(double dt, double s, double lat, double v, bool offRoad, const std::vector<Car>& cars);
    void crash();                                 // carpisma: ceza + kombo sifir

    const Stats& stats() const { return st_; }
    long   score() const { return st_.score; }
    int    combo() const { return combo_; }
    double comboLeft() const { return comboT_; }  // kombo bitimine kalan (s)
    std::vector<std::string> drainMessages() { auto m = std::move(msgs_); msgs_.clear(); return m; }

private:
    struct Track { int uid; int side; double minGap; bool oncoming; bool seen; };
    void award(long pts, const std::string& what, bool comboEvent);
    std::vector<std::pair<double, double>> apexes_;
    size_t nextApex_ = 0;
    double halfW_, prevS_ = -1, speedAcc_ = 0;
    std::vector<Track> tracks_;
    Stats st_;
    int combo_ = 1; double comboT_ = 0;
    std::vector<std::string> msgs_;
};

// Yol egriligi dizisinden viraj tepe noktalari: yaricapi maxRadius'tan dar her viraj icin bir nokta
// (en dar kismin ortasi; sabit yayda yayin ortasi)
std::vector<std::pair<double, double>> findApexes(const std::vector<double>& s, const std::vector<double>& k, double maxRadius);

} // namespace zk
