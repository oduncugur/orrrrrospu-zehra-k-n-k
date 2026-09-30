// ZEHRA KINIK - Faz 2: Prosedurel low-poly (PS1 tarzi) arac modeli
// Kasa, gercek olculerden (uzunluk/genislik/yukseklik/dingil/kasa tipi) istasyon kesitleri
// loft edilerek uretilir. Gercek bir tasarimin kopyasi degildir, logo/amblem icermez.
#pragma once
#include "VehicleCatalog.h"
#include <string>
#include <vector>

namespace zk {

struct Vertex { float x, y, z; };        // x: ileri, y: sol, z: yukari (m)
struct Tri    { int a, b, c; int material; };
enum Material { MatPaint = 0, MatGlass, MatTire, MatRim, MatLight, MatTail, MatTrim, MatCount };

struct LowPolyMesh {
    std::vector<Vertex> verts;
    std::vector<Tri>    tris;
    unsigned paintRGB = 0xC0C0C0;
};

LowPolyMesh buildVehicleMesh(const VehicleDef& v);
bool writeObj(const LowPolyMesh& m, const std::string& objPath, const std::string& mtlName, const std::string& title);
bool writeMtl(const LowPolyMesh& m, const std::string& mtlPath);

} // namespace zk
