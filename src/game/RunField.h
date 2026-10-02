// ZEHRA KINIK - The Run (uzun etap) rakip alani: 10-200 arac, fizige dayali hafif model (tam VehicleSim degil).
// Her rakip: guc / kutle / hava direnci / lastik tutusu (arac + parca setinden), yol egimi ve viraj siniri, surus
// tarzi (dip gaz, eko, dengeli, stok, canavar) ve yakit (harcanan guc x BSFC x tarz; etap sikistirma carpani).
// Benzinlik molasi: bir sonraki benzinlige / bitise yetmeyecekse (dip gaz: depo %60'in altinda her benzinlikte) durur.
#pragma once
#include "game/RoadPath.h"
#include "sim/Tune.h"

#include <cstdint>
#include <string>
#include <vector>

namespace zk {

enum RunStyle { StyleBalanced = 0, StyleFlatOut, StyleEco, StyleStock, StyleMonster, StyleCount };
const char* runStyleName(int style);

struct RunEntrant { int carId; Tune tune; int style; };

struct Runner {
    int carId = 0, style = 0;
    std::string name;
    double s = 0, v = 0, lane = 0; int li = 0;
    double fuelL = 50, tankL = 50, usedL = 0;      // depo / kalan / harcanan (etap ozeti)
    double massKg = 1200, powerW = 80000, cdA = 0.7, mu = 1.0, vmax = 50;
    double bsfcMul = 1.0, cruise = 1.0, thrMax = 1.0;
    double pitS = -1, pitWait = 0; int pits = 0;   // durma noktasi (-1: yok), benzinlikte bekleme
    bool refueling = false, dry = false, finished = false;
    double finishT = 0, spin = 0, steer = 0;
};

class RunField {
public:
    // compression: surulen her metre yakitta bu kadar metre sayilir (etap gercek mesafesi / surulen)
    // playerSlot: gridde oyuncuya ayrilan sira (bos birakilir)
    void init(const std::vector<RunEntrant>& field, const RoadPath& road, double startS, double compression, uint32_t seed, int playerSlot = -1);
    // Bir adim: playerS / playerLat / playerV: oyuncu (engel); stations: benzinlik baslangiclari
    void update(double dt, const RoadPath& road, const std::vector<double>& stations, double stationLen, double refuelLps,
                double goalS, double raceT, double playerS, double playerLat, double playerV);
    std::vector<Runner>& runners() { return r_; }
    const std::vector<Runner>& runners() const { return r_; }
    // Siralama: oyuncu (s, bitis suresi) dahil 1..N+1
    int playerPosition(double playerS, bool playerFinished, double playerT) const;
    static double tankFor(int carId);               // gercekci depo (L): sinif / kutleye gore 40-90
private:
    std::vector<Runner> r_;
    double comp_ = 1.0, startS_ = 0.0;
};

} // namespace zk
