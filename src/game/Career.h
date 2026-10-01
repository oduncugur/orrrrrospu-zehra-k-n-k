// ZEHRA KINIK - Kariyer: para, garaj (sahip olunan araclar + parcalari), ekonomi, parca katalogu, kayit.
#pragma once
#include "garage/VehicleCatalog.h"
#include "game/League.h"
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
    double engineWear = 0.0;      // 0..1 motor hasari (yatak / asiri zorlanma / isi); 1 = motor patlak, revizyon sart
    bool   gearboxBroken = false; // kirik sanziman: yarisamaz
    bool   fromJunk = false;      // hurdaliktan alindi (restorasyon ekrani)
    // Gorunum (boyahane): renk paleti indeksi (-1 fabrika), cila (0 parlak 1 metalik 2 mat 3 sedef), serit (0 yok 1 orta
    // 2 cift 3 yan) ve rengi, jant rengi (-1 fabrika)
    int paint = -1, finish = 0, stripe = 0, stripeCol = 0, rimCol = -1;
    bool damaged() const { return axleBroken || gearboxBroken || engineWear > 0.02; }
    bool raceable() const { return !axleBroken && !gearboxBroken && engineWear < 1.0; }
};

// Parca kategorileri (dukkan; PartTables tablolari). Her secenek Tune'da tek bir alani degistirir. Sekmeler: partTab().
enum class PartCat {
    // MOTOR
    Head, Valve, Cam, Piston, Rod, Crank, Bearing, Gasket, Flywheel, EngineSwap,
    // BESLEME
    Intake, Exhaust, Turbo, Turbine, Wastegate, BoostCtl, Supercharger, Intercooler, Nitrous, FuelSys, Fuel,
    // AKTARMA
    Clutch, Gearbox, GbStrength, FinalDrive, Diff, Axles,
    // SASI
    Tires, Rims, Suspension, Brakes, Weight, Aero,
    // ECU / SOGUTMA
    Ecu, Electronics, Cooling, DrySump,
    Count
};
constexpr int kPartTabs = 5;
const char* partTabName(int tab);
int  partTab(PartCat c);
// Atolyede (ozel uretim) uretilebilen kategori ve ozel satir indeksi (-1: yok)
int  customOption(PartCat c);

struct PartOption { const char* name; int basePrice; };
const char* partCatName(PartCat c);
const std::vector<PartOption>& partOptions(PartCat c);
int  partLevel(const Tune& t, PartCat c, const VehicleDef& v);   // takili seviye
void setPartLevel(Tune& t, PartCat c, int level, const VehicleDef& v);
int  partPrice(PartCat c, int level, const VehicleDef& v);      // arac sinifina gore olceklenir
// t: takili parcalar (ELEKTRONIK icin ECU sarti); nullptr ise yalniz arac kosullari denetlenir
bool partAvailable(PartCat c, int level, const VehicleDef& v, std::string* why = nullptr, const Tune* t = nullptr);

int  carPrice(const VehicleDef& v);
// Kayit: v2 parca alanlari "anahtar:deger,..." (eski surum bu satiri yok sayar)
std::string tuneV2String(const Tune& t);
void parseTuneV2(const std::string& v, Tune& t);
// Satis fiyati dokumu (galeri onay penceresi): arac + parcalar - hasar = toplam
struct SaleQuote { int car = 0, parts = 0, damage = 0, total = 0; };
long repairCostFor(const OwnedCar& c);
SaleQuote saleQuote(const OwnedCar& c);
int  sellPrice(const OwnedCar& c);
// Yaris degerlendirme endeksi (guc/agirlik + parca etkisi), rakip eslestirme icin
double performanceIndex(const VehicleDef& v, const Tune& t);
int  racePrize(const VehicleDef& opponent, bool won);

// Rakip: oyuncunun (parcali) performansina yakin arac + yapay zekanin parca seti (stok / sokak / drag)
// estEt: rakibin tahmini 1/4 mili (tablodan, s); playerEt: oyuncunun tahmini (eslesme ani)
struct Opponent { int carId; Tune tune; double index; double estEt = -1, playerEt = -1; };
Opponent pickOpponent(int playerCarId, const Tune& playerTune, uint32_t seed);
// YZ rakip parca setleri: 0 stok, 1 sokak (emme/egzoz, yari slick, 1.5W), 2 drag (slick, ECU, ST1, aks)
constexpr int kOpponentPresets = 3;
Tune opponentPreset(int i);
// Tahmini 1/4 mil (s): YZ setleri icin uretilmis tablo (EtTable.inc, yaristaki fizik); baska parca setinde lastik
// tipine en yakin satirdan guc/agirlik endeksiyle olceklenir (ET ~ endeks^-1/3). Tablo yoksa endeks formulu.
double estimatedEt(const VehicleDef& v, const Tune& t);
double tableEt(int carId, int preset);              // -1: yok / tamamlayamadi
// Odul zorluk carpani: rakip ne kadar hizliysa o kadar cok (0.5-1.8); gap = oyuncu ET - rakip ET
double prizeDifficulty(double gapSeconds);
// Kisa parca ozeti (HUD): "SLICK ST2 KM 1.5W T1"
std::string tuneSummary(const Tune& t);

// Hurdalik: hasarli araclar kelepir fiyata; bilesenleri harbi bitik (restorasyon emek ister)
struct JunkCar { int carId = 0; int price = 0; Tune tune; double engineWear = 0; bool axleBroken = false, gearboxBroken = false; };
std::vector<JunkCar> junkyardOffers(uint32_t seed);
// Restorasyon: bilesenler (motor, sanziman+aks, lastik, fren, suspansiyon, kaporta, elektrik)
constexpr int kRestoreParts = 7;
const char* restoreName(int comp);
double restoreDamage(const OwnedCar& c, int comp);       // 0..1
long restoreStepCost(const OwnedCar& c, int comp);       // bir adim (0: saglam)

struct Career {
    static constexpr int kVersion = 1;
    long money = 0;
    std::vector<OwnedCar> cars;
    int  current = 0;
    int  races = 0, wins = 0;
    long earnings = 0;
    bool treePro = false;
    int  form = 0;                         // son yaris formu -3..+3 (galibiyet +1, maglubiyet -1): rakip zorlugu
    int  lastOppId = 0, sameOppWins = 0;   // ayni rakibi tekrar tekrar yenme (odul azalir, para kasma engeli)
    long bestFlow = 0;                     // otoban akisi en iyi skoru
    // ---- ligler (League.h) ----
    int  rep = 0;                          // un puani
    uint64_t eventWins = 0;                // kazanilmis etkinlikler (leagueEvents indeksi -> bit)
    int  dailyDay = -1;                    // gunluk gorevlerin gunu
    long dailyProg[3] = {0, 0, 0};
    int  dailyDone = 0;                    // tamamlanan gorev bitleri
    uint32_t achieved = 0;                 // basarimlar (Achievements.h indeksi -> bit)
    // Yeni kazanilan basarimlar: odulu ode, bitini isle, indeksleri dondur
    std::vector<int> checkAchievements();
    int  leagueUnlocked() const;           // acik en yuksek lig (onceki ligin patronu yenildiyse)
    int  leagueWins(int league) const;
    bool eventWon(int idx) const { return idx >= 0 && idx < 64 && ((eventWins >> idx) & 1u); }
    // Etkinlige girilebilir mi (lig kilidi, patron icin 3 galibiyet, pink slip 2+ arac, sinif siniri)
    bool eventAvailable(int idx, std::string* why = nullptr) const;
    // Etkinlik sonucu: odul (ilk galibiyet tam, tekrar %40), un, pink slip (araci al / kaybet), gunluk gorevler.
    // Donus: odul. pinkOut: pink slip'te el degistiren arac kimligi (0: yok)
    long recordEvent(int idx, bool won, double et, long flowScore, int* pinkOut = nullptr);
    // Gunluk gorevler: gun degistiyse sifirlar; ilerleme ekler, tamamlananin odulunu verir (donus: verilen odul)
    void dailyRefresh();
    long dailyAdd(TaskType t, long amount);
    // Rakip sec: oyuncunun tahmini ET'sine (+ forma gore hedef) yakin, son rakipten farkli
    Opponent pickOpponentFor(uint32_t seed) const;

    static Career newGame();
    OwnedCar& car() { return cars[current]; }
    const OwnedCar& car() const { return cars[current]; }

    // Islemler (basarisizsa false + neden)
    bool buyCar(int carId, std::string* why = nullptr);
    bool sellCurrent(std::string* why = nullptr);
    bool buyPart(PartCat c, int level, std::string* why = nullptr);
    bool buyJunk(const JunkCar& j, std::string* why = nullptr);
    bool restoreStep(int comp, std::string* why = nullptr);  // secili arac: bir restorasyon adimi
    // prizeScale: yaris uzunluguna gore odul carpani (karma uzun yol), tekrar-galibiyet azalmasindan sonra uygulanir
    void recordRace(const VehicleDef& opponent, bool won, double et, long* prizeOut = nullptr, double prizeScale = 1.0);
    // Yaris sonu hasar: aks, motor (yatak / zorlanma 0..1, patlama), sanziman
    void recordDamage(bool axleBroke, double bearingDamage, bool bearingSpun, bool gearboxBroke = false, double engineStress = 0.0, double tireWear = 0.0);
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
