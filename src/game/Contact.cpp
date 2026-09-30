#include "game/Contact.h"

#include <algorithm>
#include <cmath>

namespace zk {

namespace {
struct Box { double cx, cy, ux, uy, vx, vy, hl, hw; };   // merkez, boyuna / yanal birim eksen, yari boylar

Box boxOf(const CarBox& c) {
    const double h = c.sim->heading();
    return {c.sim->posX(), c.sim->posY(), std::cos(h), std::sin(h), -std::sin(h), std::cos(h), c.halfL, c.halfW};
}
double radius(const Box& b, double nx, double ny) {
    return b.hl * std::fabs(b.ux * nx + b.uy * ny) + b.hw * std::fabs(b.vx * nx + b.vy * ny);
}
bool inside(const Box& b, double x, double y) {
    const double dx = x - b.cx, dy = y - b.cy;
    return std::fabs(dx * b.ux + dy * b.uy) <= b.hl && std::fabs(dx * b.vx + dy * b.vy) <= b.hw;
}
double cross(double ax, double ay, double bx, double by) { return ax * by - ay * bx; }
} // namespace

ContactResult resolveContact(CarBox ca, CarBox cb, double e, double mu) {
    ContactResult res;
    const Box A = boxOf(ca), B = boxOf(cb);
    // Ayirici eksen testi: 4 eksen (iki kutunun kenar normalleri); en kucuk gecme ekseni temas normali
    const double axes[4][2] = {{A.ux, A.uy}, {A.vx, A.vy}, {B.ux, B.uy}, {B.vx, B.vy}};
    const double dx = B.cx - A.cx, dy = B.cy - A.cy;
    double best = 1e9, nx = 0, ny = 0;
    for (const auto& ax : axes) {
        const double d = dx * ax[0] + dy * ax[1];
        const double over = radius(A, ax[0], ax[1]) + radius(B, ax[0], ax[1]) - std::fabs(d);
        if (over <= 0.0) return res;                                   // ayirici eksen var: temas yok
        if (over < best) { best = over; nx = d > 0 ? -ax[0] : ax[0]; ny = d > 0 ? -ax[1] : ax[1]; }   // B -> A yonu
    }
    res.touching = true; res.depth = best;

    // Temas noktasi: digerinin icinde kalan koselerin ortalamasi (yoksa merkezlerin ortasi)
    double px = 0, py = 0; int n = 0;
    for (const Box* p : {&A, &B}) {
        const Box& o = p == &A ? B : A;
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sy = -1; sy <= 1; sy += 2) {
                const double x = p->cx + sx * p->hl * p->ux + sy * p->hw * p->vx, y = p->cy + sx * p->hl * p->uy + sy * p->hw * p->vy;
                if (inside(o, x, y)) { px += x; py += y; ++n; }
            }
    }
    if (n) { px /= n; py /= n; } else { px = 0.5 * (A.cx + B.cx); py = 0.5 * (A.cy + B.cy); }

    VehicleSim& sa = *ca.sim; VehicleSim& sb = *cb.sim;
    const double ma = sa.mass(), mb = sb.mass(), Ia = sa.yawInertia(), Ib = sb.yawInertia();
    // Konum ayirma (kutleyle ters orantili)
    const double corr = best + 0.01;
    sa.nudge(nx * corr * mb / (ma + mb), ny * corr * mb / (ma + mb));
    sb.nudge(-nx * corr * ma / (ma + mb), -ny * corr * ma / (ma + mb));

    // Temas noktasinda goreli hiz
    const double rax = px - A.cx, ray = py - A.cy, rbx = px - B.cx, rby = py - B.cy;
    double vax, vay, vbx, vby;
    sa.worldVelocity(vax, vay); sb.worldVelocity(vbx, vby);
    const double wa = sa.yawRate(), wb = sb.yawRate();
    const double relx = (vax - wa * ray) - (vbx - wb * rby), rely = (vay + wa * rax) - (vby + wb * rbx);
    const double vn = relx * nx + rely * ny;
    res.closingSpeed = std::max(0.0, -vn);
    if (vn >= 0.0) return res;                                         // ayriliyorlar: impuls yok

    const double ran = cross(rax, ray, nx, ny), rbn = cross(rbx, rby, nx, ny);
    const double k = 1.0 / ma + 1.0 / mb + ran * ran / Ia + rbn * rbn / Ib;
    const double j = -(1.0 + e) * vn / k;
    sa.applyImpulse(j * nx, j * ny, rax, ray);
    sb.applyImpulse(-j * nx, -j * ny, rbx, rby);
    res.impulse = j;

    // Surtunme (Coulomb): teget goreli hizi azaltan impuls, |jt| <= mu j
    double tx = relx - vn * nx, ty = rely - vn * ny;
    const double tl = std::hypot(tx, ty);
    if (tl > 1e-6) {
        tx /= tl; ty /= tl;
        const double rat = cross(rax, ray, tx, ty), rbt = cross(rbx, rby, tx, ty);
        const double kt = 1.0 / ma + 1.0 / mb + rat * rat / Ia + rbt * rbt / Ib;
        const double jt = std::clamp(-tl / kt, -mu * j, mu * j);
        sa.applyImpulse(jt * tx, jt * ty, rax, ray);
        sb.applyImpulse(-jt * tx, -jt * ty, rbx, rby);
    }
    return res;
}

} // namespace zk
