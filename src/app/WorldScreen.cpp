// ZEHRA KINIK - Acik dunya ekrani (zehra_world surumu): butun sehirlerden gecen otoban, sehir izgaralari (kavsakta
// donus), binalar, trafik (polis ~%3, serseri ~%1), benzinlikler (sehre gore litre fiyati), bulusma meydanlari
// (modifiyeli araclar), yaris baslangiclari, hurdalik. Harita: dokunarak waypoint, rota; otonom surus rotayi izler.
#include "Screens.h"
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
struct Proj { float x, y, w; bool ok; };
Proj project(const Mat4& vp, double X, double Y, double h, int vw, int vh) {
    const float x = (float)X, y = (float)h, z = (float)-Y;
    const float cx = vp.m[0] * x + vp.m[4] * y + vp.m[8] * z + vp.m[12];
    const float cy = vp.m[1] * x + vp.m[5] * y + vp.m[9] * z + vp.m[13];
    const float cw = vp.m[3] * x + vp.m[7] * y + vp.m[11] * z + vp.m[15];
    if (cw < 0.4f) return {0, 0, cw, false};
    return {(cx / cw * 0.5f + 0.5f) * vw, (1.0f - (cy / cw * 0.5f + 0.5f)) * vh, cw, true};
}
void triP(Renderer& r, const Proj& a, const Proj& b, const Proj& c, Color col) { r.triZ(a.x, a.y, a.w, b.x, b.y, b.w, c.x, c.y, c.w, col); }
void quadP(Renderer& r, const Proj& a, const Proj& b, const Proj& c, const Proj& d, Color col) {
    if (a.ok && b.ok && c.ok && d.ok) { triP(r, a, b, c, col); triP(r, a, c, d, col); }
}
float hashW(int i) { unsigned x = (unsigned)i * 2654435761u; x ^= x >> 13; x *= 0x5bd1e995u; x ^= x >> 15; return (x & 0xFFFF) / 65535.0f; }
constexpr double kTau = 6.283185307179586;
} // namespace

struct WorldScreen::Impl {
    App& app;
    const World& w = World::get();
    int carId; Tune tune;
    int W = 640, H = 360; bool land = true;
    WorldLeg leg{0, false};
    std::unique_ptr<RoadCar> car;
    Cockpit cockpit;
    double steer = 0, tiltF = 0, camPsi = 0, envT = 0, spin = 0, shakeT = 0;
    bool kL = false, kR = false;
    std::string msg; double msgT = 0;
    int city = -2;
    // Trafik
    struct T { WorldLeg leg; double s, v, v0; int lane, carId, role, uid; double prevRel; };
    std::vector<T> traffic; int nextUid = 1; uint32_t rng = 0x9E3779B9u;
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
    if (land_) { M.mapBtn = {470, 2, 576, 34}; M.autoBtn = {362, 2, 466, 34}; }
    else { M.mapBtn = {252, 4, 356, 26}; M.autoBtn = {252, 30, 356, 52}; }
    M.mPlus = {(float)M.W - 60, 40, (float)M.W - 8, 80}; M.mMinus = {(float)M.W - 60, 86, (float)M.W - 8, 126};
    M.mClose = {(float)M.W - 120, (float)M.H - 44, (float)M.W - 8, (float)M.H - 8}; M.mClear = {8, (float)M.H - 44, 140, (float)M.H - 8};
    M.rng ^= (uint32_t)(app.career.races * 7919 + 13);
    if (std::getenv("ZK_WORLD_MAP")) { M.mapOpen = true; M.mapCx = M.car->sim().posX(); M.mapCy = M.car->sim().posY(); }
    if (const char* rp = std::getenv("ZK_WORLD_ROUTE")) {                  // test: n. noktaya rota + otonom
        const int k = std::clamp(std::atoi(rp), 0, (int)M.w.pois.size() - 1);
        WorldLeg l; double s, lat; M.hasWp = true;
        if (M.w.nearestLeg(M.w.pois[k].x, M.w.pois[k].y, 0.0, l, s, lat, 200.0)) { const RoadPoint q = M.w.path(l).at(s); M.wpX = q.x; M.wpY = q.y; }
        else { M.wpX = M.w.pois[k].x; M.wpY = M.w.pois[k].y; }
        M.route = M.w.route(M.leg, M.car->s(), M.wpX, M.wpY); M.routeIdx = 0; M.autoDrive = true;
    }
    M.flash("ACIK DUNYA: HARITADAN HEDEF SEC, SUR YA DA OTONOM", 3.0);
}

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
    const double slipAllow = std::min(0.45, std::fabs(sim.bodySlipAngle()) * 1.3);
    const double maxSteer = std::clamp(sim.vehicleLoad().wheelbase * 1.1 * 9.81 / std::max(v * v, 1.0) + slipAllow, 0.035, 0.55);
    double target = (M.kL ? maxSteer : 0.0) - (M.kR ? maxSteer : 0.0);
    if (!M.kL && !M.kR && app.tiltAvailable && app.settings.tiltSteer) {
        const double raw = std::clamp((app.settings.tiltInvert ? -1.0 : 1.0) * app.tilt() * app.settings.tiltSens / 100.0, -1.0, 1.0);
        M.tiltF += (raw - M.tiltF) * std::min(1.0, dt / 0.12);
        const double dz = 0.08, t = M.tiltF, u = std::fabs(t) < dz ? 0.0 : (t - std::copysign(dz, t)) / (1.0 - dz);
        target = std::copysign(std::pow(std::fabs(u), 1.0 + 0.8 * std::clamp((v - 8.0) / 32.0, 0.0, 1.0)), u) * maxSteer;
    }
    const double rate = (std::fabs(target) > std::fabs(M.steer) ? 1.0 : 2.5) * dt;
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
    // Binalar: yonlu kutu, arac (2.2 x 0.9) carparsa itilir
    for (const WorldBuilding& b : M.w.buildings) {
        if (std::fabs(b.cx - sim.posX()) > 70 || std::fabs(b.cy - sim.posY()) > 70) continue;
        double rel = 0;
        RoadSession::contact(P, b.cx, b.cy, std::atan2(b.uy, b.ux), 0.0, 1e7, b.hu, b.hv, rel);
        if (rel > 2.0) { M.shakeT = std::min(0.5, rel * 0.04); app.haptic(150, 220); if (rel > 6.0) M.flash("BINAYA CARPTIN!", 1.0); }
    }
    // Sehir sinirlari
    const int cNow = M.w.cityAt(sim.posX(), sim.posY());
    if (cNow != M.city) {
        if (cNow >= 0) { app.career.city = cNow; app.toast(std::string(M.w.cities[cNow].name) + "'A HOS GELDIN"); app.saveCareer(); }
        else if (M.city >= 0) app.toast("OTOBAN: SEHIRLERARASI");
        M.city = cNow;
    }
    // Trafik: oyuncu cevresinde ~24 arac; kavsakta rastgele cikis; polis / serseri
    {
        const double px = sim.posX(), py = sim.posY();
        M.traffic.erase(std::remove_if(M.traffic.begin(), M.traffic.end(), [&](const Impl::T& t) {
            const RoadPoint q = M.w.path(t.leg).at(t.s); return std::hypot(q.x - px, q.y - py) > 750.0; }), M.traffic.end());
        int guard = 0;
        std::vector<int> nearE;                                          // yakindaki yollar (dogma adaylari)
        if (M.traffic.size() < 24)
            for (int e = 0; e < (int)M.w.edges.size(); ++e) {
                const WorldEdge& E = M.w.edges[e];
                if (!(px < E.minX - 650 || px > E.maxX + 650 || py < E.minY - 650 || py > E.maxY + 650)) nearE.push_back(e);
            }
        while (!nearE.empty() && M.traffic.size() < 24 && guard++ < 40) {
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
        for (auto& t : M.traffic) {
            const RoadPath& tp = M.w.path(t.leg);
            double gap = 1e9, lv = t.v0;
            for (const auto& o : M.traffic)
                if (&o != &t && o.leg.edge == t.leg.edge && o.leg.rev == t.leg.rev && o.lane == t.lane && o.s > t.s && o.s - t.s < gap) { gap = o.s - t.s; lv = o.v; }
            if (M.leg.edge == t.leg.edge && M.leg.rev == t.leg.rev && P.s() > t.s && P.s() - t.s < gap && std::fabs(P.lateral() - tp.laneOffset(P.s(), false, t.lane)) < 2.0) { gap = P.s() - t.s; lv = v; }
            const bool nearEnd = tp.length() - t.s < 25.0 && !M.w.edges[t.leg.edge].highway;   // kavsakta yavasla
            const double want = gap < 1e8 ? std::min(t.v0, lv + (gap - 10.0) * 0.4) : t.v0;
            const double tg = nearEnd ? std::min(want, 7.0) : want;
            t.v += std::clamp(tg - t.v, -6.0 * dt, 1.6 * dt);
            t.v = std::max(0.0, t.v);
            t.s += t.v * dt;
            if (t.s > tp.length()) {
                const auto ex = M.w.exits(t.leg);
                if (ex.empty()) { t.s = tp.length(); t.v = 0; }
                else { t.s -= tp.length(); t.leg = ex[(size_t)(M.rnd() * ex.size()) % ex.size()]; t.lane = std::min(t.lane, M.w.path(t.leg).lanesFwd(0.0) - 1); }
            }
            // Carpisma (oyuncu)
            const RoadPath& tq = M.w.path(t.leg);
            const RoadPoint q = tq.at(t.s);
            const double lo = tq.laneOffset(t.s, false, t.lane);
            const double tx = q.x - lo * std::sin(q.heading), ty = q.y + lo * std::cos(q.heading);
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
    r.setSceneLight(1.0f, 1.0f);
    auto front = [&](double x, double y, double maxD) {                   // gorus konisinde mi (kamera onunde)
        const double dx = x - ex, dy = y - ey;
        const double along = dx * fx + dy * fy;
        return along > -30.0 && dx * dx + dy * dy < maxD * maxD;
    };
    // Sehir zeminleri (kaldirim / beton): 20 m karolar (yakindaki buyuk karo kameranin arkasina tasip atlaniyordu)
    for (const WorldCity& c : w.cities) {
        if (std::hypot(c.x - ex, c.y - ey) > 2400.0) continue;              // merkez arkada kalsa da sehrin bir kismi onde olabilir
        const double R = World::kBlock * (World::kGrid - 1) * 0.5 + 40.0, px = -c.dirY, py = c.dirX;
        const int N = (int)(2 * R / 20.0);
        const double cu = (ex - c.x) * c.dirX + (ey - c.y) * c.dirY, cv = (ex - c.x) * px + (ey - c.y) * py;   // kamera yerel
        const int i0 = std::max(0, (int)((cu - 650.0 + R) / 20.0)), i1 = std::min(N, (int)((cu + 650.0 + R) / 20.0) + 1);
        const int j0 = std::max(0, (int)((cv - 650.0 + R) / 20.0)), j1 = std::min(N, (int)((cv + 650.0 + R) / 20.0) + 1);
        for (int i = i0; i < i1; ++i)
            for (int j = j0; j < j1; ++j) {
                const double u0 = -R + 2 * R * i / N, u1 = -R + 2 * R * (i + 1) / N, v0 = -R + 2 * R * j / N, v1 = -R + 2 * R * (j + 1) / N;
                const double mu = 0.5 * (u0 + u1), mv = 0.5 * (v0 + v1);
                if (!front(c.x + mu * c.dirX + mv * px, c.y + mu * c.dirY + mv * py, 650.0)) continue;
                auto Q = [&](double u, double v) { return P3(c.x + u * c.dirX + v * px, c.y + u * c.dirY + v * py, 0.0); };
                const double cxw = c.x + mu * c.dirX + mv * px - ex, cyw = c.y + mu * c.dirY + mv * py - ey;
                const int sub = cxw * cxw + cyw * cyw < 60.0 * 60.0 ? 5 : 1;   // yakinda 4 m alt karolar (yakin duzlem kirpmasi)
                for (int a2 = 0; a2 < sub; ++a2)
                    for (int b2 = 0; b2 < sub; ++b2) {
                        const double ua = u0 + (u1 - u0) * a2 / sub, ub = u0 + (u1 - u0) * (a2 + 1) / sub;
                        const double va = v0 + (v1 - v0) * b2 / sub, vb = v0 + (v1 - v0) * (b2 + 1) / sub;
                        const Proj a = Q(ua, va);
                        quadP(r, a, Q(ub, va), Q(ub, vb), Q(ua, vb), fog({0.62f, 0.62f, 0.60f}, a.w));
                    }
            }
    }
    // Yollar: gorus mesafesindeki kenarlar; asfalt + serit cizgileri
    for (const WorldEdge& E : w.edges) {
        if (ex < E.minX - 900 || ex > E.maxX + 900 || ey < E.minY - 900 || ey > E.maxY + 900) continue;
        const auto& pts = E.fwd().points();
        const int step = E.highway ? 2 : 1;
        for (size_t i = 0; i + step < pts.size(); i += step) {
            const RoadPoint &a = pts[i], &b = pts[i + step];
            if (!front(a.x, a.y, 900.0)) continue;
            auto edge = [&](const RoadPoint& p, double off, double h) { return P3(p.x - off * std::sin(p.heading), p.y + off * std::cos(p.heading), h); };
            const Proj aL = edge(a, a.hw, 0.01), aR = edge(a, -a.hw, 0.01), bL = edge(b, b.hw, 0.01), bR = edge(b, -b.hw, 0.01);
            const bool band = (i / step / 4) % 2 == 0;
            quadP(r, aL, bL, bR, aR, fog(band ? Color{0.30f, 0.30f, 0.32f} : Color{0.28f, 0.28f, 0.30f}, aL.w));
            if (aL.w < 260.0f && (i / step) % 3 == 0) {                     // kesik cizgiler: serit sinirlari
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
        if (!front(b.cx, b.cy, 700.0)) continue;
        items.push_back({std::hypot(b.cx - ex, b.cy - ey), 0, i});
    }
    for (int i = 0; i < (int)w.pois.size(); ++i) if (front(w.pois[i].x, w.pois[i].y, 800.0)) items.push_back({std::hypot(w.pois[i].x - ex, w.pois[i].y - ey), 1, i});
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
        } else {
            const WorldPoi& q = w.pois[it.idx];
            const Proj b0 = P3(q.x, q.y, 0.0);
            if (!b0.ok) continue;
            const float sc = pxPerM / b0.w;
            r.setDepthW(b0.w);
            static const Color kc[4] = {{0.95f, 0.45f, 0.08f}, {0.75f, 0.15f, 0.55f}, {0.45f, 0.30f, 0.15f}, {0.85f, 0.15f, 0.12f}};
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
        const RoadPath& tp = w.path(t.leg);
        const RoadPoint q = tp.at(t.s);
        const double lo = tp.laneOffset(t.s, false, t.lane);
        const double x = q.x - lo * std::sin(q.heading), y = q.y + lo * std::cos(q.heading);
        if (!front(x, y, 700.0)) continue;
        objs.push_back({std::hypot(x - ex, y - ey), t.carId, matMul(matTranslate((float)x, 0.0f, (float)-y), matRotY((float)q.heading)), t.role, (float)(M.envT * t.v / 0.31)});
    }
    std::sort(objs.begin(), objs.end(), [](const Obj& a, const Obj& b) { return a.d > b.d; });
    for (size_t k = 0; k < parked.size(); ++k) { r.setCarLook(parkedLook[k]); r.drawCar(parked[k].first, 0, 0, W, H, proj, view, parked[k].second); }
    for (const Obj& o : objs) {
        if (o.d < 3.0) continue;
        if (o.role == 1) {
            Renderer::CarLook pl; pl.paintOn = true; pl.paint[0] = pl.paint[1] = pl.paint[2] = 0.93f;
            pl.stripe = 3; pl.stripeCol[0] = 0.08f; pl.stripeCol[1] = 0.12f; pl.stripeCol[2] = 0.45f;
            r.setCarLook(pl);
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
        const Mat4 pm = matMul(matTranslate((float)X, 0.0f, (float)-Y), matRotY((float)sim.heading()));
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
    button(r, M.mapBtn, "HARITA", {0.15f, 0.35f, 0.6f}, 1);
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
    }
    if (M.msgT > 0) { r.rect(0, H * 0.3f, W, H * 0.3f + 26, {0.02f, 0.02f, 0.04f, 0.75f}); r.textCentered(W / 2.0f, H * 0.3f + 7, M.msg, 2, kUiGold); }
    M.cockpit.render(r, g, P.grinding());
    // ---- Harita ----
    if (M.mapOpen) {
        r.rect(0, 0, W, H, {0.06f, 0.09f, 0.08f, 0.97f});
        auto S = [&](double x, double y, float& sx, float& sy) { sx = (float)(W * 0.5 + (x - M.mapCx) / M.mapScale); sy = (float)(H * 0.5 - (y - M.mapCy) / M.mapScale); };
        auto seg = [&](double x0, double y0, double x1, double y1, float wd, Color col) {
            float a, bb, c2, d; S(x0, y0, a, bb); S(x1, y1, c2, d);
            const float dx = c2 - a, dy = d - bb, l = std::sqrt(dx * dx + dy * dy) + 1e-3f, nx = -dy / l * wd, ny = dx / l * wd;
            r.tri(a + nx, bb + ny, c2 + nx, d + ny, c2 - nx, d - ny, col); r.tri(a + nx, bb + ny, c2 - nx, d - ny, a - nx, bb - ny, col);
        };
        for (const WorldCity& c : w.cities) { float sx, sy; S(c.x, c.y, sx, sy); r.circle(sx, sy, (float)(c.r / M.mapScale), 20, {0.18f, 0.2f, 0.22f}); }
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
        static const Color pc[4] = {{1.0f, 0.5f, 0.1f}, {0.9f, 0.3f, 0.7f}, {0.6f, 0.45f, 0.25f}, {0.9f, 0.2f, 0.15f}};
        for (const WorldPoi& q : w.pois) { float sx, sy; S(q.x, q.y, sx, sy); r.circle(sx, sy, 4.0f, 10, pc[q.type]); }
        for (const WorldCity& c : w.cities) { float sx, sy; S(c.x, c.y, sx, sy); r.textCentered(sx, sy - (float)(c.r / M.mapScale) - 12, c.name, 1, {1, 1, 1}); }
        if (M.hasWp) { float sx, sy; S(M.wpX, M.wpY, sx, sy); r.circle(sx, sy, 7, 14, {0.2f, 0.8f, 1.0f}); r.circle(sx, sy, 3, 10, {1, 1, 1}); }
        {   float sx, sy; S(X, Y, sx, sy);
            const float h = (float)sim.heading(), c2 = std::cos(h), s2 = -std::sin(h);
            r.tri(sx + c2 * 9, sy + s2 * 9, sx - c2 * 6 - s2 * 5, sy - s2 * 6 + c2 * 5, sx - c2 * 6 + s2 * 5, sy - s2 * 6 - c2 * 5, {0.2f, 1.0f, 0.4f}); }
        r.text(8, 8, "HARITA: DOKUN = HEDEF, SURUKLE = KAYDIR", 1, {0.9f, 0.9f, 0.9f});
        r.text(8, 20, "TURUNCU YARIS  PEMBE BULUSMA  KAHVE HURDALIK  KIRMIZI BENZIN", 1, {0.7f, 0.7f, 0.7f});
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
