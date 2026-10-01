// ZEHRA KINIK - Arac parca / ayar durumu (kariyerde kaydedilir, fizikte uygulanir)
// Her alan fizikte gercek bir parametreyi degistirir; bkz. applyTune (VehicleSim.cpp).
#pragma once
#include "sim/PowertrainCore.h"

namespace zk {

enum class TireType { Street = 0, SemiSlick = 1, DragSlick = 2 };
enum class DiffType { Open = 0, OneAndHalfWay = 1, TwoWay = 2, Spool = 3 };

struct Tune {
    TireType tires = TireType::Street;
    double   psi = 0.0;           // 0 = lastik tipinin varsayilani
    int      clutch = 0;          // 0 stok, 1 stage 1 (+%30), 2 stage 2 (+%60), 3 stage 3 metal (+%100)
    int      axles = 0;           // 0 stok, 1 krom-moly 4340 (+%12 cap, ~2.1x), 2 yaris 300M (+%25 cap, ~3.4x)
    DiffType diff = DiffType::Open;
    double   finalDrive = 0.0;    // 0 = fabrika
    int      weight = 0;          // 0..3 hafifletme kademesi (-0 / -40 / -85 / -140 kg)
    int      intake = 0;          // 0..2 (+%0 / +%3 / +%6 tork)
    int      exhaust = 0;         // 0..2 (stok / spor / duz boru: +%0 / +%3 / +%6)
    int      ecu = 0;             // 0..1 (atmosferik +%5, turbolu +%12)
    int      turbo = 0;           // 0 yok/fabrika, 1 kucuk kit (+0.6 bar), 2 buyuk kit (+1.2 bar, gec spool)
    bool     drySump = false;
    bool     absKit = false, tcKit = false;   // sonradan ABS / cekis kontrolu (ECU yukseltmesi sart)
    FuelType fuel = FuelType::Race100;

    static int weightKg(int level) { static const int w[4] = {0, 40, 85, 140}; return w[level < 0 ? 0 : level > 3 ? 3 : level]; }
};

} // namespace zk
