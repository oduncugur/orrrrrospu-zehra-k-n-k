#include "LowPolyModel.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace zk {

namespace {

constexpr double kPi = 3.14159265358979323846;

#include "BodyShapes.inc"
#include "SideTables.inc"
#include "FaceTables.inc"

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
    const SideTab* tab = nullptr;   // olculu yan cizgi (varsa parametrik egrinin yerine)
    double tabH = 1.0;              // tablonun tavan yuksekligi (z / tabH = H orani)
    // Bel: kabin boyunca %18 yukari (cam / tavan bolgesi govdeye gore cok uzundu); uclarda yumusak gecis
    double belt(double s) const {
        if (tab) {                                                       // kabin (ustu acikta kokpit) boyunca cam alti cizgisi
            const bool cock = cabin(s) || (a.open && s >= a.cowlX && s <= a.backX);
            return (cock ? tabLerp(tab->belt, 10, s) : tabLerp(tab->top, 24, s)) / tabH;
        }
        const double b = beltRaw(s);
        if (a.open) return b;
        const double w = smooth((s - a.cowlX) / 0.06) * smooth((a.backX - s) / 0.06);
        return b + 0.18 * (1.0 - b) * w;
    }
    double beltRaw(double s) const {
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
        if (tab) return a.open ? belt(s) : std::max(tabLerp(tab->top, 24, s) / tabH, belt(s));   // ustu acik: on cam ayri cizilir
        const double b = belt(s);
        const double edge = 0.988;                                // tavan on / arka kenari (kubbe ortada 1.0)
        if (s < a.roofFX) {                                       // on cam: hafif kavis, tavan kenarinda biter (tavani asmaz)
            const double t = (s - a.cowlX) / std::max(a.roofFX - a.cowlX, 1e-3);
            return b + (edge - b) * (0.65 * t + 0.35 * std::sin(t * kPi * 0.5));
        }
        if (s <= a.roofRX) {                                      // tavan: ortada cok hafif kubbe (balon olmasin)
            const double t = (s - a.roofFX) / std::max(a.roofRX - a.roofFX, 1e-3);
            return 1.0 - (1.0 - edge) * std::pow(2.0 * t - 1.0, 2);
        }
        const double t = (s - a.roofRX) / std::max(a.backX - a.roofRX, 1e-3);   // arka cam (fastback disbukey)
        return b + (edge - b) * (1.0 - smooth(t) * 0.35 - t * 0.65);
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
        const int N = 24;
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
        // Jant yuzu: gercek kollar (geometri), aralarda iceride fren diski + kaliper gorunur
        const double rimIn = rimR * 0.88 * r, hubR = rimIn * 0.24;
        const double yFace = yOut - side * 0.006, yDeep = yOut - side * 0.085, yHub = yOut + side * 0.004;
        {   // fren diski (iceride, gri; teker ile doner)
            const int dc = v(cx, yDeep, cz);
            const int M2 = 20; int d0 = -1, dp = -1;
            for (int i = 0; i <= M2; ++i) {
                const double a = 2 * kPi * i / M2;
                const int p = i == M2 ? d0 : v(cx + rimIn * 0.82 * std::cos(a), yDeep, cz + rimIn * 0.82 * std::sin(a));
                if (i == 0) d0 = p; else tri(dc, dp, p, MatRim);
                dp = p;
            }
        }
        auto spokeAt = [&](double a) -> bool {                           // bu acida kol var mi
            const double d = a / (2 * kPi);
            switch (rim) {
            case W_5: { const double f = d * 5 - std::floor(d * 5); return f < 0.24 || f > 0.92; }
            case W_M: { const double f = d * 10 - std::floor(d * 10); return f < 0.42; }
            case W_T: { const double f = d * 12 - std::floor(d * 12); return f > 0.25; }   // turbofan: dar yariklar
            default:  { const double f = d * 6 - std::floor(d * 6); return f > 0.30; }     // celik: yuvarlak delikli disk
            }
        };
        const int NS = 40;                                               // jant yuzu ince acisal dilimler (kol kenarlari keskin)
        const int hub = v(cx, yHub, cz);
        std::vector<int> hubRing(NS), rimRing(NS), rimDeep(NS), hubDeep(NS);
        for (int i = 0; i < NS; ++i) {
            const double a = 2 * kPi * i / NS, c = std::cos(a), s = std::sin(a);
            hubRing[i] = v(cx + hubR * c, yFace, cz + hubR * s);
            rimRing[i] = v(cx + rimIn * c, yFace, cz + rimIn * s);
            rimDeep[i] = v(cx + rimIn * c, yDeep + side * 0.01, cz + rimIn * s);
            hubDeep[i] = v(cx + hubR * c, yDeep + side * 0.01, cz + hubR * s);
        }
        for (int i = 0; i < NS; ++i) {
            const int j = (i + 1) % NS;
            tri(hub, hubRing[i], hubRing[j], MatRim);                    // gobek
            const double am = 2 * kPi * (i + 0.5) / NS;
            if (spokeAt(am)) quad(hubRing[i], rimRing[i], rimRing[j], hubRing[j], MatRim);   // kol yuzu
            else {
                quad(hubRing[i], hubDeep[i], hubDeep[j], hubRing[j], MatRim);                 // kol yan duvarlari (derinlik)
                quad(rimRing[i], rimRing[j], rimDeep[j], rimDeep[i], MatRim);
                const double ap = 2 * kPi * (i - 0.5) / NS, an = 2 * kPi * (i + 1.5) / NS;
                if (spokeAt(ap)) quad(hubRing[i], rimRing[i], rimDeep[i], hubDeep[i], MatRim);
                if (spokeAt(an)) quad(hubRing[j], hubDeep[j], rimDeep[j], rimRing[j], MatRim);
            }
        }
        for (int i = 0; i < NS; ++i) {                                   // jant dudagi -> yuz arasi (dudak ici)
            const int j = (i + 1) % NS;
            const double a0 = 2 * kPi * i / NS, a1 = 2 * kPi * j / NS;
            const int l0 = v(cx + rimR * 0.88 * r * std::cos(a0), yOut - side * 0.004, cz + rimR * 0.88 * r * std::sin(a0));
            const int l1 = v(cx + rimR * 0.88 * r * std::cos(a1), yOut - side * 0.004, cz + rimR * 0.88 * r * std::sin(a1));
            quad(l0, l1, rimRing[j], rimRing[i], MatChrome);
        }
        if (rim == W_0) { const int c2 = v(cx, yOut + side * 0.014, cz);  // celik: kucuk krom gobek kapagi
            for (int i = 0; i < NS; i += 4) tri(c2, hubRing[i], hubRing[(i + 4) % NS], MatChrome); }
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

// Ortak ic mekan (tum araclar; cam yari saydam cizilir): taban, torpido + gosterge kabi, direksiyon, on koltuklar
// (minder + sirtlik + baslik), arka koltuk (yer varsa), orta konsol + vites kolu. xDash: torpido on yuzu (on cam dibi),
// xBack: kabin arka siniri, hw: ic yari genislik, zF taban, zB bel (kapi ust kenari), zR tavan. Surucu solda (y +).
static void addInterior(Builder& B, double xDash, double xBack, double hw, double zF, double zB, double zR) {
    const double len = xDash - xBack;
    if (len < 0.9 || hw < 0.3 || zR - zF < 0.45) return;
    B.panel(xDash, hw, zF, xBack, hw, zF, xBack, -hw, zF, xDash, -hw, zF, MatDark);                     // taban
    for (double sg : {1.0, -1.0})                                                                       // kapi dosemeleri (yan camdan
        B.panel(xDash, sg * (hw + 0.03), zF, xBack, sg * (hw + 0.03), zF,                              //  karsi kapinin boyali ici gorunmez)
                xBack, sg * (hw + 0.03), zB - 0.01, xDash, sg * (hw + 0.03), zB - 0.01, MatTrim);
    B.box(xDash - 0.30, xDash, -hw, hw, zB - 0.24, zB + 0.02, MatDark);                                 // torpido
    B.box(xDash - 0.34, xDash - 0.20, hw * 0.5 - 0.20, hw * 0.5 + 0.20, zB + 0.02, zB + 0.08, MatDark);  // gosterge kabi
    const double xw = xDash - 0.48, yw = hw * 0.5, zw = zB - 0.02;                                       // direksiyon simidi
    B.diskX(xw, yw, zw, 0.185, 0.185, 16, MatDark);
    B.diskX(xw - 0.004, yw, zw, 0.150, 0.150, 16, MatTrim);
    B.diskX(xw - 0.008, yw, zw, 0.055, 0.055, 10, MatDark);
    B.box(xw, xDash - 0.30, yw - 0.03, yw + 0.03, zw - 0.03, zw + 0.03, MatDark);                       // kolon
    const double xs = std::max(xBack + 0.25, xDash - 1.05);                                             // on koltuk sirtligi
    const double zTop = std::min(zR - 0.10, zB + 0.40);
    for (double sg : {1.0, -1.0}) {
        const double yc = sg * hw * 0.5;
        B.box(xs + 0.02, xs + 0.52, yc - 0.23, yc + 0.23, zF, zF + 0.26, MatTrim);                     // minder
        B.box(xs - 0.10, xs + 0.04, yc - 0.24, yc + 0.24, zF + 0.20, zTop - 0.14, MatTrim);             // sirtlik
        B.box(xs - 0.09, xs + 0.02, yc - 0.13, yc + 0.13, zTop - 0.12, zTop, MatTrim);                  // baslik
    }
    B.box(xs + 0.10, xDash - 0.30, -0.09, 0.09, zF, zF + 0.24, MatDark);                                // orta konsol
    B.box(xs + 0.60, xs + 0.63, -0.015, 0.015, zF + 0.24, zF + 0.40, MatChrome);                        // vites kolu
    if (xs - xBack > 0.75) {                                                                            // arka koltuk
        const double xr = xs - 0.55;
        B.box(xr, xr + 0.45, -hw * 0.9, hw * 0.9, zF, zF + 0.24, MatTrim);
        B.box(std::max(xBack + 0.02, xr - 0.14), xr, -hw * 0.9, hw * 0.9, zF + 0.18, std::min(zR - 0.16, zB + 0.25), MatTrim);
    }
}

#include "HandModels.inc"
#include "imported/Imported.inc"

LowPolyMesh buildVehicleMesh(const VehicleDef& v) {
    { LowPolyMesh im; if (buildImportedModel(v, im)) return im; }     // hazir 3D model (tools/glb2zk)
    { LowPolyMesh hm; if (buildHandModel(v, hm)) return hm; }
    LowPolyMesh m; m.paintRGB = v.paintRGB;
    Builder B{m};
    CarShape tmp;
    const CarShape& sh = shapeFor(v, tmp);
    const CarProfile* cp = profileFor(v.id);
    Profile P{cp && cp->a.noseZ >= 0 ? cp->a : kArch[sh.arch]};
    if (cp && cp->a.noseZ >= 0) { P.a.open = kArch[sh.arch].open; P.a.bed = kArch[sh.arch].bed; }
    if (const SideTab* st = sideTabFor(v.id)) {                                 // olculu yan cizgi: anahtar noktalar tablodan
        P.tab = st;
        for (int i = 0; i < 24; ++i) P.tabH = std::max(P.tabH, st->top[i][1]);
        P.a.cowlX = st->cowl; P.a.roofFX = st->roofF; P.a.roofRX = st->roofR; P.a.backX = st->back;
        P.a.noseZ = tabLerp(st->top, 24, 0.02) / P.tabH; P.a.tailZ = tabLerp(st->top, 24, 0.98) / P.tabH;
        P.a.hoodZ = tabLerp(st->top, 24, st->cowl) / P.tabH; P.a.deckZ = tabLerp(st->top, 24, std::min(0.97, st->back + 0.05)) / P.tabH;
    }
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
    const double overhangF = overhang * (rearEngine ? 0.46 : v.frontWeight > 0.55 ? 0.52 : 0.47);   // gercek on / arka cikinti orani
    const double xFront = L * 0.5 - overhangF, xRear = xFront - v.wheelbaseM;
    const double sFront = (L * 0.5 - xFront) / L, sRear = (L * 0.5 - xRear) / L;

    // ---- istasyonlar: uclarda sik (kosinus dagilimi) + profil anahtarlari + teker merkezleri ----
    std::vector<double> ss;
    const int NS = 80;
    for (int i = 0; i <= NS; ++i) ss.push_back(0.5 - 0.5 * std::cos(kPi * i / NS));
    for (double k : {A.cowlX, A.roofFX, A.roofRX, A.backX, sFront, sRear}) ss.push_back(std::clamp(k, 0.0, 1.0));
    if (P.tab) for (int i = 0; i < 24 && (i == 0 || P.tab->top[i][0] > P.tab->top[i - 1][0]); ++i) ss.push_back(P.tab->top[i][0]);   // tablo koseleri keskin
    if (!A.open) {                                                                // direk kenarlari (direk yuzeyde keskin dursun)
        const double sB = A.roofFX + (A.roofRX - A.roofFX) * (sh.doors == 4 ? 0.48 : 0.86), sC = A.roofRX - 0.01;
        for (double d : {-0.05, 0.05}) { ss.push_back(sB + d / L); if (sh.doors == 4) ss.push_back(sC + 1.6 * d / L); }
    }
    std::sort(ss.begin(), ss.end());
    ss.erase(std::unique(ss.begin(), ss.end(), [](double a, double b) { return std::fabs(a - b) < 0.006; }), ss.end());

    // ---- kesit halkalari: 24 nokta (sol 0..11, sag 23..12 ayna) ----
    // 0 taban kenari, 1 marspiyel, 2-4 bombeli yan, 5 omuz, 6 ust kenar, 7 bel, 8-9 yan cam, 10 tavan rayi, 11 tavan
    const int RH = 12;                                   // anahtar noktalar (yari kesit)
    const int RH2 = 2 * RH - 1, R = 2 * RH2;             // 4 noktali yumusatma ile ara noktalar: yari 23, halka 46
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
        if (P.tab) return z;                                                      // olculu cizgi: uc yuvarlatmasi tabloda
        if (s < 0.035) z -= H * 0.05 * (1.0 - smooth(s / 0.035));
        if (s > 0.975) z -= H * 0.04 * smooth((s - 0.975) / 0.025);
        return z;
    };
    const double bulge = classic ? 0.02 : 0.04, bulgeP = classic ? 4.0 : 2.0;     // yan bombe (klasik: duz yan)
    std::vector<std::array<int, R>> rings;
    std::vector<std::array<std::array<double, 2>, RH2>> subs;            // her istasyonun yari kesiti (direkler yuzeyi izler)
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
            // Tumsek degil: aks cevresinde kaputun tum genisligi yumusakca yukselir (yuzey tekerin ustunde duz kalir)
            const double gF = (s - sFront) / 0.12, gR = (s - sRear) / 0.12;
            const double w = std::max(std::exp(-gF * gF), std::exp(-gR * gR));
            const double lift = std::max(0.0, 2.0 * r + 0.07 - pts[7][1]) * std::min(1.0, w * 1.4);
            if (lift > 0.0) for (int i = 5; i < RH; ++i) pts[i][1] += lift * (i == 5 ? 0.6 : 1.0);
        }
        // Ara noktalar: 4 noktali enterpolasyon (-1, 9, 9, -1) / 16; uclar tekrarlanir (ust orta ayna simetrisi korunur)
        double sub[RH2][2];
        for (int i = 0; i < RH; ++i) { sub[2 * i][0] = pts[i][0]; sub[2 * i][1] = pts[i][1]; }
        for (int i = 0; i + 1 < RH; ++i) {
            const int a = std::max(i - 1, 0), d = std::min(i + 2, RH - 1);
            for (int c = 0; c < 2; ++c) sub[2 * i + 1][c] = (-pts[a][c] + 9 * pts[i][c] + 9 * pts[i + 1][c] - pts[d][c]) / 16.0;
        }
        std::array<std::array<double, 2>, RH2> sc;
        for (int i = 0; i < RH2; ++i) sc[i] = {sub[i][0], sub[i][1]};
        subs.push_back(sc);
        std::array<int, R> ring;
        for (int i = 0; i < RH2; ++i) ring[i] = B.v(x, sub[i][0], sub[i][1]);
        for (int i = 0; i < RH2; ++i) ring[R - 1 - i] = B.v(x, -sub[i][0], sub[i][1]);
        rings.push_back(ring);
    }
    std::vector<double> pillarS;                                                  // B (ve 4 kapida C) direk konumlari
    if (!A.open) {
        const double sB = A.roofFX + (A.roofRX - A.roofFX) * (sh.doors == 4 ? 0.48 : 0.86);
        if (sB > A.roofFX + 0.02 && sB < A.backX - 0.01) pillarS.push_back(sB);
        if (sh.doors == 4 && A.backX - A.roofRX < 0.18) pillarS.push_back(A.roofRX - 0.01);
    }
    if (pillarS.empty()) pillarS.push_back(-1.0);
    for (size_t k = 0; k + 1 < rings.size(); ++k) {
        const auto& a = rings[k]; const auto& b = rings[k + 1];
        const double s0 = ss[k], s1 = ss[k + 1], sm = 0.5 * (s0 + s1);
        const bool cab = P.cabin(s0) && P.cabin(s1) && P.roof(sm) * H > topZ(sm) + 0.02 * H;
        const bool steep = cab && P.steep(s0, s1, L, H);
        const double rearLen = A.backX - A.roofRX;
        const double sideEnd = P.tab && P.tab->sideEnd > 0 ? P.tab->sideEnd : cp && cp->sideEnd > 0 ? cp->sideEnd
                             : rearLen > 0.15 ? A.roofRX + 0.30 * rearLen : A.backX;      // fastback: C direk yelkeni
        const bool sideGlass = cab && sm <= sideEnd;
        const double bm = P.belt(sm), rm = P.roof(sm);
        const bool rearSlope = sm > A.roofRX;
        const bool glassHigh = !rearSlope || rearLen < 0.15 || rm > bm + 0.45 * (1.0 - bm);   // arka cam ust bolum
        const bool bedOpen = A.bed && sm > A.backX + 0.01 && sm < 0.985;
        const bool windshield = cab && sm < A.roofFX;                             // konuma gore (egim degil): seritsiz cam
        const bool rearGlass = cab && sm > A.roofRX && glassHigh;
        (void)steep;
        bool pillarHere = false;                                                  // B / C direk: yuzeyin parcasi (ayri panel yok)
        for (double sp : pillarS) if (std::fabs(sm - sp) * L < (sp == pillarS[0] ? 0.035 : 0.06)) pillarHere = true;
        for (int i = 0; i < R; ++i) {
            const int j = (i + 1) % R;
            const int si = i < RH2 ? i : R - 2 - i;
            const int side = si / 2;                                              // sol anahtar segmente esle (ayna)
            int mat = MatPaint;
            if (i == R - 1) mat = MatDark;                                        // taban
            else if ((i == RH2 - 1 || side == 10) && (windshield || rearGlass)) mat = MatGlass;   // on / arka cam (kenara kadar)
            else if (side == 9 && (windshield || rearGlass)) mat = MatDark;                        // A / C direk: koyu fitil (ince gorunur)
            else if (sideGlass && side >= 7 && side <= 9)                        // yan camlar; direk ve ust cerceve govdeden
                mat = pillarHere || si == 19 ? (v.year >= 1980 ? MatDark : MatPaint) : MatGlass;   // 80 sonrasi siyah direk / cerceve
            if (bedOpen && side >= 7) mat = MatDark;                              // kamyonet kasasi ici
            if (A.open && side >= 8 && sm > A.roofFX + 0.01 && sm < A.backX - 0.02) continue;   // ustu acik: kokpit deligi (ic mekan gorunur)
            B.quad(a[i], b[i], b[j], a[j], mat);
        }
    }
    auto cap = [&](const std::array<int, R>& ring, bool front) {                  // uc kapak: bombeli (iki kademe), duz plaka degil
        float cx = 0, cy = 0, cz = 0;
        for (int i : ring) { cx += m.verts[i].x; cy += m.verts[i].y; cz += m.verts[i].z; }
        (void)cy;
        // Yatay seritler: sol nokta i ile aynasi R-1-i arasina orta (bombeli) ve ceyrek kolonlar; yelpaze "X" golgesi yok
        const float bulge = (front ? 1.0f : -1.0f) * (float)std::clamp(0.010 * L, 0.025, 0.05);
        int prev[5] = {-1, -1, -1, -1, -1};
        for (int i = 0; i < RH2; ++i) {
            const Vertex a = m.verts[ring[i]], b = m.verts[ring[R - 1 - i]];
            const float zt = (a.z - cz);
            const float hz = 1.0f - std::min(1.0f, std::fabs(zt) / 0.6f);           // ust / alt kenarda bombe sifira iner
            int cur[5];
            cur[0] = ring[i]; cur[4] = ring[R - 1 - i];
            for (int k = 1; k < 4; ++k) {
                const float f = k / 4.0f, w = 1.0f - std::fabs(2.0f * f - 1.0f);   // ortada en fazla
                cur[k] = B.v(a.x + (b.x - a.x) * f + bulge * hz * (0.55f + 0.45f * w), a.y + (b.y - a.y) * f, a.z + (b.z - a.z) * f);
            }
            if (prev[0] >= 0)
                for (int k = 0; k < 4; ++k) {
                    if (front) B.quad(prev[k], cur[k], cur[k + 1], prev[k + 1], MatPaint);
                    else B.quad(prev[k + 1], cur[k + 1], cur[k], prev[k], MatPaint);
                }
            for (int k = 0; k < 5; ++k) prev[k] = cur[k];
        }
        (void)cx;
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
        const double cx1 = L * (0.5 - cs1);
        const double zc = H * P.belt(0.5 * (cs0 + cs1));
        const double cwid = halfW(0.5 * (cs0 + cs1)) * A.beltW - 0.04;
        addInterior(B, x0 - 0.05, cx1, cwid, ride + 0.28, zc, std::min(H * 1.05, zc + 0.42));   // basliklar on cam ustunu gecmesin
        const double seatX = std::max(cx1 + 0.25, x0 - 0.05 - 1.05);
        if (sh.arch == ROADSTER_MID || v.year >= 1998)                                              // roll hoop
            for (double sg : {-1.0, 1.0}) B.box(seatX - 0.20, seatX - 0.14, sg * cwid * 0.5 - 0.2, sg * cwid * 0.5 + 0.2, zc + 0.30, zc + 0.36, MatTrim);
    } else {                                                                                       // kapali: camdan gorunen ic mekan
        const double sm = 0.5 * (A.roofFX + A.roofRX);
        addInterior(B, L * (0.5 - A.cowlX) - 0.20, L * (0.5 - A.backX) + 0.15, halfW(sm) * A.beltW - 0.08,
                    ride + 0.26, H * P.belt(sm), H * P.roof(sm));
    }

    // ---- on cam dibi: siyah conta bandi + iki silecek (cam / govde birlesimi) ----
    if (!A.open && A.roofFX > A.cowlX + 0.02) {
        const double s0 = A.cowlX, s1 = A.cowlX + 0.018;
        const double x0 = L * (0.5 - s0), x1 = L * (0.5 - s1), z0 = H * P.belt(s0) + 0.004, z1 = H * P.roof(s1) + 0.004;
        const double w0 = halfW(s0) * A.beltW * 0.96, w1 = halfW(s1) * A.beltW * 0.94;
        B.panel(x0, w0, z0, x1, w1, z1, x1, -w1, z1, x0, -w0, z0, MatDark);
        B.panel(x0, -w0, z0, x1, -w1, z1, x1, w1, z1, x0, w0, z0, MatDark);
        for (double yw : {0.30, -0.15}) {
            const double xa = x1 - 0.01, za = z1 + 0.006;
            B.panel(xa, yw + 0.25, za, xa, yw - 0.25, za, xa - 0.01, yw - 0.25, za + 0.012, xa - 0.01, yw + 0.25, za + 0.012, MatDark);
        }
    }

    // ---- direkler (B ve 4 kapida C) + kapi cizgileri + kollar ----
    if (!A.open) {
        const double sB = A.roofFX + (A.roofRX - A.roofFX) * (sh.doors == 4 ? 0.48 : 0.86);
        const double sC = A.roofRX - 0.01;
        std::vector<double> pillars = {sB};
        if (sh.doors == 4 && A.backX - A.roofRX < 0.18) pillars.push_back(sC);
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
    // Tampon farin hemen altinda (aradaki bos duz duvar yok); yukseklik doneme gore: klasik ince krom, modern kalin
    const double lampZ0 = zNose - 0.075;
    const double bumperZ1 = std::max(ride + 0.20, std::min(lampZ0 - 0.10, zNose - 0.12));
    const double bumperZ0 = std::max(ride + 0.06, bumperZ1 - (classic ? 0.11 : modern ? 0.24 : 0.17));
    const int bumperMat = classic ? MatChrome : modern ? MatPaint : MatTrim;
    // Saran tampon: on / arka yuzden kose yayi ile yanlara ~0.3 m doner; ortasi hafif disari bombeli. Modern: govde
    // rengi + altta koyu dudak; klasik: krom; 80-90'lar: siyah plastik.
    auto wrapBumper = [&](bool front, double z0, double z1, int mat) {
        const double sg = front ? 1.0 : -1.0, xe = front ? L * 0.5 : -L * 0.5;
        const double hwE = halfW(front ? 0.03 : 0.97) * 1.01, rc = std::min(0.24, hwE * 0.3), back = 0.30;
        std::vector<std::pair<double, double>> pl;                                // plan konturu (x, y): bir yan -> on -> diger yan
        pl.push_back({xe - sg * back, -hwE});
        for (int k = 0; k <= 5; ++k) { const double a = kPi * 0.5 * k / 5.0; pl.push_back({xe - sg * rc * (1.0 - std::sin(a)), -(hwE - rc * (1.0 - std::cos(a)))}); }
        for (int k = 5; k >= 0; --k) { const double a = kPi * 0.5 * k / 5.0; pl.push_back({xe - sg * rc * (1.0 - std::sin(a)), hwE - rc * (1.0 - std::cos(a))}); }
        pl.push_back({xe - sg * back, hwE});
        const double zs[3] = {z0, 0.5 * (z0 + z1), z1}, push[3] = {0.0, 0.018, 0.004};
        std::vector<std::array<int, 3>> col;
        for (size_t k = 0; k < pl.size(); ++k) {
            const double ny = pl[k].second > 0 ? 1.0 : -1.0;
            const double side = std::fabs(pl[k].second) > hwE - rc * 0.3 ? 1.0 : 0.0;
            std::array<int, 3> c3{};
            for (int r2 = 0; r2 < 3; ++r2)
                c3[r2] = B.v(pl[k].first + sg * push[r2] * (1.0 - side), pl[k].second + ny * push[r2] * side, zs[r2]);
            col.push_back(c3);
        }
        for (size_t k = 0; k + 1 < col.size(); ++k)
            for (int r2 = 0; r2 < 2; ++r2) {
                if (front) B.quad(col[k][r2], col[k + 1][r2], col[k + 1][r2 + 1], col[k][r2 + 1], mat);
                else B.quad(col[k + 1][r2], col[k][r2], col[k][r2 + 1], col[k + 1][r2 + 1], mat);
            }
        for (size_t k = 0; k + 1 < col.size(); ++k) {                             // ust kenar kapagi (govdeye doner)
            const Vertex a = m.verts[col[k][2]], b = m.verts[col[k + 1][2]];
            const int a2 = B.v(a.x - sg * 0.04, a.y * 0.98, a.z), b2 = B.v(b.x - sg * 0.04, b.y * 0.98, b.z);
            if (front) B.quad(col[k][2], col[k + 1][2], b2, a2, mat); else B.quad(col[k + 1][2], col[k][2], a2, b2, mat);
        }
        if (modern) {                                                             // alt dudak / splitter (koyu)
            const double zl = z0 - 0.015;
            B.panel(xe + sg * 0.025, hwE * 0.88, zl, xe - sg * 0.05, hwE * 0.9, zl, xe - sg * 0.05, -hwE * 0.9, zl, xe + sg * 0.025, -hwE * 0.88, zl, MatDark);
            B.panel(xe + sg * 0.025, -hwE * 0.88, zl, xe - sg * 0.05, -hwE * 0.9, zl, xe - sg * 0.05, hwE * 0.9, zl, xe + sg * 0.025, hwE * 0.88, zl, MatDark);
        }
    };
    wrapBumper(true, bumperZ0, bumperZ1, bumperMat);
    if (modern) B.box(xf - 0.02, xf + 0.035, -hwF * 0.55, hwF * 0.55, bumperZ0 + 0.01, bumperZ0 + 0.09, MatDark);   // alt hava girisi
    B.box(xf + 0.03, xf + 0.04, -0.26, 0.26, bumperZ0 + 0.015, bumperZ0 + 0.125, MatPlate);
    const double lampZ = std::max(bumperZ1 + 0.06, lampZ0);
    const double lampY = hwF * 0.66;
    const FaceSpec* fs = faceSpecFor(v.id);
    // ---- arac basina yuz: parcalar on / arka yuzun egrisine oturur (bombe + kose donusu) ----
    // Gercek govde yuzeyi: istasyon kesitlerinde (y, z) noktasinin ilk icerde kaldigi yer (onden / arkadan tarama).
    // Yari kesitin z yuksekligindeki dis siniri (y); kesit o yuksekligi kapsamiyorsa -1
    auto extentAt = [&](size_t k, double z) {
        const auto& h = subs[k];
        double best = -1.0;
        for (int i = 0; i + 1 < RH2; ++i) {
            const double z0 = h[i][1], z1 = h[i + 1][1];
            if ((z - z0) * (z - z1) > 0.0) continue;
            const double t = std::fabs(z1 - z0) < 1e-9 ? 0.0 : (z - z0) / (z1 - z0);
            best = std::max(best, h[i][0] + (h[i + 1][0] - h[i][0]) * t);
        }
        return best;
    };
    auto faceX = [&](bool front, double y, double z) {
        const double sg = front ? 1.0 : -1.0, ay = std::fabs(y);
        const int n = (int)ss.size();
        double prevE = -1.0;
        for (int q = 0; q < n; ++q) {
            const size_t k = front ? q : n - 1 - q;
            const double e = extentAt(k, z);
            if (e >= ay) {
                const double xk = L * (0.5 - ss[k]);
                if (q == 0) {                                                    // uc kapak: bombeli seritler (cap ile ayni formul)
                    float czr = 0; for (int i : (front ? rings.front() : rings.back())) czr += m.verts[i].z; czr /= R;
                    const double bul = std::clamp(0.010 * L, 0.025, 0.05), w = 1.0 - std::min(1.0, ay / std::max(e, 1e-3));
                    const double hz = 1.0 - std::min(1.0, std::fabs(z - czr) / 0.6);
                    return xk + sg * (bul * hz * (0.55 + 0.45 * w) + 0.004);
                }
                const size_t kp = front ? k - 1 : k + 1;
                const double e0 = std::max(prevE, 0.0), f = std::clamp((ay - e0) / std::max(e - e0, 1e-4), 0.0, 1.0);
                const double xp = L * (0.5 - ss[kp]);
                return xp + (xk - xp) * f + sg * 0.004;
            }
            prevE = e;
        }
        return (front ? L * 0.5 : -L * 0.5);
    };
    using Poly = std::vector<std::pair<double, double>>;                          // (y, z)
    auto facePoly = [&](bool front, const Poly& pts, int mat, double off) {
        double cy = 0, cz = 0;
        for (const auto& p : pts) { cy += p.first; cz += p.second; }
        cy /= pts.size(); cz /= pts.size();
        const double sg = front ? 1.0 : -1.0;
        // Parca tek duzlemde: merkezdeki yuzey + egim (yan / dikey); hicbir kose govdeye gomulmesin diye disa kaydirilir
        const double d = 0.035, xc = faceX(front, cy, cz);
        const double gy = (faceX(front, cy + d, cz) - faceX(front, cy - d, cz)) / (2 * d);
        const double gz = (faceX(front, cy, cz + d) - faceX(front, cy, cz - d)) / (2 * d);
        const double gy2 = std::clamp(gy, -1.2, 1.2), gz2 = std::clamp(gz, -1.2, 1.2);   // asiri egimli burunda tasma olmasin
        auto plane = [&](double y, double z) {
            const double x = xc + gy2 * (y - cy) + gz2 * (z - cz);
            return std::clamp(x, -L * 0.5 - 0.04, L * 0.5 + 0.04);
        };
        double lift = 0.0;
        for (const auto& p : pts) lift = std::max(lift, sg * (faceX(front, p.first, p.second) - plane(p.first, p.second)));
        for (int k = 0; k < 12; ++k) {                                            // ic ice katmanlar ayni duzlemi paylassin
            const double a = 2 * kPi * k / 12, y = cy + 0.13 * std::cos(a), z = cz + 0.13 * std::sin(a);
            lift = std::max(lift, sg * (faceX(front, y, z) - plane(y, z)));
        }
        lift = std::min(lift, 0.03);
        const int c = B.v(plane(cy, cz) + sg * (off + lift), cy, cz);
        std::vector<int> id;
        for (const auto& p : pts) id.push_back(B.v(plane(p.first, p.second) + sg * (off + lift), p.first, p.second));
        for (size_t i = 0; i < id.size(); ++i) {
            const size_t j = (i + 1) % id.size();
            B.tri(c, id[i], id[j], mat); B.tri(c, id[j], id[i], mat);              // iki yuzlu (egri yuzeyde yon degisir)
        }
    };
    auto ellipse = [](double cy, double cz, double ry, double rz, int n) {
        Poly p; for (int k = 0; k < n; ++k) { const double a = 2 * kPi * k / n; p.push_back({cy + ry * std::cos(a), cz + rz * std::sin(a)}); } return p;
    };
    auto rrect = [](double cy, double cz, double w, double h, double r) {          // yuvarlak koseli dikdortgen
        Poly p; r = std::min(r, std::min(w, h) * 0.5);
        const double cs[4][2] = {{w / 2 - r, h / 2 - r}, {-w / 2 + r, h / 2 - r}, {-w / 2 + r, -h / 2 + r}, {w / 2 - r, -h / 2 + r}};
        for (int q = 0; q < 4; ++q)
            for (int k = 0; k <= 3; ++k) { const double a = kPi * 0.5 * (q + k / 3.0); p.push_back({cy + cs[q][0] + r * std::cos(a), cz + cs[q][1] + r * std::sin(a)}); }
        return p;
    };
    auto scaleP = [](const Poly& p, double s) {
        double cy = 0, cz = 0; for (const auto& q : p) { cy += q.first; cz += q.second; } cy /= p.size(); cz /= p.size();
        Poly o; for (const auto& q : p) o.push_back({cy + (q.first - cy) * s, cz + (q.second - cz) * s}); return o;
    };
    // Far sekli (yan: sg; dis kenar sg yonunde)
    auto lampShape = [&](FaceLamp k, double cy, double cz, double w, double h, double sg) {
        const double yi = cy - sg * w / 2, yo = cy + sg * w / 2;
        switch (k) {
        case FL_SWEPT: return Poly{{yi, cz - h * 0.30}, {yo, cz - h * 0.50}, {yo + sg * 0.01, cz + h * 0.10}, {yo - sg * w * 0.10, cz + h * 0.55}, {yi, cz + h * 0.30}};
        case FL_SQUINT: return Poly{{yi - sg * 0.02, cz - h * 0.45}, {yo, cz - h * 0.25}, {yo, cz + h * 0.50}, {yi + sg * w * 0.15, cz + h * 0.50}, {yi - sg * 0.02, cz + h * 0.05}};
        case FL_OVAL: return ellipse(cy, cz, w / 2, h / 2, 16);
        case FL_ROUND: return ellipse(cy, cz, w / 2, w / 2, 16);
        default: return rrect(cy, cz, w, h, std::min(w, h) * 0.18);
        }
    };
    if (fs) {
        const double zl = lampZ;
        // Izgara (farlardan once: tam genislik izgarada farlar ustunde durur)
        const double gzc = zl - 0.01;
        switch (fs->grille) {
        case FG_NONE: break;
        case FG_SLOT: facePoly(true, rrect(0, gzc, fs->gw, fs->gh, 0.02), MatDark, 0.004); break;
        case FG_WIDE: {
            const double gw = std::max(0.2, 2.0 * (fs->y * hwF - fs->w / 2) - 0.06);
            facePoly(true, rrect(0, gzc, gw, fs->gh, 0.02), MatDark, 0.004);
            for (int k = -1; k <= 1; ++k) facePoly(true, rrect(0, gzc + k * fs->gh * 0.28, gw - 0.02, 0.008, 0.002), bumperMat == MatChrome ? MatChrome : MatTrim, 0.008);
        } break;
        case FG_FULL: {
            const double gw = fs->w > 0 ? 2.0 * (fs->y * hwF + fs->w / 2 + 0.02) : 2.0 * hwF * 0.92;   // gizli farda tum yuz
            facePoly(true, rrect(0, gzc, std::min(gw, 2 * hwF * 0.94), fs->gh, 0.02), MatDark, 0.004);
            facePoly(true, rrect(0, gzc - fs->gh * 0.5 - 0.01, std::min(gw, 2 * hwF * 0.94), 0.012, 0.004), MatChrome, 0.008);
        } break;
        case FG_KIDNEY:
            for (double sg : {-1.0, 1.0}) {
                facePoly(true, rrect(sg * (0.04 + fs->gw / 4), gzc, fs->gw / 2 - 0.02, fs->gh, 0.035), MatChrome, 0.004);
                facePoly(true, rrect(sg * (0.04 + fs->gw / 4), gzc, fs->gw / 2 - 0.045, fs->gh - 0.025, 0.03), MatDark, 0.008);
            }
            break;
        case FG_MOUTH: {
            const double mz = 0.5 * (bumperZ0 + bumperZ1);
            facePoly(true, Poly{{-fs->gw / 2, mz + fs->gh / 2}, {fs->gw / 2, mz + fs->gh / 2}, {fs->gw / 2 * 0.92, mz - fs->gh / 2}, {-fs->gw / 2 * 0.92, mz - fs->gh / 2}}, MatDark, 0.03);
            facePoly(true, rrect(0, gzc, fs->gw * 0.42, 0.045, 0.015), MatDark, 0.004);   // ust ince giris
        } break;
        case FG_HEX: {
            const double top = gzc + 0.03, bot = std::max(bumperZ0 + 0.02, top - fs->gh);
            facePoly(true, Poly{{-fs->gw * 0.40, top}, {fs->gw * 0.40, top}, {fs->gw / 2, (top + bot) * 0.5}, {fs->gw * 0.42, bot}, {-fs->gw * 0.42, bot}, {-fs->gw / 2, (top + bot) * 0.5}}, MatDark, 0.03);
            for (int k = 1; k < 4; ++k) { const double z = bot + (top - bot) * k / 4.0; facePoly(true, rrect(0, z, fs->gw * 0.78, 0.006, 0.002), MatTrim, 0.034); }
        } break;
        }
        // Farlar
        for (double sg : {-1.0, 1.0}) {
            const double cy = sg * fs->y * hwF;
            switch (fs->lamp) {
            case FL_POP: if (fs->w > 0) {                                            // kapali acilir far: kaput ustunde ince ayrim
                const double zt = H * P.belt(0.05) + 0.006, x0 = L * 0.5 - 0.04 * L, x1 = L * 0.5 - 0.11 * L;
                B.panel(x0, cy + fs->w / 2, zt, x1, cy + fs->w / 2, zt + 0.012, x1, cy - fs->w / 2, zt + 0.012, x0, cy - fs->w / 2, zt, MatDark);
                facePoly(true, rrect(cy, bumperZ1 + 0.03, fs->w * 0.7, 0.035, 0.01), MatLight, 0.006);   // tampon ustu park lambasi
            } break;
            case FL_FENDER: {                                                         // camurluk ustunde dik yuvarlak far (911)
                const double sF2 = 0.06, xF2 = L * (0.5 - sF2) + 0.02, zF2 = H * P.belt(sF2) - 0.03, yF = sg * halfW(sF2) * 0.62;
                B.diskX(xF2, yF, zF2, fs->w / 2 + 0.015, fs->w / 2 + 0.015, 16, MatChrome);
                B.diskX(xF2 + 0.006, yF, zF2, fs->w / 2, fs->w / 2, 16, MatLight);
                B.diskX(xF2 + 0.009, yF, zF2, fs->w / 4, fs->w / 4, 12, MatChrome);
            } break;
            case FL_TWIN: {
                const double d = std::min(fs->w / 2, fs->h);
                facePoly(true, rrect(cy, zl, fs->w + 0.02, d + 0.02, 0.02), MatDark, 0.002);
                for (double o : {-1.0, 1.0}) {
                    facePoly(true, ellipse(cy + o * fs->w / 4, zl, d / 2, d / 2, 14), MatChrome, 0.006);
                    facePoly(true, ellipse(cy + o * fs->w / 4, zl, d / 3, d / 3, 12), MatLight, 0.010);
                }
            } break;
            default: {
                const Poly sh = lampShape(fs->lamp, cy, zl, fs->w, fs->h, sg);
                facePoly(true, scaleP(sh, 1.12), MatDark, 0.002);                      // cerceve / golge
                facePoly(true, sh, MatChrome, 0.006);                                  // yansitici
                if (fs->lamp == FL_ROUND) facePoly(true, ellipse(cy, zl, fs->w * 0.30, fs->w * 0.30, 14), MatLight, 0.010);
                else {
                    const double py = cy - sg * fs->w * 0.18;                          // ic tarafta projektor + dis tarafta ikinci
                    facePoly(true, ellipse(py, zl, std::min(fs->h, fs->w) * 0.30, std::min(fs->h, fs->w) * 0.30, 12), MatLight, 0.010);
                    facePoly(true, ellipse(cy + sg * fs->w * 0.20, zl, std::min(fs->h, fs->w) * 0.24, std::min(fs->h, fs->w) * 0.24, 12), MatLight, 0.010);
                    if (modern) facePoly(true, rrect(cy, zl - fs->h * 0.38, fs->w * 0.7, 0.012, 0.004), MatLight, 0.010);   // gunduz seridi
                }
            } break;
            }
            facePoly(true, rrect(sg * hwF * 0.86, bumperZ0 + 0.045, 0.07, 0.03, 0.008), MatIndicator, 0.03);   // sinyal
        }
    }
    // Izgara
    if (!fs) {
        const double gz0 = bumperZ1 + 0.015, gz1 = std::max(gz0 + 0.04, zNose - 0.03);
        switch (sh.grille) {
        case G_0: break;
        case G_1: B.box(xf - 0.01, xf + 0.012, -hwF * 0.32, hwF * 0.32, lampZ - 0.025, lampZ + 0.02, MatDark); break;
        case G_2: B.box(xf - 0.01, xf + 0.012, -lampY + 0.12, lampY - 0.12, std::max(gz0, lampZ - 0.085), std::min(gz1, lampZ + 0.055), MatDark);   // far hizasinda
                  B.box(xf - 0.008, xf + 0.016, -lampY + 0.12, lampY - 0.12, lampZ - 0.016, lampZ - 0.008, MatChrome); break;
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
    if (!fs && (v.streetLegal || sh.lamp == L_O || sh.lamp == L_Q)) {
        for (double sg : {-1.0, 1.0}) {
            const double y = sg * lampY;
            switch (sh.lamp) {
            case L_R: {                                                                   // dikdortgen far: cerceve + krom yansitici + iki projektor
                B.box(xf - 0.03, xf + 0.010, y - 0.16, y + 0.16, lampZ - 0.058, lampZ + 0.048, MatDark);
                B.box(xf - 0.03, xf + 0.014, y - 0.148, y + 0.148, lampZ - 0.046, lampZ + 0.036, MatChrome);
                for (double o : {-0.07, 0.07}) {
                    B.diskX(xf + 0.017, y + o, lampZ - 0.005, 0.036, 0.036, 12, MatDark);
                    B.diskX(xf + 0.019, y + o, lampZ - 0.005, 0.027, 0.027, 12, MatLight);
                }
                if (v.year >= 2005) B.box(xf - 0.01, xf + 0.018, y - 0.14, y + 0.14, lampZ + 0.026, lampZ + 0.034, MatLight);   // gunduz fari seridi
            } break;
            case L_O: B.diskX(xf + 0.014, y, lampZ, 0.085, 0.085, 12, MatLight);
                      B.diskX(xf + 0.010, y, lampZ, 0.10, 0.10, 12, MatChrome); break;
            case L_Q: for (double o : {-0.075, 0.075}) { B.diskX(xf + 0.014, y + sg * o, lampZ, 0.065, 0.065, 10, MatLight);
                                                         B.diskX(xf + 0.010, y + sg * o, lampZ, 0.078, 0.078, 10, MatDark); } break;
            case L_P: { const double zt = H * P.belt(0.04) + 0.004;                       // kapali acilir far: kaput ustu cizgi
                        const double x0 = xf - 0.05 * L, x1 = xf - 0.10 * L;
                        B.panel(x0, y + 0.16, zt, x1, y + 0.16, zt + 0.01, x1, y - 0.16, zt + 0.01, x0, y - 0.16, zt, MatTrim);
                        B.box(xf - 0.02, xf + 0.01, y - 0.12, y + 0.12, bumperZ1 + 0.01, bumperZ1 + 0.04, MatLight); } break;
            case L_S: {                                                                   // yatik damla far: koyu cerceve + krom ic + projektor + DRL
                auto lens = [&](double dx, double grow, int mat) {
                    B.panel(xf + dx, y - sg * (0.02 + grow), lampZ + 0.025 + grow, xf + dx, y + sg * (0.20 + grow), lampZ + 0.04 + grow,
                            xf - 0.02 + dx, y + sg * (0.22 + grow), lampZ - 0.01 - grow, xf + dx, y - sg * (0.04 + grow), lampZ - 0.03 - grow, mat);
                    B.panel(xf + dx, y - sg * (0.04 + grow), lampZ - 0.03 - grow, xf - 0.02 + dx, y + sg * (0.22 + grow), lampZ - 0.01 - grow,
                            xf + dx, y + sg * (0.20 + grow), lampZ + 0.04 + grow, xf + dx, y - sg * (0.02 + grow), lampZ + 0.025 + grow, mat);
                };
                lens(0.010, 0.012, MatDark); lens(0.012, 0.0, MatChrome);
                B.diskX(xf + 0.016, y + sg * 0.04, lampZ + 0.002, 0.030, 0.030, 12, MatDark);
                B.diskX(xf + 0.018, y + sg * 0.04, lampZ + 0.002, 0.022, 0.022, 12, MatLight);
                B.panel(xf + 0.015, y + sg * 0.08, lampZ - 0.016, xf + 0.015, y + sg * 0.20, lampZ - 0.006,
                        xf + 0.015, y + sg * 0.20, lampZ + 0.002, xf + 0.015, y + sg * 0.08, lampZ - 0.008, MatLight);   // LED seridi
                B.panel(xf + 0.015, y + sg * 0.08, lampZ - 0.008, xf + 0.015, y + sg * 0.20, lampZ + 0.002,
                        xf + 0.015, y + sg * 0.20, lampZ - 0.006, xf + 0.015, y + sg * 0.08, lampZ - 0.016, MatLight);
            } break;
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
    wrapBumper(false, bumperZ0, bumperZ1, bumperMat);
    if (modern) B.box(xr - 0.035, xr + 0.02, -hwR * 0.6, hwR * 0.6, bumperZ0 - 0.01, bumperZ0 + 0.05, MatDark);   // difuzor
    B.box(xr - 0.04, xr - 0.03, -0.26, 0.26, bumperZ1 + 0.02, bumperZ1 + 0.13, MatPlate);
    if (fs) {                                                                     // arac basina stoplar (kuyruk egrisine oturur)
        const double zt = std::max(bumperZ1 + 0.10, zTail - 0.10);
        auto tailLamp = [&](const Poly& p) {
            facePoly(false, scaleP(p, 1.10), MatDark, 0.002);
            facePoly(false, p, MatTail, 0.006);
        };
        switch (fs->tail) {
        case FT_BAR: {                                                            // tam genislik serit (ortasi koyu)
            tailLamp(rrect(0, zt, 2 * hwR * 0.88, fs->th, 0.02));
            facePoly(false, rrect(0, zt, 0.40, fs->th * 0.6, 0.01), MatDark, 0.010);
        } break;
        case FT_TWINR:
            for (double sg : {-1.0, 1.0}) {
                const double c0 = sg * fs->ty * hwR;
                for (double o : {-0.55, 0.55}) {
                    tailLamp(ellipse(c0 + o * fs->tw, zt, fs->tw / 2, fs->th / 2, 16));
                    facePoly(false, ellipse(c0 + o * fs->tw, zt, fs->tw / 5, fs->th / 5, 10), MatDark, 0.010);
                }
            }
            break;
        default:
            for (double sg : {-1.0, 1.0}) {
                const double cy = sg * fs->ty * hwR;
                switch (fs->tail) {
                case FT_ROUND: tailLamp(ellipse(cy, zt, fs->tw / 2, fs->th / 2, 16)); break;
                case FT_VERT:
                    if (fs->tw < 0.15) tailLamp(rrect(cy, zt + 0.02, fs->tw, fs->th, 0.015));
                    else for (int k = -1; k <= 1; ++k) tailLamp(rrect(cy + k * fs->tw / 3, zt, fs->tw / 3 - 0.02, fs->th, 0.01));   // uclu dikey
                    break;
                case FT_SLIM: tailLamp(Poly{{cy - sg * fs->tw / 2, zt}, {cy + sg * fs->tw / 2, zt + fs->th * 0.3}, {cy + sg * fs->tw / 2, zt + fs->th}, {cy - sg * fs->tw / 2, zt + fs->th * 0.5}}); break;
                default: {
                    tailLamp(rrect(cy, zt, fs->tw, fs->th, 0.02));
                    facePoly(false, rrect(cy - sg * fs->tw * 0.30, zt, fs->tw * 0.22, fs->th * 0.5, 0.01), MatIndicator, 0.010);
                    facePoly(false, rrect(cy + sg * fs->tw * 0.05, zt - fs->th * 0.25, fs->tw * 0.25, fs->th * 0.25, 0.005), MatLight, 0.010);   // geri vites
                } break;
                }
            }
            break;
        }
    } else {
        const double zt = std::max(bumperZ1 + 0.09, zTail - 0.09);
        const double ty = hwR * 0.70;
        for (double sg : {-1.0, 1.0}) {
            const double y = sg * ty;
            switch (sh.tail) {
            case T_R: B.box(xr - 0.010, xr + 0.01, y - 0.16, y + 0.16, zt - 0.06, zt + 0.06, MatDark);          // cerceve + iki tonlu stop
                      B.box(xr - 0.014, xr + 0.01, y - 0.148, y + 0.148, zt - 0.048, zt + 0.048, MatTail);
                      B.box(xr - 0.017, xr + 0.01, y - sg * 0.13, y - sg * 0.07, zt - 0.03, zt + 0.03, MatIndicator); break;
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

    // ---- esik (marspiyel) seridi: tekerler arasi alt kenar koyu; klasikte bel hizasinda krom serit (yan duvar tek parca durmasin) ----
    {
        const double s0 = sFront + 0.07, s1 = sRear - 0.07;
        const int ns = 10;
        for (double sg : {1.0, -1.0})
            for (int k = 0; k < ns; ++k) {
                const double a = s0 + (s1 - s0) * k / ns, b = s0 + (s1 - s0) * (k + 1) / ns;
                const double ya = sg * (halfW(a) * 0.985 + 0.006), yb = sg * (halfW(b) * 0.985 + 0.006);
                const double z0 = ride + 0.05, z1 = ride + 0.12;
                B.panel(L * (0.5 - a), ya, z0, L * (0.5 - b), yb, z0, L * (0.5 - b), yb, z1, L * (0.5 - a), ya, z1, MatTrim);
                if (classic && !A.open) {                                         // krom bel seridi
                    const double za = H * P.belt(a) - 0.05, zb = H * P.belt(b) - 0.05;
                    const double ya2 = sg * (halfW(a) * 0.99 + 0.008), yb2 = sg * (halfW(b) * 0.99 + 0.008);
                    B.panel(L * (0.5 - a), ya2, za, L * (0.5 - b), yb2, zb, L * (0.5 - b), yb2, zb + 0.014, L * (0.5 - a), ya2, za + 0.014, MatChrome);
                }
            }
    }
    // ---- panel bosluklari: kaput ve bagaj kapagi cevresinde ince koyu cizgi (tek parca oyuncak hissi olmasin) ----
    {
        auto seamAlong = [&](double s0, double s1, double yf, int n) {           // s0..s1 boyunca y = yf * halfW(s) cizgisi
            for (int k = 0; k < n; ++k) {
                const double a = s0 + (s1 - s0) * k / n, b = s0 + (s1 - s0) * (k + 1) / n;
                const double ya = yf * halfW(a), yb = yf * halfW(b), za = topZ(a) + 0.006, zb = topZ(b) + 0.006;
                B.panel(L * (0.5 - a), ya - 0.005, za, L * (0.5 - b), yb - 0.005, zb, L * (0.5 - b), yb + 0.005, zb, L * (0.5 - a), ya + 0.005, za, MatDark);
            }
        };
        auto seamAcross = [&](double s, double yf) {                             // s'de enine cizgi (-yf..yf)
            const int n = 6; const double x = L * (0.5 - s);
            for (int k = 0; k < n; ++k) {
                const double ya = -yf * halfW(s) + 2 * yf * halfW(s) * k / n, yb = -yf * halfW(s) + 2 * yf * halfW(s) * (k + 1) / n;
                const double z = topZ(s) + 0.007;
                B.panel(x + 0.005, ya, z, x + 0.005, yb, z, x - 0.005, yb, z, x - 0.005, ya, z, MatDark);
            }
        };
        if (!rearEngine && A.cowlX > 0.15) {                                     // on motor: kaput kenarlari + on cam dibi
            for (double sg : {-1.0, 1.0}) seamAlong(0.02, A.cowlX - 0.02, sg * 0.80, 8);
            seamAcross(A.cowlX - 0.02, 0.80);
        }
        if (A.backX < 0.93 && !A.bed) {                                          // bagaj kapagi (sedan / kupe)
            for (double sg : {-1.0, 1.0}) seamAlong(A.backX + 0.015, 0.975, sg * 0.82, 4);
            seamAcross(A.backX + 0.015, 0.82);
        }
    }
    // ---- aynalar ----
    {
        const double sm = std::min(A.cowlX + 0.03, A.roofFX), xm = L * (0.5 - sm);
        const double zm = H * P.belt(sm) + 0.07, ym = halfW(sm) * A.beltW;
        // Ayna: ayak (kapiya baglanir) + one dogru incelen yuvarlak govde + arka yuzde cam. Klasik krom, 80-90 siyah, modern boya
        const int hm = v.year < 1978 ? MatChrome : v.year < 1995 ? MatTrim : MatPaint;
        for (double sg : {-1.0, 1.0}) {
            const double y0 = sg * (ym + 0.015), y1 = sg * (ym + 0.17), xb = xm - 0.05, xfm = xm + 0.06;
            B.box(xm - 0.03, xm + 0.03, std::min(sg * (ym - 0.01), y0), std::max(sg * (ym - 0.01), y0), zm - 0.06, zm - 0.035, MatTrim);   // ayak
            const int NA = 10;                                                    // elips kesitli govde: arka buyuk, on kucuk
            std::vector<int> back(NA), frontR(NA);
            const double yc = 0.5 * (y0 + y1), ry = std::fabs(y1 - y0) * 0.5, rz = 0.05;
            for (int k = 0; k < NA; ++k) {
                const double a = 2 * kPi * k / NA, cy2 = std::cos(a), sz = std::sin(a);
                back[k] = B.v(xb, yc + ry * cy2, zm + rz * sz);
                frontR[k] = B.v(xfm, yc + ry * 0.55 * cy2 + sg * ry * 0.25, zm + rz * 0.55 * sz - 0.005);
            }
            const int tip = B.v(xfm + 0.02, yc + sg * ry * 0.3, zm - 0.005);
            for (int k = 0; k < NA; ++k) {
                const int j = (k + 1) % NA;
                B.quad(back[k], frontR[k], frontR[j], back[j], hm);
                B.tri(tip, frontR[j], frontR[k], hm);
            }
            const int gc = B.v(xb - 0.004, yc, zm);                               // arka yuz: cam (ince cerceveli)
            for (int k = 0; k < NA; ++k) {
                const int j = (k + 1) % NA;
                const double a0 = 2 * kPi * k / NA, a1 = 2 * kPi * j / NA;
                const int g0 = B.v(xb - 0.004, yc + ry * 0.86 * std::cos(a0), zm + rz * 0.82 * std::sin(a0));
                const int g1 = B.v(xb - 0.004, yc + ry * 0.86 * std::cos(a1), zm + rz * 0.82 * std::sin(a1));
                B.tri(gc, g0, g1, MatGlass); B.tri(gc, g1, g0, MatGlass);
                B.quad(back[k], back[j], g1, g0, hm);
            }
        }
    }

    // ---- aero ----
    // Kanat: fabrika kanadi (profil tablosu) yol araclarinda; yaris araci / genel super icin yuksek kanat.
    // VehicleDef.wing super araclarda varsayilan acik; profilde kanat tipi tanimliysa gercek araca uyulur.
    int wingType = v.wing ? (v.body == Body::Super && v.streetLegal && !v.widebody ? K1 : K3) : K0;   // yol super: entegre dudak
    if (cp && v.streetLegal && !v.widebody) wingType = std::max(cp->wing, v.wing && v.body != Body::Super ? (int)K3 : (int)K0);
    {
        const double sw = std::max(A.backX, 0.80);                                   // kanat bolgesi: bagaj / kuyruk
        const double xw = -L * 0.44, zDeck = topZ(std::min(std::max(0.94, sw), 0.97));
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
