// ZEHRA KINIK - Kariyer: para, garaj (sahip olunan araclar + parcalari), ekonomi, parca katalogu, kayit.
#pragma once
#include "garage/VehicleCatalog.h"
#include "sim/Tune.h"

#include <cstdint>
#include <string>
#include <vector>

namespace zk {

struct OwnedCar {
    int  carId = 0;
    Tune tune;
    int  races = 0, wins = 0;
    double bestEt = 0.0;          // en iyi 1/4 mil (0 = yok)
    int  paidParts = 0;           // takilan parcalara odenen toplam (satista kismi geri donus)
};

// Parca kategorileri ve seviyeleri (dukkan). Her secenek Tune'da tek bir alani degistirir.
enum class PartCat { Tires, Clutch, Axles, Diff, FinalDrive, Weight, Intake, Exhaust, Ecu, Turbo, DrySump, Fuel, Count };

struct PartOption { const char* name; int basePrice; };
const char* partCatName(PartCat c);
const std::vector<PartOption>& partOptions(PartCat c);
int  partLevel(const Tune& t, PartCat c, const VehicleDef& v);   // takili seviye
void setPartLevel(Tune& t, PartCat c, int level, const VehicleDef& v);
int  partPrice(PartCat c, int level, const VehicleDef& v);      // arac sinifina gore olceklenir
bool partAvailable(PartCat c, int level, const VehicleDef& v, std::string* why = nullptr);

int  carPrice(const VehicleDef& v);
int  sellPrice(const OwnedCar& c);
// Yaris degerlendirme endeksi (guc/agirlik + parca etkisi), rakip eslestirme icin
double performanceIndex(const VehicleDef& v, const Tune& t);
int  racePrize(const VehicleDef& opponent, bool won);

struct Career {
    static constexpr int kVersion = 1;
    long money = 0;
    std::vector<OwnedCar> cars;
    int  current = 0;
    int  races = 0, wins = 0;
    long earnings = 0;
    bool treePro = false;

    static Career newGame();
    OwnedCar& car() { return cars[current]; }
    const OwnedCar& car() const { return cars[current]; }

    // Islemler (basarisizsa false + neden)
    bool buyCar(int carId, std::string* why = nullptr);
    bool sellCurrent(std::string* why = nullptr);
    bool buyPart(PartCat c, int level, std::string* why = nullptr);
    void recordRace(const VehicleDef& opponent, bool won, double et, long* prizeOut = nullptr);

    std::string serialize() const;
    static bool parse(const std::string& text, Career& out);   // bozuk/eksik veride false
    bool save(const std::string& path) const;                  // atomik: gecici dosya + yeniden adlandirma
    static Career loadOrNew(const std::string& path);
};

} // namespace zk
