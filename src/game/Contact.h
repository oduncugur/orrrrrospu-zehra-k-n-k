// ZEHRA KINIK - Iki arac arasinda govde temasi: yonlu kutu (OBB) ayirici eksen testi + impuls cozumu.
// Kutle ve yaw ataleti dikkate alinir: arkadan vuran hiz aktarir, yan temas savurur, sert darbe hiz keser.
#pragma once
#include "sim/VehicleSim.h"

namespace zk {

struct ContactResult {
    bool   touching = false;
    double depth = 0;          // gecme derinligi (m)
    double impulse = 0;        // normal impuls (N s)
    double closingSpeed = 0;   // temas anindaki yaklasma hizi (m/s)
};

struct CarBox { VehicleSim* sim; double halfL, halfW; };

// Cakisiyorsa konumlari ayirir ve impuls uygular. restitution: 0 (plastik) .. 1 (esnek); friction: yanal surtunme
ContactResult resolveContact(CarBox a, CarBox b, double restitution = 0.25, double friction = 0.4);

} // namespace zk
