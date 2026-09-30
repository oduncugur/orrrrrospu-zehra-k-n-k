// Arac-arac temasi (OBB + impuls): momentum korunumu, hiz aktarimi, ayrilma, yan temasta savrulma
#include "game/Contact.h"
#include "garage/VehicleCatalog.h"

#include <cmath>
#include <cstdio>
#include <memory>

using namespace zk;
static int failures = 0;
#define CHECK(cond, msg) do { if (cond) std::printf("  GECTI  %s\n", msg); else { std::printf("  KALDI  %s\n", msg); ++failures; } } while (0)

struct Car {
    const VehicleDef* v; Tune t; std::unique_ptr<VehicleSim> s;
    Car(int id, double x, double y, double psi, double speed) : v(findVehicle(id)) {
        VehicleSimConfig c; c.car = v; c.tune = &t; c.planar = true; c.road = "acikyol"; c.laneAsymmetry = false;
        s = std::make_unique<VehicleSim>(c);
        s->resetPose(x, y, psi);
        s->applyImpulse(s->mass() * speed * std::cos(psi), s->mass() * speed * std::sin(psi), 0, 0);
    }
    CarBox box() { return {s.get(), 0.5 * v->lengthM, 0.5 * v->widthM}; }
    double vx() const { double x, y; s->worldVelocity(x, y); return x; }
    double vy() const { double x, y; s->worldVelocity(x, y); return y; }
    double px() const { return s->mass() * vx(); }
    double py() const { return s->mass() * vy(); }
    double ke() const { return 0.5 * s->mass() * (vx() * vx() + vy() * vy()) + 0.5 * s->yawInertia() * s->yawRate() * s->yawRate(); }
};

int main() {
    std::printf("[1] Uzak araclar\n");
    {
        Car a(5, 0, 0, 0, 30), b(227, 20, 0, 0, 20);
        const ContactResult r = resolveContact(a.box(), b.box());
        CHECK(!r.touching && std::fabs(a.s->speed() - 30.0) < 1e-9 && std::fabs(b.s->speed() - 20.0) < 1e-9, "temas yok, hizlar ayni");
    }
    std::printf("[2] Arkadan carpma (#5 30 m/s, #227 onde 20 m/s, 0.3 m ic ice)\n");
    {
        const double la = findVehicle(5)->lengthM, lb = findVehicle(227)->lengthM;
        Car a(5, 0, 0, 0, 30), b(227, 0.5 * (la + lb) - 0.3, 0, 0, 20);
        const double p0 = a.px() + b.px(), ke0 = a.ke() + b.ke();
        const ContactResult r = resolveContact(a.box(), b.box());
        const double p1 = a.px() + b.px(), ke1 = a.ke() + b.ke();
        std::printf("    kapanma %.1f m/s, derinlik %.2f m -> arka %.2f m/s, on %.2f m/s, ayrilma %.2f m/s\n",
                    r.closingSpeed, r.depth, a.vx(), b.vx(), b.vx() - a.vx());
        CHECK(r.touching && std::fabs(r.closingSpeed - 10.0) < 1e-6 && std::fabs(r.depth - 0.3) < 1e-6, "temas, kapanma 10 m/s");
        CHECK(std::fabs(p1 - p0) < 1e-6 * std::fabs(p0), "momentum korunur");
        CHECK(ke1 <= ke0 + 1e-6, "enerji artmaz");
        CHECK(a.vx() < 30.0 && b.vx() > 20.0 && std::fabs((b.vx() - a.vx()) - 2.5) < 0.01, "hiz aktarimi, ayrilma = e x 10 = 2.5 m/s");
        CHECK(std::fabs(a.s->yawRate()) < 1e-9 && std::fabs(b.s->yawRate()) < 1e-9, "eksenel darbede donme yok");
        CHECK(!resolveContact(a.box(), b.box()).touching, "konumlar ayrildi");
        CHECK(std::fabs((30.0 - a.vx()) * a.s->mass() - (b.vx() - 20.0) * b.s->mass()) < 1e-6, "agir arac daha az hiz degistirir (m dv esit)");
    }
    std::printf("[3] Yandan surtme (#227 sagdan 6 derece iceri kiriyor)\n");
    {
        const double wa = findVehicle(5)->widthM, wb = findVehicle(227)->widthM;
        Car a(5, 0, 0, 0, 30), b(227, 1.0, 0.5 * (wa + wb) - 0.12, -0.105, 30);
        const double py0 = a.py() + b.py(), px0 = a.px() + b.px(), ke0 = a.ke() + b.ke();
        const ContactResult r = resolveContact(a.box(), b.box());
        std::printf("    kapanma %.2f m/s, impuls %.0f N s, #5 vy %.2f m/s yaw %.3f rad/s, #227 yaw %.3f rad/s\n",
                    r.closingSpeed, r.impulse, a.vy(), a.s->yawRate(), b.s->yawRate());
        CHECK(r.touching && r.closingSpeed > 2.0, "yanal kapanma");
        CHECK(std::fabs(a.py() + b.py() - py0) < 1e-6 * (1 + std::fabs(py0)) && std::fabs(a.px() + b.px() - px0) < 1e-6 * px0, "momentum (x, y) korunur");
        CHECK(a.vy() < -0.5, "oyuncu yana itildi");
        CHECK(std::fabs(a.s->yawRate()) > 0.01 && std::fabs(b.s->yawRate()) > 0.01, "iki arac da savruldu (yaw)");
        CHECK(a.ke() + b.ke() <= ke0 + 1e-6, "enerji artmaz");
    }
    std::printf("[4] Yan yana, ayni hizda hafif temas\n");
    {
        const double wa = findVehicle(5)->widthM, wb = findVehicle(227)->widthM;
        Car a(5, 0, 0, 0, 25), b(227, 0.5, 0.5 * (wa + wb) - 0.05, 0, 25);
        const ContactResult r = resolveContact(a.box(), b.box());
        CHECK(r.touching && r.impulse == 0.0 && std::fabs(a.vx() - 25) < 1e-9 && std::fabs(b.vx() - 25) < 1e-9, "yalniz ayrilir, hiz kaybi yok");
    }
    std::printf(failures ? "\nSONUC: %d test KALDI\n" : "\nSONUC: tum testler gecti\n", failures);
    return failures ? 1 : 0;
}
