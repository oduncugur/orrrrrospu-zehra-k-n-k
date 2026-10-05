// ZEHRA KINIK - Kariyer: para, garaj (sahip olunan araclar + parcalari), ekonomi, parca katalogu, kayit.
#pragma once
#include "garage/VehicleCatalog.h"
#include "game/League.h"
#include "sim/Tune.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include "game/RunField.h"
#include <vector>

namespace zk {

std::string setupString(const Tune& t);                    // kurulum profili metni
bool applySetupString(const std::string& s, Tune& t);    // profil metnini uygula (parcalara gore sinirlar)

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
    // Kurulum profilleri (DRAG / YOL / PIST): kurulum alanlari tuneV2String bicimiyle; bos = kayitli degil
    std::string profile[3];
    double nosFill = 1.0;         // NOS tupu doluluk (yarista harcanir, garajda parayla dolar)
    // Musteri araci (sokak / musteri isi): hedef beygir ve odeme; 0 = kendi aracin. Yarisamaz, satilamaz.
    int  jobHp = 0; long jobReward = 0;
    // Kadran (gosterge): secili stil (0 fabrika, 1 analog, 2 dijital, 3 ikisi), satin alinanlar (bit: stil; 8 turbo gostergesi)
    int  gauge = 0, gaugeOwned = 0; bool boostGauge = false;
    bool hasProfile(int k) const { return k >= 0 && k < 3 && !profile[k].empty(); }
    bool damaged() const { return axleBroken || gearboxBroken || engineWear > 0.02; }
    bool raceable() const { return !axleBroken && !gearboxBroken && engineWear < 1.0 && jobHp == 0; }
};

// Parca kategorileri (dukkan; PartTables tablolari). Her secenek Tune'da tek bir alani degistirir. Sekmeler: partTab().
// Ayni anda takilabilen gercek parcalar ayri kategoridir (filtre + kelebek + manifold, balata + disk + kaliper ...).
enum class PartCat {
    // MOTOR
    Head, Valve, Cam, Piston, Rod, Crank, Bearing, MainSupport, Gasket, HeadStud, Flywheel, EngineSwap,
    // EMME / EGZOZ
    Intake, Throttle, IntakeMani, Header, Catalyst, Exhaust,
    // TURBO / YAKIT
    Turbo, Turbine, Wastegate, BoostCtl, Supercharger, Intercooler, Meth, Nitrous, FuelPump, Injector, FuelLine, Fuel,
    // AKTARMA
    Clutch, Gearbox, GbStrength, FinalDrive, Diff, Axles,
    // SASI
    Tires, RoadTires, Rims, Suspension, Brakes, BrakeDisc, BrakeCaliper,
    // KAPORTA (hafifletme + aero)
    Weight, WeightBody, WeightGlass, WeightChassis, AeroFront, AeroSide, Aero, AeroUnder,
    // ECU / SOGUTMA / YAG
    Ecu, Electronics, Cooling, Fan, CoolMisc, DrySump, OilCooler, OilPump,
    Adas,                                   // surus yardimi (hiz sabitleyici / adaptif / serit / otonom)
    Count
};
constexpr int kPartTabs = 7;
const char* partTabName(int tab);
// Surus yardimi: fabrika seviyesi model yilindan (1998+ hiz sabitleyici, 2014+ adaptif, 2020+ serit takip);
// takili seviye = max(fabrika, satin alinan)
int adasFactory(const VehicleDef& v);
int adasLevel(const VehicleDef& v, const Tune& t);
const char* adasName(int level);
int  partTab(PartCat c);
bool usedAvailable(PartCat c, int level);   // ikinci el satiliyor mu (aktarma, atolye ve ucretsiz parcalar haric)
int  usedPrice(int newPrice);               // %55
int  gaugeStylePrice(int style);             // kadran stili fiyati (0: fabrika)
constexpr int kBoostGaugePrice = 350;        // turbo basinc gostergesi
double marketMul(PartCat c, int week);
double peakHpOf(const VehicleDef& v, const Tune& t);   // fizikteki en yuksek beygir (kesici altinda)       // haftalik parca pazari carpani (0.80..1.20; week < 0: 1)
void applyUsedWear(Tune& t, PartCat c);     // ikinci el parcanin yipranmasi
int  ecuSwPrice(int sw, int level, const VehicleDef& v);   // yazilim modulunun bu seviyesinin fiyati (arac sinifina gore)
void migrateTune(Tune& t);                  // eski kayit: tek yakit sistemi / ECU paketi -> ayri parcalar + yazilim
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
    long wager = 0;            // siradaki lig yarisinin bahsi (kaydedilmez): kazanirsa + bahis, kaybederse - bahis
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
    // Donus: net para degisimi (odul + bahis; kaybedilen bahiste negatif)
    long recordEvent(int idx, bool won, double et, long flowScore, int* pinkOut = nullptr);
    long eventPrize(int idx, bool first) const;     // lig carpani dahil odul (tekrar %40)
    static constexpr int kWagerSteps = 5;           // bahis: odulun 0, 0.5, 1, 2, 3 kati
    long wagerFor(int idx, int step) const;         // paraya gore sinirli
    // Gunluk gorevler: gun degistiyse sifirlar; ilerleme ekler, tamamlananin odulunu verir (donus: verilen odul)
    void dailyRefresh();
    long dailyAdd(TaskType t, long amount);
    // Rakip sec: oyuncunun tahmini ET'sine (+ forma gore hedef) yakin, son rakipten farkli
    // etOffset: hedef ET kaydirmasi (negatif = daha hizli rakip; haftalik turnuva turlari)
    Opponent pickOpponentFor(uint32_t seed, double etOffset = 0.0) const;

    static Career newGame();
    OwnedCar& car() { return cars[current]; }
    const OwnedCar& car() const { return cars[current]; }

    // Islemler (basarisizsa false + neden)
    bool buyCar(int carId, std::string* why = nullptr);
    bool sellCurrent(std::string* why = nullptr);
    // Parca al ve tak. used: ikinci el (%55 fiyat, ilgili bilesene yipranma ekler; aktarma / atolye parcasi yok).
    // Sokulen eski (stok olmayan) parca %35'e satilir: refund (verildiyse) ciktisi.
    // Kurulum profilleri (0 DRAG, 1 YOL, 2 PIST): kaydet / yukle (gecerli parcalara gore sinirlanir)
    int  marketWeek() const;                     // pazar haftasi (-1: kapali, testler)
    int  shopPrice(PartCat c, int level) const;  // dukkan fiyati: liste x haftalik pazar
    bool marketOff = false;                      // testler: sabit liste fiyati (kaydedilmez)
    void saveProfile(int k);
    void recordNosUse(double leftFrac);          // yaris sonu tupte kalan (0..1)
    int  nosRefillPrice() const;                 // 0: dolu / kit yok
    bool refillNos(std::string* why = nullptr);
    bool loadProfile(int k);
    bool buyPart(PartCat c, int level, std::string* why = nullptr, bool used = false, long* refund = nullptr);
    // ECU yazilimi: modulu bir seviye yukselt (yuva / ECU siniri / para) ya da bir seviye dusur (iade yok)
    bool buyEcuSoftware(int sw, std::string* why = nullptr);
    void dropEcuSoftware(int sw);
    bool buyJunk(const JunkCar& j, std::string* why = nullptr);
    bool restoreStep(int comp, std::string* why = nullptr);  // secili arac: bir restorasyon adimi
    // prizeScale: yaris uzunluguna gore odul carpani (karma uzun yol), tekrar-galibiyet azalmasindan sonra uygulanir
    void recordRace(const VehicleDef& opponent, bool won, double et, long* prizeOut = nullptr, double prizeScale = 1.0);
    // Yaris sonu hasar: aks, motor (yatak / zorlanma 0..1, patlama), sanziman
    void recordDamage(bool axleBroke, double bearingDamage, bool bearingSpun, bool gearboxBroke = false, double engineStress = 0.0, double tireWear = 0.0);
    // Otoban akisi sonu: odul = skor / 20 (en fazla kFlowPrizeCap); rekor kirilirsa +%50. Donus: odul
    static constexpr long kFlowPrizeCap = 4000;
    long recordFlow(long score, bool* newRecord = nullptr);
    // Polis kovalamacasi: kacis odulu (temassiz +%50), yakalanma cezasi (donus negatif)
    long recordChase(bool escaped, int collisions);
    int  chaseEscapes = 0;
    // Sponsor sozlesmesi: N yarista W galibiyet; galibiyet basina prim, tamamlaninca bonus, tutturamazsa ceza
    int  sponsor = -1, spWins = 0, spRaces = 0;
    uint32_t sponsorsDone = 0;
    int  sponsorOffer() const;                       // su an teklif eden sponsor (-1: yok / sozlesme surerken)
    bool signSponsor(int s);
    std::string sponsorNote;                         // son sponsor olayi (ekranda gosterilir, kaydedilmez)
    void sponsorRace(bool won);                      // her yaris sonunda (lig / serbest yaris)
    // Garaj yuvalari: baslangic 4, en fazla 12; dolu garaja galeri / hurdalik / pink slip araci giremez
    static constexpr int kStartSlots = 4, kMaxSlots = 12;
    int  garageSlots = kStartSlots;
    bool garageFull() const { return (int)cars.size() >= garageSlots; }
    long slotPrice() const;                // sonraki yuvanin fiyati (her yuva %60 pahali)
    bool buySlot(std::string* why = nullptr);
    // Haftalik turnuva: 3 tur drag eleme (her tur rakip hizlanir), kaybeden o hafta elenir; 3 tur = buyuk odul + un
    static constexpr int kTourRounds = 3;
    int  tourWeek = -1, tourRound = 0;
    bool tourOut = false;
    void tourRefresh();                    // yeni hafta: sifirla
    long tourEntry() const;                // giris ucreti (ilk tur)
    long tourPrize() const;                // sampiyonluk odulu (acik lige gore)
    bool tourAvailable(std::string* why = nullptr) const;
    bool tourStart(std::string* why = nullptr);   // ilk turda giris ucreti alinir
    long recordTour(bool won);             // donus: verilen odul (yalniz son tur galibiyetinde)
    // The Run etabi: siraya ve etap gercek mesafesine gore odul (ilk uc buyuk, digerleri katilim), un
    long recordRun(int position, int count, double realKm);
    // ---- Sehirler (hikaye): her lig bir sehirde; sehirler arasi seyahat THE RUN etabiyla (her ara bir etap) ya da
    //      parali hizli gecisle. Lig etkinlikleri bulunulan sehirde oynanir.
    static constexpr int kCities = 10;          // her ligde iki sehir (lig = sehir / 2)
    static int cityLeague(int c) { return std::clamp(c, 0, kCities - 1) / 2; }
    static const char* cityName(int c);
    static double legKm(int from);              // c -> c+1 gercek mesafe (km)
    static int legField(int from);              // o etabin rakip sayisi (20 / 50 / 100 / 200)
    int  city = 0;
    double travelKm(int to) const;              // bulunulan sehirden toplam
    long fastTravelPrice(int to) const;
    bool canTravel(int to, std::string* why = nullptr) const;
    bool fastTravel(int to, std::string* why = nullptr);
    void arriveCity(int c) { city = std::clamp(c, 0, kCities - 1); }
    // Etap alani: oyuncu seviyesine yakin araclar + tarz dagilimi (dengeli, dip gaz, eko, stok, canavar)
    std::vector<RunEntrant> runField(int n, uint32_t seed) const;
    // ---- Sokak: gece bulusmasi (3 saatte bir yeni rakip, bahisli drag, polis baskini riski),
    //      haftalik dyno yarismasi (en yuksek beygir), musteri isleri (araci hedef beygire cikar, teslim et)
    int  clockSlot = -1;                   // testler: sabit 3 saatlik dilim (-1: gercek saat)
    int  meetSlot() const;                 // 3 saatlik dilim (bulusma rakibi bu dilimde sabit)
    int  meetDone = -1;                    // son yarisilan dilim
    long meetStake() const;                // bahis (aracin degerine gore)
    Opponent meetOpponent() const;
    bool meetAvailable(std::string* why = nullptr) const;
    bool meetStart(std::string* why = nullptr);   // bahis masaya konur
    long recordMeet(bool won, long* fine); // net kazanc (+2 x bahis ya da 0) ; fine: polis cezasi (baskin yoksa 0)
    static constexpr double kRaidChance = 0.18;
    bool meetPink = false;                 // bu bulusma pink slip (para yok: kazanan karsinin arabasini alir; 2+ arac)
    bool meetPinkAvailable(std::string* why = nullptr) const;
    int  meetPinkWon = 0;                  // son pink slip bulusmasinda kazanilan arac (0: yok, -id: kaybedilen)
    // ---- Tefeci (kredi): acik lige gore limit; her yarista %5 faiz; 12 yarista kapanmazsa once para, sonra araba alinir
    long loan = 0; int loanRaces = 0;
    long loanLimit() const;
    bool borrow(long amount, std::string* why = nullptr);
    long repay(long amount);               // odenen
    void loanRace();                       // her yaris sonunda (lig / serbest / bulusma / turnuva)
    std::string loanNote;                  // son tefeci olayi (ekranda gosterilir, kaydedilmez)
    struct DynoEntry { const char* name; int carId; double hp; bool player; };
    // ---- Haftalik arac gosterisi: boya / cila / serit / jant / govde kiti / basiklik / guc puani
    int  showWeek = -1;
    double showScore(const OwnedCar& c) const;
    std::vector<DynoEntry> showField() const;   // bu haftanin 9 rakibi (hp alani: puan)
    bool showAvailable(std::string* why = nullptr) const;
    bool showEnter(int* place, long* prize, std::vector<DynoEntry>* board = nullptr, std::string* why = nullptr);
    int  dynoWeek = -1;                    // son katilinan hafta
    long dynoEntryFee() const;
    std::vector<DynoEntry> dynoField() const;    // bu haftanin 9 rakibi (oyuncunun seviyesinde)
    bool dynoAvailable(std::string* why = nullptr) const;
    // Katil: giris ucreti alinir, siralama hesaplanir; place 1..10, prize odul (ilk uce)
    bool dynoEnter(int* place, long* prize, std::vector<DynoEntry>* board = nullptr, std::string* why = nullptr);
    struct JobOffer { int carId; int targetHp; long reward; int stockHp; };
    int  jobDay = -1, jobMask = 0;         // (eski kayit uyumu)
    int  jobSeq[3] = {0, 0, 0};            // teklif sirasi: kabul edilince o slota yeni teklif gelir
    JobOffer jobOffer(int k) const;        // bugunun 3 teklifi
    bool jobTaken(int k) const;
    bool acceptJob(int k, std::string* why = nullptr);
    bool deliverJob(long* paid, std::string* why = nullptr);   // secili musteri araci hedefe ulastiysa
    bool cancelJob();                      // musteri aracini geri ver (takilan parcalar gider)
    // Kadran: alinmamissa satin al ve sec, alinmissa ucretsiz sec; turbo gostergesi: al / ac-kapa
    bool selectGauge(int style, std::string* why = nullptr);
    bool toggleBoostGauge(std::string* why = nullptr);
    long repairCost() const;                                                    // secili arac
    bool repairCurrent(std::string* why = nullptr);

    std::string serialize() const;
    static bool parse(const std::string& text, Career& out);   // bozuk/eksik veride false
    bool save(const std::string& path) const;                  // atomik: gecici dosya + yeniden adlandirma
    // corrupt: dosya var ama okunamadi (bozuk dosya .bozuk olarak saklanir, yeni oyun baslar)
    static Career loadOrNew(const std::string& path, bool* corrupt = nullptr);
};

} // namespace zk
