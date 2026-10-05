// ZEHRA KINIK - Acik dunya (ayri surum: zehra_world): butun sehirlerden gecen otoban + her sehirde kavsakli sokak
// izgarasi, binalar, benzinlikler (fiyatli), bulusma meydanlari, hurdaliklar, yaris baslangiclari. Duz zemin (z = 0).
// Yol agi: her yol (kenar) iki yonlu iki RoadPath (ileri / geri); arac her karede en uygun yola gecer (kavsakta donus).
#pragma once
#include "game/RoadPath.h"

#include <string>
#include <utility>
#include <vector>

namespace zk {

struct WorldCity { std::string name; double x, y, dirX, dirY, r; double fuelPrice; };
struct WorldBuilding { double cx, cy, hu, hv, ux, uy, h; int city; float tone; };   // yonlu kutu: merkez, yari boylar (u: sehir ekseni), yukseklik
enum WorldPoiType { WPoiRace = 0, WPoiMeet = 1, WPoiJunk = 2, WPoiGas = 3 };
struct WorldPoi { int type; double x, y, heading; int city; int ref; std::string name; };
struct WorldNode { double x, y; std::vector<int> edges; };
struct WorldEdge {
    RoadPath fwd, rev;                  // a -> b ve b -> a
    int a, b; bool highway; int city;   // city: -1 sehirlerarasi
    double minX, minY, maxX, maxY;      // sinir kutusu (yol genisligi dahil)
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
    double minX = 0, minY = 0, maxX = 0, maxY = 0;
    static constexpr double kBlock = 150.0;   // sehir blok araligi (m)
    static constexpr int kGrid = 5;           // 5 x 5 kavsak

    const RoadPath& path(const WorldLeg& l) const { return l.rev ? edges[l.edge].rev : edges[l.edge].fwd; }
    int cityAt(double x, double y) const;     // -1: sehir disi
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
