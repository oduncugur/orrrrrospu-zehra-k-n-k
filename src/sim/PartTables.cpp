#include "PartTables.h"
#include "garage/VehicleCatalog.h"
#include "sim/PowertrainCore.h"
#include "sim/Tune.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>

namespace zk {

// Not: ilk satirlar v1 seviyeleri (ayni ifadelerle: bit duzeyinde ayni fizik). Fiyatlar taban fiyat (arac degeriyle olceklenir).
const std::vector<IntakeOpt>& intakeTable() {
    static const std::vector<IntakeOpt> t = {
        {"STOK", 0, 1.0, 1.0},
        {"SPOR FILTRE", 350, 1.0 + 0.03 * 1, 1.0 + 0.03 * 1},
        {"SOGUK HAVA EMME", 900, 1.0 + 0.03 * 2, 1.0 + 0.03 * 2},
        {"PANEL FILTRE", 150, 1.01, 1.01},
        {"KONIK FILTRE", 250, 1.02, 1.025},
        {"KISA EMME BORUSU", 450, 1.035, 1.04},
        {"KARBON AIRBOX", 1600, 1.04, 1.06},
        {"RAM AIR KAPUT GIRISI", 1300, 1.03, 1.06},
        {"CIFT KONIK FILTRE", 500, 1.025, 1.035},
        {"YARIS HAVA KUTUSU", 1800, 1.05, 1.07},
    };
    return t;
}
const std::vector<ShapeOpt>& exhaustTable() {
    static const std::vector<ShapeOpt> t = {
        {"STOK", 0, 1.0, 1.0},
        {"SPOR SUSTURUCU", 500, 1.0 + 0.03 * 1, 1.0 + 0.03 * 1},
        {"DUZ BORU", 1200, 1.0 + 0.03 * 2, 1.0 + 0.03 * 2},
        {"ARKA SUSTURUCU", 300, 1.01, 1.015},
        {"CAT-BACK", 800, 1.02, 1.03},
        {"3 INC HAT", 700, 1.02, 1.035},
        {"CIFT CIKIS", 650, 1.015, 1.03},
        {"VALFLI SUSTURUCU", 1100, 1.02, 1.035},
        {"YARIS SUSTURUCU", 900, 1.025, 1.045},
        {"TITANYUM YARIS HATTI", 3000, 1.03, 1.06},
    };
    return t;
}
const std::vector<CamOpt>& camTable() {
    static const std::vector<CamOpt> t = {
        {"STOK", 0, 1.0, 1.0, 0},
        {"256 DERECE SOKAK", 600, 1.02, 1.03, 0},
        {"264 DERECE", 850, 1.01, 1.05, 100},
        {"272 DERECE", 1100, 0.99, 1.07, 200},
        {"280 DERECE", 1400, 0.97, 1.09, 300},
        {"288 DERECE", 1700, 0.94, 1.11, 400},
        {"296 DERECE", 2000, 0.91, 1.13, 500},
        {"304 DERECE YARIS", 2400, 0.88, 1.15, 600},
        {"312 DERECE YARIS", 2800, 0.85, 1.17, 700},
        {"320 DERECE DRAG", 3300, 0.82, 1.19, 800},
        {"VVT / VTEC YUKSELTME", 2600, 1.05, 1.06, 200},
        {"OZEL KAM (ATOLYE)", 0, 1.0, 1.0, 0},
    };
    return t;
}
const std::vector<ValveOpt>& valveTable() {
    static const std::vector<ValveOpt> t = {
        {"STOK", 0, 0}, {"SERT SUPAP YAYI", 400, 200}, {"CIFT YAY", 700, 350}, {"TITANYUM TUTUCU", 1000, 500},
        {"HAFIF SUPAP", 1300, 650}, {"BRONZ KILAVUZ + YAY", 1600, 800}, {"TITANYUM SUPAP", 2300, 1000},
        {"YARIS SUPAP SETI", 3000, 1200}, {"PNOMATIK BASLANGIC", 4500, 1400}, {"TAM YARIS KAPAK", 6500, 1600},
    };
    return t;
}
const std::vector<HeadOpt>& headTable() {
    static const std::vector<HeadOpt> t = {
        {"STOK", 0, 1.0, 1.0, 0}, {"3 ACI SUPAP OTURMA", 500, 1.0, 1.02, 0}, {"HAFIF PORT", 900, 1.0, 1.04, 0},
        {"PORT + POLISAJ", 1500, 0.99, 1.07, 100}, {"BUYUK SUPAP", 2200, 0.98, 1.09, 200}, {"CNC PORT", 3200, 1.0, 1.10, 200},
        {"YARIS KAPAGI", 4800, 0.97, 1.14, 400}, {"CIFT BUJI ATESLEME", 1800, 1.02, 1.03, 0},
        {"PLANYA (YUKSEK SIKISTIRMA)", 700, 1.03, 1.03, 0}, {"BILLET KAPAK", 7500, 0.98, 1.16, 600},
    };
    return t;
}
const std::vector<PistonOpt>& pistonTable() {
    static const std::vector<PistonOpt> t = {
        {"STOK DOKUM", 0, 1.0, 1.0}, {"DOKUM YUKSEK SIKISTIRMA", 700, 1.03, 0.95}, {"HIPEREUTEKTIK", 900, 1.01, 1.10},
        {"DOVME SOKAK", 1600, 1.0, 1.45}, {"DOVME DUSUK SIKISTIRMA", 1800, 0.97, 1.60}, {"DOVME YUKSEK SIKISTIRMA", 1900, 1.04, 1.40},
        {"2618 YARIS", 2600, 1.01, 1.80}, {"KAPLAMALI DOVME", 2300, 1.01, 1.70}, {"HAFIF YARIS", 2800, 1.02, 1.55},
        {"PRO MOD BILLET", 5200, 1.0, 2.20},
    };
    return t;
}
const std::vector<RodOpt>& rodTable() {
    static const std::vector<RodOpt> t = {
        {"STOK", 0, 1.0, 1.0}, {"SERTLESTIRILMIS STOK", 400, 1.15, 1.0}, {"I-BEAM DOVME", 1100, 1.35, 1.0},
        {"H-BEAM", 1500, 1.55, 1.0}, {"H-BEAM + ARP 2000", 1900, 1.70, 1.0}, {"TITANYUM", 4200, 1.60, 0.95},
        {"ALUMINYUM DRAG", 2600, 1.90, 0.96}, {"BILLET H", 3200, 2.00, 1.0}, {"CARRILLO", 3800, 2.10, 1.0}, {"PRO MOD", 5500, 2.60, 1.0},
    };
    return t;
}
const std::vector<CrankOpt>& crankTable() {
    static const std::vector<CrankOpt> t = {
        {"STOK", 0, 1.0, 1.0, 1.0, 0}, {"DENGELENMIS", 600, 1.0, 1.05, 1.0, 200}, {"HAFIFLETILMIS", 1200, 1.0, 1.0, 0.92, 100},
        {"DOVME 4340", 2400, 1.0, 1.50, 1.0, 0}, {"BILLET", 4200, 1.0, 1.90, 1.0, 200},
        {"STROKER +%5", 3500, 1.05, 1.30, 1.03, -100}, {"STROKER +%10", 4800, 1.10, 1.30, 1.05, -250},
        {"STROKER +%15", 6200, 1.15, 1.35, 1.07, -400}, {"STROKER +%20 BLOK", 8500, 1.20, 1.40, 1.09, -550},
        {"OZEL STROKER (ATOLYE)", 0, 1.0, 1.35, 1.0, 0},
    };
    return t;
}
const std::vector<BearingOpt>& bearingTable() {
    static const std::vector<BearingOpt> t = {
        {"STOK", 0, 1.0}, {"YARIS YATAK", 400, 1.10}, {"KAPLAMALI YATAK", 550, 1.15}, {"TRI-METAL", 650, 1.20},
        {"GENIS YATAK", 800, 1.25}, {"ACL RACE", 700, 1.22}, {"KING XP", 750, 1.25}, {"CALICO KAPLAMA", 900, 1.28},
        {"YARIS TRI-METAL", 1000, 1.30}, {"PRO MOD YATAK", 1500, 1.35},
    };
    return t;
}
const std::vector<GasketOpt>& gasketTable() {
    static const std::vector<GasketOpt> t = {
        {"STOK", 0, 0.0, 1.0}, {"MLS 0.5 MM (YUKSEK CR)", 300, 0.50, 1.10}, {"MLS 0.7 MM", 300, 0.70, 1.20},
        {"MLS 1.0 MM", 350, 1.00, 1.30}, {"MLS 1.3 MM (BOOST)", 400, 1.30, 1.45}, {"BAKIR CONTA", 600, 1.00, 1.60},
        {"O-RING BLOK", 1500, 0.90, 1.90}, {"BAKIR + O-RING", 1900, 1.10, 2.20}, {"COOPER RING", 1700, 1.00, 2.30},
        {"YARIS MLS 1.5 MM", 500, 1.50, 1.70},
    };
    return t;
}
const std::vector<TurbineOpt>& turbineTable() {
    static const std::vector<TurbineOpt> t = {
        {"STOK GOVDE", 0, 0.0, 1.0}, {"0.48 A/R (HIZLI SPOOL)", 700, -0.05, 0.93}, {"0.63 A/R", 700, -0.03, 0.97},
        {"0.82 A/R", 700, 0.0, 1.0}, {"1.06 A/R (TEPE GUC)", 800, 0.04, 1.05}, {"1.32 A/R (DRAG)", 900, 0.07, 1.08},
        {"TWIN-SCROLL", 1500, -0.04, 1.02}, {"BILYALI MIL", 1800, -0.04, 1.0}, {"INCONEL TURBIN CARKI", 2200, -0.02, 1.03},
        {"TITANYUM ALUMINIDE", 3400, -0.05, 1.02},
    };
    return t;
}
const std::vector<GateOpt>& wastegateTable() {
    static const std::vector<GateOpt> t = {
        {"STOK", 0, 1.0}, {"DAHILI BUYUK PORT", 350, 1.03}, {"HARICI 38 MM", 700, 1.05}, {"HARICI 44 MM", 900, 1.07},
        {"HARICI 46 MM", 1000, 1.08}, {"HARICI 50 MM", 1200, 1.10}, {"HARICI 60 MM", 1500, 1.12}, {"CIFT 44 MM", 1700, 1.13},
        {"ELEKTRONIK WASTEGATE", 1900, 1.12}, {"CO2 KONTROLLU", 2100, 1.15},
    };
    return t;
}
const std::vector<BoostCtlOpt>& boostCtlTable() {
    static const std::vector<BoostCtlOpt> t = {
        {"STOK", 0, 0.0}, {"MANUEL BOOST VALFI", 150, 0.10}, {"ELEKTRONIK (EBC)", 600, 0.15}, {"VITES BAZLI EBC", 850, 0.18},
        {"HIZ BAZLI EBC", 900, 0.20}, {"CIFT KADEME", 1000, 0.25}, {"SCRAMBLE BOOST", 1100, 0.30}, {"CO2 BOOST KONTROL", 1500, 0.35},
        {"YARIS EBC", 1900, 0.40}, {"TAM ACIK (WASTEGATE KAPALI)", 300, 0.55},
    };
    return t;
}
const std::vector<GbStrOpt>& gbStrengthTable() {
    static const std::vector<GbStrOpt> t = {
        {"STOK", 0, 1.0}, {"SERTLESTIRILMIS DISLI", 900, 1.20}, {"SHOT PEEN DISLI", 1100, 1.25}, {"KASA DESTEGI", 800, 1.30},
        {"SANZIMAN YAG SOGUTUCU", 600, 1.15}, {"KOMPLE DISLI SETI", 2800, 1.50}, {"DOGBOX DISLI SETI", 4200, 1.80},
        {"BILLET DISLI", 5600, 2.20}, {"YARIS SENKROMEC + DISLI", 3600, 1.65}, {"PRO MOD", 8000, 2.80},
    };
    return t;
}
const std::vector<CoolOpt>& coolingTable() {
    static const std::vector<CoolOpt> t = {
        {"STOK", 0, 1.0, 0.40}, {"ALU RADYATOR 2 SIRA", 500, 1.25, 0.40}, {"ALU RADYATOR 3 SIRA", 750, 1.45, 0.40},
        {"BUYUK ALU RADYATOR", 1000, 1.70, 0.45}, {"CIFT SIRA BAKIR", 650, 1.30, 0.40}, {"YARIS RADYATORU", 1500, 1.90, 0.45},
        {"ON + YAN RADYATOR", 2400, 2.20, 0.50}, {"TAM YARIS RADYATORU", 3000, 2.40, 0.50},
    };
    return t;
}
const std::vector<FlyOpt>& flywheelTable() {
    static const std::vector<FlyOpt> t = {
        {"STOK", 0, 1.0}, {"HAFIF CELIK", 450, 0.90}, {"KROM-MOLY", 650, 0.82}, {"ALUMINYUM", 800, 0.74},
        {"HAFIF ALUMINYUM", 1000, 0.67}, {"YARIS ALUMINYUM", 1300, 0.60}, {"KARBON", 1900, 0.54},
        {"ULTRA HAFIF YARIS", 2400, 0.48}, {"AGIR DRAG VOLANI", 700, 1.25}, {"COK AGIR (KALKIS)", 900, 1.45},
    };
    return t;
}
const std::vector<TurboOpt>& turboTable() {
    static const std::vector<TurboOpt> t = {
        {"YOK / FABRIKA", 0, 0, 0, 0},
        {"KUCUK KIT", 7000, 0.6 * 1, 0.32 + 0.08 * 1, 0.55 + 0.07 * 1},
        {"BUYUK KIT", 15000, 0.6 * 2, 0.32 + 0.08 * 2, 0.55 + 0.07 * 2},
        {"T25 KUCUK", 3200, 0.5, 0.28, 0.48},
        {"T28", 4000, 0.7, 0.32, 0.53},
        {"GT2560 BILYALI", 4600, 0.8, 0.31, 0.51},
        {"GT2860", 5200, 1.0, 0.35, 0.56},
        {"GT2871", 5800, 1.1, 0.37, 0.59},
        {"GT3071", 6500, 1.3, 0.39, 0.61},
        {"GT3076", 7200, 1.5, 0.41, 0.63},
        {"GT3582", 8400, 1.8, 0.45, 0.67},
        {"GT3586", 9200, 2.0, 0.47, 0.69},
        {"GT4088", 10500, 2.3, 0.51, 0.73},
        {"GT4294", 11800, 2.6, 0.54, 0.77},
        {"GT45 DRAG", 13000, 2.8, 0.57, 0.80},
        {"GT47 DRAG", 14500, 3.0, 0.60, 0.82},
        {"CIFT T25", 7000, 1.0, 0.29, 0.49},
        {"CIFT GT28", 8800, 1.4, 0.33, 0.54},
        {"CIFT GT30", 10500, 1.8, 0.37, 0.58},
        {"CIFT GT35", 13500, 2.4, 0.43, 0.66},
        {"SIRALI CIFT TURBO", 12000, 1.6, 0.24, 0.55},
        {"EFR 6758", 6800, 1.2, 0.32, 0.53},
        {"EFR 7163", 7800, 1.4, 0.34, 0.56},
        {"EFR 8374", 9800, 2.0, 0.40, 0.62},
        {"EFR 9180", 12500, 2.5, 0.46, 0.68},
        {"PRO MOD 88 MM", 18000, 3.5, 0.62, 0.86},
        {"PRO MOD 98 MM", 22000, 4.0, 0.66, 0.90},
        {"ELEKTRIK DESTEKLI", 15000, 1.6, 0.18, 0.44},
        {"VGT DEGISKEN KANAT", 11000, 1.5, 0.24, 0.50},
        {"OZEL TURBO (ATOLYE)", 0, 0, 0, 0},
    };
    return t;
}
const std::vector<SuperOpt>& superTable() {
    static const std::vector<SuperOpt> t = {
        {"YOK", 0, 0, 0}, {"ROOTS KUCUK", 3800, 0.35, 0}, {"ROOTS ORTA", 5200, 0.55, 0}, {"ROOTS BUYUK (6-71)", 7500, 0.80, 0},
        {"TWIN-SCREW KUCUK", 4800, 0.45, 1}, {"TWIN-SCREW ORTA", 6800, 0.65, 1}, {"TWIN-SCREW BUYUK", 9200, 0.90, 1},
        {"SANTRIFUJ KUCUK", 4200, 0.60, 2}, {"SANTRIFUJ ORTA", 6000, 0.90, 2}, {"SANTRIFUJ BUYUK", 8500, 1.25, 2},
    };
    return t;
}
const std::vector<IcOpt>& intercoolerTable() {
    static const std::vector<IcOpt> t = {
        {"STOK / YOK", 0, 1.0}, {"KUCUK ON MONTAJ", 450, 1.02}, {"ORTA ON MONTAJ", 700, 1.04}, {"BUYUK ON MONTAJ", 1000, 1.06},
        {"BAR-PLATE YARIS", 1400, 1.08}, {"SU-HAVA", 1800, 1.09}, {"SU-HAVA + BUZ KUTUSU", 2400, 1.12},
        {"DEV ON MONTAJ", 1300, 1.07}, {"CIFT GECIS", 1500, 1.075}, {"YARIS SU-HAVA", 2600, 1.10},
    };
    return t;
}
const std::vector<FuelSysOpt>& fuelSysTable() {
    static const std::vector<FuelSysOpt> t = {
        {"STOK", 0, 1.35}, {"YUKSEK DEBI POMPA", 400, 1.50}, {"ENJEKTOR 550 CC", 700, 1.70}, {"ENJEKTOR 750 CC", 950, 2.00},
        {"ENJEKTOR 1000 CC", 1250, 2.40}, {"ENJEKTOR 1300 CC + POMPA", 1700, 2.80}, {"ENJEKTOR 1650 CC", 2200, 3.30},
        {"2000 CC + CIFT POMPA", 3000, 4.00}, {"METANOL SISTEMI", 4200, 4.80}, {"YARIS YAKIT SISTEMI", 6000, 6.00},
    };
    return t;
}
// ---- bilesen tablolari (ayni anda takilabilen parcalar)
const std::vector<IntakeOpt>& throttleTable() {
    static const std::vector<IntakeOpt> t = {
        {"STOK KELEBEK", 0, 1.0, 1.0}, {"+2 MM", 250, 1.0, 1.015}, {"+4 MM", 400, 1.0, 1.03}, {"+6 MM", 550, 1.0, 1.045},
        {"BUYUK GAZ KELEBEGI", 700, 1.0, 1.07}, {"70 MM", 800, 0.995, 1.075}, {"75 MM", 900, 0.99, 1.08}, {"80 MM", 1050, 0.985, 1.085},
        {"CIFT KELEBEK", 1400, 0.99, 1.09}, {"YARIS ELEKTRONIK KELEBEK", 1700, 1.0, 1.09},
    };
    return t;
}
const std::vector<IntakeOpt>& intakeManiTable() {
    static const std::vector<IntakeOpt> t = {
        {"STOK MANIFOLD", 0, 1.0, 1.0}, {"PORTLU MANIFOLD", 1400, 0.98, 1.10}, {"UZUN YOLLU (TORK)", 900, 1.05, 0.99},
        {"KISA YOLLU (DEVIR)", 1000, 0.97, 1.08}, {"DEGISKEN UZUNLUK", 1900, 1.04, 1.06}, {"BUYUK PLENUM", 1300, 0.98, 1.09},
        {"KOMPOZIT MANIFOLD", 700, 1.01, 1.04}, {"BILLET MANIFOLD", 2600, 1.0, 1.11}, {"ITB TEKIL KELEBEK", 4200, 1.04, 1.13},
        {"YARIS PLENUMU", 3000, 0.99, 1.12},
    };
    return t;
}
const std::vector<ShapeOpt>& headerTable() {
    static const std::vector<ShapeOpt> t = {
        {"STOK MANIFOLD", 0, 1.0, 1.0}, {"HEADER 4-2-1", 1300, 1.05, 1.04}, {"HEADER 4-1 (TEPE)", 1600, 0.99, 1.08},
        {"TUBULAR MANIFOLD", 2000, 1.03, 1.08}, {"PASLANMAZ 4-2-1", 900, 1.04, 1.035}, {"KISA HEADER", 600, 1.02, 1.03},
        {"SERAMIK KAPLAMA HEADER", 1700, 1.05, 1.05}, {"ESIT UZUNLUK", 2200, 1.04, 1.07}, {"INCONEL", 3400, 1.04, 1.09},
        {"YARIS 4-1 MEGAFON", 2800, 0.97, 1.11},
    };
    return t;
}
const std::vector<ShapeOpt>& catalystTable() {
    static const std::vector<ShapeOpt> t = {
        {"STOK KATALIZOR", 0, 1.0, 1.0}, {"200 HUCRE", 400, 1.01, 1.015}, {"100 HUCRE METAL", 700, 1.015, 1.025},
        {"SPOR KATALIZOR", 900, 1.02, 1.03}, {"TEST BORUSU (IPTAL)", 400, 1.02, 1.03}, {"DOWNPIPE + IPTAL", 700, 1.025, 1.04},
    };
    return t;
}
const std::vector<IcOpt>& methTable() {
    static const std::vector<IcOpt> t = {
        {"YOK", 0, 1.0}, {"SU ENJEKSIYONU", 900, 1.03}, {"METANOL ENJEKSIYON", 1600, 1.06}, {"SU + METANOL", 2000, 1.08},
        {"CIFT NOZUL SU + METANOL", 2500, 1.09}, {"CO2 SOGUTMA SPREYI", 1200, 1.03},
    };
    return t;
}
const std::vector<BearingOpt>& headStudTable() {
    static const std::vector<BearingOpt> t = {
        {"STOK CIVATA", 0, 1.0}, {"ARP KAPAK SAPLAMASI", 600, 1.15}, {"ARP 2000", 800, 1.22}, {"ARP L19", 1100, 1.30},
        {"CUSTOM AGE 625", 1600, 1.38},
    };
    return t;
}
const std::vector<BearingOpt>& mainSupportTable() {
    static const std::vector<BearingOpt> t = {
        {"STOK", 0, 1.0}, {"ARP ANA CIVATA", 500, 1.08}, {"ANA YATAK KUSAGI", 1400, 1.15}, {"BILLET ANA KAPAK", 1900, 1.18},
        {"KUSAK + ARP", 2200, 1.25}, {"TAM BLOK DESTEK", 3600, 1.35},
    };
    return t;
}
const std::vector<BrakeOpt>& brakeDiscTable() {
    static const std::vector<BrakeOpt> t = {
        {"STOK DISK", 0, 1.0}, {"DELIKLI DISK", 450, 1.05}, {"YIVLI DISK", 400, 1.06}, {"DELIKLI + YIVLI", 600, 1.08},
        {"BUYUK DISK 330 MM", 1400, 1.12}, {"IKI PARCA DISK", 2200, 1.16}, {"BUYUK DISK 380 MM", 3200, 1.20},
        {"KARBON SERAMIK", 7000, 1.32}, {"KARBON-KARBON", 10000, 1.42},
    };
    return t;
}
const std::vector<BrakeOpt>& brakeCaliperTable() {
    static const std::vector<BrakeOpt> t = {
        {"STOK KALIPER", 0, 1.0}, {"4 PISTONLU", 1600, 1.10}, {"6 PISTONLU", 2600, 1.18}, {"MONOBLOK 6", 3400, 1.22},
        {"8 PISTONLU", 4200, 1.26}, {"YARIS KALIPERI", 5000, 1.30},
    };
    return t;
}
const std::vector<WeightOpt>& weightBodyTable() {
    static const std::vector<WeightOpt> t = {
        {"STOK", 0, 0}, {"KARBON BAGAJ -10 KG", 900, 10}, {"KARBON KAPUT -12 KG", 1200, 12}, {"KAPUT + BAGAJ -22 KG", 2000, 22},
        {"FIBER KAPILAR -30 KG", 2600, 30}, {"KARBON PANELLER -70 KG", 5500, 70}, {"KOMPLE KARBON KASA -110 KG", 9000, 110},
    };
    return t;
}
const std::vector<WeightOpt>& weightGlassTable() {
    static const std::vector<WeightOpt> t = {
        {"STOK", 0, 0}, {"LEXAN ARKA CAM -8 KG", 600, 8}, {"LEXAN YAN CAMLAR -12 KG", 900, 12},
        {"LEXAN ARKA + YAN -20 KG", 1400, 20}, {"TUM LEXAN -28 KG", 2200, 28},
    };
    return t;
}
const std::vector<WeightOpt>& weightChassisTable() {
    static const std::vector<WeightOpt> t = {
        {"STOK", 0, 0}, {"ALU SALINCAKLAR -15 KG", 1500, 15}, {"ALU SUBFRAME -25 KG", 2500, 25}, {"BORU ON SASI -45 KG", 6000, 45},
        {"BORU SASI -90 KG", 12000, 90},
    };
    return t;
}
const std::vector<AeroOpt>& aeroFrontTable() {
    static const std::vector<AeroOpt> t = {
        {"STOK", 0, 1.0, 0}, {"ON LIP", 300, 0.99, 120}, {"SPLITTER", 650, 1.0, 200}, {"CANARD", 450, 1.01, 90},
        {"YARIS ON TAMPONU", 1500, 0.99, 260},
    };
    return t;
}
const std::vector<AeroOpt>& aeroSideTable() {
    static const std::vector<AeroOpt> t = {
        {"STOK", 0, 1.0, 0}, {"YAN ETEK", 400, 0.99, 40}, {"KARBON YAN ETEK", 900, 0.985, 50}, {"YAN KANATCIK", 500, 1.005, 60},
        {"GENIS KASA KITI", 3500, 1.03, 120},
    };
    return t;
}
const std::vector<AeroOpt>& aeroUnderTable() {
    static const std::vector<AeroOpt> t = {
        {"STOK", 0, 1.0, 0}, {"ALT KAPLAMA", 600, 0.98, 60}, {"DIFUZOR + ALT KAPLAMA", 1800, 0.97, 260}, {"DUZ TABAN", 1400, 0.96, 150},
        {"YARIS DIFUZORU", 2800, 0.97, 380},
    };
    return t;
}
const std::vector<CoolOpt>& fanTable() {
    static const std::vector<CoolOpt> t = {
        {"STOK FAN", 0, 1.0, 0.40}, {"ELEKTRIK FAN", 350, 1.05, 0.70}, {"CIFT FAN", 550, 1.08, 0.85}, {"YUKSEK DEBI FAN", 500, 1.06, 0.90},
        {"CIFT YUKSEK DEBI", 800, 1.10, 1.00}, {"DAVLUMBAZ + CIFT FAN", 1100, 1.12, 1.00},
    };
    return t;
}
const std::vector<CoolOpt>& coolMiscTable() {
    static const std::vector<CoolOpt> t = {
        {"STOK", 0, 1.0, 0}, {"DUSUK ISI TERMOSTAT", 120, 1.10, 0}, {"SU KATKISI", 60, 1.08, 0}, {"TERMOSTAT + KATKI", 170, 1.18, 0},
        {"YARIS SU POMPASI", 600, 1.12, 0},
    };
    return t;
}
const std::vector<CoolOpt>& oilCoolerTable() {
    static const std::vector<CoolOpt> t = {
        {"YOK", 0, 1.0, 0}, {"KUCUK YAG SOGUTUCU", 400, 1.04, 0}, {"BUYUK YAG SOGUTUCU", 650, 1.07, 0},
        {"TERMOSTATLI SOGUTUCU", 800, 1.08, 0}, {"YARIS YAG SOGUTUCU", 1200, 1.10, 0},
    };
    return t;
}
const std::vector<OilOpt>& oilPumpTable() {
    static const std::vector<OilOpt> t = {
        {"STOK POMPA", 0, false, 0.0}, {"YUKSEK HACIMLI", 700, false, 1.0}, {"YUKSEK BASINC", 600, false, 0.5},
        {"AKUMULATOR", 800, false, 1.1}, {"YARIS POMPASI", 1500, false, 1.5},
    };
    return t;
}
namespace {
template <class T> const T& rowOf(const std::vector<T>& t, int i) { return t[std::clamp(i, 0, (int)t.size() - 1)]; }
}
double coolingCapMul(const Tune& t) {
    return rowOf(coolingTable(), t.cooling).cap * rowOf(fanTable(), t.fan).cap * rowOf(coolMiscTable(), t.coolMisc).cap
         * rowOf(oilCoolerTable(), t.oilCooler).cap;
}
double coolingLowSpeed(const Tune& t) { return std::max(rowOf(coolingTable(), t.cooling).lowSpeed, rowOf(fanTable(), t.fan).lowSpeed); }
int Tune::totalWeightKg() const {
    return weightKg(weight) + (int)rowOf(weightBodyTable(), weightBody).kg + (int)rowOf(weightGlassTable(), weightGlass).kg
         + (int)rowOf(weightChassisTable(), weightChassis).kg;
}

const std::vector<FuelSysOpt>& fuelPumpTable() {
    static const std::vector<FuelSysOpt> t = {
        {"STOK POMPA", 0, 1.35}, {"190 LPH", 250, 1.60}, {"255 LPH", 400, 2.00}, {"340 LPH", 600, 2.50}, {"450 LPH", 850, 3.00},
        {"525 LPH", 1100, 3.40}, {"CIFT 340 LPH", 1500, 4.00}, {"CIFT 450 LPH", 2000, 4.80}, {"MEKANIK POMPA", 2800, 5.60},
        {"SURGE TANK + UC POMPA", 3800, 6.50},
    };
    return t;
}
const std::vector<FuelSysOpt>& injectorTable() {
    static const std::vector<FuelSysOpt> t = {
        {"STOK ENJEKTOR", 0, 1.35}, {"440 CC", 350, 1.55}, {"550 CC", 500, 1.75}, {"650 CC", 620, 2.00}, {"750 CC", 750, 2.25},
        {"1000 CC", 1000, 2.90}, {"1300 CC", 1300, 3.60}, {"1650 CC", 1700, 4.40}, {"2000 CC", 2200, 5.20},
        {"2400 CC + IKINCI SIRA", 3000, 6.50},
    };
    return t;
}
const std::vector<FuelSysOpt>& fuelLineTable() {
    static const std::vector<FuelSysOpt> t = {
        {"STOK HAT", 0, 1.00}, {"ORGULU HAT", 150, 1.02}, {"AYARLI REGULATOR", 250, 1.04}, {"-6AN HAT", 300, 1.05},
        {"-8AN HAT", 400, 1.07}, {"YUKSEK DEBI RAY", 450, 1.08}, {"RAY + REGULATOR", 650, 1.10}, {"-10AN + RAY", 800, 1.12},
        {"DONUS HATLI SISTEM", 1000, 1.14}, {"YARIS HATTI + FILTRE", 1300, 1.16},
    };
    return t;
}
double fuelCap(const Tune& t) {
    if (t.fuelPump == 0 && t.injector == 0 && t.fuelLine == 0) return fuelSysTable()[std::clamp(t.fuelSys, 0, 9)].cap;   // eski kayit
    const double pump = fuelPumpTable()[std::clamp(t.fuelPump, 0, 9)].cap, inj = injectorTable()[std::clamp(t.injector, 0, 9)].cap;
    return std::min(pump, inj) * fuelLineTable()[std::clamp(t.fuelLine, 0, 9)].cap;
}

// ECU donanimi: yuva, modul seviye siniri {harita, devir(+250 adim), launch, flat, anti-lag, flex, vuruntu, tcu}
const std::vector<EcuHwOpt>& ecuHwTable() {
    static const std::vector<EcuHwOpt> t = {
        {"STOK ECU", 0, 1, {1, 0, 0, 0, 0, 0, 0, 0}, true},
        {"CHIP (EPROM)", 350, 1, {2, 10, 0, 0, 0, 0, 0, 1}, true},
        {"PIGGYBACK", 600, 2, {1, 15, 0, 0, 0, 1, 0, 1}, true},
        {"PIGGYBACK PRO", 900, 3, {2, 20, 1, 0, 0, 1, 0, 1}, true},
        {"PLUG-IN ECU", 1400, 3, {2, 30, 1, 1, 0, 1, 0, 2}, true},
        {"PLUG-IN PRO", 1900, 4, {3, 35, 1, 1, 0, 1, 0, 2}, true},
        {"STANDALONE", 2800, 5, {3, 50, 1, 1, 1, 1, 1, 2}, false},
        {"STANDALONE PRO", 3600, 6, {3, 60, 1, 1, 1, 1, 1, 2}, false},
        {"YARIS ECU", 5200, 7, {3, 70, 1, 1, 1, 1, 0, 2}, true},
        {"TAKIM YARIS ECU", 7500, 8, {3, 80, 1, 1, 1, 1, 0, 2}, true},
    };
    return t;
}
bool suspAdjustable(const Tune& t) { const int s = t.susp; return s == 2 || s == 3 || s == 4 || s == 7 || s == 8 || s == 9; }
bool lsdAdjustable(const Tune& t) { const int d = (int)t.diff; return d == 1 || d == 2 || d == 6 || d == 7 || d == 8; }
void clampSetup(Tune& t) {
    if (!suspAdjustable(t)) t.setRide = t.setSpring = t.setDamp = t.setArbF = t.setArbR = t.setRideR = t.setSpringR = t.setDampR = 0;
    t.setRide = std::clamp(t.setRide, -40, 20); t.setSpring = std::clamp(t.setSpring, -5, 5); t.setDamp = std::clamp(t.setDamp, -5, 5);
    t.setRideR = std::clamp(t.setRideR, -40, 20); t.setSpringR = std::clamp(t.setSpringR, -5, 5); t.setDampR = std::clamp(t.setDampR, -5, 5);
    t.setArbF = std::clamp(t.setArbF, -3, 3); t.setArbR = std::clamp(t.setArbR, -3, 3);
    t.setPreload = lsdAdjustable(t) ? std::clamp(t.setPreload, -5, 5) : 0;
    t.setNos = t.nitrous > 0 && t.setNos >= 50 ? std::min(t.setNos, 95) : 0;
}

const EcuSwDef& ecuSwDef(int sw) {
    static const EcuSwDef d[SwCount] = {
        {"HARITA (STAGE)", "ATESLEME + YAKIT: STAGE 1/2/3", {500, 900, 1400}},
        {"DEVIR SINIRI", "HER SEVIYE +50 RPM (HASSAS)", {60, 60, 60, 60, 60, 60, 60, 60, 60, 60}},
        {"LAUNCH KONTROL", "AYARLANABILIR 2-STEP", {700}},
        {"FLAT SHIFT", "GAZDAN AYAK KALKMADAN VITES", {600}},
        {"ANTI-LAG", "TURBO ERKEN DOLAR (SPOOL)", {1500}},
        {"FLEX FUEL", "E85 / METANOLDE +%5 GUC", {800}},
        {"VURUNTU KONTROL", "STANDALONE VURUNTU KORUMASI", {500}},
        {"TCU YAZILIMI", "OTOMATIK: HIZLI VITES, SPOR HARITA", {600, 900}},
    };
    return d[std::clamp(sw, 0, SwCount - 1)];
}
namespace {
int* swRef(Tune& t, int sw) {
    switch (sw) {
    case SwMap: return &t.swMap; case SwRev: return &t.swRev; case SwLaunch: return &t.swLaunch; case SwFlat: return &t.swFlat;
    case SwAntiLag: return &t.swAntiLag; case SwFlex: return &t.swFlex; case SwTcu: return &t.swTcu; default: return &t.swKnock;
    }
}
}
int ecuSwLevel(const Tune& t, int sw) { return *swRef(const_cast<Tune&>(t), sw); }
void setEcuSwLevel(Tune& t, int sw, int lv) { *swRef(t, sw) = std::max(0, lv); }
int ecuSwMax(const Tune& t, int sw) { return ecuHwTable()[std::clamp(t.ecuHw, 0, 9)].maxLv[std::clamp(sw, 0, SwCount - 1)]; }
int ecuSlotsUsed(const Tune& t) { int n = 0; for (int i = 0; i < SwCount; ++i) n += ecuSwLevel(t, i) > 0; return n; }
void clampEcuSoftware(Tune& t) {
    for (int i = 0; i < SwCount; ++i) setEcuSwLevel(t, i, std::min(ecuSwLevel(t, i), ecuSwMax(t, i)));
    const int slots = ecuHwTable()[std::clamp(t.ecuHw, 0, 9)].slots;
    for (int i = SwCount - 1; i >= 0 && ecuSlotsUsed(t) > slots; --i) setEcuSwLevel(t, i, 0);   // sigmayan (son) moduller silinir
}
bool ecuNewSystem(const Tune& t) { return t.ecuHw > 0 || ecuSlotsUsed(t) > 0; }
EcuOpt effectiveEcu(const Tune& t, bool* knockSensor) {
    if (!ecuNewSystem(t)) {
        if (knockSensor) *knockSensor = !(t.ecu == 4 || t.ecu == 9);   // eski standalone / yaris haritasi: koruma yok
        return ecuTable()[std::clamp(t.ecu, 0, (int)ecuTable().size() - 1)];
    }
    static const double na[4] = {1.0, 1.04, 1.07, 1.10}, fo[4] = {1.0, 1.10, 1.18, 1.26};
    const int m = std::clamp(t.swMap, 0, 3);
    EcuOpt e{"ECU", 0, na[m], fo[m], 50.0 * t.swRev, t.swAntiLag ? 0.07 : 0.0};   // seviye basina +50 rpm
    if (t.swFlex && t.fuel == FuelType::E85) { e.na *= 1.05; e.forced *= 1.05; }
    if (knockSensor) *knockSensor = ecuHwTable()[std::clamp(t.ecuHw, 0, 9)].knockBuiltin || t.swKnock > 0;
    return e;
}
bool ecuFineTuneAvailable(const Tune& t) { return t.ecuHw >= 4; }
bool launchControlAvailable(const Tune& t) {
    return ecuNewSystem(t) ? t.swLaunch > 0 : (t.ecu == 4 || t.ecu == 8 || t.ecu == 9);
}

const std::vector<NosOpt>& nosTable() {
    static const std::vector<NosOpt> t = {
        {"YOK", 0, 0, 0}, {"KURU 25 HP", 900, 25, 16}, {"KURU 50 HP", 1300, 50, 14}, {"KURU 75 HP", 1700, 75, 12},
        {"ISLAK 100 HP", 2300, 100, 12}, {"ISLAK 150 HP", 3000, 150, 10}, {"ISLAK 200 HP", 3800, 200, 9},
        {"DIREKT PORT 250 HP", 5200, 250, 8}, {"DIREKT PORT 300 HP", 6500, 300, 7}, {"KADEMELI 400 HP", 9000, 400, 6},
    };
    return t;
}
const std::vector<EcuOpt>& ecuTable() {
    static const std::vector<EcuOpt> t = {
        {"STOK", 0, 1.0, 1.0, 0, 0},
        {"REMAP", 1100, 1.0 + 0.05, 1.0 + 0.12, 0, 0},
        {"STAGE 2", 1600, 1.07, 1.18, 0, 0},
        {"STAGE 3", 2300, 1.09, 1.25, 100, 0},
        {"STANDALONE", 3200, 1.11, 1.30, 300, 0},
        {"DEVIR SINIRI +500", 1300, 1.03, 1.06, 500, 0},
        {"FLEX FUEL HARITASI", 2000, 1.05, 1.16, 0, 0},
        {"ANTI-LAG", 2600, 1.03, 1.12, 0, 0.07},
        {"LAUNCH + FLAT SHIFT", 2200, 1.06, 1.14, 200, 0.03},
        {"YARIS HARITASI", 4000, 1.13, 1.35, 400, 0.02},
    };
    return t;
}
const std::vector<FuelOpt>& fuelTable() {
    static const std::vector<FuelOpt> t = {
        {"100 OKTAN", 0, (int)FuelType::Race100, 1.0}, {"95 OKTAN", 0, (int)FuelType::Pump95, 1.0},
        {"E85 + SISTEM", 1500, (int)FuelType::E85, 1.0}, {"98 OKTAN", 0, (int)FuelType::Race100, 0.99},
        {"102 OKTAN", 200, (int)FuelType::Race100, 1.01}, {"104 YARIS BENZINI", 400, (int)FuelType::Race100, 1.02},
        {"110 YARIS BENZINI", 700, (int)FuelType::Race100, 1.03}, {"116 OKSIJENLI", 1100, (int)FuelType::Race100, 1.045},
        {"E50 KARISIM", 900, (int)FuelType::E85, 0.98}, {"METANOL (M100)", 2600, (int)FuelType::E85, 1.06},
    };
    return t;
}
const std::vector<OilOpt>& oilTable() {
    static const std::vector<OilOpt> t = {
        {"ISLAK KARTER", 0, false, 4.5}, {"KURU KARTER", 3500, true, 4.5}, {"BAFILLI KARTER", 450, false, 5.0},
        {"GENIS KARTER", 900, false, 6.5}, {"KANATLI KARTER", 650, false, 5.2}, {"ALU YARIS KARTERI", 1300, false, 6.0},
        {"KURU KARTER 3 KADEME", 4200, true, 6.0}, {"KURU KARTER 5 KADEME", 5800, true, 7.0}, {"YARIS YAG SISTEMI", 7500, true, 8.0},
    };
    return t;
}
const std::vector<ClutchOpt>& clutchTable() {
    static const std::vector<ClutchOpt> t = {
        {"STOK", 0, 1.0}, {"STAGE 1", 900, 1.3}, {"STAGE 2", 1800, 1.6}, {"STAGE 3 METAL", 3200, 2.0},
        {"GUCLENDIRILMIS ORGANIK", 600, 1.15}, {"KEVLAR", 1400, 1.45}, {"SERAMIK PUCK", 2400, 1.8},
        {"CIFT PLAKA", 4200, 2.4}, {"UC PLAKA", 5600, 2.9}, {"KAYMALI YARIS (SLIPPER)", 7500, 3.4},
    };
    return t;
}
const std::vector<AxleOpt>& axleTable() {
    static const std::vector<AxleOpt> t = {
        {"STOK", 0, 0, 1.0}, {"KROM-MOLY", 1500, 1, 1.12}, {"YARIS 300M", 3800, 2, 1.25}, {"KROM-MOLY KALIN", 2200, 1, 1.18},
        {"4340 DOVME", 2800, 1, 1.22}, {"300M KALIN", 4600, 2, 1.32}, {"TAM YUZER AKS", 5500, 2, 1.40},
        {"35 KANAL", 6200, 2, 1.45}, {"40 KANAL", 7400, 2, 1.55}, {"PRO MOD", 9500, 2, 1.70},
    };
    return t;
}
const std::vector<DiffOpt>& diffTable() {
    static const std::vector<DiffOpt> t = {
        {"ACIK", 0, 0.0, 0.0, 45.0, 60.0}, {"1.5-WAY LSD", 1400, 60.0, 0.55, 45.0, 60.0}, {"2-WAY LSD", 1700, 80.0, 0.55, 45.0, 45.0},
        {"KAYNAK SPOOL", 300, 2500.0, 1.0, 45.0, 60.0}, {"VISKOZ LSD", 900, 25.0, 0.30, 45.0, 60.0},
        {"TORSEN", 1300, 20.0, 0.45, 40.0, 70.0}, {"1-WAY LSD", 1500, 50.0, 0.55, 45.0, 85.0},
        {"SERT 1.5-WAY YARIS", 2200, 150.0, 0.70, 40.0, 55.0}, {"DRAG LSD (YUKSEK ON YUK)", 2000, 300.0, 0.80, 35.0, 60.0},
        {"BILLET SPOOL", 1600, 2500.0, 1.0, 45.0, 60.0},
    };
    return t;
}
const std::vector<FinalOpt>& finalTable() {
    static const std::vector<FinalOpt> t = {
        {"FABRIKA", 0, 1.0}, {"KISA +%15", 700, 1.15}, {"UZUN -%10", 700, 0.90}, {"KISA +%5", 600, 1.05},
        {"KISA +%10", 650, 1.10}, {"KISA +%20", 750, 1.20}, {"KISA +%25", 800, 1.25}, {"DRAG +%30", 850, 1.30},
        {"DRAG +%40", 950, 1.40}, {"UZUN -%15 (SON HIZ)", 750, 0.85}, {"UZUN -%5", 600, 0.95}, {"OZEL ORAN (ATOLYE)", 0, 1.0},
    };
    return t;
}
const std::vector<GearOpt>& gearTable() {
    static const std::vector<GearOpt> t = {
        {"FABRIKA", 0, nullptr, 0},
        {"HONDA K 6 ILERI", 2600, "KZ6K", 380}, {"GETRAG V160 6 ILERI", 5200, "TS6V", 700}, {"NISSAN CD009 6 ILERI", 3400, "NS6C", 550},
        {"TOYOTA W58 5 ILERI", 1800, "TS5W", 420}, {"TOYOTA R154 5 ILERI", 2600, "TS5R", 560}, {"GETRAG 420G 6 ILERI", 3800, "BM6G", 600},
        {"GETRAG 265 DOGLEG", 3000, "BM5E", 380}, {"PORSCHE G96 6 ILERI", 4800, "PO6G", 650}, {"TREMEC T56 6 ILERI", 4200, "GM6T", 750},
        {"TREMEC TR6060", 4800, "GM6R", 900}, {"TREMEC 7 ILERI", 5600, "GM7T", 950}, {"BORGWARNER T5", 1500, "FO5T", 400},
        {"TREMEC TR-3160", 4000, "FO6T", 650}, {"MUNCIE M22 4 ILERI", 2800, "GM4M", 850}, {"CHRYSLER A833", 2400, "CH4M", 800},
        {"NISSAN GR6 (CIFT KAVRAMA)", 7500, "NS6D", 850, 0.8}, {"VW DQ250 DSG (CIFT KAVRAMA)", 4200, "VW6D", 400, 1.0}, {"VW DQ500 DSG 7 (CIFT KAVRAMA)", 6200, "VW7D", 650, 0.9},
        {"PORSCHE PDK 7 (CIFT KAVRAMA)", 8800, "PO7P", 800, 0.7}, {"GETRAG 7 DCT (CIFT KAVRAMA)", 7400, "BM7D", 700, 0.85}, {"PORSCHE CUP (SIRALI)", 9800, "PO6S", 900, 0.8},
        {"LAMBORGHINI ISR 7 (SIRALI)", 11000, "LB7I", 900, 1.4}, {"HEWLAND DOGBOX (SIRALI)", 12500, "RC6S", 1300, 0.7}, {"TH400 3 ILERI (OTOMATIK)", 1800, "GM3A", 1200, 1.3},
        {"TORQUEFLITE 727 (OTOMATIK)", 1600, "CH3A", 950, 1.4}, {"6L80 6 ILERI (OTOMATIK)", 3200, "GM6A", 760, 1.0}, {"ZF 8HP (OTOMATIK)", 4800, "ZF8H", 900, 0.7},
        {"FORD 10R80 10 ILERI (OTOMATIK)", 5600, "TR10", 1000, 0.8}, {"GRAZIANO 7 (SIRALI)", 9500, "MC7D", 900, 0.9}, {"OZEL VITES ORANLARI (ATOLYE)", 0, nullptr, 0},
        // 31+: ucuz / eski kutular (yavas gecis). Kayit indeksleri korunur: yeniler sona eklenir.
        {"ESKI 4 ILERI (OTOMATIK, YAVAS)", 700, "GM4A", 520, 1.8}, {"KAMYONET 4 ILERI (OTOMATIK, YAVAS)", 600, "TR4A", 700, 1.9},
        {"AISIN 5 ILERI (OTOMATIK)", 1300, "TS5A", 480, 1.4}, {"SUBARU 4EAT (OTOMATIK, YAVAS)", 800, "SB4A", 420, 1.7},
        {"MERCEDES 722.6 5 ILERI (OTOMATIK)", 1900, "MB5A", 700, 1.3}, {"ZF 6HP (OTOMATIK)", 2600, "ZF6H", 650, 1.1},
        {"UCUZ 5 ILERI MANUEL (SERT VITES)", 650, "TS5T", 330, 1.5}, {"KULLANILMIS T5 5 ILERI", 900, "FO5T", 380, 1.3},
    };
    return t;
}
const std::vector<WeightOpt>& weightTable() {
    static const std::vector<WeightOpt> t = {
        {"STOK", 0, 0}, {"STEPNE + BAGAJ -15 KG", 100, 15}, {"ARKA KOLTUK -25 KG", 250, 25}, {"IC DOSEME -60 KG", 900, 60},
        {"YARIS KOLTUKLARI -75 KG", 1500, 75}, {"TAM YARIS ICI -110 KG", 3000, 110},
    };
    return t;
}
const std::vector<TireOpt>& tireTable() {
    const int S = (int)TireType::Street, M = (int)TireType::SemiSlick, D = (int)TireType::DragSlick;
    static const std::vector<TireOpt> t = {
        {"SOKAK", 0, S, 1.0, 0}, {"YARI-SLICK", 1200, M, 1.0, 0}, {"DRAG SLICK", 2800, D, 1.0, 0},
        {"EKO 175/65", 150, S, 0.86, -2.0}, {"TOURING 195/60", 300, S, 0.93, -1.0}, {"SPOR 205/50", 500, S, 1.02, 0},
        {"UHP 225/45", 750, S, 1.06, 0.5}, {"UHP 245/40", 950, S, 1.09, 1.0}, {"UHP 275/35", 1200, S, 1.12, 1.5},
        {"KIS LASTIGI", 450, S, 0.90, 0}, {"DRIFT 235 SERT", 400, S, 0.95, 0.5}, {"RALLY TOPRAK", 650, S, 0.92, 1.0},
        {"YARI SLICK 225", 1000, M, 0.97, -0.5}, {"R-COMPOUND 245", 1500, M, 1.04, 0.5}, {"R-COMPOUND 275", 1900, M, 1.08, 1.0},
        {"PIST SLICK 285", 2600, M, 1.12, 1.5}, {"YARI SLICK 315", 2400, M, 1.10, 2.5},
        {"DRAG RADIAL 245/60", 1600, D, 0.90, -1.0}, {"DRAG RADIAL 275/60", 2000, D, 0.95, 0}, {"DRAG RADIAL 315/60", 2500, D, 0.99, 1.5},
        {"SLICK 26X8", 1900, D, 0.92, -1.5}, {"SLICK 26X10", 2300, D, 0.97, -0.5}, {"SLICK 28X10.5", 3000, D, 1.02, 0.5},
        {"SLICK 29.5X10.5", 3400, D, 1.05, 1.0}, {"SLICK 30X12", 4000, D, 1.09, 2.0}, {"SLICK 32X14", 4800, D, 1.12, 3.0},
        {"SLICK 33X10.5", 4400, D, 1.10, 2.5}, {"PRO MOD 34.5X17", 6500, D, 1.18, 5.0}, {"KOSE LASTIGI 315 R", 2200, M, 1.11, 2.0},
        {"YAPISKAN SOKAK 255", 1300, S, 1.10, 1.0},
    };
    return t;
}
const std::vector<RimOpt>& rimTable() {
    static const std::vector<RimOpt> t = {
        {"STOK", 0, 0}, {"DOKUM 15 INC", 400, -1.0}, {"DOKUM 17 INC", 700, 0.5}, {"DOVME 17 INC", 1600, -2.5},
        {"FLOW-FORM 18 INC", 1400, -1.0}, {"DOVME 18 INC", 2200, -2.0}, {"MAGNEZYUM", 3200, -4.0}, {"KARBON", 5200, -6.0},
        {"DRAG ON INCE (RUNNER)", 1200, -3.0}, {"BEADLOCK DRAG", 1800, 1.0},
    };
    return t;
}
const std::vector<SuspOpt>& suspTable() {
    static const std::vector<SuspOpt> t = {
        {"STOK", 0, 0, 1.0}, {"SPOR YAY -25 MM", 450, 25, 1.15}, {"COILOVER SOKAK", 1200, 35, 1.25}, {"COILOVER YARIS", 2400, 50, 1.50},
        {"HAVALI SUSPANSIYON", 2800, 40, 1.0}, {"DRAG ARKA YAY", 900, 10, 0.85}, {"DRAG PAKETI", 1800, 20, 0.80},
        {"DRIFT KITI", 2000, 45, 1.60}, {"RALLY (+20 MM)", 1600, -20, 1.10}, {"PIST PAKETI", 3600, 60, 1.80},
    };
    return t;
}
const std::vector<BrakeOpt>& brakeTable() {
    static const std::vector<BrakeOpt> t = {
        {"STOK", 0, 1.0}, {"SPOR BALATA", 200, 1.10}, {"YARIS BALATA", 400, 1.22}, {"SERAMIK SOKAK", 250, 1.08},
        {"PERFORMANS", 300, 1.15}, {"PIST BALATASI", 550, 1.28}, {"KARBON METALIK", 700, 1.32}, {"DRAG BALATASI", 350, 1.12},
    };
    return t;
}
const std::vector<AeroOpt>& aeroTable() {
    static const std::vector<AeroOpt> t = {
        {"STOK", 0, 1.0, 0}, {"LIP SPOILER", 300, 1.00, 80}, {"KUCUK KANAT", 700, 1.02, 250}, {"ARKA SPOILER", 500, 1.01, 180},
        {"DUCKTAIL", 650, 1.00, 140}, {"GT KANAT", 1500, 1.05, 450}, {"YARIS KANADI", 2600, 1.08, 700},
        {"SWAN NECK KANAT", 3200, 1.06, 750}, {"DRAG PAKETI (DUSUK SURTUNME)", 1400, 0.90, 0},
        {"OZEL KANAT (ATOLYE)", 0, 1.0, 0},
    };
    return t;
}
const std::vector<ElecOpt>& elecTable() {
    static const std::vector<ElecOpt> t = {
        {"FABRIKA", 0, false, false, 0.10, 0.12}, {"ABS (ECU ILE)", 1600, true, false, 0.10, 0.12},
        {"ABS + TC (ECU ILE)", 3000, true, true, 0.10, 0.12}, {"ABS + TC SPOR", 3300, true, true, 0.14, 0.13},
        {"ABS + TC DRAG", 3400, true, true, 0.18, 0.14}, {"ABS + TC YAGMUR", 3200, true, true, 0.06, 0.10},
        {"ABS YARIS + TC PIST", 3800, true, true, 0.12, 0.16}, {"SADECE TC", 1900, false, true, 0.10, 0.12},
        {"SADECE TC DRAG", 2100, false, true, 0.20, 0.12}, {"YARIS ELEKTRONIGI", 5200, true, true, 0.13, 0.15},
    };
    return t;
}

// ---- atolye (ozel uretim) ----
TurboOpt customTurbo(const Tune& t) {
    const double mm = std::clamp(t.custTurboMm > 0 ? t.custTurboMm : 60.0, 38.0, 100.0);
    const double ar = std::clamp(t.custTurboAr > 0 ? t.custTurboAr : 0.82, 0.48, 1.32);
    const double bar = std::max(0.2, (mm - 34.0) * 0.06);
    const double lo = std::clamp(0.16 + (mm - 38.0) * 0.0068 + (ar - 0.82) * 0.14, 0.12, 0.75);
    const double hi = std::clamp(lo + 0.20 + (mm - 38.0) * 0.0018 + (ar - 0.82) * 0.06, lo + 0.12, 0.95);
    return {"OZEL TURBO", 0, bar, lo, hi};
}
CamOpt customCam(const Tune& t) {
    const double d = std::clamp(t.custCamDeg > 0 ? t.custCamDeg : 256.0, 230.0, 330.0);
    return {"OZEL KAM", 0, 1.04 - (d - 250.0) * 0.0032, 1.0 + (d - 250.0) * 0.0026, std::max(0.0, (d - 260.0) * 9.0)};
}
AeroOpt customAero(const Tune& t) {
    const double df = std::clamp(t.custWingN, 0.0, 1600.0);
    return {"OZEL KANAT", 0, 1.0 + df / 1600.0 * 0.12, df};
}

// ---- motor / sanziman swap ----
const std::vector<int>& swapEngines() {
    static const std::vector<int> list = [] {
        // Kayitta swap indeksi tutulur: ilk 280 motor guce gore siralanir (eski kayit uyumu), sonra eklenenler kendi
        // aralarinda guce gore listenin sonuna (araya girmez)
        constexpr int kLegacy = 280;
        std::vector<int> idx(engineTable().size());
        std::iota(idx.begin(), idx.end(), 0);
        auto byHp = [](int a, int b) { return engineTable()[a].powerHp < engineTable()[b].powerHp; };
        const int n0 = std::min(kLegacy, (int)idx.size());
        std::stable_sort(idx.begin(), idx.begin() + n0, byHp);
        std::stable_sort(idx.begin() + n0, idx.end(), byHp);
        return idx;
    }();
    return list;
}
int effectiveEngine(const VehicleDef& v, const Tune* t) {
    if (t && t->engineSwap > 0 && t->engineSwap <= (int)swapEngines().size()) return swapEngines()[t->engineSwap - 1];
    return v.engine;
}
double gearShiftMul(const Tune* t) {
    if (!t || t->gearSwap <= 0 || t->gearSwap >= (int)gearTable().size()) return 1.0;
    return gearTable()[t->gearSwap].shiftMul;
}
int effectiveGearbox(const VehicleDef& v, const Tune* t) {
    if (t && t->gearSwap > 0 && t->gearSwap < (int)gearTable().size() && gearTable()[t->gearSwap].code) {
        const int g = findGearbox(gearTable()[t->gearSwap].code);
        if (g >= 0) return g;
    }
    return v.gearbox;
}
double swapMassDelta(const VehicleDef& v, const Tune& t) {
    const int e = effectiveEngine(v, &t);
    if (e == v.engine) return 0.0;
    const EngineDef &a = engineTable()[v.engine], &b = engineTable()[e];
    auto m = [](const EngineDef& d) {
        const bool forced = d.induction == Induction::Turbo || d.induction == Induction::TwinTurbo || d.induction == Induction::Supercharger;
        return 60.0 + 28.0 * d.displacementL + 9.0 * d.cylinders + (forced ? 18.0 : 0.0);
    };
    return m(b) - m(a);
}

} // namespace zk

namespace zk {

int Tune::weightKg(int level) {
    const auto& t = weightTable();
    return (int)t[level < 0 ? 0 : level >= (int)t.size() ? (int)t.size() - 1 : level].kg;
}

std::string Tune::signature() const {
    char b[512];
    std::snprintf(b, sizeof b, "%d|%.1f|%d|%d|%d|%.3f|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d"
                  "|%d|%.1f|%.2f|%.0f|%.3f|%.3f|%.0f|%.2f|%.2f|%.2f|%.2f|%.2f|%.2f|%.2f|%.2f",
                  (int)tires, psi, clutch, axles, (int)diff, finalDrive, weight, intake, exhaust, ecu, turbo, drySump ? 1 : 0,
                  absKit ? 1 : 0, tcKit ? 1 : 0, (int)fuel, tireSel, rims, finalSel, gearSwap, engineSwap, cam, valve,
                  flywheel, superch, intercooler, fuelSys, nitrous, oil, fuelSel, elec, susp, brakes, aero,
                  head * 1000000 + piston * 10000 + rod * 100 + crank, bearing * 10000 + gasket * 100 + turbine,
                  wastegate * 10000 + boostCtl * 100 + gbStrength, cooling,
                  custTurboMm, custTurboAr, custCamDeg, custDisp, custFinal, custWingN,
                  custGear[0], custGear[1], custGear[2], custGear[3], custGear[4], custGear[5], custGear[6], custGear[7]);
    char w[96];
    std::snprintf(w, sizeof w, "|w%.2f,%.2f,%.2f,%.2f,%.2f,%.2f", wearEngine, wearTires, wearBrakes, wearSusp, wearBody, wearElec);
    std::string out = std::string(b) + w;
    if (launchRpm > 0) { char l[16]; std::snprintf(l, sizeof l, "|L%d", launchRpm); out += l; }
    if (throttleBody || intakeMani || header || catalyst || meth || headStud || mainSupport || brakeDisc || brakeCaliper || weightBody
        || weightGlass || weightChassis || aeroFront || aeroSide || aeroUnder || fan || coolMisc || oilCooler || oilPump) {
        char c[160];
        std::snprintf(c, sizeof c, "|C%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", throttleBody, intakeMani, header, catalyst, meth,
                      headStud, mainSupport, brakeDisc, brakeCaliper, weightBody, weightGlass, weightChassis, aeroFront, aeroSide, aeroUnder,
                      fan, coolMisc, oilCooler, oilPump);
        out += c;
    }
    if (ecuTiming || ecuAfr || ecuBoost) { char d[48]; std::snprintf(d, sizeof d, "|D%d,%d,%d", ecuTiming, ecuAfr, ecuBoost); out += d; }
    if (fuelPump || injector || fuelLine || ecuNewSystem(*this)) {
        char e[96];
        std::snprintf(e, sizeof e, "|F%d,%d,%d|E%d,%d,%d,%d,%d,%d,%d,%d", fuelPump, injector, fuelLine, ecuHw, swMap, swRev, swLaunch, swFlat,
                      swAntiLag, swFlex, swKnock);
        out += e;
        if (swTcu) { char g[16]; std::snprintf(g, sizeof g, "|T%d", swTcu); out += g; }
    }
    if (setRideR || setSpringR || setDampR) { char q[48]; std::snprintf(q, sizeof q, "|Q%d,%d,%d", setRideR, setSpringR, setDampR); out += q; }
    if (setRide || setSpring || setDamp || setArbF || setArbR || setPreload || setNos) {
        char k[64];
        std::snprintf(k, sizeof k, "|K%d,%d,%d,%d,%d,%d,%d", setRide, setSpring, setDamp, setArbF, setArbR, setPreload, setNos);
        out += k;
    }
    return out;
}

} // namespace zk

namespace zk {

Durability durability(const VehicleDef& v, const Tune* t) {
    double tf = 0.0;                                           // takili motorun fabrika tepe torku
    const EngineSpec e = buildEngineSpecFor(effectiveEngine(v, t));
    for (auto* c : {&e.lowCam, &e.highCam}) for (auto& p : *c) tf = std::max(tf, p.second);
    double factoryBox = 0.0;                                   // fabrika sanzimani fabrika motoruna gore boyutlanir
    {
        const EngineSpec f = buildEngineSpec(v);
        for (auto* c : {&f.lowCam, &f.highCam}) for (auto& p : *c) factoryBox = std::max(factoryBox, p.second);
        factoryBox *= 1.9;
    }
    if (!t) return {tf * 1.30, factoryBox};
    auto rowT = [](const auto& tab, int i) -> const auto& { return tab[std::clamp(i, 0, (int)tab.size() - 1)]; };
    double weakest = std::min({rowT(pistonTable(), t->piston).strength, rowT(rodTable(), t->rod).strength,
                               rowT(crankTable(), t->crank).strength});
    double gasket = rowT(gasketTable(), t->gasket).strength * rowT(headStudTable(), t->headStud).strength;
    if (t->turbo == 1 || t->turbo == 2) { weakest = std::max(weakest, 1.9); gasket = std::max(gasket, 1.6); }   // v1 kitleri dovme ic aksamli
    const EngineDef& ed = engineTable()[effectiveEngine(v, t)];
    const bool boosted = ed.induction == Induction::Turbo || ed.induction == Induction::TwinTurbo ||
                         ed.induction == Induction::Supercharger || t->turbo > 0 || t->superch > 0;
    const double eng = tf * 1.30 * weakest * rowT(bearingTable(), t->bearing).strength * rowT(mainSupportTable(), t->mainSupport).strength * (boosted ? std::min(gasket, 1.0 + (weakest - 1.0)) : 1.0);
    double box = factoryBox;
    if (t->gearSwap > 0 && t->gearSwap < (int)gearTable().size() && gearTable()[t->gearSwap].strengthNm > 0) box = gearTable()[t->gearSwap].strengthNm;
    box *= rowT(gbStrengthTable(), t->gbStrength).mul;
    return {std::max(eng, tf * 1.30 * std::min(1.0, weakest)), box};
}

} // namespace zk
