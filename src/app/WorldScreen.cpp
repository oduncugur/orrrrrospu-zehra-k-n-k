// ZEHRA KINIK - Acik dunya ekrani (zehra_world surumu): butun sehirlerden gecen otoban, sehir izgaralari (kavsakta
// donus), binalar, trafik (polis ~%3, serseri ~%1), benzinlikler (sehre gore litre fiyati), bulusma meydanlari
// (modifiyeli araclar), yaris baslangiclari, hurdalik. Harita: dokunarak waypoint, rota; otonom surus rotayi izler.
#include "Screens.h"
#include "Gauges.h"
#include "Ui.h"
#include "app/Looks.h"
#include "game/League.h"
#include "game/RunField.h"
#include "game/World.h"
#include "garage/VehicleCatalog.h"
#include "sim/VehicleSim.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace zk {

namespace {
// Proj: ekran konumu + kirpma uzayi (cx, cy, cw): kameranin arkasina tasan yuzeyler atlanmaz, yakin duzlemde kirpilir
struct Proj { float x, y, w; bool ok; float cx = 0, cy = 0, cw = 0; };
constexpr float kNearW = 0.4f;
int gVw = 640, gVh = 360;
Proj fromClip(float cx, float cy, float cw) {
    return {(cx / cw * 0.5f + 0.5f) * gVw, (1.0f - (cy / cw * 0.5f + 0.5f)) * gVh, cw, true, cx, cy, cw};
}
Proj project(const Mat4& vp, double X, double Y, double h, int vw, int vh) {
    gVw = vw; gVh = vh;
    const float x = (float)X, y = (float)h, z = (float)-Y;
    const float cx = vp.m[0] * x + vp.m[4] * y + vp.m[8] * z + vp.m[12];
    const float cy = vp.m[1] * x + vp.m[5] * y + vp.m[9] * z + vp.m[13];
    const float cw = vp.m[3] * x + vp.m[7] * y + vp.m[11] * z + vp.m[15];
    if (cw < kNearW) return {0, 0, cw, false, cx, cy, cw};
    return fromClip(cx, cy, cw);
}
void triP(Renderer& r, const Proj& a, const Proj& b, const Proj& c, Color col) { r.triZ(a.x, a.y, a.w, b.x, b.y, b.w, c.x, c.y, c.w, col); }
// Dortgen: tamami gorunurse dogrudan; bir kismi kameranin arkasindaysa yakin duzlemde kirpilir (Sutherland-Hodgman),
// boylece arac yanindaki / altindaki yol, kaldirim ve zemin parcalari asla kaybolmaz
void quadP(Renderer& r, const Proj& a, const Proj& b, const Proj& c, const Proj& d, Color col) {
    if (a.ok && b.ok && c.ok && d.ok) { triP(r, a, b, c, col); triP(r, a, c, d, col); return; }
    if (!a.ok && !b.ok && !c.ok && !d.ok) return;
    const Proj* in[4] = {&a, &b, &c, &d};
    Proj out[8]; int n = 0;
    for (int i = 0; i < 4; ++i) {
        const Proj& p = *in[i]; const Proj& q = *in[(i + 1) % 4];
        if (p.ok) out[n++] = p;
        if (p.ok != q.ok) {
            const float t = (kNearW + 0.001f - p.cw) / (q.cw - p.cw);
            out[n++] = fromClip(p.cx + (q.cx - p.cx) * t, p.cy + (q.cy - p.cy) * t, kNearW + 0.001f);
        }
    }
    for (int i = 1; i + 1 < n; ++i) triP(r, out[0], out[i], out[i + 1], col);
}
struct Parked { double x, y, h; int carId; };
// Site duvari: binalarin cogu 5 m bahceli, 1.8 m duvarla cevrili; sokak yonlerinde (v kenarlari) 6 m kapi boslugu.
// Parca: merkez, yon (heading), yari boy, yari kalinlik. Cizim ve carpisma ayni listeyi kullanir.
struct WallSeg { double x, y, h, hl, ht; };
float hashW(int k);
// Trafik isigi: sehir ici 3+ kollu kavsak; dik yonler iki grup, 24 s dongu (10 yesil, 2 sari, 12 kirmizi)
int lightState(const World& w, int ni, int ei, double T, double* dirX = nullptr, double* dirY = nullptr) {
    const WorldNode& nd = w.nodes[ni];
    if (nd.edges.size() < 3) return -1;
    auto dirOf = [&](int e, double& dx, double& dy) {
        const WorldEdge& E = w.edges[e];
        if (E.highway || E.city < 0 || E.pts.size() < 2) return false;
        const auto& q = E.a == ni ? E.pts[1] : E.pts[E.pts.size() - 2];
        dx = q.first - nd.x; dy = q.second - nd.y; const double l = std::hypot(dx, dy);
        if (l < 1.0) return false;
        dx /= l; dy /= l; return true;
    };
    double dx, dy, d0x = 0, d0y = 0; bool any = false;
    for (int e : nd.edges) if (dirOf(e, d0x, d0y)) { any = true; break; }
    if (!any || !dirOf(ei, dx, dy)) return -1;
    if (dirX) { *dirX = dx; *dirY = dy; }
    const int grp = std::fabs(dx * d0y - dy * d0x) > 0.7 ? 1 : 0;
    const double t = std::fmod(T + 24.0 * hashW(ni * 7) + (grp ? 12.0 : 0.0), 24.0);
    return t < 10.0 ? 0 : t < 12.0 ? 1 : 2;
}
// Site bahcesi (yerel v araligi): yol tarafinda bordura ~2.8 m kalana kadar genis on bahce, arkada 4 m
void siteBox(const WorldBuilding& b, double& U, double& v0, double& v1) {
    const double front = std::clamp(b.gap - 2.8, 3.0, 40.0), back = 4.0;
    U = b.hu + 3.5;
    v0 = b.roadSide > 0 ? -(b.hv + back) : -(b.hv + front);
    v1 = b.roadSide > 0 ? b.hv + front : b.hv + back;
}
int siteWalls(const WorldBuilding& b, int idx, WallSeg out[6]) {
    if (hashW(idx * 23 + 5) < 0.2f) return 0;                           // %20 duvarsiz (acik bina onu)
    double U, v0, v1; siteBox(b, U, v0, v1);
    const double vx = -b.uy, vy = b.ux, hd = std::atan2(b.uy, b.ux), vm = 0.5 * (v0 + v1), vh = 0.5 * (v1 - v0);
    int n = 0;
    for (double sg : {1.0, -1.0}) out[n++] = {b.cx + sg * U * b.ux + vm * vx, b.cy + sg * U * b.uy + vm * vy, hd + 1.5707963, vh, 0.15};   // yan duvarlar
    const double vRoad = b.roadSide > 0 ? v1 : v0, vBack = b.roadSide > 0 ? v0 : v1;
    out[n++] = {b.cx + vBack * vx, b.cy + vBack * vy, hd, U, 0.15};                                 // arka duvar (tam)
    const double gate = 3.0, hl = (U - gate) * 0.5, off = (U + gate) * 0.5;                        // on duvar: ortada 6 m kapi
    if (hl > 0.5) for (double su : {1.0, -1.0}) out[n++] = {b.cx + su * off * b.ux + vRoad * vx, b.cy + su * off * b.uy + vRoad * vy, hd, hl, 0.15};
    return n;
}
// Park etmis araclar: sokaklarin iki yaninda (13 m aralik, %65 dolu); her karede ayni (belirlenimci)
void parkedOn(const World& w, int e, std::vector<Parked>& out) {
    const WorldEdge& E = w.edges[e];
    if (E.highway || E.bridge || E.city < 0) return;
    static std::vector<int> legal;
    if (legal.empty()) for (const VehicleDef& d : vehicleCatalog()) if (d.streetLegal) legal.push_back(d.id);
    const RoadPath& p = E.fwd();
    for (int side = 0; side < 2; ++side)
        for (int k = 0; 18.0 + k * 13.0 < p.length() - 18.0; ++k) {
            unsigned hsh = (unsigned)(e * 7919 + k * 31 + side * 17) * 2654435761u; hsh ^= hsh >> 15;
            if ((hsh & 0xFF) < 90) continue;                                 // %65 dolu (Turkiye: yarisi kaldirimda)
            const double s = 18.0 + k * 13.0;
            const RoadPoint q = p.at(s);
            const double lat = side ? (q.hw + 1.1) : -(q.hw + 1.1);
            out.push_back({q.x - lat * std::sin(q.heading), q.y + lat * std::cos(q.heading), q.heading + (side ? 3.14159265 : 0.0), legal[(hsh >> 8) % legal.size()]});
        }
}
float hashW(int i) { unsigned x = (unsigned)i * 2654435761u; x ^= x >> 13; x *= 0x5bd1e995u; x ^= x >> 15; return (x & 0xFFFF) / 65535.0f; }
constexpr double kTau = 6.283185307179586;
constexpr double kWalk = 1.6;   // kaldirim genisligi (dar: Turkiye usulu)
// Yayalar: sehir sokaklarinin dar kaldiriminda (yol basina 8), zamana gore yuruyen; arac yaklasinca duvara siginir.
// key: kalici kimlik (ezilen yaya bir sure yerde yatar, o sure yurumez)
struct Ped { double x, y, h; float col; int key; };
void pedsNear(const World& w, double T, double cx, double cy, double rad, double X, double Y, std::vector<Ped>& out) {
    for (int e = 0; e < (int)w.edges.size(); ++e) {
        const WorldEdge& E = w.edges[e];
        if (E.highway || E.bridge || E.city < 0) continue;
        if (cx < E.minX - rad || cx > E.maxX + rad || cy < E.minY - rad || cy > E.maxY + rad) continue;
        const RoadPath& p = E.fwd();
        for (int k = 0; k < 8; ++k) {
            const float hh = hashW(e * 13 + k * 7);
            const double dir = (k & 1) ? 1.0 : -1.0, L = p.length();
            const double s = std::fmod(hh * L + dir * T * (1.0 + 0.6 * hashW(e + k)) + 100.0 * L, L);
            const RoadPoint q = p.at(s);
            double lat = (k & 1 ? 1.0 : -1.0) * (q.hw + 0.8);
            const double qx = q.x - lat * std::sin(q.heading), qy = q.y + lat * std::cos(q.heading);
            if (std::hypot(qx - X, qy - Y) < 5.0) lat += (lat > 0 ? 0.6 : -0.6);   // arac yaklasinca duvara yapisir (yer dar)
            const double fx = q.x - lat * std::sin(q.heading), fy = q.y + lat * std::cos(q.heading);
            if (std::hypot(fx - cx, fy - cy) > rad) continue;
            out.push_back({fx, fy, q.heading + (dir > 0 ? 0.0 : 3.14159265), hashW(e * 5 + k * 3), e * 8 + k});
        }
    }
}
} // namespace

struct WorldScreen::Impl {
    App& app;
    const World& w = World::get();
    int carId; Tune tune;
    int W = 640, H = 360; bool land = true;
    double gaugeT = 0.0;
    WorldLeg leg{0, false};
    std::unique_ptr<RoadCar> car;
    Cockpit cockpit;
    double steer = 0, tiltF = 0, camPsi = 0, envT = 0, spin = 0, shakeT = 0;
    bool kL = false, kR = false;
    std::string msg; double msgT = 0;
    // Ezilen yayalar (yerde yatar, 25 s), alkis gosterisi, bordur (kaldirima cikinca 15 cm)
    struct Down { double x, y, h, t; float col; int key; };
    std::vector<Down> downs; double clapT = -1.0, curbH = 0.0; bool onCurb = false; int runOver = 0;
    bool isDown(int key) const { for (const Down& d : downs) if (d.key == key) return true; return false; }
    int city = -2;
    // Trafik
    // bx/by/bh/bl: kavsak donusu (onceki yolun sonundaki poz -> yeni yolda bl metreye Bezier egrisi; isinlanma yok)
    struct T { WorldLeg leg; double s, v, v0; int lane, carId, role, uid; double prevRel; double bx = 0, by = 0, bh = 0, bl = 0; };
    std::vector<T> traffic; int nextUid = 1; uint32_t rng = 0x9E3779B9u;
    void lanePose(const T& t, double s, double& x, double& y, double& h) const {
        const RoadPath& tp = w.path(t.leg);
        const RoadPoint q = tp.at(s);
        const double lo = tp.laneOffset(s, false, t.lane);
        x = q.x - lo * std::sin(q.heading); y = q.y + lo * std::cos(q.heading); h = q.heading;
    }
    void tPose(const T& t, double& x, double& y, double& h) const {
        if (t.bl <= 0.0 || t.s >= t.bl) { lanePose(t, t.s, x, y, h); return; }
        double x2, y2, h2; lanePose(t, t.bl, x2, y2, h2);
        const double k = 0.45 * std::hypot(x2 - t.bx, y2 - t.by);       // kontrol noktalari: giris / cikis yonunde
        const double c1x = t.bx + k * std::cos(t.bh), c1y = t.by + k * std::sin(t.bh);
        const double c2x = x2 - k * std::cos(h2), c2y = y2 - k * std::sin(h2);
        const double u = std::max(0.0, t.s) / t.bl, m = 1.0 - u;
        x = m * m * m * t.bx + 3 * m * m * u * c1x + 3 * m * u * u * c2x + u * u * u * x2;
        y = m * m * m * t.by + 3 * m * m * u * c1y + 3 * m * u * u * c2y + u * u * u * y2;
        const double dx = 3 * m * m * (c1x - t.bx) + 6 * m * u * (c2x - c1x) + 3 * u * u * (x2 - c2x);
        const double dy = 3 * m * m * (c1y - t.by) + 6 * m * u * (c2y - c1y) + 3 * u * u * (y2 - c2y);
        h = (std::fabs(dx) + std::fabs(dy) > 1e-6) ? std::atan2(dy, dx) : h2;
    }
    double rnd() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (rng & 0xFFFFFF) / double(0x1000000); }
    // Harita / rota / otonom
    bool mapOpen = false; double mapCx = 0, mapCy = 0, mapScale = 30.0;   // m / piksel
    bool hasWp = false; double wpX = 0, wpY = 0;
    std::vector<WorldLeg> route; size_t routeIdx = 0;
    bool autoDrive = false; double ccI = 0, stuckT = 0;
    int adasLevel = 0;
    float dragX = -1, dragY = -1; int dragId = -1;
    Rect mapBtn, autoBtn, poiBtn{0, 0, 0, 0}, mPlus, mMinus, mClose, mClear;
    int lastGear = -2;
    // Kesicide 5 s: yanindaki modifiyeli araca kapisma teklifi
    double limT = 0;
    // Hiz kameralari / radar / gecis rekoru
    std::vector<double> camCool; int legFrom = -1; double legStart = 0; double radarBlink = 0;
    // Isler (kurye / taksi)
    struct Job { int type; double tx, ty; double reward; double deadline; int phase; double dx, dy; std::string name; };
    bool jobsOpen = false; std::vector<Job> offers; bool hasJob = false; Job job{};
    Rect jobBtn, jobRows[3], jobClose, buyBtn{0, 0, 0, 0};
    void makeOffers();
    void startJobTarget(double x, double y);

    Impl(App& a) : app(a) {}
    void flash(const std::string& m, double t = 1.8) { msg = m; msgT = t; }
    const RoadPath& path() const { return w.path(leg); }
    double speedLimit() const { return city >= 0 ? 50.0 / 3.6 : 110.0 / 3.6; }
    double fuelPrice() const { return w.fuelPriceAt(city >= 0 ? city : app.career.city, app.career.marketWeek() < 0 ? 0 : app.career.marketWeek()); }
    int poiHere() const {
        if (car->sim().speed() > 2.0) return -1;
        for (size_t k = 0; k < w.pois.size(); ++k)
            if (std::hypot(w.pois[k].x - car->sim().posX(), w.pois[k].y - car->sim().posY()) < (w.pois[k].type == WPoiRace ? 14.0 : 34.0)) return (int)k;
        return -1;
    }
    void pickLeg(bool force) {                                            // kavsak / serit disi: en uygun yola gec
        const RoadPath& p = path();
        const double s = car->s(), lat = car->lateral();
        const double hd = std::remainder(car->sim().heading() - p.at(s).heading, kTau);
        if (!force && std::fabs(lat) < p.halfWidthAt(s) + 1.0 && s > 4.0 && s < p.length() - 4.0 && std::cos(hd) > 0.6) return;
        WorldLeg nl; double ns, nla;
        if (w.nearestLeg(car->sim().posX(), car->sim().posY(), car->sim().heading(), nl, ns, nla, 30.0) &&
            (nl.edge != leg.edge || nl.rev != leg.rev)) { leg = nl; car->setRoad(path()); }
    }
};

void WorldScreen::Impl::startJobTarget(double x, double y) {
    WorldLeg l; double s, lat;
    if (w.nearestLeg(x, y, 0.0, l, s, lat, 300.0)) { const RoadPoint q = w.path(l).at(s); wpX = q.x; wpY = q.y; }
    else { wpX = x; wpY = y; }
    hasWp = true; route = w.route(leg, car->s(), wpX, wpY); routeIdx = 0;
}

// Is teklifleri: kurye (paketi hedefe) / taksi (yolcuyu al, birak); odul mesafe ve sureye gore
void WorldScreen::Impl::makeOffers() {
    offers.clear();
    const double px = car->sim().posX(), py = car->sim().posY();
    for (int k = 0; k < 3; ++k) {
        const int type = k == 2 ? 1 : 0;
        // hedef: rastgele sokak noktasi (kurye: sehirde ya da komsu sehirde; taksi: yakinda al, sehirde birak)
        auto pick = [&](double minD, double maxD) {
            for (int g = 0; g < 200; ++g) {
                const int e = (int)(rnd() * w.edges.size());
                const WorldEdge& E = w.edges[e];
                if (E.highway || E.city < 0) continue;
                const RoadPoint q = E.fwd().at(E.fwd().length() * 0.5);
                const double d = std::hypot(q.x - px, q.y - py);
                if (d >= minD && d <= maxD) return std::make_pair(q.x, q.y);
            }
            return std::make_pair(px + 800.0, py);
        };
        Job j{};
        j.type = type;
        const auto t = pick(type ? 1200.0 : 1500.0, k == 1 ? 14000.0 : 4500.0);
        j.tx = t.first; j.ty = t.second;
        if (type == 1) { const auto pu = pick(200.0, 900.0); j.dx = j.tx; j.dy = j.ty; j.tx = pu.first; j.ty = pu.second; j.phase = 0; }
        else j.phase = 1;
        const double dist = std::hypot(j.tx - px, j.ty - py) + (type ? std::hypot(j.dx - j.tx, j.dy - j.ty) : 0.0);
        j.deadline = 40.0 + dist / 11.0;
        j.reward = std::round((150.0 + dist * 0.35) / 10.0) * 10.0;
        const int c = w.cityAt(type ? j.dx : j.tx, type ? j.dy : j.ty);
        j.name = std::string(type ? "TAKSI: YOLCUYU " : "KURYE: PAKETI ") + (c >= 0 ? w.cities[c].name : std::string("OTOBAN")) + (type ? "'A GOTUR" : "'A TESLIM ET");
        offers.push_back(j);
    }
}

WorldScreen::WorldScreen(App& app) : m_(std::make_unique<Impl>(app)) {
    Impl& M = *m_;
    land_ = M.land = !app.settings.roadPortrait;
    M.W = land_ ? 640 : 360; M.H = land_ ? 360 : 640;
    const OwnedCar& oc = app.career.car();
    M.carId = oc.carId; M.tune = oc.tune;
    app.setVoiceTuned(0, M.carId, &M.tune);
    app.setVoice(1, nullptr);
    // Baslangic: donus noktasi ya da bulunulan sehrin bati girisi (otoban caddesi, doguya)
    double x, y, h;
    if (const char* lm = std::getenv("ZK_WORLD_LM")) {                   // test: n. simge yapiya bakan yolda baslar
        const WorldLandmark& l = M.w.landmarks[std::clamp(std::atoi(lm), 0, (int)M.w.landmarks.size() - 1)];
        const double back = 120.0 + l.h * 1.2;
        WorldLeg lg; double s0, la;
        x = l.x - back * std::cos(l.heading + 0.5); y = l.y - back * std::sin(l.heading + 0.5);
        if (M.w.nearestLeg(x, y, 0.0, lg, s0, la, 400.0)) { const RoadPoint q = M.w.path(lg).at(s0); x = q.x; y = q.y; }
        h = std::atan2(l.y - y, l.x - x);
    } else if (const char* ap = std::getenv("ZK_WORLD_AT")) {            // test: n. noktanin yanindaki yolda baslar
        const WorldPoi& q = M.w.pois[(size_t)std::clamp(std::atoi(ap), 0, (int)M.w.pois.size() - 1)];
        x = q.x; y = q.y; h = q.heading;
    } else
    if (app.worldValid) { x = app.worldX; y = app.worldY; h = app.worldH; }
    else {
        const WorldCity& c = M.w.cities[std::clamp(app.career.city, 0, (int)M.w.cities.size() - 1)];
        x = c.x - c.dirX * World::kBlock * 1.8; y = c.y - c.dirY * World::kBlock * 1.8; h = std::atan2(c.dirY, c.dirX);
    }
    double s = 0, lat = 0;
    if (!M.w.nearestLeg(x, y, h, M.leg, s, lat, 400.0)) M.leg = {0, false};
    const RoadPath& p = M.path();
    const RoadPoint q = p.at(s);
    const double lane = p.laneOffset(s, false, 0);
    M.car = std::make_unique<RoadCar>(findVehicle(M.carId), &M.tune, p, s, app.worldValid ? lat : lane);
    if (app.worldValid) M.car->sim().resetPose(x, y, h);
    (void)q;
    M.car->freeRoam = true;
    M.car->assist = app.settings.assist; M.car->stability = true; M.car->esp = app.settings.esp;
    // Gercekci depo (40-90 L); mesafeler ~40 kat kucuk: tuketim x10 (bir depo ~1.5 dunya turu, benzinlik onemli)
    M.car->sim().setFuelSystem(RunField::tankFor(M.carId), 10.0);
    if (oc.fuelL >= 0) M.car->sim().setFuelLevel(oc.fuelL);
    M.camPsi = M.car->sim().heading();
    const Gearbox box = M.car->sim().gearboxType();
    const int gears = M.car->sim().powertrain().gearCount();
    if (box == Gearbox::HPattern) { M.cockpit.configure(Cockpit::Lever::HPattern, gears, !app.settings.autoClutch); M.car->manual = true; M.car->slowClutch = app.settings.autoClutch; }
    else if (box == Gearbox::TorqueConverter || box == Gearbox::DCT) { M.cockpit.configure(Cockpit::Lever::Automatic, gears, false); M.car->manual = false; }
    else { M.cockpit.configure(Cockpit::Lever::Sequential, gears, false); M.car->manual = true; }
    M.cockpit.setPortrait(!land_);
    M.cockpit.setKnobGear(1);
    M.adasLevel = adasLevel(*findVehicle(M.carId), M.tune);
    if (land_) { M.mapBtn = {470, 2, 576, 34}; M.autoBtn = {362, 2, 466, 34}; M.jobBtn = {580, 2, 636, 34}; }
    else { M.mapBtn = {252, 4, 356, 26}; M.autoBtn = {252, 30, 356, 52}; M.jobBtn = {252, 56, 356, 78}; }
    for (int k = 0; k < 3; ++k) M.jobRows[k] = {20, 70.0f + k * 62.0f, (float)M.W - 20, 124.0f + k * 62.0f};
    M.jobClose = {(float)M.W / 2 - 60, (float)M.H - 44, (float)M.W / 2 + 60, (float)M.H - 8};
    M.camCool.assign(M.w.cameras.size(), 0.0);
    if (app.career.camBest.size() < M.w.cameras.size()) app.career.camBest.resize(M.w.cameras.size(), 0);
    if (app.career.legBest.size() < M.w.cities.size()) app.career.legBest.resize(M.w.cities.size(), 0.0);
    M.mPlus = {(float)M.W - 60, 40, (float)M.W - 8, 80}; M.mMinus = {(float)M.W - 60, 86, (float)M.W - 8, 126};
    M.mClose = {(float)M.W - 120, (float)M.H - 44, (float)M.W - 8, (float)M.H - 8}; M.mClear = {8, (float)M.H - 44, 140, (float)M.H - 8};
    M.rng ^= (uint32_t)(app.career.races * 7919 + 13);
    if (const char* mp = std::getenv("ZK_WORLD_MAP")) { M.mapOpen = true; M.mapCx = M.car->sim().posX(); M.mapCy = M.car->sim().posY(); if (std::atof(mp) > 1.5) M.mapScale = std::atof(mp);
        if (const char* mc = std::getenv("ZK_WORLD_MAPCITY")) { const WorldCity& wc = M.w.cities[std::clamp(std::atoi(mc), 0, 9)]; M.mapCx = wc.x; M.mapCy = wc.y; } }
    if (const char* rp = std::getenv("ZK_WORLD_ROUTE")) {                  // test: n. noktaya rota + otonom
        const int k = std::clamp(std::atoi(rp), 0, (int)M.w.pois.size() - 1);
        WorldLeg l; double s, lat; M.hasWp = true;
        if (M.w.nearestLeg(M.w.pois[k].x, M.w.pois[k].y, 0.0, l, s, lat, 200.0)) { const RoadPoint q = M.w.path(l).at(s); M.wpX = q.x; M.wpY = q.y; }
        else { M.wpX = M.w.pois[k].x; M.wpY = M.w.pois[k].y; }
        M.route = M.w.route(M.leg, M.car->s(), M.wpX, M.wpY); M.routeIdx = 0; M.autoDrive = true;
    }
    M.flash("ACIK DUNYA: HARITADAN HEDEF SEC, SUR YA DA OTONOM", 3.0);
}

bool WorldScreen::backLeaves() const { return !m_->mapOpen; }
int WorldScreen::shifter() const { return m_->mapOpen ? 0 : m_->cockpit.lever() == Cockpit::Lever::HPattern ? 1 : 2; }
int WorldScreen::shifterGear() const { return m_->cockpit.knobGear(); }   // harita aciksa geri = haritayi kapat

WorldScreen::~WorldScreen() {
    Impl& M = *m_;
    M.app.career.car().fuelL = M.car->sim().fuelLiters();                // yakit kalici
}

void WorldScreen::update(double dt) {
    Impl& M = *m_;
    App& app = M.app;
    M.msgT -= dt; M.envT += dt;
    RoadCar& P = *M.car;
    VehicleSim& sim = P.sim();
    const double v = sim.speed();
    if (M.mapOpen) { app.voice(0, sim.powertrain().rpm(), 0, false, false, 0.5f); app.tire(0, 0); }
    M.cockpit.update(dt);
    M.cockpit.setPadPedals(app.padThrottle(), app.padBrake());
    if (M.cockpit.takeSeated()) app.haptic(18, 160);
    // Direksiyon (RoadScreen ile ayni): hiza gore sinir, klavye ya da egim
    // Hiza duyarli direksiyon (RoadCar::steerLimit): sehir hizinda genis kilit, hizlandikca sertlesir
    double target = (M.kL ? RoadCar::steerLimit(sim, v, 1.0) : 0.0) - (M.kR ? RoadCar::steerLimit(sim, v, -1.0) : 0.0);
    if (!M.kL && !M.kR && app.tiltAvailable && app.settings.tiltSteer) {
        const double raw = std::clamp(1.3 * (app.settings.tiltInvert ? -1.0 : 1.0) * app.tilt() * app.settings.tiltSens / 100.0, -1.0, 1.0);   // ~23 deg tam
        M.tiltF += (raw - M.tiltF) * std::min(1.0, dt / 0.12);
        const double dz = 0.08, t = M.tiltF, u = std::fabs(t) < dz ? 0.0 : (t - std::copysign(dz, t)) / (1.0 - dz);
        target = std::copysign(std::pow(std::fabs(u), 1.0 + 0.45 * std::clamp((v - 14.0) / 26.0, 0.0, 1.0)), u) * RoadCar::steerLimit(sim, v, u);
    }
    const double rate = RoadCar::steerRate(v, std::fabs(target) > std::fabs(M.steer)) * dt;
    M.steer += std::clamp(target - M.steer, -rate, rate);
    RoadControls c;
    c.steer = M.steer; c.throttle = M.cockpit.throttle(); c.brake = M.cockpit.brake();
    PowertrainCore& pt = sim.powertrain();
    switch (M.cockpit.lever()) {
    case Cockpit::Lever::HPattern:
        c.gear = std::max(0, M.cockpit.knobGear()); c.reverse = M.cockpit.knobGear() < 0;
        if (M.cockpit.clutchPedal()) c.clutch = M.cockpit.clutch();
        break;
    case Cockpit::Lever::Sequential: c.shift = M.cockpit.takeShift(); break;
    case Cockpit::Lever::Automatic: {
        const Cockpit::AutoPos ap = M.cockpit.autoPos();
        c.neutral = ap == Cockpit::AutoPos::P || ap == Cockpit::AutoPos::N; c.reverse = ap == Cockpit::AutoPos::R;
        c.autoMode = ap == Cockpit::AutoPos::S ? 1 : ap == Cockpit::AutoPos::M ? 2 : 0; c.shift = M.cockpit.takeShift();
        if (ap == Cockpit::AutoPos::P && std::fabs(v) < 0.5) c.brake = std::max(c.brake, 0.6);
        break;
    }
    }
    // Otonom: rota varsa izler (kavsakta rotadaki yola gecer), yoksa seritte; hiz siniri + ondeki arac; ekonomik
    if (M.autoDrive) {
        if (c.brake > 0.05 || M.kL || M.kR) { M.autoDrive = false; M.flash("OTONOM KAPANDI", 1.2); P.manual = M.cockpit.lever() != Cockpit::Lever::Automatic; }
        else {
            const RoadPath& rp = M.path();
            if (!M.route.empty() && M.routeIdx + 1 < M.route.size() && P.s() > rp.length() - std::max(4.0, v * 0.5)) {   // donuse erken basla
                ++M.routeIdx; M.leg = M.route[M.routeIdx]; P.setRoad(M.path());
            }
            double tgt = M.speedLimit();
            const double toWp = M.hasWp ? std::hypot(M.wpX - sim.posX(), M.wpY - sim.posY()) : 1e9;
            if (M.hasWp && M.routeIdx + 1 >= M.route.size()) tgt = std::min(tgt, std::max(0.0, (toWp - 12.0) * 0.4));
            const RoadPath& cp = M.path();
            if (M.routeIdx + 1 < M.route.size()) {                           // donus oncesi yavasla
                const double toEnd = cp.length() - P.s();
                const RoadPath& np = M.w.path(M.route[M.routeIdx + 1]);
                const double turn = std::fabs(std::remainder(np.at(2.0).heading - cp.at(cp.length()).heading, kTau));
                if (turn > 0.5) tgt = std::min(tgt, std::sqrt(5.0 * 5.0 + 2.0 * 3.5 * std::max(0.0, toEnd - 6.0)));   // donus 18 km/h
            }
            {   // otonom: kirmizi isikta durma cizgisinde bekler
                const WorldEdge& CE = M.w.edges[M.leg.edge];
                const double toEnd = cp.length() - P.s(), stopAt = toEnd - (CE.hw + 5.0);
                const int st = toEnd < 80.0 ? lightState(M.w, M.leg.rev ? CE.a : CE.b, M.leg.edge, M.envT) : -1;
                if (stopAt > -2.0 && (st == 2 || (st == 1 && stopAt > v * v / 8.0))) tgt = std::min(tgt, std::max(0.0, stopAt) * 0.5);
            }
            for (const auto& t : M.traffic)                                  // ACC
                if (t.leg.edge == M.leg.edge && t.leg.rev == M.leg.rev && t.s > P.s() && t.s - P.s() < 40.0 && std::fabs(cp.laneOffset(t.s, false, t.lane) - P.lateral()) < 1.8)
                    tgt = std::min(tgt, t.v + (t.s - P.s() - 12.0) * 0.3);
            const RoadControls ai = P.aiControls(cp.laneOffset(P.s(), false, 0), 0.45, tgt);
            c.steer = ai.steer;
            const double e = tgt - v;
            M.ccI = std::clamp(M.ccI + e * dt, -6.0, 6.0);
            c.throttle = std::clamp(0.12 * e + 0.05 * M.ccI + 0.12, 0.0, 0.5);
            c.brake = e < -0.8 ? std::clamp(-e * 0.15, 0.0, 0.7) : 0.0;
            M.stuckT = (v < 0.4 && tgt > 2.0) ? M.stuckT + dt : 0.0;
            if (M.stuckT > 3.0) {                                            // takildi (bina / bordur): seride geri al
                const double ss = std::max(0.0, P.s() - 6.0);
                P.recoverAt(ss, cp.laneOffset(ss, false, 0)); M.stuckT = 0; M.flash("OTONOM: YOLA ALINDI", 1.2);
            }
            if (c.brake > 0) c.throttle = 0;
            P.manual = false; c.gear = -1; c.shift = 0; c.clutch = -1.0; c.neutral = false; c.reverse = false;
            if (c.autoMode >= 0) c.autoMode = 0;
            M.cockpit.setKnobGear(pt.gear());
            if (M.hasWp && M.routeIdx + 1 >= M.route.size() && toWp < 16.0 && v < 1.0) { M.autoDrive = false; M.hasWp = false; M.route.clear(); M.flash("HEDEFE VARILDI", 2.0); }
        }
    }
    if (M.mapOpen) { c.throttle = 0; c.brake = std::max(c.brake, 0.5); }   // harita acikken arac durur
    P.update(dt, c);
    if (!M.autoDrive) M.pickLeg(false);
    {   // Bordur: sehir sokaginda asfalttan kaldirima (15 cm) cikis / inis: sarsinti + hiz kaybi, arac kaldirimda yukarda
        const WorldEdge& CE = M.w.edges[M.leg.edge];
        const RoadPath& cp = M.path();
        const double hw = cp.halfWidthAt(P.s()), alat = std::fabs(P.lateral());
        const bool street = !CE.highway && !CE.bridge && CE.city >= 0 && P.s() > hw * 2 + 5 && P.s() < cp.length() - hw * 2 - 5;
        const bool on = street && alat > hw + 0.1 && alat < hw + kWalk + 2.5;
        if (on != M.onCurb && v > 1.0) {
            M.shakeT = std::max(M.shakeT, on ? 0.22 : 0.14); app.haptic(on ? 90 : 60, on ? 230 : 160);
            P.bump(on ? (v > 12.0 ? 0.9 : 0.96) : 0.98);                     // bordura carpma: hizli girince ciddi kayip
            if (on && v > 14.0) M.flash("BORDURA VURDUN!", 1.0);
        }
        M.onCurb = on;
        M.curbH += ((on ? 0.15 : 0.0) - M.curbH) * std::min(1.0, dt / 0.08);
    }
    {   // Yayalar: ezilince yere yigilir (25 s), kalabalik alkislar
        std::vector<Ped> near;
        pedsNear(M.w, M.envT, sim.posX(), sim.posY(), 8.0, sim.posX(), sim.posY(), near);
        const double ch = std::cos(sim.heading()), sh = std::sin(sim.heading());
        for (const Ped& pd : near) {
            if (M.isDown(pd.key)) continue;
            const double dx = pd.x - sim.posX(), dy = pd.y - sim.posY();
            const double lon = dx * ch + dy * sh, lat2 = -dx * sh + dy * ch;
            if (std::fabs(lon) < 2.4 && std::fabs(lat2) < 1.15 && v > 2.0) {
                M.downs.push_back({pd.x + ch * 1.5, pd.y + sh * 1.5, sim.heading() + (lat2 > 0 ? 1.2 : -1.2), M.envT, pd.col, pd.key});
                M.shakeT = std::max(M.shakeT, 0.3); app.haptic(120, 255);
                P.bump(0.97);
                ++M.runOver;
                M.clapT = 0.0; app.applause();
                M.flash(M.runOver > 1 ? "YINE MI? HALK COSTU, ALKISLAR!" : "YAYA EZILDI - ALKISLAR!", 2.6);
            }
        }
        M.downs.erase(std::remove_if(M.downs.begin(), M.downs.end(), [&](const Impl::Down& d) { return M.envT - d.t > 25.0; }), M.downs.end());
        if (M.clapT >= 0.0) { M.clapT += dt; if (M.clapT > 3.5) M.clapT = -1.0; }
    }
    // Binalar: yonlu kutu, arac (2.2 x 0.9) carparsa itilir
    for (int bi = 0; bi < (int)M.w.buildings.size(); ++bi) {
        const WorldBuilding& b = M.w.buildings[bi];
        if (std::fabs(b.cx - sim.posX()) > 70 || std::fabs(b.cy - sim.posY()) > 70) continue;
        double rel = 0;
        RoadSession::contact(P, b.cx, b.cy, std::atan2(b.uy, b.ux), 0.0, 1e7, b.hu, b.hv, rel);
        if (rel > 2.0) { M.shakeT = std::min(0.5, rel * 0.04); app.haptic(150, 220); if (rel > 6.0) M.flash("BINAYA CARPTIN!", 1.0); }
        WallSeg ws[6]; const int nw = siteWalls(b, bi, ws);
        for (int k = 0; k < nw; ++k) {
            double rw = 0;
            RoadSession::contact(P, ws[k].x, ws[k].y, ws[k].h, 0.0, 1e7, ws[k].hl, ws[k].ht, rw);
            if (rw > 2.0) { M.shakeT = std::min(0.5, rw * 0.04); app.haptic(150, 220); if (rw > 6.0) M.flash("DUVARA CARPTIN!", 1.0); }
        }
    }
    {   // Park etmis araclar ve simge yapilar: carpisma
        std::vector<Parked> pk;
        for (int e = 0; e < (int)M.w.edges.size(); ++e) {
            const WorldEdge& E = M.w.edges[e];
            if (sim.posX() < E.minX - 15 || sim.posX() > E.maxX + 15 || sim.posY() < E.minY - 15 || sim.posY() > E.maxY + 15) continue;
            parkedOn(M.w, e, pk);
        }
        for (const Parked& q : pk) {
            if (std::fabs(q.x - sim.posX()) > 8 || std::fabs(q.y - sim.posY()) > 8) continue;
            double rel = 0;
            RoadSession::contact(P, q.x, q.y, q.h, 0.0, 1300.0, 2.2, 0.9, rel);
            if (rel > 2.0) { M.shakeT = 0.35; app.haptic(160, 230); if (rel > 5.0) M.flash("PARK ETMIS ARACA CARPTIN", 1.0); }
        }
        for (const WorldLandmark& l : M.w.landmarks) {
            if (l.type == LmBalloon || l.type == LmMountain) continue;
            if (std::fabs(l.x - sim.posX()) > l.r + 8 || std::fabs(l.y - sim.posY()) > l.r + 8) continue;
            double rel = 0;
            RoadSession::contact(P, l.x, l.y, l.heading, 0.0, 1e7, l.r * 0.85, l.r * 0.85, rel);
            if (rel > 2.0) { M.shakeT = 0.4; app.haptic(160, 230); }
        }
    }
    // Sehir sinirlari
    const int cNow = M.w.cityAt(sim.posX(), sim.posY());
    if (cNow != M.city) {
        if (cNow >= 0) { app.career.city = cNow; app.toast(std::string(M.w.cities[cNow].name) + "'A HOS GELDIN"); app.saveCareer(); }
        else if (M.city >= 0) app.toast("OTOBAN: SEHIRLERARASI");
        M.city = cNow;
    }
    // Sehirlerarasi gecis rekoru: sehirden cikis -> komsu sehre giris suresi
    if (cNow < 0 && M.legFrom < 0 && M.city < 0) {}
    {
        static int lastC = -2;
        if (lastC >= 0 && cNow < 0) { M.legFrom = lastC; M.legStart = M.envT; }
        if (cNow >= 0 && M.legFrom >= 0 && cNow != M.legFrom && std::abs(cNow - M.legFrom) == 1) {
            const double tl = M.envT - M.legStart; const int li = std::min(cNow, M.legFrom);
            double& best = app.career.legBest[li];
            char b[96];
            if (best <= 0 || tl < best) { best = tl; std::snprintf(b, sizeof b, "%s - %s: %d:%04.1f  REKOR!", M.w.cities[M.legFrom].name.c_str(), M.w.cities[cNow].name.c_str(), (int)tl / 60, std::fmod(tl, 60.0)); app.saveCareer(); }
            else std::snprintf(b, sizeof b, "GECIS %d:%04.1f (REKOR %d:%04.1f)", (int)tl / 60, std::fmod(tl, 60.0), (int)best / 60, std::fmod(best, 60.0));
            app.toast(b); M.legFrom = -1;
        }
        if (cNow >= 0) M.legFrom = cNow == M.legFrom ? M.legFrom : (lastC < 0 && M.legFrom >= 0 ? M.legFrom : -1);
        lastC = cNow;
    }
    // Hiz kameralari: gecerken olcer; siniri 10 km/h asarsan ceza; rekor; radar dedektoru onceden uyarir
    M.radarBlink = 0;
    for (size_t k = 0; k < M.w.cameras.size(); ++k) {
        const WorldCamera& cm = M.w.cameras[k];
        M.camCool[k] = std::max(0.0, M.camCool[k] - dt);
        const double ddx = cm.x - sim.posX(), ddy = cm.y - sim.posY(), d = std::hypot(ddx, ddy);
        if (app.career.radarDetector && d < 450.0 && d > 14.0 && ddx * std::cos(sim.heading()) + ddy * std::sin(sim.heading()) > 0) M.radarBlink = std::max(M.radarBlink, 450.0 - d);
        if (d < 14.0 && M.camCool[k] <= 0) {
            M.camCool[k] = 6.0;
            const int kmh = (int)std::lround(v * 3.6), lim = (int)std::lround(cm.limit * 3.6);
            char b[96];
            if (kmh > app.career.camBest[k]) app.career.camBest[k] = kmh;
            if (kmh > lim + 10) {
                const long fine = std::min(app.career.money, 300L + 20L * (kmh - lim));
                app.career.money -= fine;
                std::snprintf(b, sizeof b, "RADAR %d KM/H (SINIR %d) CEZA -%s  REKOR %d", kmh, lim, money(fine).c_str(), app.career.camBest[k]);
            } else std::snprintf(b, sizeof b, "RADAR %d KM/H (SINIR %d)  REKOR %d", kmh, lim, app.career.camBest[k]);
            app.toast(b); app.saveCareer();
        }
    }
    // Toplanabilirler: nadir parca (yanindan gec) / ahir bulgusu (yaninda dur)
    for (const WorldCollect& cl : M.w.collect) {
        if (app.career.gotCollect(cl.idx)) continue;
        const double d = std::hypot(cl.x - sim.posX(), cl.y - sim.posY());
        if (cl.type == 0 && d < 4.5) {
            app.career.setCollect(cl.idx);
            static const char* const kPart[6] = {"TURBO SALYANGOZU", "DOVME PISTON", "YARIS KAM MILI", "TITANYUM EGZOZ", "KARBON KAPUT", "YARIS DEBRIYAJI"};
            const long prize = 750 + 250 * (cl.idx % 7);
            app.career.money += prize;
            const int n = app.career.collectedCount();
            char b[96]; std::snprintf(b, sizeof b, "NADIR PARCA: %s +%s (%d/%d)", kPart[cl.idx % 6], money(prize).c_str(), n, (int)M.w.collect.size());
            app.toast(b); app.saveCareer();
        }
        if (cl.type == 1 && d < 14.0 && v < 2.0) {
            app.career.setCollect(cl.idx);
            std::string why;
            if (app.career.claimBarn(cl.city, &why))
                app.toast(std::string("KOLEKSIYON: ") + upper(findVehicle(Career::barnCar(cl.city))->model) + (Career::barnMint(cl.city) ? " - SIFIR, 0 KM!" : " - HURDA: RESTORE ET"));
            else app.toast(why);
            app.saveCareer();
        }
    }
    for (const WorldCollect& cl : M.w.collect) {                          // pert arac (yaninda dur) / ozel yapim kasasi (gec)
        if (app.career.gotCollect(cl.idx) || (cl.type != 2 && cl.type != 3)) continue;
        const double d = std::hypot(cl.x - sim.posX(), cl.y - sim.posY());
        if (cl.type == 3 && d < 5.0) {
            app.career.setCollect(cl.idx); app.career.foundParts |= 1u << cl.ref;
            app.toast(std::string("OZEL YAPIM: ") + mapOnlyName(cl.ref) + " BULUNDU! (PARCA: BEDAVA TAK)"); app.saveCareer();
        }
        if (cl.type == 2 && d < 12.0 && v < 2.0) {
            std::string why; int cid = 0;
            if (app.career.claimWreck(cl.ref, &why, &cid)) { app.career.setCollect(cl.idx); app.toast(std::string("PERT: ") + upper(findVehicle(cid)->model) + " (AGIR MODIFIYELI) GARAJINDA"); app.saveCareer(); }
            else M.flash(why, 1.5);
        }
    }
    // Kesif: 250 m icindeki dukkanlar / kameralar / simge yapilar haritaya islenir (yarislar bastan gorunur)
    {
        static double acc = 0; acc += dt;
        if (acc > 0.5) {
            acc = 0; bool any = false;
            for (size_t k = 0; k < M.w.pois.size(); ++k) if (std::hypot(M.w.pois[k].x - sim.posX(), M.w.pois[k].y - sim.posY()) < 250.0) any |= app.career.discover((int)k);
            for (size_t k = 0; k < M.w.cameras.size(); ++k) if (std::hypot(M.w.cameras[k].x - sim.posX(), M.w.cameras[k].y - sim.posY()) < 250.0) any |= app.career.discover(512 + (int)k);
            for (size_t k = 0; k < M.w.landmarks.size(); ++k) if (std::hypot(M.w.landmarks[k].x - sim.posX(), M.w.landmarks[k].y - sim.posY()) < 600.0) any |= app.career.discover(768 + (int)k);
            if (any) app.saveCareer();
        }
    }
    // Is (kurye / taksi): hedefe varinca (dur) asama / odul; sure biterse basarisiz
    if (M.hasJob) {
        M.job.deadline -= dt;
        if (M.job.deadline <= 0) { M.hasJob = false; M.hasWp = false; M.route.clear(); app.toast("IS BASARISIZ: SURE BITTI"); }
        else if (std::hypot(M.job.tx - sim.posX(), M.job.ty - sim.posY()) < 25.0 && v < 2.5) {
            if (M.job.type == 1 && M.job.phase == 0) { M.job.phase = 1; M.job.tx = M.job.dx; M.job.ty = M.job.dy; M.startJobTarget(M.job.tx, M.job.ty); app.toast("YOLCU BINDI: HEDEFE GOTUR"); }
            else { app.career.money += (long)M.job.reward; app.toast("IS TAMAM +" + money((long)M.job.reward)); app.saveCareer(); M.hasJob = false; M.hasWp = false; M.route.clear(); }
        }
    }
    // Kesicide 5 s: yanindaki modifiyeli arac (serseri / bulusma / trafik) kapismaya gelir
    {
        int cand = 0; Tune ct{};
        if (v < 1.5 && pt.limiterHit()) {
            double bd = 1e9;
            for (const auto& t : M.traffic) {
                const RoadPoint q = M.w.path(t.leg).at(t.s);
                const double d = std::hypot(q.x - sim.posX(), q.y - sim.posY());
                if (d < (t.role == 2 ? 25.0 : 14.0) && d < bd) { bd = d; cand = t.carId; ct = t.role == 2 ? opponentPreset(2) : Tune{}; }
            }
            for (const WorldPoi& q : M.w.pois)
                if (q.type == WPoiMeet && std::hypot(q.x - sim.posX(), q.y - sim.posY()) < 55.0) { static const int kMeet[12] = {2, 5, 12, 24, 34, 50, 59, 78, 88, 100, 122, 152}; cand = kMeet[(q.city * 5 + (int)M.envT) % 12]; ct = opponentPreset(1 + (q.city & 1)); }
        }
        M.limT = cand ? M.limT + dt : 0.0;
        if (cand && M.limT > 5.0) {
            M.limT = 0;
            app.pendMode = 1; app.pendRival = cand; app.pendTune = ct;
            app.worldValid = true; app.worldX = sim.posX(); app.worldY = sim.posY(); app.worldH = sim.heading();
            app.worldReturn = true; app.career.car().fuelL = sim.fuelLiters();
            app.goRoad();
            return;
        }
    }
    // Trafik: oyuncu cevresinde ~24 arac; kavsakta rastgele cikis; polis / serseri
    {
        const double px = sim.posX(), py = sim.posY();
        M.traffic.erase(std::remove_if(M.traffic.begin(), M.traffic.end(), [&](const Impl::T& t) {
            const RoadPoint q = M.w.path(t.leg).at(t.s); return std::hypot(q.x - px, q.y - py) > 750.0; }), M.traffic.end());
        int guard = 0;
        std::vector<int> nearE;                                          // yakindaki yollar (dogma adaylari)
        const size_t cap = M.city >= 0 ? 40 : 24;                        // sehir ici kalabalik
        if (M.traffic.size() < cap)
            for (int e = 0; e < (int)M.w.edges.size(); ++e) {
                const WorldEdge& E = M.w.edges[e];
                if (!(px < E.minX - 650 || px > E.maxX + 650 || py < E.minY - 650 || py > E.maxY + 650)) nearE.push_back(e);
            }
        while (!nearE.empty() && M.traffic.size() < cap && guard++ < 60) {
            const int e = nearE[(size_t)(M.rnd() * nearE.size()) % nearE.size()];
            const WorldEdge& E = M.w.edges[e];
            Impl::T t{};
            t.leg = {e, M.rnd() < 0.5}; const RoadPath& tp = M.w.path(t.leg);
            t.s = M.rnd() * tp.length();
            const RoadPoint q = tp.at(t.s);
            const double d = std::hypot(q.x - px, q.y - py);
            if (d < 140.0 || d > 650.0) continue;
            t.lane = (int)(M.rnd() * tp.lanesFwd(t.s) * 0.999);
            t.v0 = (E.highway ? (E.city < 0 ? 24.0 : 14.0) : 11.0) * (0.85 + 0.3 * M.rnd());
            t.v = t.v0; t.uid = M.nextUid++;
            const double r = M.rnd();
            static const int kHool[8] = {2, 12, 59, 24, 34, 100, 50, 78};
            if (r < 0.03) { t.role = 1; t.carId = 217; }
            else if (r < 0.04) { t.role = 2; t.carId = kHool[t.uid % 8]; t.v0 *= 1.2; }
            else { const auto& cat = vehicleCatalog(); do { t.carId = 1 + (int)(M.rnd() * cat.size()); } while (!cat[t.carId - 1].streetLegal); }
            t.prevRel = 0;
            M.traffic.push_back(t);
        }
        // Dunya uzayinda onu gorme: her aracin pozu + hiz vektoru (oyuncu sonda); kavsakta karsidan gelen / kesen araclar
        struct Pose { double x, y, h, v; };
        std::vector<Pose> poses; poses.reserve(M.traffic.size() + 1);
        for (const auto& o : M.traffic) { double ox, oy, oh; M.tPose(o, ox, oy, oh); poses.push_back({ox, oy, oh, o.v}); }
        poses.push_back({sim.posX(), sim.posY(), sim.heading() + sim.bodySlipAngle() + (sim.vx() < 0 ? 3.14159265 : 0.0), v});   // gidis yonu (geri dahil)
        for (auto& t : M.traffic) {
            const RoadPath& tp = M.w.path(t.leg);
            double gap = 1e9, lv = t.v0;
            double brakeTo = 1e9;                                          // engel icin hedef hiz
            {
                const size_t me = (size_t)(&t - &M.traffic[0]);
                const Pose& a = poses[me];
                const double fx = std::cos(a.h), fy = std::sin(a.h);
                const double look = 10.0 + a.v * 2.2;                      // ~2.2 s ileri
                for (size_t j = 0; j < poses.size(); ++j) {
                    if (j == me) continue;
                    const Pose& b = poses[j];
                    // simdiki ve 1 s sonraki konum (kesen arac): ikisinden en tehlikelisi
                    for (double tp2 : {0.0, 1.0}) {
                        const double bx = b.x + std::cos(b.h) * b.v * tp2 - (a.x + fx * a.v * tp2);
                        const double by = b.y + std::sin(b.h) * b.v * tp2 - (a.y + fy * a.v * tp2);
                        const double lon = bx * fx + by * fy, lat = -bx * fy + by * fx;
                        if (lon < 1.5 || lon > look || std::fabs(lat) > 2.3) continue;
                        const double along = b.v * std::cos(b.h - a.h);        // onumdekinin benim yonumdeki hizi
                        const double free = lon + (tp2 > 0 ? a.v * tp2 : 0.0) - 7.0;
                        brakeTo = std::min(brakeTo, std::max(0.0, std::max(0.0, along) + free * 0.45));
                    }
                }
            }
            for (const auto& o : M.traffic)
                if (&o != &t && o.leg.edge == t.leg.edge && o.leg.rev == t.leg.rev && o.lane == t.lane && o.s > t.s && o.s - t.s < gap) { gap = o.s - t.s; lv = o.v; }
            if (M.leg.edge == t.leg.edge && M.leg.rev == t.leg.rev && P.s() > t.s && P.s() - t.s < gap && std::fabs(P.lateral() - tp.laneOffset(P.s(), false, t.lane)) < 2.0) { gap = P.s() - t.s; lv = v; }
            const bool nearEnd = tp.length() - t.s < 25.0 && !M.w.edges[t.leg.edge].highway;   // kavsakta yavasla
            double want = gap < 1e8 ? std::min(t.v0, lv + (gap - 10.0) * 0.4) : t.v0;
            {   // trafik isigi: kirmizida (sarida durabiliyorsa) durma cizgisinde (kavsaktan ~hw+5 m once) bekler
                const WorldEdge& TE = M.w.edges[t.leg.edge];
                const int endNode = t.leg.rev ? TE.a : TE.b;
                const double toEnd = tp.length() - t.s;
                if (toEnd < 60.0 && t.role != 2) {                       // serseri isik dinlemez
                    const int st = lightState(M.w, endNode, t.leg.edge, M.envT);
                    const double stopAt = toEnd - (TE.hw + 5.0);
                    if (st == 2 || (st == 1 && stopAt > t.v * t.v / 8.0)) want = std::min(want, std::max(0.0, stopAt) * 0.5);
                }
            }
            const double tg = nearEnd ? std::min(want, 7.0) : want;
            const double tg2 = std::min(tg, brakeTo);
            t.v += std::clamp(tg2 - t.v, (brakeTo < t.v - 3.0 ? -9.0 : -6.0) * dt, 1.6 * dt);   // engelde sert fren
            t.v = std::max(0.0, t.v);
            t.s += t.v * dt;
            if (t.s > tp.length()) {
                const auto ex = M.w.exits(t.leg);
                if (ex.empty()) { t.s = tp.length(); t.v = 0; }
                else {
                    double ox, oy, oh; M.tPose(t, ox, oy, oh);                  // eski yolun sonundaki gercek poz
                    t.s -= tp.length(); t.leg = ex[(size_t)(M.rnd() * ex.size()) % ex.size()]; t.lane = std::min(t.lane, M.w.path(t.leg).lanesFwd(0.0) - 1);
                    double nx, ny, nh; M.lanePose(t, 0.0, nx, ny, nh);
                    const double turn = std::fabs(std::remainder(nh - oh, 6.283185307179586));
                    t.bx = ox; t.by = oy; t.bh = oh;
                    t.bl = std::min(M.w.path(t.leg).length() * 0.6, std::clamp(6.0 + 14.0 * turn + 1.5 * std::hypot(nx - ox, ny - oy), 6.0, 30.0));
                }
            }
            // Carpisma (oyuncu)
            double tx, ty, th; M.tPose(t, tx, ty, th);
            struct { double heading; } q{th};
            if (std::fabs(tx - sim.posX()) < 9 && std::fabs(ty - sim.posY()) < 9) {
                double rel = 0;
                const double nv = RoadSession::contact(P, tx, ty, q.heading, t.v, 1400.0, 2.2, 0.9, rel);
                if (rel > 0.5) { t.v = std::max(0.0, std::fabs(nv)); if (rel > 1.5) { M.shakeT = 0.4; app.haptic(200, 255); M.flash(rel > 20 ? "AGIR CARPISMA!" : "CARPISMA", 1.0); } }
            }
            // Polis: yaninda hiz siniri + 15 km/h; serseri: 40 km/h fark ile gecilirse
            const double dd = std::hypot(tx - sim.posX(), ty - sim.posY());
            const double relS = (sim.posX() - tx) * std::cos(q.heading) + (sim.posY() - ty) * std::sin(q.heading);
            if (t.role == 1 && dd < 45.0 && v > M.speedLimit() + 15.0 / 3.6) {
                app.pendMode = 0; app.pendRival = t.carId; app.pendTune = Tune{};
            }
            if (t.role == 2 && dd < 12.0 && t.prevRel < 0.0 && relS >= 0.0 && v - t.v > 40.0 / 3.6) {
                app.pendMode = 1; app.pendRival = t.carId; app.pendTune = opponentPreset(2);
            }
            t.prevRel = relS;
        }
        if (app.pendMode >= 0) {                                         // kovalamaca / kapisma: yaris ekrani, sonra buraya
            app.worldValid = true; app.worldX = sim.posX(); app.worldY = sim.posY(); app.worldH = sim.heading();
            app.worldReturn = true;
            app.career.car().fuelL = sim.fuelLiters();
            app.goRoad();
            return;
        }
    }
    if (const char* lg = std::getenv("ZK_WORLD_LOG")) {                  // test: otonom / yol secimi gunlugu
        static double acc = 0; acc += dt;
        if (acc > 0.5) { acc = 0; if (FILE* f = std::fopen(lg, "a")) { std::fprintf(f, "t=%.1f leg=%d%s s=%.1f/%.1f lat=%.2f ri=%zu/%zu v=%.1f x=%.0f y=%.0f\n", M.envT, M.leg.edge, M.leg.rev ? "r" : "f", P.s(), M.path().length(), P.lateral(), M.routeIdx, M.route.size(), v, sim.posX(), sim.posY()); std::fclose(f); } }
    }
    M.spin += std::clamp(sim.wheel(0).omega() * dt, -0.55, 0.55);
    M.camPsi += std::remainder(sim.heading() - M.camPsi, kTau) * std::min(1.0, dt * 4.0);
    M.shakeT = std::max(0.0, M.shakeT - dt);
    if (P.grinding()) { M.cockpit.setKnobGear(pt.gear()); M.flash("DEBRIYAJ!", 0.6); }
    for (auto& m : sim.drainFailEvents()) M.flash(m, 3.0);
    if (sim.fuelLiters() < 0.05 && v < 1.0) M.flash("YAKIT BITTI! BENZINLIGE ITTIR / HARITADAN EN YAKIN", 2.0);
    app.voice(0, pt.rpm(), pt.throttleEffective(), pt.limiterHit(), pt.gear() > 0, 1.0f);
    if (pt.gear() != M.lastGear) { if (M.lastGear > -2) app.sfxShift(); M.lastGear = pt.gear(); }
    app.tire(0, P.tireSlipSpeed(), P.tireLockSpeed());
    app.wind(v);
}

void WorldScreen::render(Renderer& r) {
    Impl& M = *m_;
    const int W = M.W, H = M.H;
    const World& w = M.w;
    RoadCar& P = *M.car;
    const VehicleSim& sim = P.sim();
    r.begin(W, H, {0.36f, 0.55f, 0.28f});
    const double X = sim.posX(), Y = sim.posY();
    const double cpz = std::cos(M.camPsi), spz = std::sin(M.camPsi);
    const double camUp = M.land ? 2.6 : 3.2;
    double ex = X - 8.0 * cpz, ey = Y - 8.0 * spz, ez = camUp;
    const double tx = X + 6.0 * cpz, ty = Y + 6.0 * spz, tz = M.land ? 0.9 : 0.1;
    if (M.shakeT > 0) { ez += M.shakeT * std::sin(M.envT * 61.0) * 0.5; ex += M.shakeT * std::sin(M.envT * 47.0) * 0.3; }
    const float fov = M.land ? 0.95f : 1.15f;
    const Mat4 proj = matPerspective(fov, (float)W / H, 0.3f, 2500.0f);
    const Mat4 view = matLookAt((float)ex, (float)ez, (float)-ey, (float)tx, (float)tz, (float)-ty);
    const Mat4 vp = matMul(proj, view);
    r.beginWorldDepth(0.3f, 2500.0f);
    const float pxPerM = H * 0.5f / std::tan(fov * 0.5f);
    auto P3 = [&](double x, double y, double h) { return project(vp, x, y, h, W, H); };
    double fx = tx - ex, fy = ty - ey; { const double l = std::hypot(fx, fy); fx /= l; fy /= l; }
    const Proj hz = P3(ex + 3000 * fx, ey + 3000 * fy, 0);
    const float horizon = hz.ok ? std::clamp(hz.y, 0.0f, (float)H) : H * 0.4f;
    const Color skyLow{0.85f, 0.72f, 0.58f}, fogCol{0.82f, 0.72f, 0.62f};
    auto fog = [&](Color c, float wd) {
        const float f = std::clamp((wd - 80.0f) / 1300.0f, 0.0f, 0.85f);
        return Color{c.r + (fogCol.r - c.r) * f, c.g + (fogCol.g - c.g) * f, c.b + (fogCol.b - c.b) * f, c.a};
    };
    r.rect(0, horizon, W, H, {0.36f, 0.55f, 0.28f});
    r.gradientV(0, 0, W, horizon, {0.30f, 0.45f, 0.85f}, skyLow);
    {   // Ufuk: iki kat tepe / dag silueti (bakis yonune gore doner, sonsuz uzakta: ufuk asla bos kalmaz)
        const double yaw = std::atan2(fy, fx), hfov = std::atan(std::tan(fov * 0.5) * (double)W / H);
        auto ridge = [&](double a, double f1, double f2, double ph) {
            return 0.55 + 0.25 * std::sin(a * f1 + ph) + 0.15 * std::sin(a * f2 + ph * 1.7) + 0.08 * std::sin(a * 23.0 + ph);
        };
        for (int layer = 0; layer < 2; ++layer) {
            const float hMax = layer == 0 ? H * 0.11f : H * 0.06f;
            const Color c = layer == 0 ? Color{0.55f, 0.6f, 0.72f} : Color{0.42f, 0.52f, 0.42f};
            for (int x = 0; x < W; x += 4) {
                const double a = yaw - ((x + 2.0) / W - 0.5) * 2.0 * hfov;
                const float hh = (float)(hMax * ridge(a, layer ? 5.0 : 3.0, layer ? 11.0 : 7.0, layer * 2.3));
                r.rect((float)x, horizon - hh, (float)x + 4.0f, horizon + 1.0f, c);
            }
        }
    }
    r.setSceneLight(1.0f, 1.0f);
    auto front = [&](double x, double y, double maxD) {                   // gorus konisinde mi (kamera onunde)
        const double dx = x - ex, dy = y - ey;
        const double along = dx * fx + dy * fy;
        if (std::fabs(x - X) < 250.0 && std::fabs(y - Y) < 250.0) return true;   // aracin cevresi 500x500 m: asla silinmez
        return along > -80.0 && dx * dx + dy * dy < maxD * maxD;   // genis koni: donuste yandakiler erken silinmez
    };
    // Sehir zeminleri (kaldirim / beton): 20 m karolar (yakindaki buyuk karo kameranin arkasina tasip atlaniyordu)
    for (const WorldCity& c : w.cities) {
        if (std::hypot(c.x - ex, c.y - ey) > c.hu + c.hv + 4000.0) continue;   // butun sehir (uzaktan da)
        const double px = -c.dirY, py = c.dirX;
        const double cu = (ex - c.x) * c.dirX + (ey - c.y) * c.dirY, cv = (ex - c.x) * px + (ey - c.y) * py;   // kamera yerel
        const double T = 100.0;                                          // kaba karo; yakinda 20 m, cok yakinda 4 m
        const int iu0 = (int)std::floor(std::max(-c.hu, cu - 4000.0) / T), iu1 = (int)std::ceil(std::min(c.hu, cu + 4000.0) / T);
        const int iv0 = (int)std::floor(std::max(-c.hv, cv - 4000.0) / T), iv1 = (int)std::ceil(std::min(c.hv, cv + 4000.0) / T);
        auto Q = [&](double u, double v) { return P3(c.x + u * c.dirX + v * px, c.y + u * c.dirY + v * py, 0.0); };
        for (int i = iu0; i < iu1; ++i)
            for (int j = iv0; j < iv1; ++j) {
                const double u0 = i * T, u1 = u0 + T, v0 = j * T, v1 = v0 + T, mu = u0 + T * 0.5, mv = v0 + T * 0.5;
                const double wx = c.x + mu * c.dirX + mv * px, wy = c.y + mu * c.dirY + mv * py;
                if (!front(wx, wy, 4000.0)) continue;
                const double d = std::hypot(wx - ex, wy - ey);
                if (d > 700.0 && World::surfaceLocal(c.style, mu, mv) == 2) continue;
                const int sub = d < 110.0 ? 25 : d < 700.0 ? 5 : 1;            // yakin duzlem kirpmasi icin kucuk karolar
                for (int a2 = 0; a2 < sub; ++a2)
                    for (int b2 = 0; b2 < sub; ++b2) {
                        const double ua = u0 + T * a2 / sub, ub = u0 + T * (a2 + 1) / sub;
                        const double va = v0 + T * b2 / sub, vb = v0 + T * (b2 + 1) / sub;
                        const int surf = World::surfaceLocal(c.style, 0.5 * (ua + ub), 0.5 * (va + vb));
                        if (surf == 2) continue;                              // yesil: cim zemini kalir
                        const Proj q0 = Q(ua, va);
                        const float wv = 0.04f * std::sin((float)(M.envT * 1.3 + ua * 0.05 + va * 0.03));   // su parlamasi
                        quadP(r, q0, Q(ub, va), Q(ub, vb), Q(ua, vb), fog(surf == 1 ? Color{0.16f + wv, 0.38f + wv, 0.62f + wv} : Color{0.44f, 0.46f, 0.36f}, q0.w));
                    }
            }
    }
    // Yollar: gorus mesafesindeki kenarlar; asfalt + serit cizgileri
    for (const WorldEdge& E : w.edges) {
        if (ex < E.minX - 2500 || ex > E.maxX + 2500 || ey < E.minY - 2500 || ey > E.maxY + 2500) continue;   // uzak yollar da (LOD)
        const auto& pts = E.fwd().points();
        const int step0 = E.highway ? 2 : 1;
        int step = step0;
        const bool curbs = !E.highway && !E.bridge && E.city >= 0;
        const WorldNode& nA = w.nodes[E.a]; const WorldNode& nB = w.nodes[E.b];
        for (size_t i = 0; i + 1 < pts.size(); i += step) {
            const RoadPoint& a = pts[i];
            const double da = std::hypot(a.x - ex, a.y - ey);
            const bool nearCar = std::fabs(a.x - X) < 250.0 && std::fabs(a.y - Y) < 250.0;
            step = (nearCar || da < 350.0) ? step0 : da < 1000.0 ? step0 * 3 : step0 * 8;   // uzakta seyrek nokta
            const RoadPoint& b = pts[std::min(i + step, pts.size() - 1)];
            if (!front(a.x, a.y, 2500.0) && !front(b.x, b.y, 2500.0)) continue;
            auto edge = [&](const RoadPoint& p, double off, double h) { return P3(p.x - off * std::sin(p.heading), p.y + off * std::cos(p.heading), h); };
            const Proj aL = edge(a, a.hw, 0.01), aR = edge(a, -a.hw, 0.01), bL = edge(b, b.hw, 0.01), bR = edge(b, -b.hw, 0.01);
            const bool band = (i / step0 / 4) % 2 == 0;
            const Color rc = fog(band ? Color{0.30f, 0.30f, 0.32f} : Color{0.28f, 0.28f, 0.30f}, aL.ok ? aL.w : bL.w);
            quadP(r, aL, bL, bR, aR, rc);                                   // kismen arkadaysa yakin duzlemde kirpilir
            if (curbs && da < 450.0) {                                      // kaldirim + bordur
                const double dA = std::min(std::hypot(a.x - nA.x, a.y - nA.y), std::hypot(a.x - nB.x, a.y - nB.y));
                const double dB = std::min(std::hypot(b.x - nA.x, b.y - nA.y), std::hypot(b.x - nB.x, b.y - nB.y));
                const double cut = a.hw * 2.0 + 5.0;
                if (dA > cut && dB > cut) {
                    const Color top = fog({0.70f, 0.69f, 0.66f}, aL.ok ? aL.w : (float)da), face = fog({0.82f, 0.82f, 0.80f}, aL.ok ? aL.w : (float)da);
                    for (double sg : {1.0, -1.0}) {
                        quadP(r, edge(a, sg * a.hw, 0.0), edge(b, sg * b.hw, 0.0), edge(b, sg * b.hw, 0.15), edge(a, sg * a.hw, 0.15), face);
                        quadP(r, edge(a, sg * a.hw, 0.15), edge(b, sg * b.hw, 0.15), edge(b, sg * (b.hw + kWalk), 0.15), edge(a, sg * (a.hw + kWalk), 0.15), top);
                    }
                }
            }
            if (E.bridge && aL.ok && aL.w < 400.0f)                                   // kopru korkulugu
                for (double sg : {1.0, -1.0}) {
                    const Proj k0 = edge(a, sg * (a.hw + 0.3), 0.0), k1 = edge(b, sg * (b.hw + 0.3), 0.0), k2 = edge(b, sg * (b.hw + 0.3), 1.1), k3 = edge(a, sg * (a.hw + 0.3), 1.1);
                    quadP(r, k0, k1, k2, k3, fog({0.72f, 0.72f, 0.75f}, aL.w));
                }
            if (aL.ok && aL.w < 260.0f && (i / step0) % 3 == 0) {                     // kesik cizgiler: serit sinirlari
                const int nl = (int)(a.lf + a.lb);
                for (int k = 1; k < nl; ++k) {
                    const double off = -a.hw + 2.0 * a.hw * k / nl;
                    const bool mid = k == (int)a.lf;
                    const Proj c0 = edge(a, off + 0.08, 0.02), c1 = edge(b, off + 0.08, 0.02), c2 = edge(b, off - 0.08, 0.02), c3 = edge(a, off - 0.08, 0.02);
                    quadP(r, c0, c1, c2, c3, mid ? Color{0.95f, 0.78f, 0.2f} : Color{0.92f, 0.92f, 0.9f});
                }
            }
        }
    }
    // Rota (yerde mavi cizgi)
    if (!M.route.empty())
        for (size_t k = M.routeIdx; k < M.route.size() && k < M.routeIdx + 4; ++k) {
            const RoadPath& rp = w.path(M.route[k]);
            for (double s = (k == M.routeIdx ? P.s() : 0.0); s + 4.0 < rp.length(); s += 4.0) {
                const RoadPoint a = rp.at(s), b = rp.at(s + 2.5);
                if (!front(a.x, a.y, 300.0)) continue;
                const double lo = rp.laneOffset(s, false, 0);
                auto e2 = [&](const RoadPoint& p, double off) { return P3(p.x - off * std::sin(p.heading), p.y + off * std::cos(p.heading), 0.03); };
                quadP(r, e2(a, lo + 0.35), e2(b, lo + 0.35), e2(b, lo - 0.35), e2(a, lo - 0.35), {0.2f, 0.7f, 1.0f, 0.75f});
            }
        }
    // Binalar (uzaktan yakina, gorunen yuzler), noktalar (tabelalar), agaclar
    struct Item { double d; int kind; int idx; };
    std::vector<Item> items;
    for (int i = 0; i < (int)w.buildings.size(); ++i) {
        const WorldBuilding& b = w.buildings[i];
        if (!front(b.cx, b.cy, 6000.0)) continue;                          // butun sehir silueti
        const double d = std::hypot(b.cx - ex, b.cy - ey);
        if (d > 1800.0 && b.h < 14.0 + d * 0.004) continue;               // uzakta alcak binalar gorunmez (LOD)
        items.push_back({d, 0, i});
        if (d < 450.0) { items.push_back({d + 0.5, 7, i}); items.push_back({d - 0.5, 8, i}); }   // site duvari: arka parcalar binadan once, on parcalar sonra
    }
    for (int i = 0; i < (int)w.pois.size(); ++i) if (front(w.pois[i].x, w.pois[i].y, 2500.0)) items.push_back({std::hypot(w.pois[i].x - ex, w.pois[i].y - ey), 1, i});
    for (int i = 0; i < (int)w.landmarks.size(); ++i) {
        const WorldLandmark& l = w.landmarks[i];
        const double d = std::hypot(l.x - ex, l.y - ey);
        if (d < (l.type == LmMountain ? 25000.0 : 9000.0) && (l.type == LmMountain || front(l.x, l.y, 9000.0))) items.push_back({d, 2, i});
    }
    // Yayalar: sokak kaldirimlarinda yuruyen insanlar (belirlenimci, zamana gore), yakinda
    std::vector<Ped> peds, pedsAll;
    pedsNear(w, M.envT, ex, ey, 200.0, X, Y, pedsAll);
    for (const Ped& pd : pedsAll) if (!M.isDown(pd.key) && front(pd.x, pd.y, 200.0)) peds.push_back(pd);
    for (int i = 0; i < (int)peds.size(); ++i) items.push_back({std::hypot(peds[i].x - ex, peds[i].y - ey), 3, i});
    for (int i = 0; i < (int)M.downs.size(); ++i) if (front(M.downs[i].x, M.downs[i].y, 300.0)) items.push_back({std::hypot(M.downs[i].x - ex, M.downs[i].y - ey) + 0.3, 9, i});
    // Sokak lambalari (her 35 m, bordur ucunda) ve bahce agaclari (duvarli binalarin bahce koselerinde)
    struct Prop { double x, y; int type; float v; };                      // type 0 lamba, 1 agac
    std::vector<Prop> props;
    for (int e = 0; e < (int)w.edges.size(); ++e) {
        const WorldEdge& E = w.edges[e];
        if (E.highway || E.bridge || E.city < 0) continue;
        if (ex < E.minX - 300 || ex > E.maxX + 300 || ey < E.minY - 300 || ey > E.maxY + 300) continue;
        const RoadPath& p = E.fwd();
        for (int k = 0; 12.0 + k * 35.0 < p.length() - 12.0; ++k) {
            const RoadPoint q = p.at(12.0 + k * 35.0);
            const double lat = ((k + e) & 1 ? 1.0 : -1.0) * (q.hw + 0.25);
            const double lx = q.x - lat * std::sin(q.heading), ly = q.y + lat * std::cos(q.heading);
            if (front(lx, ly, 300.0)) props.push_back({lx, ly, 0, 0.0f});
        }
    }
    for (int e = 0; e < (int)w.edges.size(); ++e) {                      // otoban kenari: elektrik direkleri (80 m) + reklam panolari (1.1 km)
        const WorldEdge& E = w.edges[e];
        if (E.city >= 0) continue;
        if (ex < E.minX - 700 || ex > E.maxX + 700 || ey < E.minY - 700 || ey > E.maxY + 700) continue;
        const RoadPath& p = E.fwd();
        const double s0 = std::max(0.0, p.length() * 0.0);
        for (double s = s0 + 40.0; s < p.length() - 40.0; s += 80.0) {
            const RoadPoint q = p.at(s);
            if (std::fabs(q.x - ex) > 700 || std::fabs(q.y - ey) > 700) continue;
            const double lat = -(q.hw + 32.0);
            const double lx = q.x - lat * std::sin(q.heading), ly = q.y + lat * std::cos(q.heading);
            if (front(lx, ly, 700.0)) props.push_back({lx, ly, 2, 0.0f});
            if (std::fmod(s, 1120.0) < 80.0) {
                const double lb = q.hw + 14.0;
                const double bx = q.x - lb * std::sin(q.heading), by = q.y + lb * std::cos(q.heading);
                if (front(bx, by, 700.0)) props.push_back({bx, by, 3, (float)hashW((int)s + e * 7)});
            }
        }
    }
    for (int i = 0; i < (int)w.buildings.size(); ++i) {                  // site bahceleri: on bahcede sira sira, yanlarda tek tuk
        const WorldBuilding& b = w.buildings[i];
        if (b.city < 0 || std::fabs(b.cx - ex) > 260 || std::fabs(b.cy - ey) > 260) continue;
        const double vx = -b.uy, vy = b.ux;
        double U, v0, v1; siteBox(b, U, v0, v1);
        const double fa = b.roadSide > 0 ? b.hv + 1.5 : v0 + 1.5, fb = b.roadSide > 0 ? v1 - 1.5 : -(b.hv + 1.5);   // on bahce bandi
        int k = 0;
        for (double v = fa; v <= fb; v += 5.5)
            for (double u = -U + 2.0; u <= U - 2.0; u += 5.5, ++k) {
                if (std::fabs(u) < 3.5 && std::fabs(v - (b.roadSide > 0 ? fb : fa)) < 3.0) continue;   // kapi onu bos
                const float hv2 = hashW(i * 97 + k);
                if (hv2 < 0.25f) continue;
                const double uu = u + (hashW(i * 89 + k) - 0.5) * 2.0, vv = v + (hv2 - 0.5) * 2.0;
                const double tx = b.cx + uu * b.ux + vv * vx, ty = b.cy + uu * b.uy + vv * vy;
                if (front(tx, ty, 260.0)) props.push_back({tx, ty, 1, hv2});
            }
        for (double su : {1.0, -1.0}) {                                    // yan bahce
            const double tx = b.cx + su * (b.hu + 1.8) * b.ux, ty = b.cy + su * (b.hu + 1.8) * b.uy;
            if (front(tx, ty, 260.0)) props.push_back({tx, ty, 1, hashW(i * 43 + (su > 0))});
        }
    }
    for (const WorldTree& t : w.trees) {                                  // parklar / bos arsalar / yol kenari
        const double tr = t.city < 0 ? 650.0 : 320.0;
        if (std::fabs(t.x - ex) > tr || std::fabs(t.y - ey) > tr) continue;
        if (front(t.x, t.y, tr)) props.push_back({t.x, t.y, 1, t.s});
    }
    for (int i = 0; i < (int)props.size(); ++i) items.push_back({std::hypot(props[i].x - ex, props[i].y - ey), 10, i});
    struct Light { double x, y; int state; };                            // state 0 yesil, 1 sari, 2 kirmizi
    std::vector<Light> lights;
    for (int ni = 0; ni < (int)w.nodes.size(); ++ni) {
        const WorldNode& nd = w.nodes[ni];
        if (nd.edges.size() < 3 || std::fabs(nd.x - X) > 400.0 || std::fabs(nd.y - Y) > 400.0) continue;
        for (int ei : nd.edges) {
            const WorldEdge& E = w.edges[ei];
            double dx, dy;
            const int st = lightState(w, ni, ei, M.envT, &dx, &dy);
            if (st < 0) continue;
            const double o = E.hw + 3.0;
            const double lx = nd.x + dx * o + dy * (E.hw + 1.6), ly = nd.y + dy * o - dx * (E.hw + 1.6);
            if (front(lx, ly, 400.0)) lights.push_back({lx, ly, st});
        }
    }
    for (int i = 0; i < (int)lights.size(); ++i) items.push_back({std::hypot(lights[i].x - ex, lights[i].y - ey), 6, i});
    for (int i = 0; i < (int)w.collect.size(); ++i)
        if (!M.app.career.gotCollect(w.collect[i].idx) && front(w.collect[i].x, w.collect[i].y, 260.0)) items.push_back({std::hypot(w.collect[i].x - ex, w.collect[i].y - ey), 4, i});
    for (int i = 0; i < (int)w.cameras.size(); ++i) if (front(w.cameras[i].x, w.cameras[i].y, 500.0)) items.push_back({std::hypot(w.cameras[i].x - ex, w.cameras[i].y - ey), 5, i});
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.d > b.d; });
    std::vector<std::pair<int, Mat4>> parked;                             // bulusma meydani araclari
    std::vector<Renderer::CarLook> parkedLook;
    for (const Item& it : items) {
        if (it.kind == 0) {
            const WorldBuilding& b = w.buildings[it.idx];
            const double ux = b.ux, uy = b.uy, vx = -uy, vy = ux;
            double cx[4], cy[4];
            const int sg[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
            for (int k = 0; k < 4; ++k) { cx[k] = b.cx + sg[k][0] * b.hu * ux + sg[k][1] * b.hv * vx; cy[k] = b.cy + sg[k][0] * b.hu * uy + sg[k][1] * b.hv * vy; }
            const Color base{0.55f + 0.3f * b.tone, 0.52f + 0.25f * hashW(it.idx * 3), 0.50f + 0.2f * hashW(it.idx * 5)};
            for (int k = 0; k < 4; ++k) {
                const int k2 = (k + 1) % 4;
                const double mx = 0.5 * (cx[k] + cx[k2]) - b.cx, my = 0.5 * (cy[k] + cy[k2]) - b.cy;   // disa normal
                if (mx * (ex - (b.cx + mx)) + my * (ey - (b.cy + my)) <= 0) continue;
                const Proj a0 = P3(cx[k], cy[k], 0), a1 = P3(cx[k2], cy[k2], 0), a2 = P3(cx[k2], cy[k2], b.h), a3 = P3(cx[k], cy[k], b.h);
                const float shade = (k % 2) ? 0.82f : 0.95f;
                quadP(r, a0, a1, a2, a3, fog({base.r * shade, base.g * shade, base.b * shade}, a0.w));
                if (a0.ok && a0.w < 260.0f)                                  // kat pencere bantlari
                    for (double z = 3.0; z < b.h - 1.0; z += 3.4) {
                        const Proj w0 = P3(cx[k], cy[k], z), w1 = P3(cx[k2], cy[k2], z), w2 = P3(cx[k2], cy[k2], z + 1.3), w3 = P3(cx[k], cy[k], z + 1.3);
                        quadP(r, w0, w1, w2, w3, fog({0.20f, 0.26f, 0.34f}, a0.w));
                    }
            }
            quadP(r, P3(cx[0], cy[0], b.h), P3(cx[1], cy[1], b.h), P3(cx[2], cy[2], b.h), P3(cx[3], cy[3], b.h), fog({base.r * 0.7f, base.g * 0.7f, base.b * 0.7f}, (float)it.d));
        } else if (it.kind == 4) {                                        // toplanabilir: donen altin kutu / eski ahir
            const WorldCollect& cl = w.collect[it.idx];
            if (cl.type == 2) continue;                                      // pert arac: arac olarak cizilir
            if (cl.type == 0 || cl.type == 3) {
                const bool sp = cl.type == 3;                                // ozel yapim: buyuk, mavi parlayan kasa
                const double a = M.envT * 2.0, hz2 = (sp ? 1.6 : 1.2) + 0.25 * std::sin(M.envT * 3.0), rr = sp ? 1.0 : 0.6;
                Proj c4[4];
                for (int k = 0; k < 4; ++k) c4[k] = P3(cl.x + rr * std::cos(a + k * 1.5708), cl.y + rr * std::sin(a + k * 1.5708), hz2);
                const Proj top = P3(cl.x, cl.y, hz2 + rr * 1.33), bot = P3(cl.x, cl.y, hz2 - rr * 1.33);
                const Color c1 = sp ? Color{0.3f, 0.85f, 1.0f} : Color{1.0f, 0.85f, 0.2f}, c2 = sp ? Color{0.1f, 0.55f, 0.9f} : Color{0.85f, 0.65f, 0.1f};
                for (int k = 0; k < 4; ++k) {
                    if (top.ok && c4[k].ok && c4[(k + 1) % 4].ok) triP(r, top, c4[k], c4[(k + 1) % 4], k % 2 ? c1 : c2);
                    if (bot.ok && c4[k].ok && c4[(k + 1) % 4].ok) triP(r, bot, c4[k], c4[(k + 1) % 4], k % 2 ? c2 : c1);
                }
            } else {
                const Proj b0 = P3(cl.x, cl.y, 0), b1 = P3(cl.x, cl.y, 6.0);
                if (b0.ok && b1.ok) { r.setDepthW(b0.w); const float sc = pxPerM / b0.w; r.rect(b0.x - 7 * sc, b1.y, b0.x + 7 * sc, b0.y, fog({0.45f, 0.28f, 0.16f}, b0.w));
                                      r.tri(b0.x - 8 * sc, b1.y, b0.x + 8 * sc, b1.y, b0.x, b1.y - 3 * sc, fog({0.35f, 0.2f, 0.12f}, b0.w));
                                      if (it.d < 120) r.textCentered(b0.x, b1.y - 3 * sc - 12, "TERK EDILMIS AHIR?", 1, {1.0f, 0.9f, 0.5f}); }
            }
        } else if (it.kind == 7 || it.kind == 8) {                        // site duvari: 1.8 m, ust baslikli
            const WorldBuilding& wb = w.buildings[it.idx];
            WallSeg ws[6]; const int nw = siteWalls(wb, it.idx, ws);
            const double bd = std::hypot(wb.cx - ex, wb.cy - ey);
            const float tn = hashW(it.idx * 29);
            const Color wc = tn < 0.4f ? Color{0.78f, 0.74f, 0.66f} : tn < 0.75f ? Color{0.62f, 0.38f, 0.30f} : Color{0.85f, 0.85f, 0.82f};
            for (int k = 0; k < nw; ++k) {
                const WallSeg& q = ws[k];
                if ((std::hypot(q.x - ex, q.y - ey) > bd) != (it.kind == 7)) continue;
                const double ux = std::cos(q.h), uy = std::sin(q.h), vx = -uy, vy = ux;
                double bx[4], by[4]; const int sg[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
                for (int c = 0; c < 4; ++c) { bx[c] = q.x + sg[c][0] * q.hl * ux + sg[c][1] * q.ht * vx; by[c] = q.y + sg[c][0] * q.hl * uy + sg[c][1] * q.ht * vy; }
                for (int c = 0; c < 4; ++c) {
                    const int c2 = (c + 1) % 4;
                    const double mx = 0.5 * (bx[c] + bx[c2]) - q.x, my = 0.5 * (by[c] + by[c2]) - q.y;
                    if (mx * (ex - (q.x + mx)) + my * (ey - (q.y + my)) <= 0) continue;
                    const float sh = (c % 2) ? 0.8f : 0.95f;
                    const Proj p0 = P3(bx[c], by[c], 0), p1 = P3(bx[c2], by[c2], 0), p2 = P3(bx[c2], by[c2], 1.8), p3 = P3(bx[c], by[c], 1.8);
                    quadP(r, p0, p1, p2, p3, fog({wc.r * sh, wc.g * sh, wc.b * sh}, (float)it.d));
                }
                quadP(r, P3(bx[0], by[0], 1.8), P3(bx[1], by[1], 1.8), P3(bx[2], by[2], 1.8), P3(bx[3], by[3], 1.8), fog({0.55f, 0.55f, 0.55f}, (float)it.d));
            }
        } else if (it.kind == 6) {                                        // trafik lambasi: direk + 3 isikli kafa
            const Light& lt = lights[it.idx];
            const Proj b0 = P3(lt.x, lt.y, 0), b1 = P3(lt.x, lt.y, 3.6);
            if (b0.ok && b1.ok) {
                r.setDepthW(b0.w); const float sc = pxPerM / b0.w;
                r.rect(b0.x - 0.07f * sc, b1.y, b0.x + 0.07f * sc, b0.y, fog({0.25f, 0.27f, 0.28f}, b0.w));
                r.rect(b1.x - 0.22f * sc, b1.y - 1.0f * sc, b1.x + 0.22f * sc, b1.y + 0.05f * sc, fog({0.1f, 0.1f, 0.11f}, b0.w));
                const Color on[3] = {{1.0f, 0.15f, 0.1f}, {1.0f, 0.75f, 0.1f}, {0.2f, 1.0f, 0.35f}};
                for (int k = 0; k < 3; ++k) {                               // ust kirmizi, orta sari, alt yesil
                    const bool lit = (k == 0 && lt.state == 2) || (k == 1 && lt.state == 1) || (k == 2 && lt.state == 0);
                    const Color c = lit ? on[k] : Color{on[k].r * 0.2f, on[k].g * 0.2f, on[k].b * 0.2f};
                    r.circle(b1.x, b1.y - (0.82f - 0.32f * k) * sc, std::max(1.0f, 0.12f * sc), 8, c);
                }
            }
        } else if (it.kind == 5) {                                        // hiz kamerasi: direk + kutu
            const WorldCamera& cm = w.cameras[it.idx];
            const double ox = -std::sin(cm.heading) * -11.0, oy = std::cos(cm.heading) * -11.0;
            const Proj b0 = P3(cm.x + ox, cm.y + oy, 0), b1 = P3(cm.x + ox, cm.y + oy, 4.5);
            if (b0.ok && b1.ok) { r.setDepthW(b0.w); const float sc = pxPerM / b0.w;
                                  r.rect(b0.x - 0.1f * sc, b1.y, b0.x + 0.1f * sc, b0.y, fog({0.5f, 0.5f, 0.52f}, b0.w));
                                  r.rect(b1.x - 0.5f * sc, b1.y - 0.6f * sc, b1.x + 0.5f * sc, b1.y + 0.2f * sc, fog({0.9f, 0.75f, 0.1f}, b0.w));
                                  r.circle(b1.x, b1.y - 0.2f * sc, std::max(1.0f, 0.18f * sc), 8, {0.1f, 0.1f, 0.12f}); }
        } else if (it.kind == 9) {                                        // ezilen yaya: yerde boylu boyunca
            const Impl::Down& dn = M.downs[it.idx];
            const double cx = std::cos(dn.h), cy = std::sin(dn.h), px = -cy, py = cx;
            static const Color shirtD[6] = {{0.85f, 0.2f, 0.2f}, {0.2f, 0.4f, 0.85f}, {0.95f, 0.85f, 0.3f}, {0.25f, 0.6f, 0.3f}, {0.9f, 0.9f, 0.9f}, {0.3f, 0.3f, 0.35f}};
            auto flat = [&](double a0, double a1, double hw2, Color c) {
                quadP(r, P3(dn.x + a0 * cx - hw2 * px, dn.y + a0 * cy - hw2 * py, 0.05), P3(dn.x + a1 * cx - hw2 * px, dn.y + a1 * cy - hw2 * py, 0.05),
                      P3(dn.x + a1 * cx + hw2 * px, dn.y + a1 * cy + hw2 * py, 0.05), P3(dn.x + a0 * cx + hw2 * px, dn.y + a0 * cy + hw2 * py, 0.05), fog(c, (float)it.d));
            };
            flat(-0.9, -0.05, 0.22, {0.18f, 0.2f, 0.3f});                 // bacaklar
            flat(-0.05, 0.6, 0.26, shirtD[(int)(dn.col * 6) % 6]);         // govde
            flat(0.62, 0.85, 0.12, {0.85f, 0.68f, 0.52f});                // bas
        } else if (it.kind == 10) {                                       // sokak lambasi / bahce agaci
            const Prop& pr = props[it.idx];
            if (pr.type == 2) {                                           // elektrik diregi: ahsap direk + travers
                const Proj b0 = P3(pr.x, pr.y, 0), b1 = P3(pr.x, pr.y, 9.0);
                if (!b0.ok || !b1.ok) continue;
                r.setDepthW(b0.w); const float sc = pxPerM / b0.w;
                r.rect(b0.x - 0.12f * sc, b1.y, b0.x + 0.12f * sc, b0.y, fog({0.36f, 0.27f, 0.18f}, b0.w));
                r.rect(b1.x - 1.2f * sc, b1.y + 0.3f * sc, b1.x + 1.2f * sc, b1.y + 0.5f * sc, fog({0.3f, 0.22f, 0.15f}, b0.w));
                continue;
            }
            if (pr.type == 3) {                                           // reklam panosu: iki ayak + renkli yuz + yazi
                const Proj b0 = P3(pr.x, pr.y, 0), b1 = P3(pr.x, pr.y, 7.0);
                if (!b0.ok || !b1.ok) continue;
                r.setDepthW(b0.w); const float sc = pxPerM / b0.w;
                r.rect(b0.x - 2.6f * sc, b1.y + 2.5f * sc, b0.x - 2.3f * sc, b0.y, fog({0.4f, 0.4f, 0.42f}, b0.w));
                r.rect(b0.x + 2.3f * sc, b1.y + 2.5f * sc, b0.x + 2.6f * sc, b0.y, fog({0.4f, 0.4f, 0.42f}, b0.w));
                static const Color bc[5] = {{0.9f, 0.2f, 0.15f}, {0.15f, 0.45f, 0.9f}, {0.95f, 0.75f, 0.1f}, {0.1f, 0.6f, 0.3f}, {0.95f, 0.95f, 0.95f}};
                const int k = (int)(pr.v * 5) % 5;
                r.rect(b0.x - 3.2f * sc, b1.y, b0.x + 3.2f * sc, b1.y + 2.6f * sc, fog(bc[k], b0.w));
                static const char* const ads[5] = {"ZEHRA KINIK", "TOFAZ SAHIN", "KARDESLER OTO", "ACIK DUNYA", "HURDACI CEMAL"};
                if (sc > 6.0f) r.textCentered(b0.x, b1.y + 1.0f * sc - 4, ads[k], sc > 14.0f ? 2 : 1, k == 4 ? Color{0.1f, 0.1f, 0.1f} : Color{1, 1, 1});
                continue;
            }
            if (pr.type == 0) {
                const Proj b0 = P3(pr.x, pr.y, 0), b1 = P3(pr.x, pr.y, 6.0);
                if (!b0.ok || !b1.ok) continue;
                r.setDepthW(b0.w); const float sc = pxPerM / b0.w;
                r.rect(b0.x - 0.07f * sc, b1.y, b0.x + 0.07f * sc, b0.y, fog({0.32f, 0.34f, 0.36f}, b0.w));
                r.rect(b1.x - 0.45f * sc, b1.y - 0.12f * sc, b1.x + 0.45f * sc, b1.y + 0.1f * sc, fog({0.25f, 0.26f, 0.28f}, b0.w));
                r.rect(b1.x - 0.3f * sc, b1.y + 0.1f * sc, b1.x + 0.3f * sc, b1.y + 0.18f * sc, fog({1.0f, 0.92f, 0.65f}, b0.w));
            } else {
                const double hgt = 4.5 + 3.0 * pr.v;
                const Proj b0 = P3(pr.x, pr.y, 0), b1 = P3(pr.x, pr.y, hgt);
                if (!b0.ok || !b1.ok) continue;
                r.setDepthW(b0.w); const float sc = pxPerM / b0.w;
                r.rect(b0.x - 0.15f * sc, b1.y + 1.2f * sc, b0.x + 0.15f * sc, b0.y, fog({0.4f, 0.28f, 0.18f}, b0.w));
                const Color leaf = fog({0.18f + 0.1f * pr.v, 0.45f + 0.12f * pr.v, 0.18f}, b0.w);
                r.circle(b1.x, b1.y + 0.6f * sc, std::max(1.5f, (1.6f + 0.6f * pr.v) * sc), 12, leaf);
                r.circle(b1.x - 0.8f * sc, b1.y + 1.2f * sc, std::max(1.0f, 1.1f * sc), 10, leaf);
                r.circle(b1.x + 0.8f * sc, b1.y + 1.1f * sc, std::max(1.0f, 1.1f * sc), 10, leaf);
            }
        } else if (it.kind == 3) {                                        // yaya: bacak + govde + kol + bas
            const Ped& pd = peds[it.idx];
            const Proj f0 = P3(pd.x, pd.y, 0.0), f1 = P3(pd.x, pd.y, 1.75);
            if (!f0.ok || !f1.ok) continue;
            r.setDepthW(f0.w);
            const float sc = pxPerM / f0.w, hgt = f0.y - f1.y, wdt = std::max(1.0f, 0.42f * sc);
            const float step = std::sin((float)M.envT * 7.0f + pd.col * 20.0f) * 0.12f * sc;
            static const Color shirt[6] = {{0.85f, 0.2f, 0.2f}, {0.2f, 0.4f, 0.85f}, {0.95f, 0.85f, 0.3f}, {0.25f, 0.6f, 0.3f}, {0.9f, 0.9f, 0.9f}, {0.3f, 0.3f, 0.35f}};
            const Color sh = fog(shirt[(int)(pd.col * 6) % 6], f0.w);
            r.rect(f0.x - wdt * 0.45f + step, f0.y - hgt * 0.47f, f0.x - wdt * 0.05f + step, f0.y, fog({0.18f, 0.2f, 0.3f}, f0.w));   // bacaklar
            r.rect(f0.x + wdt * 0.05f - step, f0.y - hgt * 0.47f, f0.x + wdt * 0.45f - step, f0.y, fog({0.18f, 0.2f, 0.3f}, f0.w));
            r.rect(f0.x - wdt * 0.5f, f0.y - hgt * 0.84f, f0.x + wdt * 0.5f, f0.y - hgt * 0.45f, sh);                                 // govde
            r.circle(f0.x, f0.y - hgt * 0.92f, std::max(1.0f, hgt * 0.09f), 8, fog({0.85f, 0.68f, 0.52f}, f0.w));                     // bas
        } else if (it.kind == 2) {                                        // simge yapi
            const WorldLandmark& l = w.landmarks[it.idx];
            const double ux = std::cos(l.heading), uy = std::sin(l.heading);
            // Prizma (silindir yaklasimi): kameraya bakan yuzler + ust kapak
            auto prism = [&](double x, double y, double rr, double z0, double z1, int sides, Color col) {
                for (int k = 0; k < sides; ++k) {
                    const double a0 = kTau * k / sides, a1 = kTau * (k + 1) / sides, am = 0.5 * (a0 + a1);
                    const double nx = std::cos(am), ny = std::sin(am);
                    if (nx * (ex - (x + nx * rr)) + ny * (ey - (y + ny * rr)) <= 0) continue;
                    const float sh = 0.75f + 0.25f * (float)std::fabs(std::cos(am - 0.6));
                    quadP(r, P3(x + rr * std::cos(a0), y + rr * std::sin(a0), z0), P3(x + rr * std::cos(a1), y + rr * std::sin(a1), z0),
                          P3(x + rr * std::cos(a1), y + rr * std::sin(a1), z1), P3(x + rr * std::cos(a0), y + rr * std::sin(a0), z1),
                          fog({col.r * sh, col.g * sh, col.b * sh}, (float)it.d));
                }
                if (z1 < ez) return;
                const Proj c0 = P3(x, y, z1);
                for (int k = 0; k < sides; ++k) {
                    const Proj a = P3(x + rr * std::cos(kTau * k / sides), y + rr * std::sin(kTau * k / sides), z1), b2 = P3(x + rr * std::cos(kTau * (k + 1) / sides), y + rr * std::sin(kTau * (k + 1) / sides), z1);
                    if (a.ok && b2.ok && c0.ok) triP(r, c0, a, b2, fog({col.r * 0.9f, col.g * 0.9f, col.b * 0.9f}, (float)it.d));
                }
            };
            auto cone = [&](double x, double y, double rr, double z0, double z1, int sides, Color col) {
                const Proj apex = P3(x, y, z1);
                for (int k = 0; k < sides; ++k) {
                    const double a0 = kTau * k / sides, a1 = kTau * (k + 1) / sides, am = 0.5 * (a0 + a1);
                    if (std::cos(am) * (ex - x) + std::sin(am) * (ey - y) <= 0) continue;
                    const float sh = 0.7f + 0.3f * (float)std::fabs(std::cos(am - 0.6));
                    const Proj a = P3(x + rr * std::cos(a0), y + rr * std::sin(a0), z0), b2 = P3(x + rr * std::cos(a1), y + rr * std::sin(a1), z0);
                    if (a.ok && b2.ok && apex.ok) triP(r, a, b2, apex, fog({col.r * sh, col.g * sh, col.b * sh}, (float)it.d));
                }
            };
            auto box = [&](double cxb, double cyb, double hu, double hv, double z0, double z1, Color col) {
                const double vx = -uy, vy = ux;
                double bx[4], by[4]; const int sg[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
                for (int k = 0; k < 4; ++k) { bx[k] = cxb + sg[k][0] * hu * ux + sg[k][1] * hv * vx; by[k] = cyb + sg[k][0] * hu * uy + sg[k][1] * hv * vy; }
                for (int k = 0; k < 4; ++k) {
                    const int k2 = (k + 1) % 4;
                    const double mx = 0.5 * (bx[k] + bx[k2]) - cxb, my = 0.5 * (by[k] + by[k2]) - cyb;
                    if (mx * (ex - (cxb + mx)) + my * (ey - (cyb + my)) <= 0) continue;
                    const float sh = (k % 2) ? 0.82f : 0.95f;
                    quadP(r, P3(bx[k], by[k], z0), P3(bx[k2], by[k2], z0), P3(bx[k2], by[k2], z1), P3(bx[k], by[k], z1), fog({col.r * sh, col.g * sh, col.b * sh}, (float)it.d));
                }
                quadP(r, P3(bx[0], by[0], z1), P3(bx[1], by[1], z1), P3(bx[2], by[2], z1), P3(bx[3], by[3], z1), fog({col.r * 0.75f, col.g * 0.75f, col.b * 0.75f}, (float)it.d));
            };
            auto dome = [&](double x, double y, double rr, double zb, Color col) {
                for (int k = 0; k < 5; ++k) {
                    const double a0 = 1.5708 * k / 5, a1 = 1.5708 * (k + 1) / 5;
                    prism(x, y, rr * std::cos(a0), zb + rr * std::sin(a0), zb + rr * std::sin(a1), 12, col);
                }
            };
            const Color stone{0.82f, 0.78f, 0.68f}, dark{0.32f, 0.32f, 0.36f};
            const double x = l.x, y = l.y;
            switch (l.type) {
            case LmTower: prism(x, y, l.r, 0, l.h * 0.72, 12, stone); prism(x, y, l.r * 1.15, l.h * 0.72, l.h * 0.78, 12, {0.55f, 0.5f, 0.45f}); cone(x, y, l.r, l.h * 0.78, l.h, 12, dark); break;
            case LmMaidenTower: prism(x, y, 16, -0.5, 2.5, 10, {0.5f, 0.48f, 0.44f}); box(x + 4, y, 7, 5, 2.5, 10, {0.92f, 0.9f, 0.85f});
                                prism(x - 3, y, 4, 2.5, l.h * 0.8, 8, {0.95f, 0.93f, 0.88f}); cone(x - 3, y, 4.6, l.h * 0.8, l.h, 8, dark); break;
            case LmPylon: box(x, y, 2.5, 4.5, 0, l.h, {0.75f, 0.75f, 0.78f}); box(x, y, 2.5, 4.5, l.h * 0.55, l.h * 0.6, {0.6f, 0.6f, 0.65f}); break;
            case LmMausoleum:
                box(x, y, l.r, l.r * 0.65, 0, 6, stone);
                for (int k = -5; k <= 5; ++k) for (double sv : {-1.0, 1.0}) prism(x + ux * k * l.r * 0.15 - uy * sv * l.r * 0.32, y + uy * k * l.r * 0.15 + ux * sv * l.r * 0.32, 1.2, 6, 18, 6, {0.93f, 0.9f, 0.82f});
                box(x, y, l.r * 0.85, l.r * 0.38, 18, l.h, stone); break;
            case LmTvTower: prism(x, y, 5, 0, l.h * 0.84, 10, {0.9f, 0.9f, 0.88f}); prism(x, y, 15, l.h * 0.84, l.h * 0.92, 14, {0.35f, 0.5f, 0.6f}); prism(x, y, 1, l.h * 0.92, l.h, 6, {0.8f, 0.2f, 0.2f}); break;
            case LmGreenDome: box(x, y, l.r, l.r, 0, 11, stone); prism(x, y, 8, 11, 15, 16, {0.2f, 0.65f, 0.55f}); cone(x, y, 8.5, 15, l.h, 16, {0.15f, 0.7f, 0.6f}); break;
            case LmMosque: {
                box(x, y, l.r * 0.5, l.r * 0.5, 0, l.h * 0.45, {0.9f, 0.88f, 0.82f});
                dome(x, y, l.r * 0.42, l.h * 0.45, {0.5f, 0.55f, 0.6f});
                for (double su2 : {-1.0, 1.0}) for (double sv2 : {-1.0, 1.0}) {
                    const double mx = x + ux * su2 * l.r * 0.55 - uy * sv2 * l.r * 0.55, my = y + uy * su2 * l.r * 0.55 + ux * sv2 * l.r * 0.55;
                    prism(mx, my, 1.8, 0, l.h * 1.4, 8, {0.95f, 0.94f, 0.9f}); cone(mx, my, 2.0, l.h * 1.4, l.h * 1.6, 8, {0.45f, 0.5f, 0.55f});
                }
                break;
            }
            case LmMountain: cone(x, y, l.r, 0, l.h, 28, {0.32f, 0.42f, 0.30f}); cone(x, y, l.r * 0.22, l.h * 0.78, l.h, 28, {0.95f, 0.95f, 0.97f}); break;
            case LmFairy: cone(x, y, l.r, 0, l.h, 10, {0.86f, 0.74f, 0.58f}); cone(x, y, l.r * 0.32, l.h * 0.92, l.h * 1.12, 8, {0.45f, 0.38f, 0.32f}); break;
            case LmBalloon: {
                const double bz = l.h + 8.0 * std::sin(M.envT * 0.3 + l.x * 0.01);
                const Proj b0 = P3(x, y, bz), bk = P3(x, y, bz - 14.0);
                if (b0.ok) {
                    r.setDepthW(b0.w);
                    const float rr = (float)(9.0 * pxPerM / b0.w);
                    static const Color bc[4] = {{0.9f, 0.25f, 0.2f}, {0.95f, 0.75f, 0.15f}, {0.25f, 0.45f, 0.85f}, {0.3f, 0.7f, 0.35f}};
                    const Color c0 = fog(bc[(int)(hashW((int)l.x) * 4) % 4], b0.w);
                    r.circle(b0.x, b0.y, rr, 16, c0);
                    r.circle(b0.x - rr * 0.25f, b0.y - rr * 0.3f, rr * 0.45f, 12, {std::min(1.0f, c0.r + 0.15f), std::min(1.0f, c0.g + 0.15f), std::min(1.0f, c0.b + 0.15f)});
                    if (bk.ok) r.rect(bk.x - rr * 0.18f, bk.y - rr * 0.15f, bk.x + rr * 0.18f, bk.y + rr * 0.15f, fog({0.45f, 0.3f, 0.15f}, b0.w));
                }
                break;
            }
            case LmClock: box(x, y, l.r, l.r, 0, l.h * 0.85, {0.85f, 0.8f, 0.7f}); cone(x, y, l.r * 1.3, l.h * 0.85, l.h, 4, {0.5f, 0.25f, 0.2f}); break;
            case LmCastle: box(x, y, l.r, l.r * 0.7, 0, l.h * 0.7, {0.62f, 0.55f, 0.45f});
                           for (double su2 : {-1.0, 1.0}) for (double sv2 : {-1.0, 1.0}) prism(x + ux * su2 * l.r - uy * sv2 * l.r * 0.7, y + uy * su2 * l.r + ux * sv2 * l.r * 0.7, 5, 0, l.h, 8, {0.58f, 0.5f, 0.42f}); break;
            case LmSkyscraper: box(x, y, l.r, l.r, 0, l.h, {0.35f, 0.5f, 0.65f}); box(x, y, l.r * 0.6, l.r * 0.6, l.h, l.h + 12, {0.75f, 0.75f, 0.8f}); break;
            case LmMinaret: prism(x, y, l.r, 0, l.h * 0.84, 12, {0.72f, 0.42f, 0.32f}); prism(x, y, l.r * 1.3, l.h * 0.84, l.h * 0.88, 12, {0.7f, 0.65f, 0.6f}); cone(x, y, l.r * 0.8, l.h * 0.88, l.h, 12, {0.35f, 0.38f, 0.42f}); break;
            case LmGate: for (int k = -2; k <= 2; k += 1) if (k % 2 == 0 || true) box(x + ux * k * l.r * 0.42, y + uy * k * l.r * 0.42, 2.0, 3.0, 0, l.h * 0.75, {0.85f, 0.78f, 0.62f});
                         box(x, y, l.r, 3.2, l.h * 0.75, l.h, {0.82f, 0.75f, 0.6f}); break;
            }
            if (it.d < 600.0 && l.type != LmFairy && l.type != LmBalloon) {   // ad
                const Proj t0 = P3(x, y, l.h + 6.0);
                if (t0.ok) { r.setDepthW(t0.w); r.textCentered(t0.x, t0.y - 8, l.name, 1, {1.0f, 0.95f, 0.75f}); }
            }
        } else {
            const WorldPoi& q = w.pois[it.idx];
            const Proj b0 = P3(q.x, q.y, 0.0);
            if (!b0.ok) continue;
            const float sc = pxPerM / b0.w;
            r.setDepthW(b0.w);
            static const Color kc[5] = {{0.95f, 0.45f, 0.08f}, {0.75f, 0.15f, 0.55f}, {0.45f, 0.30f, 0.15f}, {0.85f, 0.15f, 0.12f}, {0.15f, 0.55f, 0.3f}};
            std::string label = q.name;
            if (q.type == WPoiGas) {                                          // benzinlik: sacak + pompalar + fiyat
                char pb[48]; std::snprintf(pb, sizeof pb, "BENZIN %.2f TL", w.fuelPriceAt(q.city, M.app.career.marketWeek() < 0 ? 0 : M.app.career.marketWeek()));
                label = pb;
                const double ux = std::cos(q.heading), uy = std::sin(q.heading), vx = -uy, vy = ux;
                auto Q = [&](double u, double v, double h) { return P3(q.x + u * ux + v * vx, q.y + u * uy + v * vy, h); };
                quadP(r, Q(-14, -10, 0.05), Q(14, -10, 0.05), Q(14, 10, 0.05), Q(-14, 10, 0.05), fog({0.70f, 0.70f, 0.68f}, b0.w));
                for (int k = -1; k <= 1; ++k) {
                    const Proj p0 = Q(k * 7.0, 0, 0), p1 = Q(k * 7.0, 0, 1.8);
                    if (p0.ok && p1.ok) { const float pw = std::max(1.0f, 0.4f * pxPerM / p0.w); r.rect(p0.x - pw, p1.y, p0.x + pw, p0.y, fog({0.92f, 0.92f, 0.92f}, p0.w)); }
                }
                quadP(r, Q(-12, -6, 5.2), Q(12, -6, 5.2), Q(12, 6, 5.2), Q(-12, 6, 5.2), fog({0.92f, 0.92f, 0.94f}, b0.w));
                quadP(r, Q(-12, -6, 4.6), Q(12, -6, 4.6), Q(12, -6, 5.2), Q(-12, -6, 5.2), fog({0.85f, 0.15f, 0.12f}, b0.w));
            }
            if (q.type == WPoiMeet) {                                         // bulusma: modifiyeli park eden araclar
                const double ux = std::cos(q.heading), uy = std::sin(q.heading), vx = -uy, vy = ux;
                auto Q = [&](double u, double v, double h) { return P3(q.x + u * ux + v * vx, q.y + u * uy + v * vy, h); };
                quadP(r, Q(-40, -40, 0.04), Q(40, -40, 0.04), Q(40, 40, 0.04), Q(-40, 40, 0.04), fog({0.22f, 0.22f, 0.24f}, b0.w));
                if (it.d < 400.0)
                    for (int k = 0; k < 8; ++k) {
                        static const int kMeet[12] = {2, 5, 12, 24, 34, 50, 59, 78, 88, 100, 122, 152};
                        const int id = kMeet[(k * 5 + q.city * 3) % 12];
                        const double u = -24.0 + (k % 4) * 16.0, v = k < 4 ? -12.0 : 12.0, hdg = q.heading + (k < 4 ? 1.5708 : -1.5708);
                        const double px = q.x + u * ux + v * vx, py = q.y + u * uy + v * vy;
                        parked.push_back({id, matMul(matTranslate((float)px, 0.0f, (float)-py), matRotY((float)hdg))});
                        Renderer::CarLook L; L.paintOn = true;
                        const float hh = hashW(q.city * 31 + k);
                        L.paint[0] = 0.2f + 0.8f * hashW(k * 7 + q.city); L.paint[1] = 0.1f + 0.7f * hh; L.paint[2] = 0.2f + 0.8f * hashW(k * 13);
                        L.stripe = k % 3; L.stripeCol[0] = 1.0f; L.stripeCol[1] = 0.85f; L.stripeCol[2] = 0.2f;
                        L.aero = (k % 2) ? 5 : 3; L.drop = 0.05f; L.aeroFront = true; L.aeroSide = k % 2 == 0;
                        parkedLook.push_back(L);
                    }
            }
            if (q.type == WPoiJunk) {                                         // hurdalik: yigin kutulari
                for (int k = 0; k < 6; ++k) {
                    const Proj c0 = P3(q.x + 8.0 * std::cos(k * 1.1), q.y + 8.0 * std::sin(k * 1.1), 0.0);
                    if (c0.ok) { const float s2 = pxPerM / c0.w; r.rect(c0.x - 1.6f * s2, c0.y - 1.4f * s2, c0.x + 1.6f * s2, c0.y, fog({0.42f, 0.30f, 0.22f}, c0.w)); }
                }
            }
            const Proj t0 = P3(q.x, q.y, q.type == WPoiRace ? 4.5 : 7.5);
            if (!t0.ok) continue;
            r.rect(b0.x - 0.12f * sc, t0.y, b0.x + 0.12f * sc, b0.y, fog({0.55f, 0.55f, 0.58f}, b0.w));
            r.rect(t0.x - 2.6f * sc, t0.y - 1.5f * sc, t0.x + 2.6f * sc, t0.y, fog(kc[q.type], b0.w));
            if (q.type == WPoiRace)
                for (int k = 0; k < 13; ++k) r.rect(t0.x - 2.6f * sc + k * 0.4f * sc, t0.y - 0.2f * sc, t0.x - 2.2f * sc + k * 0.4f * sc, t0.y, (k & 1) ? Color{0, 0, 0} : Color{1, 1, 1});
            const float ts = std::min(3.0f, std::floor(sc * 0.045f));
            if (ts >= 1.0f) r.textFit(t0.x, t0.y - 1.15f * sc, label, ts, 5.0f * sc, {1, 1, 1}, true);
        }
    }
    r.flush2D();
    // Araclar: trafik (polis / serseri gorunumu), bulusma araclari, oyuncu
    struct Obj { double d; int id; Mat4 model; int role; float spin; };
    std::vector<Obj> objs;
    for (const auto& t : M.traffic) {
        double x, y, th; M.tPose(t, x, y, th);
        struct { double heading; } q{th};
        if (!front(x, y, 700.0)) continue;
        objs.push_back({std::hypot(x - ex, y - ey), t.carId, matMul(matTranslate((float)x, 0.0f, (float)-y), matRotY((float)q.heading)), t.role, (float)(M.envT * t.v / 0.31)});
    }
    for (const WorldCollect& cl : w.collect) {                          // pert araclar: yana yatik, hasarli gorunum
        if (cl.type != 2 || M.app.career.gotCollect(cl.idx) || !front(cl.x, cl.y, 350.0)) continue;
        static const int kTuners[14] = {2, 5, 8, 12, 24, 34, 50, 53, 59, 78, 88, 100, 102, 152};
        const int id = kTuners[(unsigned)(cl.ref * 7 + 3) % 14];
        objs.push_back({std::hypot(cl.x - ex, cl.y - ey), id, matMul(matTranslate((float)cl.x, 0.0f, (float)-cl.y), matRotY((float)cl.heading)), 3, 0.0f});
    }
    {   // park etmis araclar (yakindakiler)
        std::vector<Parked> pk;
        for (int e = 0; e < (int)w.edges.size(); ++e) {
            const WorldEdge& E = w.edges[e];
            if (ex < E.minX - 400 || ex > E.maxX + 400 || ey < E.minY - 400 || ey > E.maxY + 400) continue;
            parkedOn(w, e, pk);
        }
        for (const Parked& q : pk) {
            if (!front(q.x, q.y, 400.0)) continue;
            objs.push_back({std::hypot(q.x - ex, q.y - ey), q.carId, matMul(matTranslate((float)q.x, 0.0f, (float)-q.y), matRotY((float)q.h)), 0, 0.0f});
        }
    }
    std::sort(objs.begin(), objs.end(), [](const Obj& a, const Obj& b) { return a.d > b.d; });
    for (size_t k = 0; k < parked.size(); ++k) { r.setCarLook(parkedLook[k]); r.drawCar(parked[k].first, 0, 0, W, H, proj, view, parked[k].second); }
    for (const Obj& o : objs) {
        if (o.d < 3.0) continue;
        if (o.role == 1) {
            Renderer::CarLook pl; pl.paintOn = true; pl.paint[0] = pl.paint[1] = pl.paint[2] = 0.93f;
            pl.stripe = 3; pl.stripeCol[0] = 0.08f; pl.stripeCol[1] = 0.12f; pl.stripeCol[2] = 0.45f;
            r.setCarLook(pl);
        } else if (o.role == 3) {                                         // pert: ezik, koyu, serit
            Renderer::CarLook wl; wl.paintOn = true; wl.paint[0] = 0.25f; wl.paint[1] = 0.22f; wl.paint[2] = 0.22f;
            wl.stripe = 2; wl.stripeCol[0] = 0.9f; wl.stripeCol[1] = 0.3f; wl.stripeCol[2] = 0.1f; wl.aero = 5; wl.drop = 0.06f; wl.roll = 0.12f;
            r.setCarLook(wl);
        } else if (o.role == 2) {
            Renderer::CarLook hl; hl.paintOn = true; hl.paint[0] = 0.08f; hl.paint[1] = 0.08f; hl.paint[2] = 0.10f;
            hl.stripe = 2; hl.stripeCol[0] = 0.95f; hl.stripeCol[1] = 0.55f; hl.stripeCol[2] = 0.05f; hl.aero = 5; hl.drop = 0.04f;
            r.setCarLook(hl);
        }
        r.drawCar(o.id, 0, 0, W, H, proj, view, o.model, o.spin);
    }
    {   // oyuncu
        Renderer::CarLook L = lookOf(M.app.career.car());
        r.setCarLook(L);
        const Mat4 pm = matMul(matTranslate((float)X, (float)M.curbH, (float)-Y), matRotY((float)sim.heading()));
        r.drawCar(M.carId, 0, 0, W, H, proj, view, pm, (float)M.spin, (float)M.steer);
    }
    r.endWorldDepth();
    // ---- HUD ----
    char b[96];
    r.rect(0, 0, W, 30, {0.05f, 0.06f, 0.09f, 0.82f});
    std::snprintf(b, sizeof b, "%3.0f KM/H", sim.speed() * 3.6);
    r.text(8, 7, b, 2, {1, 1, 1});
    const int g = sim.powertrain().gear();
    r.text(122, 7, g == 0 ? "N" : std::to_string(g), 2, {1.0f, 0.62f, 0.05f});
    std::snprintf(b, sizeof b, "%s  YAKIT %.1f/%.0f L", M.city >= 0 ? w.cities[M.city].name.c_str() : "OTOBAN", sim.fuelLiters(), sim.tankLiters());
    r.text(150, 4, b, 1, sim.fuelLiters() < 5.0 ? Color{1.0f, 0.35f, 0.3f} : Color{0.75f, 0.85f, 1.0f});
    std::snprintf(b, sizeof b, "SINIR %.0f  %s", M.speedLimit() * 3.6, money(M.app.career.money).c_str());
    r.text(150, 16, b, 1, {0.8f, 0.8f, 0.85f});
    {   // kadran: satin alinan (yoksa temel analog); yatayda alt orta, dikeyde pedallarin ustu
        GaugeData gd;
        const auto& pt = sim.powertrain();
        gd.vtec = pt.vtecActive(); gd.rpm = (float)pt.rpm(); gd.redline = (float)sim.engineSpec().redlineRpm; gd.shiftRpm = (float)sim.shiftRpm();
        gd.speed = (float)(sim.speed() * 3.6); gd.speedMax = 260.0f;
        gd.gear = g == 0 ? "N" : std::to_string(g); gd.t = M.gaugeT += 1.0 / 60.0;
        gd.boost = (float)sim.boostNow(); gd.boostMax = (float)sim.boostMax();
        const int gs = std::max(1, M.app.career.car().gauge);
        if (M.land) drawGaugeCluster(r, W * 0.5f - 160, H - 98, W * 0.5f + 64, H - 2, gs, M.app.career.car().boostGauge, gd);
        else drawGaugeCluster(r, 40, H - 290, 320, H - 206, gs, M.app.career.car().boostGauge, gd);
    }
    button(r, M.mapBtn, "HARITA", {0.15f, 0.35f, 0.6f}, 1);
    button(r, M.jobBtn, M.hasJob ? "IS VAR" : "ISLER", M.hasJob ? Color{0.55f, 0.4f, 0.1f} : Color{0.3f, 0.3f, 0.36f}, 1);
    if (M.hasJob) {
        std::snprintf(b, sizeof b, "%s  %.0f S  %s", M.job.type == 1 && M.job.phase == 0 ? "YOLCUYU AL" : M.job.name.c_str(), M.job.deadline, money((long)M.job.reward).c_str());
        r.textFit(W * 0.42f, 60, b, 1, W * 0.8f, {1.0f, 0.85f, 0.3f}, true);
    }
    if (M.limT > 0.05) {                                                  // kesicide tut: kapisma
        r.rect(W * 0.3f, H * 0.42f, W * 0.7f, H * 0.42f + 16, {0, 0, 0, 0.6f});
        r.rect(W * 0.3f, H * 0.42f, W * 0.3f + W * 0.4f * (float)std::min(1.0, M.limT / 5.0), H * 0.42f + 16, {1.0f, 0.35f, 0.1f});
        r.textCentered(W * 0.5f, H * 0.42f + 4, "KESICIDE TUT: KAPISMA", 1, {1, 1, 1});
    }
    if (M.radarBlink > 0 && std::fmod(M.envT, 0.5) < 0.3) {
        std::snprintf(b, sizeof b, "!! RADAR %.0f M", 450.0 - M.radarBlink);
        r.rect(W * 0.35f, 74, W * 0.65f, 94, {0.8f, 0.1f, 0.1f, 0.85f}); r.textCentered(W * 0.5f, 79, b, 1, {1, 1, 1});
    }
    button(r, M.autoBtn, M.autoDrive ? "OTONOM ACIK" : "OTONOM", M.autoDrive ? Color{0.15f, 0.55f, 0.25f} : Color{0.25f, 0.27f, 0.32f}, 1);
    if (!M.route.empty() && M.routeIdx + 1 < M.route.size()) {             // sonraki donus
        const RoadPath& cp = M.path(); const RoadPath& np = w.path(M.route[M.routeIdx + 1]);
        const double turn = std::remainder(np.at(2.0).heading - cp.at(cp.length()).heading, kTau);
        std::snprintf(b, sizeof b, "%s  %.0f M", std::fabs(turn) < 0.5 ? "DUZ DEVAM" : turn > 0 ? "SOLA DON" : "SAGA DON", std::max(0.0, cp.length() - P.s()));
        r.textCentered(W * 0.42f, 44, b, 2, {0.3f, 0.8f, 1.0f});
    } else if (M.hasWp) {
        std::snprintf(b, sizeof b, "HEDEF %.0f M", std::hypot(M.wpX - X, M.wpY - Y));
        r.textCentered(W * 0.42f, 44, b, 2, {0.3f, 0.8f, 1.0f});
    }
    const int ph = M.poiHere();
    if (ph >= 0) {
        const WorldPoi& q = w.pois[ph];
        std::string lab = q.name;
        if (q.type == WPoiGas) { std::snprintf(b, sizeof b, "DEPOYU DOLDUR: %.0f L x %.2f = %s", sim.tankLiters() - sim.fuelLiters(), M.fuelPrice(),
                                               money((long)((sim.tankLiters() - sim.fuelLiters()) * M.fuelPrice())).c_str()); lab = b; }
        M.poiBtn = {W * 0.42f - 140, H - (M.land ? 86.0f : 140.0f), W * 0.42f + 140, H - (M.land ? 54.0f : 104.0f)};
        button(r, M.poiBtn, std::string("GIR: ") + lab, {0.85f, 0.45f, 0.08f}, 1);
        if (q.type == WPoiGas && !M.app.career.radarDetector) {
            M.buyBtn = {M.poiBtn.x0, M.poiBtn.y0 - 36, M.poiBtn.x1, M.poiBtn.y0 - 4};
            button(r, M.buyBtn, "MARKET: RADAR DEDEKTORU $1,500", {0.25f, 0.3f, 0.5f}, 1);
        } else M.buyBtn = {0, 0, 0, 0};
    }
    if (M.clapT >= 0.0) {                                                 // alkis: iki el carpar, kalabalik yazisi
        const float k = (float)std::min(1.0, std::min(M.clapT / 0.2, (3.5 - M.clapT) / 0.5));
        const float cxh = W * 0.5f, cyh = H * 0.5f - 40, gap = 6.0f + 22.0f * (float)std::fabs(std::sin(M.clapT * 9.0));
        const Color skin{0.95f, 0.78f, 0.6f, k};
        r.rect(cxh - gap - 26, cyh - 34, cxh - gap, cyh + 30, skin); r.rect(cxh + gap, cyh - 34, cxh + gap + 26, cyh + 30, skin);
        for (int f = 0; f < 4; ++f) { r.rect(cxh - gap - 26 + f * 6.5f, cyh - 46, cxh - gap - 21 + f * 6.5f, cyh - 34, skin); r.rect(cxh + gap + f * 6.5f, cyh - 46, cxh + gap + 5 + f * 6.5f, cyh - 34, skin); }
        if (gap < 10.0f) r.textCentered(cxh, cyh - 70, "* SAK *", 2, {1.0f, 1.0f, 1.0f, k});
        r.textCentered(cxh, cyh + 42, "ALKIS! ALKIS! ALKIS!", 2, {1.0f, 0.85f, 0.2f, k});
    }
    if (M.msgT > 0) { r.rect(0, H * 0.3f, W, H * 0.3f + 26, {0.02f, 0.02f, 0.04f, 0.75f}); r.textCentered(W / 2.0f, H * 0.3f + 7, M.msg, 2, kUiGold); }
    M.cockpit.render(r, g, P.grinding());
    if (M.jobsOpen) {                                                     // is teklifleri
        r.rect(0, 0, W, H, {0.05f, 0.06f, 0.08f, 0.95f});
        r.textCentered(W * 0.5f, 24, "ISLER: KURYE / TAKSI", 2, kUiGold);
        for (size_t k = 0; k < M.offers.size() && k < 3; ++k) {
            const auto& j = M.offers[k];
            r.rect(M.jobRows[k].x0, M.jobRows[k].y0, M.jobRows[k].x1, M.jobRows[k].y1, {0.14f, 0.16f, 0.22f});
            r.text(M.jobRows[k].x0 + 10, M.jobRows[k].y0 + 8, j.name, 1, {1, 1, 1});
            std::snprintf(b, sizeof b, "SURE %.0f S   ODUL %s   %.1f KM", j.deadline, money((long)j.reward).c_str(),
                          (std::hypot(j.tx - X, j.ty - Y) + (j.type ? std::hypot(j.dx - j.tx, j.dy - j.ty) : 0.0)) / 1000.0);
            r.text(M.jobRows[k].x0 + 10, M.jobRows[k].y0 + 28, b, 1, kUiGold);
        }
        button(r, M.jobClose, "KAPAT", kUiBtn, 2);
    }
    // ---- Harita ----
    if (M.mapOpen) {
        r.rect(0, 0, W, H, {0.06f, 0.09f, 0.08f, 0.97f});
        auto S = [&](double x, double y, float& sx, float& sy) { sx = (float)(W * 0.5 + (x - M.mapCx) / M.mapScale); sy = (float)(H * 0.5 - (y - M.mapCy) / M.mapScale); };
        auto seg = [&](double x0, double y0, double x1, double y1, float wd, Color col) {
            float a, bb, c2, d; S(x0, y0, a, bb); S(x1, y1, c2, d);
            const float dx = c2 - a, dy = d - bb, l = std::sqrt(dx * dx + dy * dy) + 1e-3f, nx = -dy / l * wd, ny = dx / l * wd;
            r.tri(a + nx, bb + ny, c2 + nx, d + ny, c2 - nx, d - ny, col); r.tri(a + nx, bb + ny, c2 - nx, d - ny, a - nx, bb - ny, col);
        };
        {   // sehir sekilleri ve su (kaba hucreler, gorunen bolge)
            const double cell = std::max(40.0, M.mapScale * 5.0);
            const double x0 = M.mapCx - W * 0.5 * M.mapScale, x1 = M.mapCx + W * 0.5 * M.mapScale, y0 = M.mapCy - H * 0.5 * M.mapScale, y1 = M.mapCy + H * 0.5 * M.mapScale;
            for (const WorldCity& c : w.cities) {
                const double ext = c.hu + c.hv + 400.0;
                if (c.x + ext < x0 || c.x - ext > x1 || c.y + ext < y0 || c.y - ext > y1) continue;
                for (double gx = std::max(x0, c.x - ext); gx < std::min(x1, c.x + ext); gx += cell)
                    for (double gy = std::max(y0, c.y - ext); gy < std::min(y1, c.y + ext); gy += cell) {
                        const int sf = w.surfaceAt(gx + cell * 0.5, gy + cell * 0.5);
                        if (sf == 2) continue;
                        float sx, sy; S(gx, gy + cell, sx, sy);
                        const float cs = (float)(cell / M.mapScale) + 0.5f;
                        r.rect(sx, sy, sx + cs, sy + cs, sf == 1 ? Color{0.12f, 0.28f, 0.48f} : Color{0.20f, 0.21f, 0.23f});
                    }
            }
        }
        for (const WorldEdge& E : w.edges) {
            const auto& pts = E.fwd().points();
            const int st = std::max(1, (int)(M.mapScale * 2.0 / RoadPath::kStep));
            for (size_t i = 0; i + st < pts.size(); i += st) seg(pts[i].x, pts[i].y, pts[i + st].x, pts[i + st].y, E.highway ? 1.6f : 0.8f, E.highway ? Color{0.95f, 0.75f, 0.3f} : Color{0.75f, 0.75f, 0.75f});
            seg(pts[pts.size() - 1 - (pts.size() - 1) % st].x, pts[pts.size() - 1 - (pts.size() - 1) % st].y, pts.back().x, pts.back().y, E.highway ? 1.6f : 0.8f, E.highway ? Color{0.95f, 0.75f, 0.3f} : Color{0.75f, 0.75f, 0.75f});
        }
        for (size_t k = M.routeIdx; k < M.route.size(); ++k) {
            const auto& pts = w.path(M.route[k]).points();
            for (size_t i = 0; i + 4 < pts.size(); i += 4) seg(pts[i].x, pts[i].y, pts[i + 4].x, pts[i + 4].y, 2.2f, {0.2f, 0.7f, 1.0f});
        }
        static const Color pc[5] = {{1.0f, 0.5f, 0.1f}, {0.9f, 0.3f, 0.7f}, {0.6f, 0.45f, 0.25f}, {0.9f, 0.2f, 0.15f}, {0.2f, 0.85f, 0.4f}};
        for (size_t k = 0; k < w.cameras.size(); ++k) { if (!M.app.career.discovered(512 + (int)k)) continue; const WorldCamera& cm = w.cameras[k]; float sx, sy; S(cm.x, cm.y, sx, sy); r.rect(sx - 2, sy - 2, sx + 2, sy + 2, {1, 1, 1}); }
        for (size_t k = 0; k < w.pois.size(); ++k) {                     // yarislar hep; digerleri kesfedilince
            const WorldPoi& q = w.pois[k];
            if (q.type != WPoiRace && !M.app.career.discovered((int)k)) continue;
            float sx, sy; S(q.x, q.y, sx, sy); r.circle(sx, sy, 4.0f, 10, pc[q.type]);
        }
        for (const WorldCity& c : w.cities) { float sx, sy; S(c.x, c.y, sx, sy); r.textCentered(sx, sy - (float)(c.hv / M.mapScale) - 14, c.name, 1, {1, 1, 1}); }
        if (M.mapScale < 25.0)
            for (size_t li = 0; li < w.landmarks.size(); ++li) {
                const WorldLandmark& l = w.landmarks[li];
                if (l.type == LmFairy || l.type == LmBalloon || l.type == LmPylon || !M.app.career.discovered(768 + (int)li)) continue;
                float sx, sy; S(l.x, l.y, sx, sy);
                if (sx < 0 || sx > W || sy < 0 || sy > H) continue;
                r.rect(sx - 3, sy - 3, sx + 3, sy + 3, {1.0f, 0.9f, 0.5f});
                r.textCentered(sx, sy + 5, l.name, 1, {1.0f, 0.9f, 0.6f});
            }
        if (M.hasWp) { float sx, sy; S(M.wpX, M.wpY, sx, sy); r.circle(sx, sy, 7, 14, {0.2f, 0.8f, 1.0f}); r.circle(sx, sy, 3, 10, {1, 1, 1}); }
        {   float sx, sy; S(X, Y, sx, sy);
            const float h = (float)sim.heading(), c2 = std::cos(h), s2 = -std::sin(h);
            r.tri(sx + c2 * 9, sy + s2 * 9, sx - c2 * 6 - s2 * 5, sy - s2 * 6 + c2 * 5, sx - c2 * 6 + s2 * 5, sy - s2 * 6 - c2 * 5, {0.2f, 1.0f, 0.4f}); }
        r.text(8, 8, "HARITA: DOKUN = HEDEF, SURUKLE = KAYDIR", 1, {0.9f, 0.9f, 0.9f});
        r.text(8, 20, "TURUNCU YARIS  PEMBE BULUSMA  KAHVE HURDALIK  KIRMIZI BENZIN  YESIL GARAJ  BEYAZ RADAR", 1, {0.7f, 0.7f, 0.7f});
        { const int n = M.app.career.collectedCount(); char cb[48]; std::snprintf(cb, sizeof cb, "TOPLANAN %d / %d", n, (int)w.collect.size()); r.text(8, 32, cb, 1, kUiGold); }
        button(r, M.mPlus, "+", kUiBtn, 3); button(r, M.mMinus, "-", kUiBtn, 3);
        button(r, M.mClose, "KAPAT", kUiBtn, 2); button(r, M.mClear, "HEDEF SIL", {0.45f, 0.2f, 0.15f}, 1);
    }
    r.flush2D();
}

void WorldScreen::pointerDown(int id, float x, float y) {
    Impl& M = *m_;
    App& app = M.app;
    if (M.mapOpen) {
        if (M.mClose.hit(x, y)) { M.mapOpen = false; return; }
        if (M.mPlus.hit(x, y)) { M.mapScale = std::max(2.0, M.mapScale / 1.6); return; }
        if (M.mMinus.hit(x, y)) { M.mapScale = std::min(200.0, M.mapScale * 1.6); return; }
        if (M.mClear.hit(x, y)) { M.hasWp = false; M.route.clear(); return; }
        M.dragX = x; M.dragY = y; M.dragId = id;
        return;
    }
    if (M.jobsOpen) {
        if (M.jobClose.hit(x, y)) { M.jobsOpen = false; return; }
        for (size_t k = 0; k < M.offers.size() && k < 3; ++k)
            if (M.jobRows[k].hit(x, y)) {
                M.job = M.offers[k]; M.hasJob = true; M.jobsOpen = false;
                M.startJobTarget(M.job.tx, M.job.ty);
                M.flash(M.job.type == 1 ? "TAKSI: ONCE YOLCUYU AL (HARITADA MAVI)" : "KURYE: PAKETI TESLIM ET (HARITADA MAVI)", 2.2);
                return;
            }
        return;
    }
    if (M.jobBtn.hit(x, y)) {
        if (M.hasJob) { M.hasJob = false; M.hasWp = false; M.route.clear(); M.flash("IS BIRAKILDI", 1.2); }
        else { M.makeOffers(); M.jobsOpen = true; }
        return;
    }
    if (M.buyBtn.x1 > 0 && M.buyBtn.hit(x, y)) {
        if (app.career.money < 1500) { M.flash("PARA YETMIYOR", 1.4); return; }
        app.career.money -= 1500; app.career.radarDetector = true; app.saveCareer();
        M.flash("RADAR DEDEKTORU TAKILDI: KAMERALARI 450 M ONCEDEN UYARIR", 2.4);
        return;
    }
    if (M.mapBtn.hit(x, y)) { M.mapOpen = true; M.mapCx = M.car->sim().posX(); M.mapCy = M.car->sim().posY(); return; }
    if (M.autoBtn.hit(x, y)) {
        if (M.adasLevel < 4 && !std::getenv("ZK_ADAS")) { M.flash("BU ARACTA OTONOM YOK (MODIFIYE > ECU > SURUS YARDIMI)", 2.2); return; }
        M.autoDrive = !M.autoDrive;
        if (M.autoDrive && M.hasWp) { M.route = M.w.route(M.leg, M.car->s(), M.wpX, M.wpY); M.routeIdx = 0; }
        M.flash(M.autoDrive ? (M.hasWp ? "OTONOM: ROTAYA GIDIYOR" : "OTONOM: SERITTE") : "OTONOM KAPANDI", 1.5);
        return;
    }
    const int ph = M.poiHere();
    if (ph >= 0 && M.poiBtn.hit(x, y)) {
        const WorldPoi& q = M.w.pois[ph];
        VehicleSim& sim = M.car->sim();
        auto leave = [&]() {
            app.worldValid = true; app.worldX = sim.posX(); app.worldY = sim.posY(); app.worldH = sim.heading();
            app.career.car().fuelL = sim.fuelLiters();
            app.worldReturn = true;
        };
        std::string why;
        switch (q.type) {
        case WPoiGas: {
            double L = sim.tankLiters() - sim.fuelLiters();
            const double price = M.fuelPrice();
            L = std::min(L, app.career.money / price);
            if (L < 0.5) { M.flash(app.career.money < price ? "PARA YETMIYOR" : "DEPO DOLU", 1.5); return; }
            sim.addFuel(L); app.career.money -= (long)std::lround(L * price);
            app.career.car().fuelL = sim.fuelLiters(); app.saveCareer();
            char b[64]; std::snprintf(b, sizeof b, "%.1f L ALINDI: %s", L, money((long)std::lround(L * price)).c_str());
            M.flash(b, 2.0);
            return;
        }
        case WPoiRace:
            if (!app.career.eventAvailable(q.ref, &why)) { M.flash(why, 1.8); return; }
            leave(); app.startEvent(q.ref); return;
        case WPoiMeet: leave(); app.goStreet(); return;
        case WPoiJunk: leave(); app.goJunkyard(); return;
        case WPoiGarage: leave(); app.goGarageDirect(); return;           // garajdan SERBEST YOL ile buraya donulur
        }
        return;
    }
    if (M.cockpit.pointerDown(id, x, y)) return;
}

void WorldScreen::pointerMove(int id, float x, float y) {
    Impl& M = *m_;
    if (M.mapOpen && id == M.dragId) {
        M.mapCx -= (x - M.dragX) * M.mapScale; M.mapCy += (y - M.dragY) * M.mapScale;
        if (std::fabs(x - M.dragX) + std::fabs(y - M.dragY) > 0) M.dragId = -2 - id;   // suruklendi: dokunus waypoint degil
        M.dragX = x; M.dragY = y;
        return;
    }
    if (M.mapOpen && M.dragId == -2 - id) { M.mapCx -= (x - M.dragX) * M.mapScale; M.mapCy += (y - M.dragY) * M.mapScale; M.dragX = x; M.dragY = y; return; }
    M.cockpit.pointerMove(id, x, y);
}

void WorldScreen::pointerUp(int id) {
    Impl& M = *m_;
    if (M.mapOpen) {
        if (M.dragId == id) {                                              // surukleme yok: hedef koy
            const double wx = M.mapCx + (M.dragX - M.W * 0.5) * M.mapScale, wy = M.mapCy - (M.dragY - M.H * 0.5) * M.mapScale;
            WorldLeg l; double s, lat;
            if (M.w.nearestLeg(wx, wy, 0.0, l, s, lat, 60.0 * M.mapScale)) {
                const RoadPoint q = M.w.path(l).at(s);
                M.hasWp = true; M.wpX = q.x; M.wpY = q.y;
                M.route = M.w.route(M.leg, M.car->s(), M.wpX, M.wpY); M.routeIdx = 0;
                double len = 0; for (const auto& g : M.route) len += M.w.path(g).length();
                char b[64]; std::snprintf(b, sizeof b, "ROTA: %.1f KM", len / 1000.0);
                M.flash(b, 2.0);
            }
        }
        M.dragId = -1;
        return;
    }
    M.cockpit.pointerUp(id);
}

void WorldScreen::key(Key k, bool down) {
    Impl& M = *m_;
    switch (k) {
    case Key::Left: M.kL = down; break;
    case Key::Right: M.kR = down; break;
    case Key::Back: if (down) { if (M.mapOpen) M.mapOpen = false; else { M.app.worldValid = true; M.app.worldX = M.car->sim().posX(); M.app.worldY = M.car->sim().posY(); M.app.worldH = M.car->sim().heading(); M.app.goGarage(); } } break;
    case Key::Enter: if (down && M.poiHere() >= 0) pointerDown(0, M.poiBtn.cx(), M.poiBtn.cy()); break;
    case Key::PageUp: if (down) pointerDown(0, M.autoBtn.cx(), M.autoBtn.cy()); break;
    case Key::PageDown: if (down) { M.mapOpen = !M.mapOpen; M.mapCx = M.car->sim().posX(); M.mapCy = M.car->sim().posY(); } break;
    default: M.cockpit.key(k, down); break;
    }
}

} // namespace zk
