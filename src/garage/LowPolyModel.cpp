#include "LowPolyModel.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace zk {

namespace {

constexpr double kPi = 3.14159265358979323846;

// Kesit istasyonu: x (0 = on tampon, 1 = arka tampon), bel cizgisi ve tavan yuksekligi (0..1 * H),
// kabin genislik orani. roof <= belt ise o istasyonda cam/kabin yok.
struct Station { double x, sill, belt, roof, cabinW, bodyW; };

std::vector<Station> profileFor(Body b) {
    switch (b) {
    case Body::Hatch: case Body::RallyHatch: case Body::Kei:
        return {{0.00, 0.28, 0.52, 0.52, 0.00, 0.92}, {0.05, 0.22, 0.60, 0.60, 0.00, 1.00},
                {0.28, 0.20, 0.66, 0.66, 0.00, 1.00}, {0.40, 0.20, 0.68, 0.98, 0.80, 1.00},
                {0.62, 0.20, 0.68, 1.00, 0.82, 1.00}, {0.90, 0.22, 0.68, 0.94, 0.80, 1.00},
                {1.00, 0.26, 0.64, 0.66, 0.00, 0.96}};
    case Body::Sedan:
        return {{0.00, 0.28, 0.50, 0.50, 0.00, 0.92}, {0.05, 0.22, 0.58, 0.58, 0.00, 1.00},
                {0.28, 0.20, 0.64, 0.64, 0.00, 1.00}, {0.38, 0.20, 0.66, 0.97, 0.80, 1.00},
                {0.62, 0.20, 0.66, 1.00, 0.82, 1.00}, {0.76, 0.20, 0.66, 0.72, 0.70, 1.00},
                {0.95, 0.22, 0.68, 0.68, 0.00, 1.00}, {1.00, 0.28, 0.62, 0.62, 0.00, 0.94}};
    case Body::Wagon: case Body::Van:
        return {{0.00, 0.28, 0.50, 0.50, 0.00, 0.92}, {0.05, 0.22, 0.58, 0.58, 0.00, 1.00},
                {0.24, 0.20, 0.64, 0.64, 0.00, 1.00}, {0.34, 0.20, 0.66, 0.98, 0.82, 1.00},
                {0.96, 0.20, 0.66, 1.00, 0.84, 1.00}, {1.00, 0.24, 0.64, 0.94, 0.80, 0.97}};
    case Body::Coupe: case Body::Muscle:
        return {{0.00, 0.28, 0.48, 0.48, 0.00, 0.92}, {0.06, 0.22, 0.56, 0.56, 0.00, 1.00},
                {0.36, 0.20, 0.62, 0.62, 0.00, 1.00}, {0.46, 0.20, 0.64, 0.99, 0.78, 1.00},
                {0.62, 0.20, 0.64, 1.00, 0.78, 1.00}, {0.84, 0.20, 0.66, 0.70, 0.66, 1.00},
                {1.00, 0.26, 0.62, 0.62, 0.00, 0.95}};
    case Body::Roadster:
        return {{0.00, 0.28, 0.52, 0.52, 0.00, 0.92}, {0.06, 0.22, 0.62, 0.62, 0.00, 1.00},
                {0.40, 0.20, 0.72, 0.72, 0.00, 1.00}, {0.45, 0.20, 0.72, 1.00, 0.84, 1.00},
                {0.50, 0.20, 0.72, 0.74, 0.80, 1.00}, {0.68, 0.20, 0.74, 0.74, 0.00, 1.00},
                {1.00, 0.26, 0.70, 0.70, 0.00, 0.96}};
    case Body::Super:
        return {{0.00, 0.22, 0.34, 0.34, 0.00, 0.90}, {0.08, 0.18, 0.46, 0.46, 0.00, 1.00},
                {0.30, 0.16, 0.56, 0.56, 0.00, 1.00}, {0.40, 0.16, 0.58, 0.98, 0.72, 1.00},
                {0.58, 0.16, 0.62, 1.00, 0.70, 1.00}, {0.80, 0.18, 0.66, 0.70, 0.50, 1.00},
                {1.00, 0.24, 0.66, 0.66, 0.00, 0.98}};
    case Body::SUV:
        return {{0.00, 0.30, 0.56, 0.56, 0.00, 0.94}, {0.06, 0.26, 0.62, 0.62, 0.00, 1.00},
                {0.25, 0.25, 0.66, 0.66, 0.00, 1.00}, {0.32, 0.25, 0.66, 0.98, 0.86, 1.00},
                {0.97, 0.25, 0.66, 1.00, 0.86, 1.00}, {1.00, 0.28, 0.64, 0.96, 0.84, 0.98}};
    case Body::Pickup:
        return {{0.00, 0.30, 0.58, 0.58, 0.00, 0.94}, {0.06, 0.26, 0.64, 0.64, 0.00, 1.00},
                {0.26, 0.25, 0.68, 0.68, 0.00, 1.00}, {0.32, 0.25, 0.68, 1.00, 0.86, 1.00},
                {0.52, 0.25, 0.68, 1.00, 0.86, 1.00}, {0.54, 0.25, 0.68, 0.68, 0.00, 1.00},
                {1.00, 0.28, 0.66, 0.66, 0.00, 1.00}};
    }
    return {};
}

struct Builder {
    LowPolyMesh& m;
    int v(double x, double y, double z) { m.verts.push_back({(float)x, (float)y, (float)z}); return (int)m.verts.size() - 1; }
    void quad(int a, int b, int c, int d, int mat) { m.tris.push_back({a, b, c, mat}); m.tris.push_back({a, c, d, mat}); }
    void box(double x0, double x1, double y0, double y1, double z0, double z1, int mat) {
        const int p[8] = {v(x0, y0, z0), v(x1, y0, z0), v(x1, y1, z0), v(x0, y1, z0),
                          v(x0, y0, z1), v(x1, y0, z1), v(x1, y1, z1), v(x0, y1, z1)};
        quad(p[0], p[3], p[2], p[1], mat); quad(p[4], p[5], p[6], p[7], mat);
        quad(p[0], p[1], p[5], p[4], mat); quad(p[2], p[3], p[7], p[6], mat);
        quad(p[1], p[2], p[6], p[5], mat); quad(p[3], p[0], p[4], p[7], mat);
    }
    // 8 kenarli teker (PS1: dusuk poligon), eksen y yonunde
    void wheel(double cx, double cy, double cz, double r, double w) {
        const int N = 8;
        int outer0[N], outer1[N], rim[N];
        const double yIn = cy - w / 2, yOut = cy + w / 2, side = cy > 0 ? 1.0 : -1.0;
        for (int i = 0; i < N; ++i) {
            const double a = 2 * kPi * (i + 0.5) / N;
            outer0[i] = v(cx + r * std::cos(a), yIn, cz + r * std::sin(a));
            outer1[i] = v(cx + r * std::cos(a), yOut, cz + r * std::sin(a));
            rim[i]    = v(cx + 0.62 * r * std::cos(a), side > 0 ? yOut + 0.002 : yIn - 0.002, cz + 0.62 * r * std::sin(a));
        }
        for (int i = 0; i < N; ++i) {
            const int j = (i + 1) % N;
            quad(outer0[i], outer0[j], outer1[j], outer1[i], MatTire);
        }
        const int* capOuter = side > 0 ? outer1 : outer0;
        for (int i = 0; i < N; ++i) {           // lastik yanagi (halka) + jant
            const int j = (i + 1) % N;
            quad(capOuter[i], capOuter[j], rim[j], rim[i], MatTire);
        }
        const int hub = v(cx, side > 0 ? yOut + 0.004 : yIn - 0.004, cz);
        for (int i = 0; i < N; ++i) m.tris.push_back({hub, rim[i], rim[(i + 1) % N], MatRim});
    }
};

} // namespace

LowPolyMesh buildVehicleMesh(const VehicleDef& v) {
    LowPolyMesh m; m.paintRGB = v.paintRGB;
    Builder B{m};
    const double L = v.lengthM, W = v.widthM * (v.widebody ? 1.04 : 1.0), H = v.heightM;
    const double ride = v.rideHeightM;
    const auto st = profileFor(v.body);

    // Istasyon halkasi: 6 kose (dis bukey): alt-sol, alt-sag, bel-sag, tavan-sag, tavan-sol, bel-sol
    std::vector<std::array<int, 6>> rings;
    for (const Station& s : st) {
        const double x = L * (0.5 - s.x);          // arac merkezi orijin, +x ileri
        const double hw = 0.5 * W * s.bodyW;
        const bool hasCabin = s.roof > s.belt + 0.01;
        // Kabinsiz istasyonda (kaput/bagaj) ust yuzey duz: tavan genisligi = govde genisligi
        const double cw = hasCabin ? 0.5 * W * s.cabinW * s.bodyW : hw * 0.97;
        const double zs = ride + (H - ride) * (s.sill - 0.18) * 0.5, zb = H * s.belt;
        const double zr = hasCabin ? H * s.roof : zb + 0.01;
        rings.push_back({B.v(x, hw, zs), B.v(x, -hw, zs), B.v(x, -hw, zb), B.v(x, -cw, zr), B.v(x, cw, zr), B.v(x, hw, zb)});
    }
    for (size_t k = 0; k + 1 < rings.size(); ++k) {
        const auto& a = rings[k]; const auto& b = rings[k + 1];
        const bool cabin = st[k].roof > st[k].belt + 0.01 || st[k + 1].roof > st[k + 1].belt + 0.01;
        B.quad(a[0], b[0], b[1], a[1], MatTrim);                          // taban
        B.quad(a[1], b[1], b[2], a[2], MatPaint);                         // sag yan
        B.quad(a[5], b[5], b[0], a[0], MatPaint);                         // sol yan
        B.quad(a[2], b[2], b[3], a[3], cabin ? MatGlass : MatPaint);      // sag cam / kaput
        B.quad(a[4], b[4], b[5], a[5], cabin ? MatGlass : MatPaint);      // sol cam
        B.quad(a[3], b[3], b[4], a[4], MatPaint);                         // tavan / kaput ustu
    }
    // Uc kapaklar (bukey fan) + far / stop
    auto cap = [&](const std::array<int, 6>& r, bool front) {
        for (int i = 1; i < 5; ++i) {
            if (front) m.tris.push_back({r[0], r[i + 1], r[i], MatPaint});
            else       m.tris.push_back({r[0], r[i], r[i + 1], MatPaint});
        }
    };
    cap(rings.front(), true); cap(rings.back(), false);
    const double xf = L * 0.5 + 0.004, xr = -L * 0.5 - 0.004, zl = H * (st.front().belt - 0.06);
    const bool lights = v.streetLegal;   // yaris araclarinda far yok (sokakta surulemez)
    for (double s : {-1.0, 1.0}) {
        if (lights) B.box(xf - 0.01, xf, s * W * 0.30 - 0.12, s * W * 0.30 + 0.12, zl - 0.05, zl + 0.03, MatLight);
        B.box(xr, xr + 0.01, s * W * 0.32 - 0.13, s * W * 0.32 + 0.13, zl - 0.04, zl + 0.04, MatTail);
    }
    // Aero / detay
    if (v.wing) {
        const double xw = -L * 0.44, zw = H * 0.78;
        B.box(xw - 0.18, xw + 0.06, -W * 0.46, W * 0.46, zw, zw + 0.03, MatTrim);
        for (double s : {-1.0, 1.0}) B.box(xw - 0.05, xw, s * W * 0.30 - 0.02, s * W * 0.30 + 0.02, H * 0.64, zw, MatTrim);
    }
    if (v.hoodScoop) B.box(L * 0.18, L * 0.34, -0.18, 0.18, H * 0.62, H * 0.62 + 0.07, MatTrim);

    // Tekerler: dingil mesafesine gore, lastik capi kasa tipinden
    const double r = (v.body == Body::SUV || v.body == Body::Pickup) ? 0.38
                   : v.body == Body::Kei ? 0.27 : v.body == Body::Super ? 0.34 : 0.31;
    const double tw = (v.body == Body::Super || v.widebody) ? 0.28 : 0.21;
    const double track = W * 0.5 - tw * 0.5 + 0.015;   // lastik yanagi kasadan hafif tasar
    // On cikinti: onden motorlularda arka cikintidan biraz uzun, orta/arka motorda kisa
    const double overhangF = (L - v.wheelbaseM) * (v.frontWeight > 0.5 ? 0.54 : 0.44);
    const double xFront = L * 0.5 - overhangF;
    const double xRear = xFront - v.wheelbaseM;
    for (double x : {xFront, xRear})
        for (double s : {-1.0, 1.0}) B.wheel(x, s * track, r, r, tw);
    return m;
}

bool writeMtl(const LowPolyMesh& m, const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return false;
    auto col = [&](const char* name, double r, double g, double b) {
        std::fprintf(f, "newmtl %s\nKd %.3f %.3f %.3f\nKa 0 0 0\nKs 0.05 0.05 0.05\nillum 1\n\n", name, r, g, b);
    };
    col("paint", ((m.paintRGB >> 16) & 255) / 255.0, ((m.paintRGB >> 8) & 255) / 255.0, (m.paintRGB & 255) / 255.0);
    col("glass", 0.08, 0.10, 0.14); col("tire", 0.05, 0.05, 0.05); col("rim", 0.70, 0.70, 0.72);
    col("light", 1.0, 0.97, 0.85);  col("tail", 0.75, 0.05, 0.05); col("trim", 0.12, 0.12, 0.12);
    std::fclose(f);
    return true;
}

bool writeObj(const LowPolyMesh& m, const std::string& path, const std::string& mtlName, const std::string& title) {
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return false;
    static const char* names[MatCount] = {"paint", "glass", "tire", "rim", "light", "tail", "trim"};
    std::fprintf(f, "# ZEHRA KINIK prosedurel low-poly: %s\n# %zu vertex, %zu ucgen\nmtllib %s\no car\n", title.c_str(),
                 m.verts.size(), m.tris.size(), mtlName.c_str());
    // OBJ konvansiyonu: Y yukari. Bizim (x ileri, y sol, z yukari) -> (x, z, -y)
    for (const Vertex& v : m.verts) std::fprintf(f, "v %.4f %.4f %.4f\n", v.x, v.z, -v.y);
    for (int mat = 0; mat < MatCount; ++mat) {
        bool any = false;
        for (const Tri& t : m.tris) {
            if (t.material != mat) continue;
            if (!any) { std::fprintf(f, "usemtl %s\n", names[mat]); any = true; }
            std::fprintf(f, "f %d %d %d\n", t.a + 1, t.b + 1, t.c + 1);
        }
    }
    std::fclose(f);
    return true;
}

} // namespace zk
