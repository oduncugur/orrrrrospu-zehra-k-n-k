// Acik dunya direksiyon olcumu: hiz basina tam direksiyonla kararli donus yaricapi ve yanal ivme
#include "game/RoadCar.h"
#include "garage/VehicleCatalog.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace zk;


int main(int argc, char** argv) {
    const int carId = argc > 1 ? std::atoi(argv[1]) : 5;
    const RoadPath road = RoadPath::polyline({{0, 0}, {5000, 0}}, 2000.0, 1, 1, 50.0);
    for (double kmh : {10.0, 20.0, 30.0, 50.0, 70.0, 100.0, 140.0}) {
        RoadCar car(findVehicle(carId), nullptr, road, 100.0, 0.0);
        car.freeRoam = true; car.stability = true; car.esp = false; car.assist = true;
        const double vt = kmh / 3.6;
        double steer = 0, t = 0, r = 0, ay = 0, beta = 0, h0 = 0, d1 = 0, d2 = 0, bmax = 0;
        for (int i = 0; i < 60 * 30; ++i, t += 1.0 / 60) {
            VehicleSim& s = car.sim();
            const double v = s.speed();
            RoadControls c;
            c.throttle = std::clamp((vt - v) * 0.6 + 0.12 * vt / 30.0, 0.0, 1.0);
            c.brake = v > vt + 1.0 ? 0.3 : 0.0;
            c.autoMode = 0;
            const bool turning = t > 12.0;
            if (std::fabs(t - 12.0) < 0.5 / 60) h0 = s.heading();
            if (std::fabs(t - 13.0) < 0.5 / 60) d1 = s.heading() - h0;
            if (std::fabs(t - 14.0) < 0.5 / 60) d2 = s.heading() - h0;
            if (t > 12.0 && s.speed() > 8.0) bmax = std::max(bmax, std::fabs(s.bodySlipAngle()));
            const double target = turning ? RoadCar::steerLimit(s, v, 1.0) : 0.0;
            const double rate = RoadCar::steerRate(v, std::fabs(target) > std::fabs(steer)) / 60.0;
            steer += std::clamp(target - steer, -rate, rate);
            c.steer = steer;
            car.update(1.0 / 60, c);
            if (t > 26.0) { r = s.yawRate(); ay = s.speed() * r; beta = s.bodySlipAngle(); }
        }
        const double v = car.sim().speed();
        std::printf("%5.0f km/h -> v=%5.1f km/h steer=%.3f yaricap=%6.1f m  ay=%.2f g  beta=%.1f deg  1s:%.0f 2s:%.0f deg maxbeta=%.1f\n", kmh, v * 3.6, steer,
                    std::fabs(r) > 1e-3 ? v / std::fabs(r) : 9999.0, std::fabs(ay) / 9.81, beta * 57.3, d1 * 57.3, d2 * 57.3, bmax * 57.3);
    }
}
