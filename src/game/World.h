// ZEHRA KINIK - Acik dunya (ayri surum: zehra_world): butun sehirlerden gecen otoban + her sehirde kendi olcusunde kavsakli sokak
// izgarasi, binalar, benzinlikler (fiyatli), bulusma meydanlari, hurdaliklar, yaris baslangiclari. Duz zemin (z = 0).
// Yol agi: her yol (kenar) iki yonlu iki RoadPath (ileri / geri); arac her karede en uygun yola gecer (kavsakta donus).
#pragma once
#include "game/RoadPath.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace zk {

struct WorldCity { std::string name; double x, y, dirX, dirY, r; double fuelPrice; int style; double hu, hv; };   // hu / hv: yari boy (dogu-bati / kuzey-guney, m)   // style: sehir kimligi (sekil / su / doku)
struct WorldBuilding { double cx, cy, hu, hv, ux, uy, h; int city; float tone; };   // yonlu kutu: merkez, yari boylar (u: sehir ekseni), yukseklik
enum WorldPoiType { WPoiRace = 0, WPoiMeet = 1, WPoiJunk = 2, WPoiGas = 3, WPoiGarage = 4 };
struct WorldPoi { int type; double x, y, heading; int city; int ref; std::string name; };
struct WorldNode { double x, y; std::vector<int> edges; };
// Simge yapilar: tip (cizim), konum, olcu, ad. Carpisma: yaricap r (silindir).
enum LandmarkType { LmTower = 0, LmMaidenTower, LmPylon, LmMausoleum, LmTvTower, LmGreenDome, LmMosque, LmMountain, LmFairy, LmBalloon,
                    LmClock, LmCastle, LmSkyscraper, LmMinaret, LmGate };
// Hiz kamerasi (yol uzerinde, gidis yonu heading); limit m/s
struct WorldCamera { double x, y, heading, limit; int city; };
// Toplanabilir: 0 nadir parca (sokak kenari), 1 ahir bulgusu (sehir disi)
struct WorldCollect { double x, y; int type, city, idx; };
struct WorldLandmark { int type; double x, y, r, h, heading; int city; std::string name; };
// Yol (kenar): kose noktalari tutulur; surus hatti (RoadPath) ilk kullanimda uretilir (4x4 km sehirlerde bellek)
struct WorldEdge {
    std::vector<std::pair<double, double>> pts;
    double hw; int lanes; double cornerR, len;
    int a, b; bool highway; int city;   // city: -1 sehirlerarasi
    bool bridge = false;                // su ustunden gecer (korkuluk)
    double minX, minY, maxX, maxY;      // sinir kutusu (yol genisligi dahil)
    const RoadPath& fwd() const;        // a -> b
    const RoadPath& rev() const;        // b -> a
    mutable std::unique_ptr<RoadPath> fwd_, rev_;
};
// Yonlu kenar: (kenar, ters mi)
struct WorldLeg { int edge; bool rev; };

class World {
public:
    static const World& get();          // tek, belirlenimci dunya
    std::vector<WorldCity> cities;
    std::vector<WorldNode> nodes;
    std::vector<WorldEdge> edges;
    std::vector<WorldBuilding> buildings;
    std::vector<WorldPoi> pois;
    std::vector<WorldLandmark> landmarks;
    std::vector<WorldCamera> cameras;
    std::vector<WorldCollect> collect;
    double minX = 0, minY = 0, maxX = 0, maxY = 0;
    static constexpr double kBlock = 333.0;   // baslangic konumu icin ornek blok
    // Sehir yari boylari (m): gercek olcege yakin (Istanbul 10 x 7 km ... Nigde 3 x 2.5 km)
    static double halfU(int style);
    static double halfV(int style);

    const RoadPath& path(const WorldLeg& l) const { return l.rev ? edges[l.edge].rev() : edges[l.edge].fwd(); }
    int cityAt(double x, double y) const;     // -1: sehir disi
    // Zemin: 0 sehir (beton), 1 su (deniz / bogaz / nehir), 2 yesil (park / dag / sehir disi)
    int surfaceAt(double x, double y) const;
    static int surfaceLocal(int style, double u, double v);   // sehir yerel ekseninde (u: otoban yonu, v: sol)
    // (x, y, yon) icin en uygun yonlu yol; maxLat: yoldan bu kadar uzaksa false
    bool nearestLeg(double x, double y, double heading, WorldLeg& out, double& s, double& lat, double maxLat = 25.0) const;
    // Rota: bulunulan yoldan hedefe en yakin yol noktasina yonlu kenar dizisi (ilk eleman bulunulan yol)
    std::vector<WorldLeg> route(const WorldLeg& from, double s0, double tx, double ty) const;
    // Kenarin b ucundaki (yonlu) cikislar
    std::vector<WorldLeg> exits(const WorldLeg& l) const;
    int endNode(const WorldLeg& l) const { return l.rev ? edges[l.edge].a : edges[l.edge].b; }
    double fuelPriceAt(int city, int week) const;   // TL / litre (sehir disi: en yakin sehir)

private:
    World();
    void build();
    int addNode(double x, double y);
    int addEdge(int a, int b, const std::vector<std::pair<double, double>>& pts, bool highway, int city);
};

} // namespace zk
