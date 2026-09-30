// ZEHRA KINIK - Acik yol: prosedurel yol ekseni (duzluk + klotoid gecisli virajlar).
// Nokta araligi 2 m; her nokta: konum, yon, egrilik. Arac konumundan yol koordinati (s, yanal sapma) bulunur.
#pragma once
#include <cstdint>
#include <vector>

namespace zk {

// z: yukseklik (m), grade: boyuna egim (dz/ds, yokus yukari +)
struct RoadPoint { double x, y, heading, curvature, s, z = 0.0, grade = 0.0; };

class RoadPath {
public:
    static constexpr double kStep = 2.0;
    // lengthM: toplam uzunluk; minRadius: en dar viraj (otoban ~400, sehirlerarasi ~120, dag ~35)
    // maxGrade: en dik egim (0 = duz; sehirlerarasi ~0.05, dag ~0.09). Egim viraj programini degistirmez.
    RoadPath(uint32_t seed, double lengthM, double minRadius, double halfWidth = 3.6, double maxGrade = 0.0);

    const std::vector<RoadPoint>& points() const { return pts_; }
    double length() const { return pts_.back().s; }
    double halfWidth() const { return halfWidth_; }
    // s'deki nokta (dogrusal ara degerleme); s sinirlara kirpilir
    RoadPoint at(double s) const;
    // (x,y) icin en yakin yol koordinati. hint: onceki indeks (yerel arama, O(1)); guncellenir
    void project(double x, double y, int& hint, double& s, double& lateral) const;   // lateral: sola +

private:
    std::vector<RoadPoint> pts_;
    double halfWidth_;
};

} // namespace zk
