#include "LowPolyModel.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace zk {

namespace {

constexpr double kPi = 3.14159265358979323846;

#include "BodyShapes.inc"

double smooth(double t) { t = std::clamp(t, 0.0, 1.0); return t * t * (3 - 2 * t); }

// Kasa tipinden varsayilan sekil (tabloda olmayan arac icin)
CarShape defaultShape(const VehicleDef& v) {
    Arch a = SEDAN_BOX;
    switch (v.body) {
    case Body::Kei: a = KEI_BOX; break;
    case Body::Hatch: case Body::RallyHatch: a = v.year < 1990 ? HATCH_BOX : v.year < 2005 ? HATCH_ROUND : HATCH_MODERN; break;
    case Body::Sedan: a = v.year < 1988 ? SEDAN_BOX : v.year < 2005 ? SEDAN_90 : SEDAN_MODERN; break;
    case Body::Coupe: a = COUPE_NOTCH; break;
    case Body::Wagon: a = WAGON; break;
    case Body::Roadster: a = ROADSTER; break;
    case Body::Muscle: a = MUSCLE_NOTCH; break;
    case Body::Super: a = MID_ENGINE; break;
    case Body::SUV: a = SUV_BOX; break;
    case Body::Pickup: a = PICKUP; break;
    case Body::Van: a = VAN_FLAT; break;
    }
    const bool old = v.year < 1980;
    return {v.id, a, old ? L_O : v.year < 2000 ? L_R : L_S, T_R, G_2, old ? W_0 : W_5, 2, 0.0};
}

const CarShape& shapeFor(const VehicleDef& v, CarShape& tmp) {
    for (const CarShape& s : kShapes) if (s.id == v.id) return s;
    tmp = defaultShape(v);
    return tmp;
}

const CarProfile* profileFor(int id) {
    for (const CarProfile& p : kProfiles) if (p.id == id) return &p;
    return nullptr;
}

// Yan profil: x orani (on 0 .. arka 1) -> bel (kaput/bagaj/kapi ust kenari) ve tavan yuksekligi (H orani)
struct Profile {
    ArchSpec a;
    double belt(double s) const {
        if (s < a.cowlX) {                                        // kaput: on kenardan cam dibine yumusak yukselis
            const double t = s / std::max(a.cowlX, 1e-3);
            return a.noseZ + (a.hoodZ - a.noseZ) * (1.0 - (1.0 - t) * (1.0 - t));
        }
        if (s <= a.backX) return a.hoodZ + (a.deckZ - a.hoodZ) * (s - a.cowlX) / std::max(a.backX - a.cowlX, 1e-3);
        return a.deckZ + (a.tailZ - a.deckZ) * smooth((s - a.backX) / std::max(1.0 - a.backX, 1e-3));
    }
    bool cabin(double s) const { return !a.open && s >= a.cowlX - 1e-9 && s <= a.backX + 1e-9; }
    double roof(double s) const {
        if (!cabin(s)) return belt(s);
        const double b = belt(s);
        if (s < a.roofFX) {                                       // on cam: hafif kavisli
            const double t = (s - a.cowlX) / std::max(a.roofFX - a.cowlX, 1e-3);
            return b + (1.0 - b) * std::sin(t * kPi * 0.5);
        }
        if (s <= a.roofRX) {                                      // tavan: ortada hafif kubbe
            const double t = (s - a.roofFX) / std::max(a.roofRX - a.roofFX, 1e-3);
            return 1.0 - 0.025 * std::pow(2.0 * t - 1.0, 2);
        }
        const double t = (s - a.roofRX) / std::max(a.backX - a.roofRX, 1e-3);   // arka cam (fastback disbukey)
        const double top = 0.975;
        return b + (top - b) * (1.0 - smooth(t) * 0.35 - t * 0.65);
    }
    // Cam / panel egimi (tavan yuksekligi degisimi); on ve arka camlarda buyuk
    bool steep(double s0, double s1, double L, double H) const {
        return std::fabs(roof(s1) - roof(s0)) * H / (std::fabs(s1 - s0) * L + 1e-6) > 0.35;
    }
};

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
    // x-normal duzleminde (on/arka yuz) elips disk: far, stop, egzoz
    void diskX(double x, double cy, double cz, double ry, double rz, int sides, int mat) {
        const int c = v(x, cy, cz);
        int first = -1, prev = -1;
        for (int i = 0; i <= sides; ++i) {
            const double a = 2 * kPi * i / sides;
            const int p = i == sides ? first : v(x, cy + ry * std::cos(a), cz + rz * std::sin(a));
            if (i == 0) first = p; else tri(c, prev, p, mat);
            prev = p;
        }
    }
    // Ince dortgen panel (iki uc nokta arasinda, kalinlik yok): kapi cizgisi, direk, cam paneli
    void panel(double x0, double y0, double z0, double x1, double y1, double z1,
               double x2, double y2, double z2, double x3, double y3, double z3, int mat) {
        quad(v(x0, y0, z0), v(x1, y1, z1), v(x2, y2, z2), v(x3, y3, z3), mat);
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
    // Teker: 16 kenarli lastik (sirt, yanak, omuz) + jant deseni
    void wheel(double cx, double cy, double cz, double r, double w, Rim rim) {
        const int N = 16;
        int o0[N], o1[N], s1[N], rr[N], ri[N];
        const double side = cy > 0 ? 1.0 : -1.0;
        const double yIn = cy - side * w / 2, yOut = cy + side * w / 2;
        const double rimR = rim == W_0 ? 0.62 : 0.70;               // celik jantta lastik yanagi yuksek
        for (int i = 0; i < N; ++i) {
            const double a = 2 * kPi * (i + 0.5) / N, c = std::cos(a), s = std::sin(a);
            o0[i] = v(cx + r * c, yIn, cz + r * s);
            o1[i] = v(cx + 0.985 * r * c, yOut, cz + 0.985 * r * s);
            s1[i] = v(cx + 0.93 * r * c, yOut + side * 0.014, cz + 0.93 * r * s);
            rr[i] = v(cx + rimR * r * c, yOut + side * 0.006, cz + rimR * r * s);
            ri[i] = v(cx + rimR * 0.88 * r * c, yOut - side * 0.004, cz + rimR * 0.88 * r * s);   // jant dudagi
        }
        for (int i = 0; i < N; ++i) {
            const int j = (i + 1) % N;
            quad(o0[i], o0[j], o1[j], o1[i], MatTire);
            quad(o1[i], o1[j], s1[j], s1[i], MatTire);
            quad(s1[i], s1[j], rr[j], rr[i], MatTire);
            quad(rr[i], rr[j], ri[j], ri[i], MatChrome);
        }
        const int hub = v(cx, yOut - side * 0.03, cz);
        for (int i = 0; i < N; ++i) {
            const int j = (i + 1) % N;
            int mat = MatRim;
            if (rim == W_5) mat = (i % 3 == 2) ? MatDark : MatRim;      // 5 kol + bosluk (16/3 ~ 5)
            else if (rim == W_M) mat = (i % 2) ? MatDark : MatRim;      // cok kollu
            else if (rim == W_T) mat = (i % 4 == 0) ? MatDark : MatRim; // disk / turbofan
            else mat = MatRim;                                          // celik canak
            tri(hub, ri[i], ri[j], mat);
        }
        if (rim == W_0) { const int c2 = v(cx, yOut + side * 0.012, cz); for (int i = 0; i < N; i += 2) tri(c2, ri[i], ri[(i + 2) % N], MatChrome); }
    }
};

} // namespace

void materialColor(int m, unsigned paint, float c[3]) {
    static const float t[MatCount][3] = {{0, 0, 0},          {0.04f, 0.05f, 0.07f}, {0.06f, 0.06f, 0.06f},
                                         {0.62f, 0.63f, 0.66f}, {1.00f, 0.96f, 0.82f}, {0.80f, 0.06f, 0.06f},
                                         {0.13f, 0.13f, 0.14f}, {0.03f, 0.03f, 0.035f}, {0.92f, 0.92f, 0.88f},
                                         {0.80f, 0.80f, 0.82f}, {1.00f, 0.55f, 0.08f}};
    if (m == MatPaint) {
        c[0] = ((paint >> 16) & 255) / 255.f; c[1] = ((paint >> 8) & 255) / 255.f; c[2] = (paint & 255) / 255.f;
    } else { c[0] = t[m][0]; c[1] = t[m][1]; c[2] = t[m][2]; }
}

#include "HandModels.inc"

LowPolyMesh buildVehicleMesh(const VehicleDef& v) {
    { LowPolyMesh hm; if (buildHandModel(v, hm)) return hm; }
    LowPolyMesh m; m.paintRGB = v.paintRGB;
    Builder B{m};
    CarShape tmp;
    const CarShape& sh = shapeFor(v, tmp);
    const CarProfile* cp = profileFor(v.id);
    Profile P{cp && cp->a.noseZ >= 0 ? cp->a : kArch[sh.arch]};
    if (cp && cp->a.noseZ >= 0) { P.a.open = kArch[sh.arch].open; P.a.bed = kArch[sh.arch].bed; }
    const ArchSpec& A = P.a;
    const double L = v.lengthM, W = v.widthM * (v.widebody ? 1.04 : 1.0), H = v.heightM;
    const double ride = v.rideHeightM;
    const bool classic = v.year < 1985;
    const bool modern = v.year >= 1995;
    const double shoulder = classic ? 0.03 : 0.05;              // omuz yuvarlakligi (H orani)

    // Teker: gercek dingil mesafesi; on cikinti motor yerlesimine gore
    double r = sh.wheelR > 0 ? sh.wheelR : (v.year < 1975 ? 0.29 : v.year < 1990 ? 0.30 : v.year < 2005 ? 0.315 : 0.335);
    r = std::min(r, H * 0.36);
    const bool rearEngine = sh.arch == P911 || sh.arch == BEETLE || sh.arch == MID_ENGINE || sh.arch == WEDGE || sh.arch == ROADSTER_MID;
    const double overhang = std::max(L - v.wheelbaseM, 2.2 * r);
    const double overhangF = overhang * (rearEngine ? 0.46 : v.frontWeight > 0.55 ? 0.56 : 0.48);
    const double xFront = L * 0.5 - overhangF, xRear = xFront - v.wheelbaseM;
    const double sFront = (L * 0.5 - xFront) / L, sRear = (L * 0.5 - xRear) / L;

    // ---- istasyonlar: uclarda sik (kosinus dagilimi) + profil anahtarlari + teker merkezleri ----
    std::vector<double> ss;
    const int NS = 46;
    for (int i = 0; i <= NS; ++i) ss.push_back(0.5 - 0.5 * std::cos(kPi * i / NS));
    for (double k : {A.cowlX, A.roofFX, A.roofRX, A.backX, sFront, sRear}) ss.push_back(std::clamp(k, 0.0, 1.0));
    std::sort(ss.begin(), ss.end());
    ss.erase(std::unique(ss.begin(), ss.end(), [](double a, double b) { return std::fabs(a - b) < 0.006; }), ss.end());

    // ---- kesit halkalari: 24 nokta (sol 0..11, sag 23..12 ayna) ----
    // 0 taban kenari, 1 marspiyel, 2-4 bombeli yan, 5 omuz, 6 ust kenar, 7 bel, 8-9 yan cam, 10 tavan rayi, 11 tavan
    const int R = 24, RH = 12;
    double flareF = 0.015, flareR = 0.02;                                         // camurluk siskinligi (W orani)
    if (sh.arch == BEETLE) flareF = flareR = 0.07;
    else if (sh.arch == P911) { flareF = 0.025; flareR = 0.05; }
    else if (sh.arch == MUSCLE_NOTCH || sh.arch == MUSCLE_FAST || sh.arch == FRONT_LONG) flareR = 0.035;
    else if (sh.arch == MID_ENGINE || sh.arch == WEDGE) flareR = 0.04;
    if (cp && cp->flF >= 0) flareF = cp->flF;
    if (cp && cp->flR >= 0) flareR = cp->flR;
    if (v.widebody) { flareF += 0.03; flareR += 0.035; }
    // Ustten gorunus: burun ve kuyruk yumusak daralir (modern araclarda daha cok), camurluklarda siskinlik
    const double noseW = classic ? 0.86 : 0.78, tailW = classic ? 0.90 : 0.84;
    auto halfW = [&](double s) {
        const double f = s < 0.5 ? noseW + (1.0 - noseW) * std::sqrt(smooth(s / 0.13)) : tailW + (1.0 - tailW) * std::sqrt(smooth((1.0 - s) / 0.11));
        const double gF = (s - sFront) / 0.085, gR = (s - sRear) / 0.095;
        const double flare = flareF * std::exp(-gF * gF) + flareR * std::exp(-gR * gR);
        return 0.5 * W * f * (1.0 - std::max(flareF, flareR) * 0.6 + flare);
    };
    // Yan profil yuvarlatma: kaput on kenari ve kuyruk ust kenari asagi kivrilir
    auto topZ = [&](double s) {
        double z = H * P.belt(s);
        if (s < 0.035) z -= H * 0.05 * (1.0 - smooth(s / 0.035));
        if (s > 0.975) z -= H * 0.04 * smooth((s - 0.975) / 0.025);
        return z;
    };
    const double bulge = classic ? 0.02 : 0.04, bulgeP = classic ? 4.0 : 2.0;     // yan bombe (klasik: duz yan)
    std::vector<std::array<int, R>> rings;
    for (double s : ss) {
        const double x = L * (0.5 - s);
        const double endT = std::min(s, 1.0 - s);
        const double hw = halfW(s);
        const double zs = ride + 0.04 + (1.0 - smooth(endT / 0.06)) * 0.08;
        const bool cab = P.cabin(s);
        const double zU = topZ(s);
        const double zR = cab ? std::max(zU, H * P.roof(s)) : zU;
        const double bw = hw * A.beltW;
        const double tumble = cab ? std::clamp((zR - zU) / (0.35 * H), 0.0, 1.0) : 0.0;
        const double cw = cab ? hw * (A.roofW + (1.0 - A.roofW) * (1.0 - tumble) * 0.6) : bw;
        const double zSh = zU - (classic ? 0.03 : 0.05) * H;
        double pts[RH][2];
        pts[0][0] = hw * 0.80; pts[0][1] = zs;
        pts[1][0] = hw * 0.93; pts[1][1] = zs + 0.025 * H;
        for (int k = 0; k < 3; ++k) {
            const double t = (k - 1) * 0.6;                                        // -0.6, 0, 0.6
            pts[2 + k][0] = hw * (1.0 - bulge * std::pow(std::fabs(t), bulgeP));
            pts[2 + k][1] = zs + 0.06 * H + (k + 1) / 4.0 * (zSh - zs - 0.06 * H);
        }
        pts[5][0] = hw * 0.985; pts[5][1] = zSh;
        pts[6][0] = hw * 0.95;  pts[6][1] = zU - 0.008 * H;
        pts[7][0] = bw;         pts[7][1] = zU;
        if (cab && zR > zU + 0.02 * H) {
            const double zRail = zR - 0.03 * H * tumble;
            for (int k = 0; k < 2; ++k) {
                const double f = (k + 1) / 3.0;
                pts[8 + k][0] = bw + (cw - bw) * std::pow(f, 0.8);
                pts[8 + k][1] = zU + (zRail - zU) * f;
            }
            pts[10][0] = cw;        pts[10][1] = zRail;
            pts[11][0] = cw * 0.55; pts[11][1] = zR;
        } else {                                                                  // kaput / bagaj: hafif kubbeli ust yuzey
            const double ww[4] = {0.70, 0.50, 0.30, 0.12}, zz[4] = {0.008, 0.012, 0.015, 0.016};
            const double top = cab ? zR : zU;
            for (int k = 0; k < 4; ++k) { pts[8 + k][0] = hw * ww[k]; pts[8 + k][1] = top + zz[k] * H * (cab ? 0.3 : 1.0); }
        }
        {   // camurluk kabarigi: teker ustunde govde kenari teker tepesinin altinda kalmasin (alcak burunlu araclarda
            // teker kaputtan fiskiriyordu). Yalniz dis kenar noktalari (y > %55) yukselir; kaput ortasi alcak kalir.
            const double gF = (s - sFront) / 0.075, gR = (s - sRear) / 0.075;
            const double need = 2.0 * r + 0.07, w = std::max(std::exp(-gF * gF), std::exp(-gR * gR));
            const double zNeed = need * std::min(1.0, w * 1.6);
            if (w > 0.05)
                for (int i = 4; i < RH; ++i)
                    if (pts[i][0] >= hw * 0.55 && pts[i][1] < zNeed) pts[i][1] = std::max(pts[i][1], zNeed - (i == 4 ? 0.10 : 0.0));
        }
        std::array<int, R> ring;
        for (int i = 0; i < RH; ++i) ring[i] = B.v(x, pts[i][0], pts[i][1]);
        for (int i = 0; i < RH; ++i) ring[R - 1 - i] = B.v(x, -pts[i][0], pts[i][1]);
        rings.push_back(ring);
    }
    for (size_t k = 0; k + 1 < rings.size(); ++k) {
        const auto& a = rings[k]; const auto& b = rings[k + 1];
        const double s0 = ss[k], s1 = ss[k + 1], sm = 0.5 * (s0 + s1);
        const bool cab = P.cabin(s0) && P.cabin(s1) && P.roof(sm) * H > topZ(sm) + 0.02 * H;
        const bool steep = cab && P.steep(s0, s1, L, H);
        const double rearLen = A.backX - A.roofRX;
        const double sideEnd = cp && cp->sideEnd > 0 ? cp->sideEnd
                             : rearLen > 0.15 ? A.roofRX + 0.30 * rearLen : A.backX;      // fastback: C direk yelkeni
        const bool sideGlass = cab && sm <= sideEnd;
        const double bm = P.belt(sm), rm = P.roof(sm);
        const bool rearSlope = sm > A.roofRX;
        const bool glassHigh = !rearSlope || rearLen < 0.15 || rm > bm + 0.45 * (1.0 - bm);   // arka cam ust bolum
        const bool bedOpen = A.bed && sm > A.backX + 0.01 && sm < 0.985;
        const bool windshield = cab && sm < A.roofFX;                             // konuma gore (egim degil): seritsiz cam
        const bool rearGlass = cab && sm > A.roofRX && glassHigh;
        (void)steep;
        for (int i = 0; i < R; ++i) {
            const int j = (i + 1) % R;
            const int side = i < RH ? i : R - 2 - i;                              // sol indekse esle (ayna)
            int mat = MatPaint;
            if (i == R - 1) mat = MatDark;                                        // taban
            else if ((i == RH - 1 || side == 10) && (windshield || rearGlass)) mat = MatGlass;   // on / arka cam (kenara kadar)
            else if (side == 9 && (windshield || rearGlass)) mat = modern ? MatDark : MatPaint;   // A / C direk
            else if (sideGlass && side >= 7 && side <= 9) mat = MatGlass;        // yan camlar
            if (bedOpen && side >= 7) mat = MatDark;                              // kamyonet kasasi ici
            B.quad(a[i], b[i], b[j], a[j], mat);
        }
    }
    auto cap = [&](const std::array<int, R>& ring, bool front) {                  // uc kapak: merkezden yelpaze
        float cx = 0, cy = 0, cz = 0;
        for (int i : ring) { cx += m.verts[i].x; cy += m.verts[i].y; cz += m.verts[i].z; }
        const int c = B.v(cx / R, cy / R, cz / R);
        for (int i = 0; i < R; ++i) {
            const int j = (i + 1) % R;
            if (front) B.tri(c, ring[j], ring[i], MatPaint); else B.tri(c, ring[i], ring[j], MatPaint);
        }
    };
    cap(rings.front(), true); cap(rings.back(), false);

    // Yan yuzey yardimcisi: s'de govdenin dis yuzeyi (y) ve bel/tavan
    auto sideY = [&](double s) { return halfW(s) * 1.004; };
    const double ox = 0.018;                                                       // yuzeyden disari (16 bit derinlikte cakisma olmasin)

    // ---- ustu acik: on cam paneli + cerceve, kokpit, koltuklar, roll bar ----
    if (A.open) {
        const double s0 = A.cowlX, s1 = A.roofFX;
        const double x0 = L * (0.5 - s0), x1 = L * (0.5 - s1);
        const double z0 = H * P.belt(s0), z1 = H * 0.97;
        const double w0 = halfW(s0) * A.beltW * 0.92, w1 = w0 * 0.86;
        B.panel(x0, w0, z0, x1, w1, z1, x1, -w1, z1, x0, -w0, z0, MatGlass);
        B.panel(x0, -w0, z0, x1, -w1, z1, x1, w1, z1, x0, w0, z0, MatGlass);
        B.box(x1 - 0.03, x1 + 0.02, -w1, w1, z1 - 0.02, z1 + 0.015, MatTrim);
        for (double sg : {-1.0, 1.0}) B.panel(x0, sg * w0, z0, x1, sg * w1, z1, x1 - 0.03, sg * w1, z1, x0 - 0.03, sg * w0, z0 - 0.01, MatTrim);
        const double cs0 = s1 + 0.01, cs1 = A.backX - 0.02;
        const double cx0 = L * (0.5 - cs0), cx1 = L * (0.5 - cs1);
        const double zc = H * P.belt(0.5 * (cs0 + cs1)) + 0.004;
        const double cwid = halfW(0.5 * (cs0 + cs1)) * A.beltW * 0.82;
        B.panel(cx0, cwid, zc, cx1, cwid, zc, cx1, -cwid, zc, cx0, -cwid, zc, MatDark);           // kokpit acikligi
        const double seatX = L * (0.5 - (cs0 + (cs1 - cs0) * 0.62));
        for (double sg : {-1.0, 1.0}) B.box(seatX - 0.08, seatX, sg * cwid * 0.52 - 0.22, sg * cwid * 0.52 + 0.22, zc, zc + 0.30, MatDark);
        if (sh.arch == ROADSTER_MID || v.year >= 1998)                                              // roll hoop
            for (double sg : {-1.0, 1.0}) B.box(seatX - 0.14, seatX - 0.09, sg * cwid * 0.52 - 0.2, sg * cwid * 0.52 + 0.2, zc + 0.30, zc + 0.36, MatTrim);
    }

    // ---- direkler (B ve 4 kapida C) + kapi cizgileri + kollar ----
    if (!A.open) {
        const double sB = A.roofFX + (A.roofRX - A.roofFX) * (sh.doors == 4 ? 0.48 : 0.86);
        const double sC = A.roofRX - 0.01;
        std::vector<double> pillars = {sB};
        if (sh.doors == 4 && A.backX - A.roofRX < 0.18) pillars.push_back(sC);
        for (double sp : pillars) {
            if (sp <= A.roofFX + 0.02 || sp >= A.backX - 0.01) continue;
            const double xb = L * (0.5 - sp), zb = H * P.belt(sp), zr = H * P.roof(sp);
            const double hw = halfW(sp), bw = hw * A.beltW;
            const double cw = hw * A.roofW;
            const double pw = sp == sB ? 0.045 : 0.07;
            for (double sg : {-1.0, 1.0})
                B.panel(xb + pw, sg * (bw + ox), zb, xb - pw, sg * (bw + ox), zb, xb - pw, sg * (cw + ox), zr - 0.05 * H, xb + pw, sg * (cw + ox), zr - 0.05 * H,
                        modern && sh.doors == 4 ? MatDark : MatPaint);
        }
        // Kapi cizgileri (koyu ince serit) ve kollar
        std::vector<double> cuts = {A.cowlX + 0.015, sh.doors == 4 ? sB : std::min(sB + 0.02, A.backX - 0.02)};
        if (sh.doors == 4) cuts.push_back(std::min(sC + 0.01, sRear - 0.06));
        for (double sc : cuts) {
                const double xc = L * (0.5 - sc), zb = H * P.belt(sc) - 0.02;
                const double zs = ride + 0.12;
                for (double sg : {-1.0, 1.0})
                    B.panel(xc + 0.006, sg * (sideY(sc) + 0.012), zs, xc - 0.006, sg * (sideY(sc) + 0.012), zs,
                            xc - 0.006, sg * (sideY(sc) * A.beltW + 0.014), zb, xc + 0.006, sg * (sideY(sc) * A.beltW + 0.014), zb, MatDark);
            }
        for (size_t d = 1; d < cuts.size(); ++d) {
            const double sh0 = cuts[d] - 0.025, xh = L * (0.5 - sh0), zh = H * P.belt(sh0) - 0.08;
            for (double sg : {-1.0, 1.0}) B.box(xh - 0.05, xh + 0.05, sg * sideY(sh0) - 0.008, sg * sideY(sh0) + 0.008, zh - 0.012, zh + 0.012, MatTrim);
        }
    }

    // ---- davlumbazlar (koyu bosluk) + camurluk dudagi ----
    const double tw = (v.body == Body::Super || v.widebody) ? 0.28 : sh.arch == PICKUP || sh.arch == SUV_BOX ? 0.26 : 0.21;
    const double track = W * 0.5 - tw * 0.5 + 0.02;
    for (double x : {xFront, xRear})
        for (double sg : {-1.0, 1.0}) {
            const double y = sg * (W * 0.5 + 0.004);
            B.archY(x, sg * (W * 0.5 - tw - 0.03), r, 0.0, r * 1.12, 12, MatDark);   // koyu yuva tekerin ARKASINDA (onde tekeri ortuyordu)
            B.archY(x, y + sg * 0.004, r, r * 1.12, r * 1.20, 12, v.widebody || sh.arch == SUV_SMALL ? MatTrim : MatPaint);
        }

    // ---- on yuz ----
    const double xf = L * 0.5;
    const double zNose = H * A.noseZ;
    const double hwF = halfW(0.02);
    const double bumperZ0 = ride + 0.07, bumperZ1 = std::min(ride + 0.22, zNose - 0.12);
    const int bumperMat = classic ? MatChrome : modern ? MatPaint : MatTrim;
    B.box(xf - 0.06, xf + 0.03, -hwF * 0.97, hwF * 0.97, bumperZ0, bumperZ1, bumperMat);
    if (modern) B.box(xf - 0.02, xf + 0.035, -hwF * 0.55, hwF * 0.55, bumperZ0 + 0.01, bumperZ0 + 0.09, MatDark);   // alt hava girisi
    B.box(xf + 0.03, xf + 0.04, -0.26, 0.26, bumperZ0 + 0.015, bumperZ0 + 0.125, MatPlate);
    const double lampZ = std::max(bumperZ1 + 0.06, zNose - 0.075);
    const double lampY = hwF * 0.66;
    // Izgara
    {
        const double gz0 = bumperZ1 + 0.015, gz1 = std::max(gz0 + 0.04, zNose - 0.03);
        switch (sh.grille) {
        case G_0: break;
        case G_1: B.box(xf - 0.01, xf + 0.012, -hwF * 0.32, hwF * 0.32, lampZ - 0.025, lampZ + 0.02, MatDark); break;
        case G_2: B.box(xf - 0.01, xf + 0.012, -lampY + 0.12, lampY - 0.12, gz0, gz1, MatDark); break;
        case G_3: B.box(xf - 0.01, xf + 0.015, -lampY + 0.08, lampY - 0.08, gz0 - 0.06, gz1, MatDark);
                  B.box(xf + 0.01, xf + 0.02, -lampY + 0.08, lampY - 0.08, (gz0 + gz1) * 0.5 - 0.01, (gz0 + gz1) * 0.5 + 0.01, MatChrome); break;
        case G_4: for (double sg : {-1.0, 1.0}) B.box(xf - 0.01, xf + 0.014, sg * 0.03, sg * 0.16, gz0 + 0.01, gz1, MatDark);
                  for (double sg : {-1.0, 1.0}) B.box(xf - 0.012, xf + 0.016, sg * 0.03 - 0.01, sg * 0.03 + 0.01, gz0 + 0.01, gz1, MatChrome); break;
        case G_5: B.box(xf - 0.01, xf + 0.014, -0.09, 0.09, gz0 - 0.05, gz1, MatDark);
                  B.box(xf - 0.01, xf + 0.012, -lampY + 0.1, lampY - 0.1, gz0 - 0.05, gz0 + 0.01, MatDark); break;
        case G_6: B.box(xf - 0.01, xf + 0.014, -lampY * 0.62, lampY * 0.62, bumperZ0 + 0.02, gz1, MatDark); break;
        }
    }
    // Farlar
    if (v.streetLegal || sh.lamp == L_O || sh.lamp == L_Q) {
        for (double sg : {-1.0, 1.0}) {
            const double y = sg * lampY;
            switch (sh.lamp) {
            case L_R: B.box(xf - 0.03, xf + 0.012, y - 0.15, y + 0.15, lampZ - 0.05, lampZ + 0.04, MatLight); break;
            case L_O: B.diskX(xf + 0.014, y, lampZ, 0.085, 0.085, 12, MatLight);
                      B.diskX(xf + 0.010, y, lampZ, 0.10, 0.10, 12, MatChrome); break;
            case L_Q: for (double o : {-0.075, 0.075}) { B.diskX(xf + 0.014, y + sg * o, lampZ, 0.065, 0.065, 10, MatLight);
                                                         B.diskX(xf + 0.010, y + sg * o, lampZ, 0.078, 0.078, 10, MatDark); } break;
            case L_P: { const double zt = H * P.belt(0.04) + 0.004;                       // kapali acilir far: kaput ustu cizgi
                        const double x0 = xf - 0.05 * L, x1 = xf - 0.10 * L;
                        B.panel(x0, y + 0.16, zt, x1, y + 0.16, zt + 0.01, x1, y - 0.16, zt + 0.01, x0, y - 0.16, zt, MatTrim);
                        B.box(xf - 0.02, xf + 0.01, y - 0.12, y + 0.12, bumperZ1 + 0.01, bumperZ1 + 0.04, MatLight); } break;
            case L_S: B.panel(xf + 0.012, y - sg * 0.02, lampZ + 0.025, xf + 0.012, y + sg * 0.20, lampZ + 0.04,
                              xf - 0.02, y + sg * 0.22, lampZ - 0.01, xf + 0.012, y - sg * 0.04, lampZ - 0.03, MatLight);
                      B.panel(xf + 0.012, y - sg * 0.04, lampZ - 0.03, xf - 0.02, y + sg * 0.22, lampZ - 0.01,
                              xf + 0.012, y + sg * 0.20, lampZ + 0.04, xf + 0.012, y - sg * 0.02, lampZ + 0.025, MatLight); break;
            case L_V: B.diskX(xf + 0.012, y, lampZ, 0.13, 0.075, 14, MatGlass);
                      B.diskX(xf + 0.014, y, lampZ, 0.08, 0.06, 12, MatLight); break;
            case L_F: { const double sF = 0.07, xF = L * (0.5 - sF) + 0.03, zF = H * P.belt(sF) - 0.02;   // camurluk ustunde dik far
                        B.diskX(xF, y * 1.05, zF, 0.09, 0.09, 12, MatChrome);
                        B.diskX(xF + 0.005, y * 1.05, zF, 0.075, 0.075, 12, MatLight); } break;
            }
            B.box(xf - 0.01, xf + 0.02, sg * hwF * 0.88 - 0.04, sg * hwF * 0.88 + 0.04, bumperZ0 + 0.025, bumperZ0 + 0.065, MatIndicator);
        }
    }
    // ---- arka yuz ----
    const double xr = -L * 0.5;
    const double zTail = H * A.tailZ;
    const double hwR = halfW(0.98);
    B.box(xr - 0.03, xr + 0.06, -hwR * 0.97, hwR * 0.97, bumperZ0, bumperZ1, bumperMat);
    if (modern) B.box(xr - 0.035, xr + 0.02, -hwR * 0.6, hwR * 0.6, bumperZ0 - 0.01, bumperZ0 + 0.05, MatDark);   // difuzor
    B.box(xr - 0.04, xr - 0.03, -0.26, 0.26, bumperZ1 + 0.02, bumperZ1 + 0.13, MatPlate);
    {
        const double zt = std::max(bumperZ1 + 0.09, zTail - 0.09);
        const double ty = hwR * 0.70;
        for (double sg : {-1.0, 1.0}) {
            const double y = sg * ty;
            switch (sh.tail) {
            case T_R: B.box(xr - 0.012, xr + 0.01, y - 0.15, y + 0.15, zt - 0.05, zt + 0.05, MatTail); break;
            case T_O: for (double o : {-0.07, 0.10}) B.diskX(xr - 0.012, y - sg * o, zt, 0.065, 0.065, 12, MatTail); break;
            case T_B: B.box(xr - 0.012, xr + 0.01, y - sg * ty, y + sg * 0.05, zt - 0.04, zt + 0.04, MatTail); break;
            case T_V: for (int k = 0; k < 3; ++k) B.box(xr - 0.012, xr + 0.01, y + sg * (k * 0.07 - 0.07) - 0.025, y + sg * (k * 0.07 - 0.07) + 0.025,
                                                         zt - 0.07, zt + 0.06, MatTail); break;
            case T_S: B.box(xr - 0.012, xr + 0.01, y - 0.20, y + 0.14, zt + 0.005, zt + 0.045, MatTail); break;
            case T_1: B.diskX(xr - 0.012, y, zt, 0.06, 0.06, 12, MatTail); break;
            }
        }
    }
    {   // egzoz
        const Layout lay = engineTable()[v.engine].layout;
        const bool dual = lay == Layout::V8Cross || lay == Layout::V8Flat || lay == Layout::V10 || lay == Layout::V12 ||
                          v.body == Body::Super || v.body == Body::Muscle;
        const double pipeR = v.exhaust == Exhaust::StraightPipe ? 0.055 : 0.04;
        for (double sg : dual ? std::vector<double>{-1.0, 1.0} : std::vector<double>{-1.0}) {
            B.diskX(xr - 0.04, sg * hwR * 0.6, bumperZ0 + 0.01, pipeR, pipeR, 10, MatChrome);
            B.diskX(xr - 0.041, sg * hwR * 0.6, bumperZ0 + 0.01, pipeR * 0.7, pipeR * 0.7, 10, MatDark);
        }
    }

    // ---- aynalar ----
    {
        const double sm = std::min(A.cowlX + 0.03, A.roofFX), xm = L * (0.5 - sm);
        const double zm = H * P.belt(sm) + 0.07, ym = halfW(sm) * A.beltW;
        for (double sg : {-1.0, 1.0}) {
            B.box(xm - 0.05, xm + 0.05, sg * (ym + 0.02), sg * (ym + 0.15), zm - 0.045, zm + 0.045, modern ? MatPaint : MatChrome);
            B.box(xm - 0.052, xm - 0.048, sg * (ym + 0.03), sg * (ym + 0.14), zm - 0.035, zm + 0.035, MatGlass);
        }
    }

    // ---- aero ----
    // Kanat: fabrika kanadi (profil tablosu) yol araclarinda; yaris araci / genel super icin yuksek kanat.
    // VehicleDef.wing super araclarda varsayilan acik; profilde kanat tipi tanimliysa gercek araca uyulur.
    int wingType = v.wing ? (v.body == Body::Super && v.streetLegal && !v.widebody ? K1 : K3) : K0;   // yol super: entegre dudak
    if (cp && v.streetLegal && !v.widebody) wingType = std::max(cp->wing, v.wing && v.body != Body::Super ? (int)K3 : (int)K0);
    {
        const double sw = std::max(A.backX, 0.80);                                   // kanat bolgesi: bagaj / kuyruk
        const double xw = -L * 0.44, zDeck = topZ(std::clamp(0.94, sw, 0.97));
        const int wm = v.year >= 1990 ? MatPaint : MatTrim;
        switch (wingType) {
        case K0: break;
        case K1: {                                                                      // dudak / ducktail: kuyruk ust kenarinda
            const double xt = -L * 0.5 + 0.03, zt = topZ(0.985);
            const double hwT = halfW(0.97) * 0.86;
            if (sh.arch == P911) {                                                      // 911 ducktail: yukari kalkik kapak ucu
                B.panel(xt + 0.32, hwT, zt + 0.07, xt, hwT, zt + 0.10, xt, -hwT, zt + 0.10, xt + 0.32, -hwT, zt + 0.07, wm);
                B.panel(xt, hwT, zt + 0.10, xt, hwT, zt - 0.02, xt, -hwT, zt - 0.02, xt, -hwT, zt + 0.10, MatDark);
            } else B.box(xt - 0.01, xt + 0.14, -hwT, hwT, zt + 0.005, zt + 0.045, wm);
        } break;
        case K2: case K3: {                                                             // ayakli kanat (orta / yuksek)
            const double zw = K2 == wingType ? zDeck + 0.13 : std::max(zDeck + 0.22, H * 0.80);
            const double span = wingType == K2 ? W * 0.43 : W * 0.46;
            B.box(xw - 0.20, xw + 0.05, -span, span, zw, zw + 0.026, wingType == K2 ? wm : MatTrim);
            for (double sg : {-1.0, 1.0}) {
                B.box(xw - 0.06, xw - 0.01, sg * W * 0.30 - 0.015, sg * W * 0.30 + 0.015, zDeck - 0.02, zw, MatTrim);
                B.box(xw - 0.20, xw + 0.05, sg * span - 0.01, sg * span + 0.01, zw - 0.045, zw + 0.05, MatTrim);
            }
        } break;
        case K4: {                                                                      // tavan spoyleri: arka cam ustu
            const double sr = A.roofRX, xr0 = L * (0.5 - sr) + 0.05, zr = H * P.roof(sr);
            const double hwr = halfW(sr) * A.roofW;
            B.panel(xr0, hwr, zr + 0.01, xr0 - 0.24, hwr, zr - 0.03, xr0 - 0.24, -hwr, zr - 0.03, xr0, -hwr, zr + 0.01, wm);
            B.box(xr0 - 0.26, xr0 - 0.22, -hwr, hwr, zr - 0.07, zr - 0.02, MatDark);
        } break;
        case K5: {                                                                      // balina kuyrugu: kalin, genis, lastik kenarli
            const double zw = zDeck + (sh.arch == P911 ? 0.06 : 0.16);
            const double hwT = halfW(0.92) * 0.92;
            B.box(xw - 0.24, xw + 0.14, -hwT, hwT, zw, zw + 0.05, wm);
            B.box(xw - 0.26, xw - 0.22, -hwT, hwT, zw + 0.02, zw + 0.075, MatDark);
            if (sh.arch != P911) {
                for (double sg : {-1.0, 1.0}) B.box(xw - 0.08, xw, sg * hwT * 0.75 - 0.02, sg * hwT * 0.75 + 0.02, zDeck - 0.02, zw, wm);
            } else B.box(xw - 0.2, xw + 0.14, -hwT, hwT, zDeck - 0.01, zw, wm);
        } break;
        }
    }
    if (cp && (cp->fx & FX_STRAKE)) {                                                   // yan izgaralar: kapidan arka tekere
        const double s0 = A.roofFX + 0.06, s1 = sRear - 0.07;
        for (int k = 0; k < 5; ++k) {
            const double sm = 0.5 * (s0 + s1), zk = ride + 0.20 + (H * P.belt(sm) - 0.08 - ride - 0.20) * k / 4.0;
            for (double sg : {-1.0, 1.0})
                B.panel(L * (0.5 - s0), sg * (sideY(s0) + 0.02), zk, L * (0.5 - s1), sg * (sideY(s1) + 0.02), zk,
                        L * (0.5 - s1), sg * (sideY(s1) + 0.02), zk + 0.022, L * (0.5 - s0), sg * (sideY(s0) + 0.02), zk + 0.022, MatPaint);
        }
        for (double sg : {-1.0, 1.0}) {                                                 // izgara arkasi koyu bosluk
            const double sm = 0.5 * (s0 + s1);
            B.panel(L * (0.5 - s0), sg * (sideY(s0) + 0.012), ride + 0.18, L * (0.5 - s1), sg * (sideY(s1) + 0.012), ride + 0.18,
                    L * (0.5 - s1), sg * (sideY(s1) + 0.012), H * P.belt(sm) - 0.06, L * (0.5 - s0), sg * (sideY(s0) + 0.012), H * P.belt(sm) - 0.06, MatDark);
        }
    }
    if (cp && (cp->fx & FX_LOUVER)) {                                                   // arka cam / motor kapagi panjurlari
        const double s0 = A.roofRX + 0.03, s1 = std::min(A.backX, 0.95);
        for (int k = 0; k < 6; ++k) {
            const double sk = s0 + (s1 - s0) * (k + 0.5) / 6.0, xk = L * (0.5 - sk), zk = std::max(H * P.roof(sk), topZ(sk)) + 0.012;
            const double hk = halfW(sk) * A.roofW * 0.85;
            B.panel(xk + 0.03, hk, zk, xk - 0.03, hk, zk + 0.02, xk - 0.03, -hk, zk + 0.02, xk + 0.03, -hk, zk, MatDark);
        }
    }
    if (cp && (cp->fx & FX_VENT)) {                                                     // kaput hava girisi / cikisi
        const double s0 = A.cowlX * 0.35, s1 = A.cowlX * 0.70;
        const double x0 = L * (0.5 - s0), x1 = L * (0.5 - s1), z0 = topZ(s0) + 0.012, z1 = topZ(s1) + 0.012;
        B.panel(x0, 0.20, z0 + 0.045, x1, 0.18, z1, x1, -0.18, z1, x0, -0.20, z0 + 0.045, MatPaint);     // onu acik kepce
        B.panel(x0, 0.20, z0 + 0.045, x0, 0.20, z0 - 0.01, x0, -0.20, z0 - 0.01, x0, -0.20, z0 + 0.045, MatDark);
    }
    if (v.hoodScoop) {
        const double s0 = A.cowlX * 0.45, s1 = A.cowlX * 0.85;
        B.box(L * (0.5 - s1), L * (0.5 - s0), -0.2, 0.2, H * P.belt(s0) - 0.01, H * P.belt(s1) + 0.06, MatDark);
    }

    // ---- tekerler ----
    for (double x : {xFront, xRear})
        for (double sg : {1.0, -1.0}) {
            const size_t t0 = m.tris.size();
            B.wheel(x, sg * track, r, r, tw, sh.rim);
            m.wheels.push_back({(float)x, (float)(sg * track), (float)r, (float)r, t0, m.tris.size()});
        }
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
