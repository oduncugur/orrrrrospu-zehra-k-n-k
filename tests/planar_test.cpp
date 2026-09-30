// Duzlemsel (viraj) dinamik testleri: bilinen fiziksel davranislar olculur.
#include "sim/VehicleSim.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>

using namespace zk;

static int failures = 0;
#define CHECK(cond, msg) do { if (cond) std::printf("  GECTI  %s\n", msg); else { std::printf("  KALDI  %s\n", msg); ++failures; } } while (0)
constexpr double kDt = 5e-5, kG = 9.81;

// Basit otomatik surucu: yumusak kalkis, devire gore vites (debriyajli), gaz komutu disaridan
struct AutoDriver {
    double t = 0, shiftT = -1; int target = 1;
    double launchPedal = 1.0; bool launched = false; // kalkis: debriyaj devre gore birakilir (motor bogulmaz)
    int lockGear = 0;                     // >0: bu vitese ulasinca sabit kal (skidpad)
    void apply(VehicleSim& s, double thrCmd, VehicleInputs& in) {
        PowertrainCore& pt = s.powertrain();
        t += kDt;
        double clutch = 0.0, thr = thrCmd;
        if (!launched) {
            if (pt.gear() == 0) pt.setGear(1);
            // Insan gibi: pedal 1.2 s'de birakilir, devir 1600'un altina duserse geri basilir
            launchPedal += (pt.rpm() < 1600.0 ? +0.6 : -1.0 / 1.2) * kDt;
            launchPedal = std::clamp(launchPedal, 0.0, 1.0);
            clutch = launchPedal; thr = std::max(thrCmd, 0.35);
            if (launchPedal <= 0.0) launched = true;
        }
        else if (lockGear > 0 && pt.gear() >= lockGear) { /* sabit vites */ }
        else if (shiftT < 0) {
            if (pt.rpm() > s.shiftRpm() - 300 && pt.gear() < pt.gearCount()) { shiftT = 0; target = pt.gear() + 1; }
            else if (pt.rpm() < 0.33 * s.engineSpec().redlineRpm && pt.gear() > 1) { shiftT = 0; target = pt.gear() - 1; }
        }
        if (shiftT >= 0) {
            shiftT += kDt;
            clutch = shiftT < 0.15 ? 1.0 : std::max(0.0, 1.0 - (shiftT - 0.15) / 0.15);
            if (shiftT < 0.15) thr = 0.0;
            if (shiftT > 0.10 && pt.gear() != target) pt.setGear(target);
            if (shiftT > 0.30) shiftT = -1;
        }
        pt.setClutchPedal(clutch);
        pt.setThrottle(std::clamp(thr, 0.0, 1.0));
        (void)in;
    }
};

static std::unique_ptr<VehicleSim> make(int car, bool planar, const Tune& t, const char* road = "drag") {
    static Tune keep[8]; static int k = 0;
    keep[k % 8] = t;
    VehicleSimConfig c; c.car = findVehicle(car); c.tune = &keep[k++ % 8]; c.planar = planar; c.laneAsymmetry = false; c.road = road;
    return std::make_unique<VehicleSim>(c);
}

int main() {
    std::printf("[1] Duz cizgi: duzlemsel = 1B (#5, 8 s tam gaz)\n");
    {
        Tune t;
        auto a = make(5, false, t, "duz"), b = make(5, true, t, "duz");
        AutoDriver da, db;
        for (int i = 0; i < (int)(8.0 / kDt); ++i) {
            VehicleInputs ia, ib;
            da.apply(*a, 1.0, ia); db.apply(*b, 1.0, ib);
            a->step(kDt, ia); b->step(kDt, ib);
        }
        std::printf("    1B: %.2f m %.2f m/s | duzlemsel: %.2f m %.2f m/s, Y %.3f m, yaw %.4f rad\n", a->distance(), a->speed(),
                    b->distance(), b->speed(), b->posY(), b->heading());
        CHECK(std::fabs(a->distance() - b->distance()) < 0.01 * a->distance(), "mesafe %1 icinde");
        CHECK(std::fabs(b->posY()) < 0.05 && std::fabs(b->heading()) < 0.002, "arac duz gitti");
    }

    // Skidpad: yaricap R, hiz yavasca artirilir; pure-pursuit direksiyon, PI hiz kontrolu
    auto skidpad = [](int car, const Tune& t, double R, double* maxAy, double* atSpeed) {
        auto s = make(car, true, t, "duz");
        AutoDriver d; d.lockGear = 2;                                       // vites gecisi olcumu bozmasin
        const double L = findVehicle(car)->wheelbaseM;
        double vTarget = 6.0, integ = 0.0;
        *maxAy = 0; *atSpeed = 0;
        int lostFor = 0;
        for (int i = 0; i < (int)(120.0 / kDt); ++i) {
            const double time = i * kDt;
            if (time > 6.0) vTarget += 0.15 * kDt;                          // 0.15 m/s^2 rampa (yari-kararli)
            // Pure pursuit: cember merkezi (0, R), sola donus
            const double x = s->posX(), y = s->posY(), psi = s->heading();
            const double ang = std::atan2(y - R, x) + 0.35 * 1.0;            // ileride bir nokta (acisal on bakis)
            const double lx = R * std::cos(ang), ly = R + R * std::sin(ang);
            const double dx = lx - x, dy = ly - y;
            const double alpha = std::atan2(dy, dx) - psi;
            const double ld = std::sqrt(dx * dx + dy * dy);
            VehicleInputs in;
            in.steer = std::clamp(std::atan2(2.0 * L * std::sin(alpha), ld), -0.6, 0.6);
            const double e = vTarget - s->speed();
            integ = std::clamp(integ + e * kDt, -5.0, 5.0);
            d.apply(*s, 0.25 * e + 0.15 * integ + 0.15, in);
            s->step(kDt, in);
            const double rad = std::sqrt(x * x + (y - R) * (y - R));
            if (time > 8.0 && (i % 200) == 0) {
                if (std::fabs(rad - R) < 1.5) {
                    const double ay = s->speed() * s->speed() / R / kG;       // yol uzerindeki yanal ivme
                    if (ay > *maxAy) { *maxAy = ay; *atSpeed = s->speed(); }
                    lostFor = 0;
                } else if (++lostFor > 50) break;                            // ~0.5 s cemberden cikti
            }
            if (!std::isfinite(s->speed())) { *maxAy = -1; break; }
        }
    };

    std::printf("[2] Skidpad R=50 m: sokak lastigi vs yari-slick takim (#5 FWD)\n");
    double ayStreet, vStreet, aySlick, vSlick;
    {
        Tune street, slick; slick.tires = TireType::SemiSlick;
        skidpad(5, street, 50.0, &ayStreet, &vStreet);
        skidpad(5, slick, 50.0, &aySlick, &vSlick);
        std::printf("    sokak: %.2f g @ %.1f km/h | yari-slick: %.2f g @ %.1f km/h\n", ayStreet, vStreet * 3.6, aySlick, vSlick * 3.6);
        CHECK(ayStreet > 0.80 && ayStreet < 1.10, "sokak lastigi yanal limit 0.80-1.10 g");
        CHECK(aySlick > ayStreet + 0.12, "yari-slick belirgin daha fazla yanal tutus");
    }

    // Sabit direksiyon + gaz: yol egriligi (yaw hizi / hiz) ve govde kayma acisi
    auto turnTest = [](int car, const Tune& t, double throttle, double* curvature, double* beta, double* maxBeta) {
        auto s = make(car, true, t);
        AutoDriver d;
        double vTarget = 13.0, integ = 0;
        *maxBeta = 0;
        for (int i = 0; i < (int)(14.0 / kDt); ++i) {
            const double time = i * kDt;
            VehicleInputs in;
            in.steer = time > 8.0 ? 0.06 : 0.0;
            double thr;
            if (time < 9.0) { const double e = vTarget - s->speed(); integ = std::clamp(integ + e * kDt, -5.0, 5.0); thr = 0.25 * e + 0.15 * integ + 0.15; }
            else thr = throttle;                                              // 9 s'den sonra sabit gaz
            d.apply(*s, thr, in);
            if (time > 9.0) d.shiftT = -1, s->powertrain().setClutchPedal(0.0), s->powertrain().setThrottle(throttle);   // vites sabit
            s->step(kDt, in);
            if (time > 9.0) *maxBeta = std::max(*maxBeta, std::fabs(s->bodySlipAngle()));
            if (!std::isfinite(s->speed())) break;
        }
        *curvature = s->yawRate() / std::max(s->speed(), 0.1);
        *beta = s->bodySlipAngle();
    };

    std::printf("[3] FWD guc altinda understeer (#5: sabit direksiyon, 9. s'den sonra gaz 0.15 vs 1.0)\n");
    {
        Tune t; double kLo, kHi, bLo, bHi, mLo, mHi;
        turnTest(5, t, 0.15, &kLo, &bLo, &mLo);
        turnTest(5, t, 1.0, &kHi, &bHi, &mHi);
        std::printf("    egrilik: az gaz %.4f 1/m, tam gaz %.4f 1/m\n", kLo, kHi);
        CHECK(kHi < kLo * 0.9, "tam gazda yol acildi (understeer)");
    }

    std::printf("[4] RWD guc altinda oversteer (#227 V8, sokak lastigi, 1.5-yol LSD)\n");
    {
        // Acik diferansiyelde ic teker bosa doner, tork sinirlanir (tek teker patinaji) -> hafif kayma;
        // gercek "arkayi atma" kilitli/LSD diferansiyel ister.
        Tune t; t.diff = DiffType::OneAndHalfWay; double kLo, kHi, bLo, bHi, mLo, mHi;
        turnTest(227, t, 0.15, &kLo, &bLo, &mLo);
        turnTest(227, t, 1.0, &kHi, &bHi, &mHi);
        std::printf("    tepe govde kayma acisi: az gaz %.1f deg, tam gaz %.1f deg\n", mLo * 57.3, mHi * 57.3);
        CHECK(mHi > 8.0 / 57.3 && mHi > 3.0 * mLo, "tam gazda arka kaydi (oversteer / spin)");
    }

    std::printf("[5] Dusuk hiz tam kilit (#5, 0.5 rad, 20 s surunme)\n");
    {
        Tune t;
        auto s = make(5, true, t);
        AutoDriver d;
        bool finite = true; double maxR = 0, minR = 1e9; int samples = 0;
        for (int i = 0; i < (int)(20.0 / kDt); ++i) {
            VehicleInputs in; in.steer = 0.5;
            d.apply(*s, 0.12, in);
            if (i * kDt > 1.2) { s->powertrain().setGear(1); d.shiftT = -1; }
            s->step(kDt, in);
            finite &= std::isfinite(s->posX()) && std::isfinite(s->yawRate());
            if (i * kDt > 8 && s->speed() > 0.5) { const double R = s->speed() / std::max(std::fabs(s->yawRate()), 1e-6); maxR = std::max(maxR, R); minR = std::min(minR, R); ++samples; }
        }
        const double Rkin = findVehicle(5)->wheelbaseM / std::tan(0.5);
        std::printf("    hiz %.2f m/s, donus yaricapi %.2f-%.2f m (kinematik %.2f m)\n", s->speed(), minR, maxR, Rkin);
        CHECK(finite, "NaN/sonsuz yok");
        CHECK(samples > 1000, "arac hareket etti (olcum var)");
        CHECK(minR > 0.7 * Rkin && maxR < 1.4 * Rkin, "yaricap kinematik degere yakin");
    }
    std::printf(failures ? "\nSONUC: %d test KALDI\n" : "\nSONUC: tum testler gecti\n", failures);
    return failures ? 1 : 0;
}
