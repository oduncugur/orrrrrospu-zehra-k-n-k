// Acik dunya uretim olcumu: sure, bina / agac / yol sayisi, sehirler arasi mesafeler
#include "game/World.h"
#include <chrono>
#include <cmath>
#include <cstdio>

using namespace zk;

int main() {
    const auto t0 = std::chrono::steady_clock::now();
    const World& w = World::get();
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::printf("uretim %.0f ms | sehir %zu, kenar %zu, dugum %zu, bina %zu, agac %zu, nokta %zu\n", ms, w.cities.size(), w.edges.size(),
                w.nodes.size(), w.buildings.size(), w.trees.size(), w.pois.size());
    for (size_t i = 0; i < w.pois.size(); ++i) if (w.pois[i].name == "DINLENME TESISI") { std::printf("  dinlenme poi %zu\n", i); break; }
    for (size_t c = 0; c + 1 < w.cities.size(); ++c)
        std::printf("  %s -> %s: %.1f km\n", w.cities[c].name.c_str(), w.cities[c + 1].name.c_str(),
                    std::hypot(w.cities[c + 1].x - w.cities[c].x, w.cities[c + 1].y - w.cities[c].y) / 1000.0);
}
