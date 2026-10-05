// ZEHRA KINIK - Kariyer ligleri: 5 lig (mahalle -> sehir -> sehirlerarasi -> dag -> pist), isimli rakipler, lig sonu
// patronlari, sinif sinirli etkinlikler, pink slip ve gunluk gorevler. Tablolar burada; ilerleme Career'da.
#pragma once
#include "sim/Tune.h"

#include <cstdint>
#include <string>
#include <vector>

namespace zk {

constexpr int kLeagues = 5;
enum class EventMode { Drag, Road, Touge, Karma, Flow, Chase, Marathon };   // Marathon: 18 km, yakit + benzinlik   // Chase: polis kacisi (kacmak = galibiyet)

struct RivalDef {
    const char* name;      // lakap (kurgusal)
    int league;
    int carId;
    int preset;            // YZ parca seti 0 stok / 1 sokak / 2 drag (opponentPreset)
    double handicap;       // hata payi carpani (1.6 = otomatik debriyajli oyuncu; patron 1.05-1.3)
    bool boss;
};

struct EventDef {
    const char* name;
    int league;
    EventMode mode;
    int rival;             // rivals() indeksi; -1 = esdeger rakip (pickOpponentFor)
    int prize;             // ilk galibiyet odulu ($); tekrar galibiyette %40
    int rep;               // ilk galibiyet un puani; tekrar %30; maglubiyette -%25
    bool pink;             // pink slip: kazanan arabayi alir (2+ arac gerekir)
    long flowTarget;       // Flow modunda kazanmak icin skor (0: yok)
};

const char* leagueName(int league);
// Patron sozleri (kariyer hikayesi): kind 0 = yaris oncesi meydan okuma, 1 = oyuncu kazandi, 2 = oyuncu kaybetti. Patron degilse "".
const char* bossLine(int rival, int kind);
double leagueIndexCap(int league);              // sinif siniri (performans endeksi; 0: sinirsiz)
const std::vector<RivalDef>& rivals();
const std::vector<EventDef>& leagueEvents();
const char* eventModeName(EventMode m);
// Etkinligin sehri: her lig iki sehirde (lig*2, lig*2+1); her sehrin kendi etkinlikleri var
int eventCity(int idx);
constexpr int kSecondCityFirst = 45;            // bu indeksten sonraki etkinlikler ligin ikinci sehrinin
// Patron etkinliginin acilmasi icin ayni ligde kazanilmasi gereken etkinlik sayisi
constexpr int kBossUnlockWins = 3;

// Gunluk gorevler: gunun numarasindan (UTC gun) belirlenimci 3 gorev
enum class TaskType { WinDrag, WinRoad, WinAny, EtUnder, FlowScore, Earn, Restore, BuyParts };
struct DailyTask { TaskType type; long target; int reward; int rep; };
struct SponsorDef { const char* name; int league; int wins; int races; long perWin; long bonus; long penalty; };
const std::vector<SponsorDef>& sponsors();
std::vector<DailyTask> dailyTasks(int day, double playerEt);
std::string taskText(const DailyTask& t);
int todayIndex();                               // 1970'ten beri gun (yerel saat)

} // namespace zk
