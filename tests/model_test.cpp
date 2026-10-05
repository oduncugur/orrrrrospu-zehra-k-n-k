// Arac modelleri: tum katalog icin ucgen butcesi (performans), gercek olculere uyum, teker zeminde, gecerli sayilar
#include "garage/LowPolyModel.h"
#include "garage/VehicleCatalog.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace zk;
static int failures = 0;
#define CHECK(cond, msg) do { if (cond) std::printf("  GECTI  %s\n", msg); else { std::printf("  KALDI  %s\n", msg); ++failures; } } while (0)

int main() {
    std::printf("[1] Tum araclar: butce, olcu, zemin\n");
    size_t maxTris = 0, sumTris = 0; int maxId = 0, badDim = 0, badNum = 0, badGround = 0;
    for (const VehicleDef& v : vehicleCatalog()) {
        const LowPolyMesh m = buildVehicleMesh(v);
        if (m.tris.size() > maxTris) { maxTris = m.tris.size(); maxId = v.id; }
        sumTris += m.tris.size();
        float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f, z0 = 1e9f, z1 = -1e9f;
        for (const Vertex& p : m.verts) {
            if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) { ++badNum; break; }
            x0 = std::min(x0, p.x); x1 = std::max(x1, p.x); y0 = std::min(y0, p.y); y1 = std::max(y1, p.y);
            z0 = std::min(z0, p.z); z1 = std::max(z1, p.z);
        }
        for (const Tri& t : m.tris)
            if (t.a < 0 || t.b < 0 || t.c < 0 || t.a >= (int)m.verts.size() || t.b >= (int)m.verts.size() || t.c >= (int)m.verts.size()) { ++badNum; break; }
        const double len = x1 - x0, wid = y1 - y0, hgt = z1 - z0;
        // Uzunluk +-%6 (tampon/egzoz cikintisi), yukseklik -%8..+%25 (kanat), genislik ayna dahil +%25
        const bool ok = len > v.lengthM * 0.97 && len < v.lengthM * 1.06 && hgt > v.heightM * 0.92 && hgt < v.heightM * 1.25 &&
                        wid > v.widthM * 0.95 && wid < v.widthM * 1.30;
        if (!ok) { ++badDim; if (badDim <= 5) std::printf("    #%d %s: %.2fx%.2fx%.2f (gercek %.2fx%.2fx%.2f)\n", v.id, v.model.c_str(), len, wid, hgt, v.lengthM, v.widthM, v.heightM); }
        if (std::fabs(z0) > 0.01) { ++badGround; if (badGround <= 3) std::printf("    #%d en alt nokta %.3f m\n", v.id, z0); }
    }
    const size_t n = vehicleCatalog().size();
    std::printf("    %zu arac, ortalama %zu ucgen, en fazla %zu (#%d)\n", n, sumTris / n, maxTris, maxId);
    CHECK(maxTris <= 9000, "ucgen butcesi <= 9000 (46 noktali kesit; mobilde 6 arac ~54 bin ucgen)");
    CHECK(badNum == 0, "tum koseler sonlu, indeksler gecerli");
    CHECK(badDim == 0, "model olculeri gercek uzunluk/genislik/yukseklige uyuyor");
    CHECK(badGround == 0, "tekerler zeminde (z = 0)");
    std::printf(failures ? "\nSONUC: %d test KALDI\n" : "\nSONUC: tum testler gecti\n", failures);
    return failures ? 1 : 0;
}
