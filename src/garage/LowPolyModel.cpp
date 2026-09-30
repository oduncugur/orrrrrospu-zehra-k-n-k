#include "LowPolyModel.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace zk {

namespace {

constexpr double kPi = 3.14159265358979323846;

// Kasa profili anahtar istasyonlari. x: 0 = on tampon, 1 = arka tampon.
// sill/belt/roof: toplam yukseklige oran; roof <= belt ise kabin yok (kaput/bagaj). cabinW: kabin genislik orani.
struct Key { double x, belt, roof, cabinW; };

std::vector<Key> keysFor(Body b) {
    switch (b) {
    case Body::Hatch: case Body::RallyHatch: case Body::Kei:
        return {{0.00, 0.50, 0, 0}, {0.06, 0.60, 0, 0}, {0.27, 0.66, 0, 0}, {0.44, 0.69, 0.97, 0.80},
                {0.60, 0.69, 1.00, 0.82}, {0.84, 0.69, 0.97, 0.80}, {0.95, 0.68, 0.84, 0.76}, {1.00, 0.62, 0, 0}};
    case Body::Sedan:
        return {{0.00, 0.48, 0, 0}, {0.06, 0.57, 0, 0}, {0.30, 0.64, 0, 0}, {0.43, 0.66, 0.96, 0.80},
                {0.62, 0.66, 1.00, 0.82}, {0.76, 0.66, 0.90, 0.78}, {0.84, 0.68, 0, 0}, {0.97, 0.68, 0, 0}, {1.00, 0.60, 0, 0}};
    case Body::Wagon: case Body::Van:
        return {{0.00, 0.48, 0, 0}, {0.06, 0.57, 0, 0}, {0.25, 0.64, 0, 0}, {0.38, 0.66, 0.98, 0.82},
                {0.94, 0.66, 1.00, 0.84}, {0.99, 0.64, 0.95, 0.80}, {1.00, 0.60, 0, 0}};
    case Body::Coupe: case Body::Muscle:
        return {{0.00, 0.46, 0, 0}, {0.07, 0.55, 0, 0}, {0.36, 0.62, 0, 0}, {0.48, 0.64, 0.98, 0.76},
                {0.64, 0.64, 1.00, 0.76}, {0.82, 0.66, 0.78, 0.70}, {0.88, 0.66, 0, 0}, {1.00, 0.60, 0, 0}};
    case Body::Roadster:
        return {{0.00, 0.50, 0, 0}, {0.07, 0.60, 0, 0}, {0.40, 0.70, 0, 0}, {0.46, 0.72, 0.98, 0.84},
                {0.50, 0.72, 0.76, 0.80}, {0.66, 0.74, 0, 0}, {1.00, 0.68, 0, 0}};
    case Body::Super:
        return {{0.00, 0.32, 0, 0}, {0.08, 0.44, 0, 0}, {0.30, 0.54, 0, 0}, {0.42, 0.57, 0.97, 0.70},
                {0.58, 0.60, 1.00, 0.68}, {0.78, 0.64, 0.76, 0.52}, {0.86, 0.66, 0, 0}, {1.00, 0.64, 0, 0}};
    case Body::SUV:
        return {{0.00, 0.55, 0, 0}, {0.06, 0.62, 0, 0}, {0.24, 0.66, 0, 0}, {0.33, 0.66, 0.98, 0.86},
                {0.96, 0.66, 1.00, 0.86}, {1.00, 0.63, 0.94, 0.84}};
    case Body::Pickup:
        return {{0.00, 0.57, 0, 0}, {0.06, 0.64, 0, 0}, {0.26, 0.68, 0, 0}, {0.33, 0.68, 1.00, 0.86},
                {0.52, 0.68, 1.00, 0.86}, {0.54, 0.68, 0, 0}, {1.00, 0.66, 0, 0}};
    }
    return {};
}

struct Station { double x, belt, roof, cabinW; bool cabin; };

// Anahtar istasyonlar arasini yumusak (smoothstep) aradegerle yogunlastirir
std::vector<Station> resample(const std::vector<Key>& k, int n) {
    std::vector<double> xs;
    for (int i = 0; i <= n; ++i) xs.push_back((double)i / n);
    for (const Key& kk : k) xs.push_back(kk.x);                 // anahtarlari kesin koru (kabin sinirlari)
    std::sort(xs.begin(), xs.end());
    xs.erase(std::unique(xs.begin(), xs.end(), [](double a, double b) { return std::fabs(a - b) < 0.012; }), xs.end());
    std::vector<Station> out;
    for (double x : xs) {
        size_t j = 1;
        while (j < k.size() - 1 && k[j].x < x) ++j;
        const Key& a = k[j - 1]; const Key& b = k[j];
        const double t0 = std::clamp((x - a.x) / std::max(b.x - a.x, 1e-6), 0.0, 1.0);
        const double t = t0 * t0 * (3 - 2 * t0);
        Station s;
        s.x = x;
        s.belt = a.belt + (b.belt - a.belt) * t;
        const bool ca = a.roof > a.belt, cb = b.roof > b.belt;
        s.cabin = (ca && cb) || (ca && t0 < 0.02) || (cb && t0 > 0.98);
        s.roof = s.cabin ? (ca && cb ? a.roof + (b.roof - a.roof) * t : (ca ? a.roof : b.roof)) : s.belt;
        s.cabinW = s.cabin ? (ca && cb ? a.cabinW + (b.cabinW - a.cabinW) * t : (ca ? a.cabinW : b.cabinW)) : 0.0;
        out.push_back(s);
    }
    return out;
}

struct Builder {
    LowPolyMesh& m;
    int v(double x, double y, double z) { m.verts.push_back({(float)x, (float)y, (float)z}); return (int)m.verts.size() - 1; }
    void tri(int a, int b, int c, int mat) { m.tris.push_back({a, b, c, mat}); }
    void quad(int a, int b, int c, int d, int mat) { tri(a, b, c, mat); tri(a, c, d, mat); }
    void box(double x0, double x1, double y0, double y1, double z0, double z1, int mat) {
        const int p[8] = {v(x0, y0, z0), v(x1, y0, z0), v(x1, y1, z0), v(x0, y1, z0),
                          v(x0, y0, z1), v(x1, y0, z1), v(x1, y1, z1), v(x0, y1, z1)};
        quad(p[0], p[3], p[2], p[1], mat); quad(p[4], p[5], p[6], p[7], mat);
        quad(p[0], p[1], p[5], p[4], mat); quad(p[2], p[3], p[7], p[6], mat);
        quad(p[1], p[2], p[6], p[5], mat); quad(p[3], p[0], p[4], p[7], mat);
    }
    // x-normal duzleminde (on/arka yuz) disk: yuvarlak far, egzoz ucu
    void diskX(double x, double cy, double cz, double r, int sides, int mat) {
        const int c = v(x, cy, cz);
        int first = -1, prev = -1;
        for (int i = 0; i <= sides; ++i) {
            const double a = 2 * kPi * i / sides;
            const int p = i == sides ? first : v(x, cy + r * std::cos(a), cz + r * std::sin(a));
            if (i == 0) first = p; else tri(c, prev, p, mat);
            prev = p;
        }
    }
    // Yan yuzeyde (y sabit) yarim daire davlumbaz / camurluk dudagi
    void archY(double cx, double y, double cz, double r0, double r1, int sides, int mat) {
        int pi0 = -1, po0 = -1;
        for (int i = 0; i <= sides; ++i) {
            const double a = kPi * i / sides;
            const int pi = v(cx + r0 * std::cos(a), y, cz + r0 * std::sin(a));
            const int po = v(cx + r1 * std::cos(a), y, cz + r1 * std::sin(a));
            if (i) quad(pi0, pi, po, po0, mat);
            pi0 = pi; po0 = po;
        }
    }
    void wheel(double cx, double cy, double cz, double r, double w) {
        const int N = 12;
        int o0[N], o1[N], s1[N], rim[N];
        const double side = cy > 0 ? 1.0 : -1.0;
        const double yIn = cy - side * w / 2, yOut = cy + side * w / 2;
        for (int i = 0; i < N; ++i) {
            const double a = 2 * kPi * (i + 0.5) / N, c = std::cos(a), s = std::sin(a);
            o0[i] = v(cx + r * c, yIn, cz + r * s);
            o1[i] = v(cx + r * c, yOut, cz + r * s);
            s1[i] = v(cx + 0.97 * r * c, yOut + side * 0.012, cz + 0.97 * r * s);   // yanak siskinligi
            rim[i] = v(cx + 0.66 * r * c, yOut + side * 0.004, cz + 0.66 * r * s);
        }
        for (int i = 0; i < N; ++i) {
            const int j = (i + 1) % N;
            quad(o0[i], o0[j], o1[j], o1[i], MatTire);
            quad(o1[i], o1[j], s1[j], s1[i], MatTire);
            quad(s1[i], s1[j], rim[j], rim[i], MatTire);
        }
        const int hub = v(cx, yOut - side * 0.01, cz);
        for (int i = 0; i < N; ++i) tri(hub, rim[i], rim[(i + 1) % N], (i % 2) ? MatRim : MatChrome);  // 6 kollu jant hissi
    }
};

} // namespace

void materialColor(int m, unsigned paint, float c[3]) {
    static const float t[MatCount][3] = {{0, 0, 0},          {0.10f, 0.13f, 0.18f}, {0.06f, 0.06f, 0.06f},
                                         {0.62f, 0.63f, 0.66f}, {1.00f, 0.96f, 0.82f}, {0.80f, 0.06f, 0.06f},
                                         {0.13f, 0.13f, 0.14f}, {0.03f, 0.03f, 0.035f}, {0.92f, 0.92f, 0.88f},
                                         {0.80f, 0.80f, 0.82f}, {1.00f, 0.55f, 0.08f}};
    if (m == MatPaint) {
        c[0] = ((paint >> 16) & 255) / 255.f; c[1] = ((paint >> 8) & 255) / 255.f; c[2] = (paint & 255) / 255.f;
    } else { c[0] = t[m][0]; c[1] = t[m][1]; c[2] = t[m][2]; }
}

LowPolyMesh buildVehicleMesh(const VehicleDef& v) {
    LowPolyMesh m; m.paintRGB = v.paintRGB;
    Builder B{m};
    const double L = v.lengthM, W = v.widthM * (v.widebody ? 1.04 : 1.0), H = v.heightM;
    const double ride = v.rideHeightM;
    const bool classic = v.year < 1985;                       // eski araclar: koseli govde, yuvarlak far
    const double round = classic ? 0.35 : 1.0;                 // omuz yuvarlakligi
    const auto st = resample(keysFor(v.body), 16);

    // Tekerlek konumu ve capi (govde kesiti davlumbaz icin gerekli)
    const double r = (v.body == Body::SUV || v.body == Body::Pickup) ? 0.38
                   : v.body == Body::Kei ? 0.27 : v.body == Body::Super ? 0.34 : 0.31;
    const double overhangF = (L - v.wheelbaseM) * (v.frontWeight > 0.5 ? 0.54 : 0.44);
    const double xFront = L * 0.5 - overhangF, xRear = xFront - v.wheelbaseM;

    // ---- govde: 12 koseli kesit halkalari ----
    // Halka sirasi (sol +y): 0 alt-sol, 1 etek-sol, 2 omuz-sol, 3 bel-sol, 4 cam-ust-sol, 5 tavan-sol,
    //                        6 tavan-sag, 7 cam-ust-sag, 8 bel-sag, 9 omuz-sag, 10 etek-sag, 11 alt-sag
    const int R = 12;
    std::vector<std::array<int, R>> rings;
    for (const Station& s : st) {
        const double x = L * (0.5 - s.x);
        // Uclarda (tampon) genislik ve alt kenar yuvarlanir
        const double endT = std::min(s.x, 1.0 - s.x);
        const double endRound = std::clamp(endT / 0.06, 0.0, 1.0);
        const double hw = 0.5 * W * (0.90 + 0.10 * std::sqrt(endRound));
        const double zs = ride + 0.05 + (1.0 - endRound) * 0.06;
        const double zb = H * s.belt, zsh = zb - (0.05 + 0.05 * round) * H;
        const double zr = s.cabin ? H * s.roof : zb + 0.012;
        const double cw = s.cabin ? 0.5 * W * s.cabinW : hw * 0.93;
        const double tuck = 0.06 * round;                      // tumblehome: bel ustu iceri egim
        const double pts[R][2] = {
            {hw * 0.93, zs},           {hw, zs + 0.12 * H},     {hw, zsh},
            {hw * (1 - tuck * 0.5), zb}, {cw, zr - 0.035 * H * (s.cabin ? 1 : 0.2)}, {cw * 0.86, zr},
            {-cw * 0.86, zr},          {-cw, zr - 0.035 * H * (s.cabin ? 1 : 0.2)}, {-hw * (1 - tuck * 0.5), zb},
            {-hw, zsh},                {-hw, zs + 0.12 * H},    {-hw * 0.93, zs}};
        std::array<int, R> ring;
        for (int i = 0; i < R; ++i) ring[i] = B.v(x, pts[i][0], pts[i][1]);
        rings.push_back(ring);
    }
    for (size_t k = 0; k + 1 < rings.size(); ++k) {
        const auto& a = rings[k]; const auto& b = rings[k + 1];
        const Station& s0 = st[k]; const Station& s1 = st[k + 1];
        const bool cabin = s0.cabin && s1.cabin;
        const double slope = std::fabs(s1.roof - s0.roof) * H / (std::fabs(s1.x - s0.x) * L + 1e-6);
        for (int i = 0; i < R; ++i) {
            const int j = (i + 1) % R;
            int mat = MatPaint;
            if (i == 11) mat = MatDark;                                          // taban
            else if (cabin && (i == 3 || i == 7)) mat = MatGlass;                // yan camlar
            else if (cabin && (i == 4 || i == 5 || i == 6) && slope > 0.45) mat = MatGlass;  // on/arka cam
            B.quad(a[i], b[i], b[j], a[j], mat);
        }
    }
    // Uc kapaklar
    auto cap = [&](const std::array<int, R>& ring, bool front) {
        for (int i = 1; i < R - 1; ++i) {
            if (front) B.tri(ring[0], ring[i + 1], ring[i], MatPaint);
            else       B.tri(ring[0], ring[i], ring[i + 1], MatPaint);
        }
    };
    cap(rings.front(), true); cap(rings.back(), false);

    // ---- B direkleri (govde renginde, cam uzerine) ----
    {
        size_t first = st.size(), last = 0;
        for (size_t k = 0; k < st.size(); ++k) if (st[k].cabin) { first = std::min(first, k); last = k; }
        if (first < last && v.body != Body::Roadster) {
            const Station& sm = st[(first + last) / 2];
            const double xb = L * (0.5 - (sm.x - (v.body == Body::Coupe || v.body == Body::Muscle || v.body == Body::Super ? 0.06 : 0.0)));
            const double zb = H * sm.belt, zr = H * sm.roof;
            for (double sgn : {-1.0, 1.0}) {
                const double y = sgn * (0.5 * W * sm.cabinW + 0.5 * W * 0.96) * 0.5 + sgn * 0.01;
                B.box(xb - 0.05, xb + 0.05, y - 0.01, y + 0.01, zb, zr - 0.02, MatPaint);
            }
        }
    }

    // ---- davlumbazlar (koyu bosluk) + camurluk dudagi ----
    const double tw = (v.body == Body::Super || v.widebody) ? 0.28 : 0.21;
    const double track = W * 0.5 - tw * 0.5 + 0.015;
    for (double x : {xFront, xRear})
        for (double sgn : {-1.0, 1.0}) {
            const double y = sgn * (W * 0.5 + 0.004);
            B.archY(x, y, r, 0.0, r * 1.14, 10, MatDark);
            B.archY(x, y + sgn * 0.004, r, r * 1.14, r * 1.22, 10, v.widebody ? MatPaint : MatTrim);
        }

    // ---- on yuz: tampon, izgara, farlar, plaka, sinyaller ----
    const double xf = L * 0.5, zFront = H * st.front().belt;
    const double bumperZ0 = ride + 0.08, bumperZ1 = ride + 0.08 + 0.12;
    B.box(xf - 0.05, xf + 0.035, -W * 0.46, W * 0.46, bumperZ0, bumperZ1, classic ? MatChrome : MatTrim);
    B.box(xf - 0.01, xf + 0.012, -W * 0.18, W * 0.18, bumperZ1 + 0.02, std::max(bumperZ1 + 0.06, zFront - 0.06), MatDark);   // izgara
    B.box(xf + 0.03, xf + 0.04, -0.26, 0.26, bumperZ0 + 0.02, bumperZ0 + 0.13, MatPlate);                                     // plaka
    if (v.streetLegal) {
        const double zl = std::max(bumperZ1 + 0.07, zFront - 0.09);
        for (double sgn : {-1.0, 1.0}) {
            if (classic) B.diskX(xf + 0.012, sgn * W * 0.34, zl, 0.085, 8, MatLight);
            else B.box(xf - 0.02, xf + 0.012, sgn * W * 0.34 - 0.16, sgn * W * 0.34 + 0.13, zl - 0.045, zl + 0.04, MatLight);
            B.box(xf - 0.01, xf + 0.02, sgn * W * 0.43 - 0.04, sgn * W * 0.43 + 0.04, bumperZ0 + 0.03, bumperZ0 + 0.08, MatIndicator);
        }
    }
    // ---- arka yuz: tampon, stoplar, plaka, egzoz ----
    const double xr = -L * 0.5, zRear = H * st.back().belt;
    B.box(xr - 0.035, xr + 0.05, -W * 0.46, W * 0.46, bumperZ0, bumperZ1, classic ? MatChrome : MatTrim);
    B.box(xr - 0.04, xr - 0.03, -0.26, 0.26, bumperZ1 + 0.02, bumperZ1 + 0.13, MatPlate);
    for (double sgn : {-1.0, 1.0}) {
        const double zt = std::max(bumperZ1 + 0.08, zRear - 0.1);
        B.box(xr - 0.012, xr + 0.01, sgn * W * 0.36 - 0.14, sgn * W * 0.36 + 0.14, zt - 0.05, zt + 0.05, MatTail);
    }
    {
        const Layout lay = engineTable()[v.engine].layout;
        const bool dual = lay == Layout::V8Cross || lay == Layout::V8Flat || lay == Layout::V10 || lay == Layout::V12 ||
                          v.body == Body::Super || v.body == Body::Muscle;
        const double pipeR = v.exhaust == Exhaust::StraightPipe ? 0.055 : 0.04;
        for (double sgn : dual ? std::vector<double>{-1.0, 1.0} : std::vector<double>{-1.0})
            B.diskX(xr - 0.04, sgn * W * 0.3, bumperZ0 + 0.01, pipeR, 8, MatChrome), B.diskX(xr - 0.041, sgn * W * 0.3, bumperZ0 + 0.01, pipeR * 0.7, 8, MatDark);
    }

    // ---- aynalar ----
    {
        size_t first = 0;
        while (first < st.size() && !st[first].cabin) ++first;
        if (first < st.size()) {
            const double xm = L * (0.5 - st[first].x) - 0.12, zm = H * st[first].belt + 0.07;
            for (double sgn : {-1.0, 1.0})
                B.box(xm - 0.06, xm + 0.06, sgn * (W * 0.5 + 0.02), sgn * (W * 0.5 + 0.15), zm - 0.04, zm + 0.05, MatPaint);
        }
    }

    // ---- aero ----
    if (v.wing) {
        const double xw = -L * 0.43, zw = H * 0.80;
        B.box(xw - 0.2, xw + 0.06, -W * 0.46, W * 0.46, zw, zw + 0.028, MatTrim);
        for (double sgn : {-1.0, 1.0}) {
            B.box(xw - 0.05, xw, sgn * W * 0.30 - 0.015, sgn * W * 0.30 + 0.015, H * 0.66, zw, MatTrim);
            B.box(xw - 0.2, xw + 0.06, sgn * W * 0.46 - 0.01, sgn * W * 0.46 + 0.01, zw - 0.05, zw + 0.06, MatTrim);  // uc plakalari
        }
    }
    if (v.hoodScoop) B.box(L * 0.16, L * 0.32, -0.2, 0.2, H * 0.60, H * 0.60 + 0.08, MatDark);

    // ---- tekerler ----
    for (double x : {xFront, xRear})
        for (double sgn : {-1.0, 1.0}) B.wheel(x, sgn * track, r, r, tw);
    return m;
}

bool writeMtl(const LowPolyMesh& m, const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return false;
    static const char* names[MatCount] = {"paint", "glass", "tire", "rim", "light", "tail", "trim", "dark", "plate", "chrome", "indicator"};
    for (int i = 0; i < MatCount; ++i) {
        float c[3]; materialColor(i, m.paintRGB, c);
        std::fprintf(f, "newmtl %s\nKd %.3f %.3f %.3f\nKa 0 0 0\nKs 0.05 0.05 0.05\nillum 1\n\n", names[i], c[0], c[1], c[2]);
    }
    std::fclose(f);
    return true;
}

bool writeObj(const LowPolyMesh& m, const std::string& path, const std::string& mtlName, const std::string& title) {
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return false;
    static const char* names[MatCount] = {"paint", "glass", "tire", "rim", "light", "tail", "trim", "dark", "plate", "chrome", "indicator"};
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
