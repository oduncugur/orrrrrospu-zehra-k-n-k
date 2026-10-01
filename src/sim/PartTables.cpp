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
        {"BUYUK GAZ KELEBEGI", 700, 1.0, 1.07},
        {"PORTLU MANIFOLD", 1400, 0.98, 1.10},
        {"KARBON AIRBOX", 2200, 1.05, 1.09},
        {"ITB TEKIL KELEBEK", 4200, 1.04, 1.13},
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
        {"HEADER 4-2-1", 1300, 1.05, 1.04},
        {"HEADER 4-1 (TEPE)", 1600, 0.99, 1.08},
        {"TUBULAR MANIFOLD", 2000, 1.03, 1.08},
        {"KATALIZOR IPTAL", 400, 1.02, 1.03},
        {"TITANYUM YARIS", 3800, 1.04, 1.10},
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
        {"GENIS YATAK", 800, 1.25}, {"ANA YATAK KUSAGI", 1400, 1.30}, {"ARP ANA CIVATA", 500, 1.15},
        {"BILLET ANA KAPAK", 1900, 1.35}, {"KUSAK + ARP", 2200, 1.45}, {"TAM BLOK DESTEK", 3600, 1.60},
    };
    return t;
}
const std::vector<GasketOpt>& gasketTable() {
    static const std::vector<GasketOpt> t = {
        {"STOK", 0, 0.0, 1.0}, {"MLS 0.5 MM (YUKSEK CR)", 300, 0.50, 1.10}, {"MLS 0.7 MM", 300, 0.70, 1.20},
        {"MLS 1.0 MM", 350, 1.00, 1.30}, {"MLS 1.3 MM (BOOST)", 400, 1.30, 1.45}, {"BAKIR CONTA", 600, 1.00, 1.60},
        {"O-RING BLOK", 1500, 0.90, 1.90}, {"BAKIR + O-RING", 1900, 1.10, 2.20}, {"ARP KAPAK SAPLAMASI", 600, 0.0, 1.30},
        {"SAPLAMA + MLS 1.0", 850, 1.00, 1.55},
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
        {"BUYUK ALU RADYATOR", 1000, 1.70, 0.45}, {"ELEKTRIK FAN", 350, 1.15, 0.70}, {"CIFT FAN", 550, 1.25, 0.85},
        {"DUSUK ISI TERMOSTAT", 120, 1.10, 0.45}, {"SU KATKISI", 60, 1.08, 0.42}, {"YARIS RADYATORU + FAN", 1800, 2.10, 0.85},
        {"TAM YARIS SOGUTMA", 3200, 2.60, 1.00},
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
        {"KUCUK KIT", 4500, 0.6 * 1, 0.32 + 0.08 * 1, 0.55 + 0.07 * 1},
        {"BUYUK KIT", 9000, 0.6 * 2, 0.32 + 0.08 * 2, 0.55 + 0.07 * 2},
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
        {"METANOL ENJEKSIYON", 1600, 1.08}, {"SU + METANOL", 2000, 1.10}, {"CO2 SOGUTMA SPREYI", 1200, 1.05},
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
        {"YAG SOGUTUCU", 600, false, 5.0}, {"AKUMULATOR", 800, false, 5.6}, {"GENIS KARTER", 900, false, 6.5},
        {"YUKSEK HACIMLI POMPA", 700, false, 5.5}, {"KURU KARTER 3 KADEME", 4200, true, 6.0},
        {"KURU KARTER 5 KADEME", 5800, true, 7.0}, {"YARIS YAG SISTEMI", 7500, true, 8.0},
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
        {"NISSAN GR6 CIFT KAVRAMA", 7500, "NS6D", 850}, {"VW DQ250 DSG", 4200, "VW6D", 400}, {"VW DQ500 DSG 7", 6200, "VW7D", 650},
        {"PORSCHE PDK 7", 8800, "PO7P", 800}, {"GETRAG 7 DCT", 7400, "BM7D", 700}, {"PORSCHE SIRALI (CUP)", 9800, "PO6S", 900},
        {"LAMBORGHINI ISR 7", 11000, "LB7I", 900}, {"HEWLAND SIRALI DOGBOX", 12500, "RC6S", 1300}, {"TH400 3 ILERI OTOMATIK", 1800, "GM3A", 1200},
        {"TORQUEFLITE 727", 1600, "CH3A", 950}, {"6L80 OTOMATIK", 3200, "GM6A", 760}, {"ZF 8HP OTOMATIK", 4800, "ZF8H", 900},
        {"FORD 10R80 10 ILERI", 5600, "TR10", 1000}, {"GRAZIANO 7 SIRALI", 9500, "MC7D", 900}, {"OZEL VITES ORANLARI (ATOLYE)", 0, nullptr, 0},
    };
    return t;
}
const std::vector<WeightOpt>& weightTable() {
    static const std::vector<WeightOpt> t = {
        {"STOK", 0, 0}, {"-40 KG", 600, 40}, {"-85 KG", 1800, 85}, {"-140 KG", 4000, 140},
        {"STEPNE + BAGAJ -15 KG", 100, 15}, {"ARKA KOLTUK -25 KG", 250, 25}, {"IC DOSEME -60 KG", 900, 60},
        {"KARBON KAPUT + BAGAJ -110 KG", 3000, 110}, {"LEXAN CAM + KARBON -175 KG", 5200, 175},
        {"TAM YARIS ICI -220 KG", 7000, 220}, {"KARBON PANELLER -280 KG", 9800, 280}, {"BORU SASI ON -350 KG", 14000, 350},
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
        {"STOK", 0, 1.0}, {"SPOR BALATA", 200, 1.10}, {"DELIKLI DISK", 450, 1.15}, {"YARIS BALATA", 400, 1.22},
        {"4 PISTONLU KIT", 1600, 1.32}, {"6 PISTONLU KIT", 2600, 1.45}, {"BUYUK DISK 380 MM", 3200, 1.55},
        {"YARIS KITI", 4200, 1.70}, {"KARBON SERAMIK", 7800, 1.90}, {"KARBON-KARBON", 11000, 2.10},
    };
    return t;
}
const std::vector<AeroOpt>& aeroTable() {
    static const std::vector<AeroOpt> t = {
        {"STOK", 0, 1.0, 0}, {"ON LIP", 300, 0.99, 120}, {"YAN ETEK", 400, 0.99, 40}, {"ARKA SPOILER", 500, 1.01, 180},
        {"DUCKTAIL", 650, 1.00, 140}, {"GT KANAT", 1500, 1.05, 450}, {"YARIS KANADI", 2600, 1.08, 700},
        {"DIFUZOR + ALT KAPLAMA", 1800, 0.97, 260}, {"DRAG PAKETI (DUSUK SURTUNME)", 1400, 0.90, 0},
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
        std::vector<int> idx(engineTable().size());
        std::iota(idx.begin(), idx.end(), 0);
        std::stable_sort(idx.begin(), idx.end(), [](int a, int b) { return engineTable()[a].powerHp < engineTable()[b].powerHp; });
        return idx;
    }();
    return list;
}
int effectiveEngine(const VehicleDef& v, const Tune* t) {
    if (t && t->engineSwap > 0 && t->engineSwap <= (int)swapEngines().size()) return swapEngines()[t->engineSwap - 1];
    return v.engine;
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
                  "|%.1f|%.2f|%.0f|%.3f|%.3f|%.0f|%.2f|%.2f|%.2f|%.2f|%.2f|%.2f|%.2f|%.2f",
                  (int)tires, psi, clutch, axles, (int)diff, finalDrive, weight, intake, exhaust, ecu, turbo, drySump ? 1 : 0,
                  absKit ? 1 : 0, tcKit ? 1 : 0, (int)fuel, tireSel, rims, finalSel, gearSwap, engineSwap, cam, valve,
                  flywheel, superch, intercooler, fuelSys, nitrous, oil, fuelSel, elec, susp, brakes, aero,
                  head * 1000000 + piston * 10000 + rod * 100 + crank, bearing * 10000 + gasket * 100 + turbine,
                  wastegate * 10000 + boostCtl * 100 + gbStrength, cooling,
                  custTurboMm, custTurboAr, custCamDeg, custDisp, custFinal, custWingN,
                  custGear[0], custGear[1], custGear[2], custGear[3], custGear[4], custGear[5], custGear[6], custGear[7]);
    char w[96];
    std::snprintf(w, sizeof w, "|w%.2f,%.2f,%.2f,%.2f,%.2f,%.2f", wearEngine, wearTires, wearBrakes, wearSusp, wearBody, wearElec);
    return std::string(b) + w;
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
    double gasket = rowT(gasketTable(), t->gasket).strength;
    if (t->turbo == 1 || t->turbo == 2) { weakest = std::max(weakest, 1.9); gasket = std::max(gasket, 1.6); }   // v1 kitleri dovme ic aksamli
    const EngineDef& ed = engineTable()[effectiveEngine(v, t)];
    const bool boosted = ed.induction == Induction::Turbo || ed.induction == Induction::TwinTurbo ||
                         ed.induction == Induction::Supercharger || t->turbo > 0 || t->superch > 0;
    const double eng = tf * 1.30 * weakest * rowT(bearingTable(), t->bearing).strength * (boosted ? std::min(gasket, 1.0 + (weakest - 1.0)) : 1.0);
    double box = factoryBox;
    if (t->gearSwap > 0 && t->gearSwap < (int)gearTable().size() && gearTable()[t->gearSwap].strengthNm > 0) box = gearTable()[t->gearSwap].strengthNm;
    box *= rowT(gbStrengthTable(), t->gbStrength).mul;
    return {std::max(eng, tf * 1.30 * std::min(1.0, weakest)), box};
}

} // namespace zk
