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
enum Material { MatPaint = 0, MatGlass, MatTire, MatRim, MatLight, MatTail, MatTrim, MatDark, MatPlate, MatChrome, MatIndicator, MatCount };

// Teker parcasi: ucgen araligi + merkez (oyunda doner / on tekerler direksiyonla sapar). Sira: on sol, on sag, arka sol, arka sag
struct WheelPart { float cx, cy, cz, r; size_t triBegin, triEnd; };
struct LowPolyMesh {
    std::vector<Vertex> verts;
    std::vector<Tri>    tris;
    std::vector<WheelPart> wheels;
    unsigned paintRGB = 0xC0C0C0;
};

LowPolyMesh buildVehicleMesh(const VehicleDef& v);
// Malzeme rengi (0..1); boya icin mesh'in paintRGB'si kullanilir. OBJ/MTL ve oyun ayni tabloyu kullanir.
void materialColor(int material, unsigned paintRGB, float out[3]);
bool writeObj(const LowPolyMesh& m, const std::string& objPath, const std::string& mtlName, const std::string& title);
bool writeMtl(const LowPolyMesh& m, const std::string& mtlPath);

} // namespace zk
