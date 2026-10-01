// ZEHRA KINIK - Acik yol: prosedurel yol ekseni (duzluk + klotoid gecisli virajlar).
// Nokta araligi 2 m; her nokta: konum, yon, egrilik. Arac konumundan yol koordinati (s, yanal sapma) bulunur.
#pragma once
#include <cstdint>
#include <vector>

namespace zk {

// z: yukseklik (m), grade: boyuna egim (dz/ds, yokus yukari +)
// hw: o noktadaki yol yari genisligi (m; yol boyunca daralir / genisler)
struct RoadPoint { double x, y, heading, curvature, s, z = 0.0, grade = 0.0, hw = 3.6; };
// Karma yarista yol bolumu: virajli (3B surus) ya da duz (drag gorunumu)
// entry: ilk virajin gercek basladigi yer; minR: bloktaki en dar yaricap (onerilen giris hizi icin)
struct RoadSection { double s0, s1; bool curvy; double entry = 0.0, minR = 0.0; };

class RoadPath {
public:
    // Karma yaris yolu: duz (drag) -> viraj blogu -> duz -> viraj blogu -> bitis duzlugu (bolumler sections())
    static RoadPath karma(uint32_t seed, double maxGrade);
    static constexpr double kKarmaLead = 450.0;  // virajli bolum ilk virajdan bu kadar once baslar (fren + kamera)
    const std::vector<RoadSection>& sections() const { return sections_; }
    bool curvyAt(double s) const;               // karma: s virajli bolumde mi (normal yolda hep false)
    static constexpr double kStep = 2.0;
    // lengthM: toplam uzunluk; minRadius: en dar viraj (otoban ~400, sehirlerarasi ~120, dag ~35)
    // maxGrade: en dik egim (0 = duz; sehirlerarasi ~0.05, dag ~0.09). Egim viraj programini degistirmez.
    RoadPath(uint32_t seed, double lengthM, double minRadius, double halfWidth = 3.6, double maxGrade = 0.0);

    const std::vector<RoadPoint>& points() const { return pts_; }
    double length() const { return pts_.back().s; }
    double halfWidth() const { return halfWidth_; }              // en dar yari genislik (serit yerlesimi)
    double halfWidthAt(double s) const { return at(s).hw; }
    // Genislik degisimi: yol boyunca 400-1100 m'lik bolumler [hwMin, hwMax] arasinda, 120 m'lik gecislerle
    void setWidthRange(uint32_t seed, double hwMin, double hwMax);
    // s'deki nokta (dogrusal ara degerleme); s sinirlara kirpilir
    RoadPoint at(double s) const;
    // (x,y) icin en yakin yol koordinati. hint: onceki indeks (yerel arama, O(1)); guncellenir
    void project(double x, double y, int& hint, double& s, double& lateral) const;   // lateral: sola +

private:
    struct Seg { double len, k0, k1; };          // egrilik programi parcasi (k0 -> k1 dogrusal)
    RoadPath() = default;
    void integrate(const std::vector<Seg>& prog, double lengthM);
    void addElevation(uint32_t seed, double maxGrade, double wl0, double wl1);
    std::vector<RoadPoint> pts_;
    std::vector<RoadSection> sections_;
    double halfWidth_ = 3.6;
};

} // namespace zk
