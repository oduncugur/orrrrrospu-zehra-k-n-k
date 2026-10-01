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
    // Kalici hasar (yarislar arasi tasinir; tamir edilene kadar kalir)
    bool   axleBroken = false;    // kirik aks: yarisamaz
    double engineWear = 0.0;      // 0..1 krank yatagi hasari; 1 = yatak sarmis, motor revizyonu sart
    bool damaged() const { return axleBroken || engineWear > 0.02; }
    bool raceable() const { return !axleBroken && engineWear < 1.0; }
};

// Parca kategorileri ve seviyeleri (dukkan). Her secenek Tune'da tek bir alani degistirir.
enum class PartCat { Tires, Clutch, Axles, Diff, FinalDrive, Weight, Intake, Exhaust, Ecu, Turbo, DrySump, Fuel, Electronics, Count };

struct PartOption { const char* name; int basePrice; };
const char* partCatName(PartCat c);
const std::vector<PartOption>& partOptions(PartCat c);
int  partLevel(const Tune& t, PartCat c, const VehicleDef& v);   // takili seviye
void setPartLevel(Tune& t, PartCat c, int level, const VehicleDef& v);
int  partPrice(PartCat c, int level, const VehicleDef& v);      // arac sinifina gore olceklenir
// t: takili parcalar (ELEKTRONIK icin ECU sarti); nullptr ise yalniz arac kosullari denetlenir
bool partAvailable(PartCat c, int level, const VehicleDef& v, std::string* why = nullptr, const Tune* t = nullptr);

int  carPrice(const VehicleDef& v);
// Satis fiyati dokumu (galeri onay penceresi): arac + parcalar - hasar = toplam
struct SaleQuote { int car = 0, parts = 0, damage = 0, total = 0; };
long repairCostFor(const OwnedCar& c);
SaleQuote saleQuote(const OwnedCar& c);
int  sellPrice(const OwnedCar& c);
// Yaris degerlendirme endeksi (guc/agirlik + parca etkisi), rakip eslestirme icin
double performanceIndex(const VehicleDef& v, const Tune& t);
int  racePrize(const VehicleDef& opponent, bool won);

// Rakip: oyuncunun (parcali) performansina yakin arac + yapay zekanin parca seti (stok / sokak / drag)
struct Opponent { int carId; Tune tune; double index; };
Opponent pickOpponent(int playerCarId, const Tune& playerTune, uint32_t seed);
// Kisa parca ozeti (HUD): "SLICK ST2 KM 1.5W T1"
std::string tuneSummary(const Tune& t);

struct Career {
    static constexpr int kVersion = 1;
    long money = 0;
    std::vector<OwnedCar> cars;
    int  current = 0;
    int  races = 0, wins = 0;
    long earnings = 0;
    bool treePro = false;
    int  lastOppId = 0, sameOppWins = 0;   // ayni rakibi tekrar tekrar yenme (odul azalir, para kasma engeli)
    long bestFlow = 0;                     // otoban akisi en iyi skoru

    static Career newGame();
    OwnedCar& car() { return cars[current]; }
    const OwnedCar& car() const { return cars[current]; }

    // Islemler (basarisizsa false + neden)
    bool buyCar(int carId, std::string* why = nullptr);
    bool sellCurrent(std::string* why = nullptr);
    bool buyPart(PartCat c, int level, std::string* why = nullptr);
    void recordRace(const VehicleDef& opponent, bool won, double et, long* prizeOut = nullptr);
    void recordDamage(bool axleBroke, double bearingDamage, bool bearingSpun);   // yaris sonu
    // Otoban akisi sonu: odul = skor / 20 (en fazla kFlowPrizeCap); rekor kirilirsa +%50. Donus: odul
    static constexpr long kFlowPrizeCap = 4000;
    long recordFlow(long score, bool* newRecord = nullptr);
    long repairCost() const;                                                    // secili arac
    bool repairCurrent(std::string* why = nullptr);

    std::string serialize() const;
    static bool parse(const std::string& text, Career& out);   // bozuk/eksik veride false
    bool save(const std::string& path) const;                  // atomik: gecici dosya + yeniden adlandirma
    // corrupt: dosya var ama okunamadi (bozuk dosya .bozuk olarak saklanir, yeni oyun baslar)
    static Career loadOrNew(const std::string& path, bool* corrupt = nullptr);
};

} // namespace zk
