// ZEHRA KINIK - Rakip dengesi araci.
//   zehra_ettable gen [cikti.inc]  : tum araclar x YZ parca setleri icin 1/4 mil (DragRace::estimateQuarter,
//                                   yaristaki fizik + YZ kalkis plani) -> src/game/EtTable.inc. Fizik/YZ degisince
//                                   yeniden uretin (career_test tablonun guncelligini ornekle denetler).
//   zehra_ettable analiz           : rakip eslesmesinin dengesi (oyuncu ET - rakip ET dagilimi, kazanma tahmini)
#include "game/Career.h"
#include "game/DragRace.h"
#include "garage/VehicleCatalog.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

using namespace zk;

static int gen(const char* path) {
    const auto& cat = vehicleCatalog();
    const int n = (int)cat.size();
    std::vector<double> et(n * kOpponentPresets, -1.0);
    std::atomic<int> next{0}, done{0};
    const int jobs = n * kOpponentPresets;
    auto worker = [&]() {
        for (int j; (j = next++) < jobs;) {
            const VehicleDef* v = &cat[j / kOpponentPresets];
            const Tune t = opponentPreset(j % kOpponentPresets);
            const QuarterEstimate e = DragRace::estimateQuarter(v, &t);
            et[j] = e.quarter > 0 && !e.broke ? e.quarter : -1.0;
            const int d = ++done;
            if (d % 50 == 0) std::fprintf(stderr, "  %d / %d\n", d, jobs);
        }
    };
    std::vector<std::thread> th;
    const unsigned hw = std::max(1u, std::thread::hardware_concurrency());
    for (unsigned i = 0; i < hw; ++i) th.emplace_back(worker);
    for (auto& t : th) t.join();
    FILE* f = std::fopen(path, "wb");
    if (!f) { std::perror(path); return 1; }
    std::fprintf(f, "// URETILDI: zehra_ettable gen (elle duzenlemeyin). Arac id, YZ parca seti 0/1/2 icin 1/4 mil (s);\n"
                    "// -1 = tamamlayamadi (aks / stop). Yaristaki fizik + YZ kalkis plani (DragRace::estimateQuarter).\n");
    for (int i = 0; i < n; ++i)
        std::fprintf(f, "{%d, %.3f, %.3f, %.3f},\n", cat[i].id, et[i * 3], et[i * 3 + 1], et[i * 3 + 2]);
    std::fclose(f);
    int bad = 0;
    for (double x : et) bad += x < 0;
    std::printf("%d arac x %d set yazildi (%s), tamamlayamayan %d\n", n, kOpponentPresets, path, bad);
    return 0;
}

// Eslesme dengesi: ornek oyuncu araclari (stok ve sokak seti) x 40 tohum; eski (endeks) ve yeni (ET) eslesme
static int analyze() {
    const auto& cat = vehicleCatalog();
    for (int mode = 0; mode < 2; ++mode) {
        std::vector<double> gaps;
        int playerFaster = 0, close = 0, total = 0, n = 0;
        for (size_t i = 0; i < cat.size(); i += 7) {
            const VehicleDef& v = cat[i];
            if (!v.streetLegal) continue;
            for (int pp : {0, 1}) {
                Career c = Career::newGame();
                c.cars[0].carId = v.id; c.cars[0].tune = opponentPreset(pp);
                const double pe = tableEt(v.id, pp);
                if (pe <= 0) continue;
                for (uint32_t seed = 1; seed <= 40; ++seed) {
                    const Opponent o = mode == 0 ? pickOpponent(v.id, c.cars[0].tune, seed) : c.pickOpponentFor(seed);
                    int k = 0;
                    for (; k < kOpponentPresets; ++k) if (opponentPreset(k).tires == o.tune.tires) break;
                    const double oe = tableEt(o.carId, k);
                    if (oe <= 0) continue;
                    const double g = pe - oe;                                   // + : oyuncu yavas
                    gaps.push_back(g);
                    ++total; playerFaster += g < 0; close += std::fabs(g) < 0.3;
                }
                ++n;
            }
        }
        std::sort(gaps.begin(), gaps.end());
        auto q = [&](double f) { return gaps[(size_t)(f * (gaps.size() - 1))]; };
        std::printf("%s: %d oyuncu kurulumu, %d eslesme. ET farki (oyuncu - rakip, s): %%5 %.2f  %%25 %.2f  medyan %.2f  %%75 %.2f  %%95 %.2f\n",
                    mode == 0 ? "ESKI (endeks)" : "YENI (ET)", n, total, q(0.05), q(0.25), q(0.5), q(0.75), q(0.95));
        std::printf("   oyuncu daha hizli: %%%.0f   |fark| < 0.3 s: %%%.0f\n", 100.0 * playerFaster / total, 100.0 * close / total);
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc >= 2 && !std::strcmp(argv[1], "gen")) return gen(argc >= 3 ? argv[2] : "src/game/EtTable.inc");
    if (argc >= 2 && !std::strcmp(argv[1], "analiz")) return analyze();
    std::printf("kullanim: zehra_ettable gen [cikti.inc] | analiz\n");
    return 1;
}
