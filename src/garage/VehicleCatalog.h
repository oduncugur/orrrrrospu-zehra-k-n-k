// ZEHRA KINIK - Faz 2: Gercek arac / motor / sanziman veri tabani (garaj katalogu)
//
// TELIF / MARKA POLITIKASI:
//  * Teknik veriler (hacim, silindir duzeni, atesleme sirasi, guc/tork, devir, vites oranlari,
//    agirlik, olculer) gercek araclardan alinmistir. Teknik olgular telif konusu degildir.
//  * Oyunda gorunen marka, model ve motor/sanziman kodlari KURGUSALDIR (BeamNG/Automation yaklasimi).
//  * `ref` alanlari yalnizca gelistirici referansidir; oyun arayuzunde GOSTERILMEZ.
//    ("~" ile baslayan ref: deger yaklasik, yayin oncesi dogrulanmali.)
//  * Kasa modelleri gercek tasarimi kopyalamaz; gercek olculerden prosedurel low-poly uretilir, logo yoktur.
#pragma once
#include "sim/PowertrainCore.h"
#include <string>
#include <vector>

namespace zk {

enum class Layout { I3, I4, I5, I6, V6_60, V6_90Odd, V8Cross, V8Flat, V10, V12, Boxer4, Boxer6, Rotary2, Rotary3, Rotary4 };
enum class Induction { NA, ITB, Turbo, TwinTurbo, Supercharger };
enum class Drive { FWD, RWD, AWD };
enum class Body { Kei, Hatch, Sedan, Coupe, Wagon, Roadster, Muscle, Super, SUV, Pickup, Van, RallyHatch };
enum class Gearbox { HPattern, TorqueConverter, Dogbox, DCT };
enum class Exhaust { Stock, Sport, StraightPipe };

struct EngineDef {
    const char* code;        // oyun ici (kurgusal)
    const char* ref;         // gercek motor (yalniz gelistirici)
    Layout layout;
    int    cylinders;        // rotaride rotor sayisi
    double displacementL;
    int    valvesPerCyl;     // 2,3,4,5 (rotari 0)
    bool   variableCam;      // kademeli kam gecisi (VTEC/VVL/MIVEC)
    double camSwitchRpm;
    double redline;
    double powerHp, powerRpm;
    double torqueNm, torqueRpm;
    Induction induction;
    double boostBar;
    const char* firingOrder; // "1-3-4-2" (rotari: "1-2")
    char   bankRule;         // 'S' tek kolektor, 'O' tek/cift silindir ayri bank, 'H' ilk yari / ikinci yari
    bool   unequalHeaders;   // boxer rumble
};

struct GearboxDef {
    const char* code;
    const char* ref;
    Gearbox type;
    std::vector<double> ratios;
    double finalDrive;
};

struct VehicleDef {
    int         id;
    std::string brand, model;  // kurgusal
    std::string ref;           // gercek arac (yalniz gelistirici)
    int         year;
    Body        body;
    Drive       drive;
    double      massKg, lengthM, widthM, heightM, wheelbaseM, frontWeight;
    int         engine;        // engineTable() indeksi (blok)
    int         head;          // kapak (Frankenstein swaplarda farkli olabilir)
    int         gearbox;       // gearboxTable() indeksi
    Exhaust     exhaust;
    bool        streetLegal;   // false: yaris araci, romork/cekici zorunlu
    bool        abs = false, tc = false;   // fabrika ABS / cekis kontrolu (SafetyTable.inc; yoksa ECU ile eklenir)
    bool        wing, hoodScoop, widebody;
    double      rideHeightM;
    unsigned    paintRGB;
    std::string fullName() const { return brand + " " + model; }
};

const std::vector<EngineDef>&  engineTable();
const std::vector<GearboxDef>& gearboxTable();
const std::vector<VehicleDef>& vehicleCatalog();   // >= 250 arac, id sirali (1..N)
const VehicleDef* findVehicle(int id);
int findEngine(const char* code);
int findGearbox(const char* code);

const char* layoutName(Layout l);
const char* inductionName(Induction i);
const char* driveName(Drive d);
const char* bodyName(Body b);
const char* gearboxName(Gearbox g);

// Simulasyon koprusu
EngineSpec  buildEngineSpec(const VehicleDef& v);
GearboxSpec buildGearbox(const VehicleDef& v);
EngineSpec  buildEngineSpecFor(int engineIdx);      // motor swap
GearboxSpec buildGearboxFor(int gearboxIdx);       // sanziman swap
double      peakPowerHp(const VehicleDef& v);

} // namespace zk
