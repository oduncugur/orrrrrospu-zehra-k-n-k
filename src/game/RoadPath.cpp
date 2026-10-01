#include "RoadPath.h"

#include <algorithm>
#include <cmath>

namespace zk {

namespace {
struct Rng {
    uint32_t s;
    double uni() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s & 0xFFFFFF) / double(0x1000000); }
    double range(double a, double b) { return a + (b - a) * uni(); }
};
}

RoadPath::RoadPath(uint32_t seed, double lengthM, double minRadius, double halfWidth, double maxGrade) : halfWidth_(halfWidth) {
    struct HwFill { RoadPath* p; double hw; ~HwFill() { for (RoadPoint& q : p->pts_) q.hw = hw; } } fill{this, halfWidth};
    Rng r{seed ? seed : 1u};
    // Egrilik programi: [duzluk][giris klotoidi][sabit yay][cikis klotoidi] tekrarlari
    std::vector<Seg> prog;
    prog.push_back({200.0, 0.0, 0.0});                           // baslangic duzlugu
    double total = 200.0;
    while (total < lengthM) {
        const double straight = r.range(40.0, 5.0 * minRadius);
        const double R = r.range(minRadius, 4.0 * minRadius);
        const double k = (r.uni() < 0.5 ? 1.0 : -1.0) / R;
        const double arc = r.range(0.15, 1.4) * R;               // 9-80 derece donus
        const double trans = std::clamp(0.25 * R, 20.0, 90.0);   // klotoid: egrilik dogrusal degisir
        prog.push_back({straight, 0.0, 0.0});
        prog.push_back({trans, 0.0, k});
        prog.push_back({arc, k, k});
        prog.push_back({trans, k, 0.0});
        total += straight + 2 * trans + arc;
    }
    integrate(prog, lengthM);
    addElevation(seed, maxGrade, minRadius < 50 ? 250.0 : 600.0, minRadius < 50 ? 1000.0 : 2400.0);
}

// Egrilik programini 2 m adimlarla x/y/yon olarak integre eder (orta nokta)
void RoadPath::integrate(const std::vector<Seg>& prog, double lengthM) {
    double x = 0, y = 0, h = 0, s = 0;
    pts_.push_back({x, y, h, 0.0, 0.0});
    for (const Seg& g : prog) {
        const int n = std::max(1, (int)std::round(g.len / kStep));
        for (int i = 0; i < n; ++i) {
            const double k = g.k0 + (g.k1 - g.k0) * (i + 0.5) / n;
            const double hm = h + 0.5 * k * kStep;               // orta nokta integrasyonu
            x += kStep * std::cos(hm); y += kStep * std::sin(hm); h += k * kStep; s += kStep;
            pts_.push_back({x, y, h, k, s});
            if (s >= lengthM) break;
        }
        if (s >= lengthM) break;
    }
}

// Yukseklik: uc sinusun toplami olarak egim (yumusak tepe/cukur), ayri tohum (viraj programi ayni kalir).
// Baslangic duzlugu (ilk 150 m) duz, 150-450 m arasi yumusakca girer.
void RoadPath::addElevation(uint32_t seed, double maxGrade, double wl0, double wl1) {
    if (maxGrade <= 0.0) return;
    Rng e{(seed ? seed : 1u) * 2654435761u + 12345u};
    double L[3], A[3], P[3];
    for (int k = 0; k < 3; ++k) { L[k] = e.range(wl0, wl1); A[k] = maxGrade * e.range(0.45, 0.6); P[k] = e.range(0.0, 6.2831853); }
    auto grade = [&](double ss) {
        const double u = std::clamp((ss - 150.0) / 300.0, 0.0, 1.0), ramp = u * u * (3.0 - 2.0 * u);
        double g = 0;
        for (int k = 0; k < 3; ++k) g += A[k] * std::sin(6.2831853 * ss / L[k] + P[k]);
        return ramp * maxGrade * std::tanh(g / maxGrade);        // sinira yumusak doyum: uzun sabit yokuslar
    };
    double z = 0;
    for (size_t i = 0; i < pts_.size(); ++i) {
        const double g = grade(pts_[i].s);
        if (i > 0) z += 0.5 * (g + pts_[i - 1].grade) * (pts_[i].s - pts_[i - 1].s);
        pts_[i].grade = g; pts_[i].z = z;
    }
}

// Karma yaris yolu: uzun duzluk (drag) -> 1-3 virajlik blok (S olabilir) -> duzluk -> viraj blogu -> bitis duzlugu.
// Viraj bolumu blogun ~120 m oncesinden (fren bolgesi) 60 m sonrasina kadar sayilir.
RoadPath RoadPath::karma(uint32_t seed, double maxGrade) {
    RoadPath p;
    p.halfWidth_ = 3.6;
    Rng r{seed ? seed * 2246822519u + 3u : 1u};
    std::vector<Seg> prog;
    double s = 0.0;
    auto straight = [&](double len) { prog.push_back({len, 0.0, 0.0}); s += len; };
    auto curveBlock = [&]() {
        const double s0 = s;
        const int n = 1 + (int)(r.uni() * 3.0);                      // 1-3 viraj
        double sign = r.uni() < 0.5 ? 1.0 : -1.0, minR = 1e9;
        for (int i = 0; i < n; ++i) {
            if (i > 0) { straight(r.range(30.0, 120.0)); if (r.uni() < 0.6) sign = -sign; }   // S ihtimali
            const double R = r.range(70.0, 220.0), k = sign / R;
            minR = std::min(minR, R);
            const double trans = std::clamp(0.25 * R, 20.0, 60.0);
            const double arc = r.range(30.0, 100.0) * 3.14159265358979 / 180.0 * R;
            prog.push_back({trans, 0.0, k}); prog.push_back({arc, k, k}); prog.push_back({trans, k, 0.0});
            s += 2 * trans + arc;
        }
        // Virajli bolum 450 m once baslar: 250 km/h'ten ~90 km/h'e frenleme (~260 m) + kamera gecisi (0.9 s) payi
        p.sections_.push_back({std::max(0.0, s0 - kKarmaLead), s + 60.0, true, s0, minR});
    };
    straight(r.range(2800.0, 3600.0));
    curveBlock();
    straight(r.range(1500.0, 2500.0));
    curveBlock();
    straight(r.range(1000.0, 1600.0) + 300.0);                       // bitis duzlugu + bitis sonrasi yavaslama
    p.integrate(prog, s);
    p.addElevation(seed + 77u, maxGrade, 600.0, 2400.0);
    return p;
}

bool RoadPath::curvyAt(double s) const {
    for (const RoadSection& q : sections_) if (q.curvy && s >= q.s0 && s <= q.s1) return true;
    return false;
}

RoadPoint RoadPath::at(double s) const {
    s = std::clamp(s, 0.0, length());
    const int i = std::min((int)(s / kStep), (int)pts_.size() - 2);
    const double f = (s - pts_[i].s) / kStep;
    const RoadPoint &a = pts_[i], &b = pts_[i + 1];
    return {a.x + (b.x - a.x) * f, a.y + (b.y - a.y) * f, a.heading + (b.heading - a.heading) * f,
            a.curvature + (b.curvature - a.curvature) * f, s, a.z + (b.z - a.z) * f, a.grade + (b.grade - a.grade) * f,
            a.hw + (b.hw - a.hw) * f};
}

void RoadPath::setWidthRange(uint32_t seed, double hwMin, double hwMax) {
    Rng r{seed ? seed * 747796405u + 11u : 7u};
    halfWidth_ = hwMin;
    double s0 = 0.0, w0 = hwMin + (hwMax - hwMin) * 0.3;
    double s1 = r.range(400.0, 1100.0), w1 = hwMin + (hwMax - hwMin) * r.uni();
    for (RoadPoint& p : pts_) {
        while (p.s > s1) { s0 = s1; w0 = w1; s1 += r.range(400.0, 1100.0); w1 = hwMin + (hwMax - hwMin) * r.uni(); }
        const double t = std::clamp((p.s - (s1 - 120.0)) / 120.0, 0.0, 1.0);   // bolum sonunda 120 m gecis
        p.hw = w0 + (w1 - w0) * t * t * (3.0 - 2.0 * t);
    }
}

void RoadPath::project(double x, double y, int& hint, double& s, double& lateral) const {
    const int n = (int)pts_.size();
    int best = std::clamp(hint, 0, n - 1);
    double bd = 1e300;
    const int lo = std::max(0, best - 60), hi = std::min(n - 1, best + 60);
    for (int i = lo; i <= hi; ++i) {
        const double dx = x - pts_[i].x, dy = y - pts_[i].y, d = dx * dx + dy * dy;
        if (d < bd) { bd = d; best = i; }
    }
    hint = best;
    // En yakin noktanin teget dogrusu uzerine izdusum
    const RoadPoint& p = pts_[best];
    const double c = std::cos(p.heading), sn = std::sin(p.heading), dx = x - p.x, dy = y - p.y;
    s = std::clamp(p.s + dx * c + dy * sn, 0.0, length());
    lateral = -dx * sn + dy * c;
}

} // namespace zk
