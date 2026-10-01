#include "Lang.h"

#include <cctype>
#include <unordered_map>
#include <vector>

namespace zk {

namespace {

struct Entry { const char* tr; const char* tx; };

// ---------------------------------------------------------------- INGILIZCE
// Tam cumleler (dil bilgisi icin; tam esleme)
const Entry kEnPhrases[] = {
    {"TEKRAR DOKUN: SATIN AL", "TAP AGAIN: BUY"}, {"SATIN ALINSIN MI?", "BUY THIS CAR?"}, {"ARAC SATILSIN MI?", "SELL THIS CAR?"},
    {"PARA YETMIYOR", "NOT ENOUGH MONEY"}, {"ZATEN TAKILI", "ALREADY FITTED"}, {"HAYIRLI OLSUN!", "CONGRATULATIONS!"},
    {"SON ARACINI SATAMAZSIN", "YOU CANT SELL YOUR LAST CAR"}, {"TEK ARACIN SATILAMAZ", "CANT SELL YOUR ONLY CAR"},
    {"ONCE ECU GEREKLI", "NEEDS ECU FIRST"}, {"ONCE TURBO GEREKLI", "NEEDS A TURBO FIRST"}, {"ATOLYEDE URET", "BUILD IN WORKSHOP"},
    {"FABRIKADA VAR", "FACTORY FITTED"}, {"KOMPRESORLU MOTORA TURBO YOK", "NO TURBO ON SUPERCHARGED ENGINE"},
    {"ASIRI BESLEME YOK", "NO FORCED INDUCTION"}, {"FABRIKADA KOMPRESOR VAR", "FACTORY SUPERCHARGED"},
    {"HER ISCILIK ADIMI HASARI YARIYA INDIRIR", "EACH LABOUR STEP HALVES THE DAMAGE"},
    {"TAM TOPARLAMAK ICIN BIRKAC KEZ UGRAS", "WORK ON IT SEVERAL TIMES TO FULLY RESTORE"},
    {"KELEPIR AMA DOKUK. HER YARISTAN SONRA YENI ARACLAR", "CHEAP BUT WRECKED. NEW CARS AFTER EVERY RACE"},
    {"BU HALIYLE YARISAMAZ", "CANT RACE IN THIS STATE"}, {"BIR ARAC SEC", "PICK A CAR"}, {"MEKANIK CALISIYOR", "MECHANICALLY OK"},
    {"KARMA: DUZDE DRAG, VIRAJDA SURUS", "MIXED: DRAG ON STRAIGHTS, DRIVE THE CURVES"}, {"KARMA: DRAG + VIRAJ", "MIXED: DRAG + CURVES"},
    {"OTOBAN AKISI: YAKIN GEC, HIZLI GIT", "HIGHWAY FLOW: PASS CLOSE, GO FAST"}, {"OTOBAN AKISI 2 DK", "HIGHWAY FLOW 2 MIN"},
    {"YOL YARISI 4 KM", "ROAD RACE 4 KM"}, {"DAG YOLU 3 KM", "MOUNTAIN PASS 3 KM"}, {"SERBEST SURUS", "FREE DRIVE"},
    {"YARISLAR: RAKIP + TRAFIK, ODULLU", "RACES: RIVAL + TRAFFIC, PRIZE MONEY"}, {"AKIS: YAKIN GECIS + HIZ + VIRAJ = SKOR", "FLOW: NEAR MISS + SPEED + APEX = SCORE"},
    {"DOKUN / ENTER: GARAJ", "TAP / ENTER: GARAGE"}, {"YOL SONU - BASA DONULDU", "END OF ROAD - RESET"}, {"ARAC YOLA ALINDI", "CAR RECOVERED"},
    {"MOTOR STOP ETTI", "ENGINE STALLED"}, {"VIRAJDA VITES YOK (OTOMATIK DEBRIYAJ)", "NO SHIFTING IN CURVES (AUTO CLUTCH)"},
    {"OTOMATIK DEBRIYAJ: ODUL %75", "AUTO CLUTCH: PRIZE 75%"}, {"DEBRIYAJA BAS VE TUT", "PRESS AND HOLD CLUTCH"}, {"FRENE BAS VE TUT", "PRESS AND HOLD BRAKE"},
    {"DEBRIYAJ + GAZ, YESILDE BIRAK", "CLUTCH + THROTTLE, RELEASE ON GREEN"}, {"FREN + GAZ, YESILDE FRENI BIRAK", "BRAKE + THROTTLE, RELEASE BRAKE ON GREEN"},
    {"BURNOUT: GAZ + DEBRIYAJI KAYDIR", "BURNOUT: THROTTLE + SLIP THE CLUTCH"}, {"BURNOUT: FRENE BAS + GAZ", "BURNOUT: BRAKE + THROTTLE"},
    {"FRENE BAS!", "BRAKE!"}, {"VIRAJ YAKLASIYOR", "CURVE AHEAD"}, {"VIRAJ BOLUMU", "CURVES"}, {"DRAG  BITIS DUZLUGU", "DRAG  FINAL STRAIGHT"},
    {"MOTOR PATLADI", "ENGINE BLOWN"}, {"MOTOR PATLADI! (BIYEL / PISTON)", "ENGINE BLEW! (ROD / PISTON)"}, {"SANZIMAN KIRILDI!", "GEARBOX BROKE!"},
    {"SANZIMAN KIRIK", "GEARBOX BROKEN"}, {"MOTOR DAYANMAZ!", "ENGINE WONT HOLD!"}, {"SANZIMAN DAYANMAZ!", "GEARBOX WONT HOLD!"},
    {"SOGUTMA YETMEZ", "COOLING TOO WEAK"}, {"(YUZDE: TEPE TORK / DAYANIM, GUC / SOGUTMA)", "(PERCENT: PEAK TORQUE / STRENGTH, POWER / COOLING)"},
    {"OZEL URETIM ATOLYESI", "CUSTOM FABRICATION SHOP"}, {"SONUC (TAKILIRSA)", "RESULT (IF FITTED)"}, {"URETILDI VE TAKILDI", "BUILT AND FITTED"},
    {"KAYIT BOZUK - YENI OYUN", "SAVE CORRUPT - NEW GAME"}, {"DEGISIKLIK ANINDA UYGULANIR VE KAYDEDILIR", "CHANGES APPLY AND SAVE INSTANTLY"},
    {"VARSAYILANLAR YUKLENDI", "DEFAULTS RESTORED"}, {"HURDA ALT SINIRI UYGULANDI", "SCRAP VALUE FLOOR APPLIED"},
    {"YARIS ARACI (SOKAKTA SURULEMEZ)", "RACE CAR (NOT STREET LEGAL)"}, {"YARIS ARACI - ROMORK", "RACE CAR - TRAILER"},
    {"ACIK YOL", "OPEN ROAD"}, {"ARA TASLAK", "WORK IN PROGRESS"}, {"KAZANDIN!", "YOU WIN!"}, {"KAZANDIN", "YOU WIN"},
    {"KAYBETTIN", "YOU LOSE"}, {"KIRMIZI ISIK!", "RED LIGHT!"}, {"YESIL!", "GREEN!"}, {"SURE BITTI", "TIME UP"}, {"YENI REKOR!", "NEW RECORD!"},
    {"AGIR CARPISMA!", "HEAVY CRASH!"}, {"SERT TEMAS!", "HARD CONTACT!"}, {"TEMAS!", "CONTACT!"}, {"YOL DISI", "OFF ROAD"},
    {"HESAPLANIYOR...", "CALCULATING..."}, {"TAMIR ET", "REPAIR"}, {"SAT", "SELL"}, {"VAZGEC", "CANCEL"}, {"EVET", "YES"},
    {"ARAC (%65)", "CAR (65%)"}, {"PARCALAR (%40)", "PARTS (40%)"}, {"YAG / KARTER", "OIL / SUMP"}, {"KAPORTA + PAS", "BODY + RUST"},
    {"ELEKTRIK TESISATI", "WIRING"}, {"MOTOR REVIZYONU", "ENGINE REBUILD"}, {"SANZIMAN + AKS", "GEARBOX + AXLES"},
    {"SON DISLI ORANI", "FINAL DRIVE RATIO"}, {"KAM SURESI (DERECE)", "CAM DURATION (DEG)"}, {"HACIM ARTISI (%)", "DISPLACEMENT INCREASE (%)"},
    {"KOMPRESOR CAPI (MM)", "COMPRESSOR SIZE (MM)"}, {"BASKI KUVVETI (N, 100 KM/H)", "DOWNFORCE (N, 100 KM/H)"},
};

// Kelimeler (Turkce -> Ingilizce); "" = kelime atlanir (ek / bag)
const Entry kEnWords[] = {
    {"ABS", "ABS"}, {"ACI", "ANGLE"}, {"ACIK", "ON"}, {"ADAPTIF", "ADAPTIVE"}, {"ADIMI", "STEP"}, {"AERO", "AERO"}, {"AGACI", "TREE"},
    {"AGIR", "HEAVY"}, {"AGIRLIK", "WEIGHT"}, {"AKICI", "SMOOTH"}, {"AKIS", "FLOW"}, {"AKISI", "FLOW"}, {"AKS", "AXLE"},
    {"AKTARMA", "DRIVETRAIN"}, {"AKUMULATOR", "ACCUMULATOR"}, {"AL", "BUY"}, {"ALINDI", "BOUGHT"}, {"ALT", "LOWER"}, {"ALU", "ALU"},
    {"ALUMINYUM", "ALUMINIUM"}, {"AMA", "BUT"}, {"ANA", "MASTER"}, {"ANINDA", "INSTANTLY"}, {"APEX", "APEX"}, {"ARA", "MID"},
    {"ARAC", "CAR"}, {"ARACI", "CAR"}, {"ARACIN", "YOUR CAR"}, {"ARACINI", "CAR"}, {"ARACLAR", "CARS"}, {"ARACTA", "CAR HAS"},
    {"ARKA", "REAR"}, {"ARTISI", "INCREASE"}, {"ASIRI", "OVER"}, {"ATESLEME", "IGNITION"}, {"ATOLYE", "WORKSHOP"}, {"ATOLYEDE", "IN WORKSHOP"},
    {"ATOLYESI", "WORKSHOP"}, {"AYAR", "SETUP"}, {"AYARLAR", "SETTINGS"}, {"AZ", "LESS"}, {"BAFILLI", "BAFFLED"}, {"BAGAJ", "TRUNK"},
    {"BAKIR", "COPPER"}, {"BALATA", "PADS"}, {"BANT", "BARS"}, {"BAS", "PRESS"}, {"BASA", "START"}, {"BASINA", "PER"},
    {"BASKI", "DOWN"}, {"BASLANGIC", "START"}, {"BAZLI", "BASED"}, {"BEKLEMEZ", "NO WAIT"}, {"BENZINI", "FUEL"}, {"BESLEME", "INDUCTION"},
    {"BILYALI", "BALL BEARING"}, {"BIR", "A"}, {"BIRAK", "RELEASE"}, {"BIRDEN", "AT ONCE"}, {"BIRIMI", "UNIT"}, {"BIRKAC", "SEVERAL"},
    {"BITIREMEDI", "DNF"}, {"BITIS", "FINISH"}, {"BITTI", "OVER"}, {"BIYEL", "CON ROD"}, {"BLOK", "BLOCK"}, {"BOLUMU", "SECTION"},
    {"BORU", "PIPE"}, {"BORUSU", "PIPE"}, {"BOSLUK", "SPACE"}, {"BOZUK", "CORRUPT"}, {"BRONZ", "BRONZE"}, {"BU", "THIS"}, {"BUJI", "SPARK"},
    {"BUYUK", "LARGE"}, {"BUZ", "ICE"}, {"CALISIYOR", "WORKS"}, {"CAM", "GLASS"}, {"CAPI", "SIZE"}, {"CARKI", "WHEEL"}, {"CARPISMA", "CRASH"},
    {"CEKILIYOR", "PULLING"}, {"CEKIS", "TRACTION"}, {"CEKISI", "PULL"}, {"CELIK", "STEEL"}, {"CIFT", "TWIN"}, {"CIGLIGI", "SQUEAL"},
    {"CITIRTISI", "GRIND"}, {"CIVATA", "BOLTS"}, {"COK", "MULTI"}, {"CONTA", "GASKET"}, {"CONTASI", "GASKET"}, {"COZUNURLUK", "RESOLUTION"},
    {"DA", ""}, {"DAG", "MOUNTAIN"}, {"DAHA", "MORE"}, {"DAHILI", "INTERNAL"}, {"DAYANIM", "STRENGTH"}, {"DAYANMAZ", "WONT HOLD"},
    {"DEBI", "FLOW"}, {"DEBR", "CLUTCH"}, {"DEBRIYAJ", "CLUTCH"}, {"DEBRIYAJA", "CLUTCH"}, {"DEBRIYAJI", "CLUTCH"}, {"DEGER", "VALUE"},
    {"DEGERI", "VALUE"}, {"DEGISIKLIK", "CHANGE"}, {"DEGISIR", "CHANGES"}, {"DEGISKEN", "VARIABLE"}, {"DEGISTIR", "CHANGE"}, {"DELIKLI", "DRILLED"},
    {"DENGELENMIS", "BALANCED"}, {"DERECE", "DEGREE"}, {"DESEN", "PATTERN"}, {"DESTEGI", "BRACE"}, {"DESTEK", "SUPPORT"}, {"DESTEKLI", "ASSISTED"},
    {"DEVIR", "RPM"}, {"DIFERANSIYEL", "DIFFERENTIAL"}, {"DIFUZOR", "DIFFUSER"}, {"DIKEY", "PORTRAIT"}, {"DIREKS", "STEER"},
    {"DIREKSIYON", "STEERING"}, {"DIREKSIYONU", "STEERING"}, {"DIREKT", "DIRECT"}, {"DISI", "OFF"}, {"DISLI", "GEAR"}, {"DOKUK", "WRECKED"},
    {"DOKUM", "CAST"}, {"DOKUN", "TAP"}, {"DONULDU", "RESET"}, {"DOSEME", "TRIM"}, {"DOVME", "FORGED"}, {"DUSUK", "LOW"},
    {"DUZ", "STRAIGHT"}, {"DUZDE", "ON STRAIGHTS"}, {"DUZLUGU", "STRAIGHT"}, {"EGEREK", "TILTING"}, {"EGIM", "TILT"}, {"EGIMLE", "TILT"},
    {"EGZ", "EXH"}, {"EGZOZ", "EXHAUST"}, {"EKO", "ECO"}, {"EKRAN", "SCREEN"}, {"EKRANDA", "ON SCREEN"}, {"EKRANI", "SCREEN"},
    {"EL", "HAND"}, {"ELEK", "ELEC"}, {"ELEKTRIK", "ELECTRIC"}, {"ELEKTRONIGI", "ELECTRONICS"}, {"ELEKTRONIK", "ELECTRONICS"},
    {"EMME", "INTAKE"}, {"EN", "MOST"}, {"ENDEKS", "INDEX"}, {"ENJEKSIYON", "INJECTION"}, {"ENJEKTOR", "INJECTOR"}, {"ESITLEME", "SYNC"},
    {"ETEK", "SKIRTS"}, {"ETTI", ""}, {"EVET", "YES"}, {"FABRIKA", "FACTORY"}, {"FABRIKADA", "FACTORY"}, {"FILTRE", "FILTER"},
    {"FIZIK", "PHYSICS"}, {"FREN", "BRAKE"}, {"FRENE", "BRAKE"}, {"FRENI", "BRAKE"}, {"FRENLER", "BRAKES"}, {"GALERI", "DEALER"},
    {"GARAJ", "GARAGE"}, {"GARAJINDA", "IN GARAGE"}, {"GAZ", "GAS"}, {"GEC", "LATE"}, {"GECERSIZ", "INVALID"}, {"GECIS", "PASS"},
    {"GENIS", "WIDE"}, {"GEREKLI", "REQUIRED"}, {"GERI", "BACK"}, {"GIT", "GO"}, {"GORUNTU", "DISPLAY"}, {"GORUS", "VIEW"},
    {"GOSTERGESI", "GAUGE"}, {"GOVDE", "HOUSING"}, {"GRAFIK", "GRAPHICS"}, {"GUC", "POWER"}, {"GUCLENDIRILMIS", "REINFORCED"},
    {"GUCLENDIRME", "STRENGTHENING"}, {"HACIM", "DISPLACEMENT"}, {"HACIMLI", "VOLUME"}, {"HAFIF", "LIGHT"}, {"HAFIFLETILMIS", "LIGHTENED"},
    {"HAFIFLETME", "WEIGHT REDUCTION"}, {"HALIYLE", "STATE"}, {"HARICI", "EXTERNAL"}, {"HARITASI", "MAP"}, {"HASAR", "DAMAGE"},
    {"HASARI", "DAMAGE"}, {"HASARLI", "DAMAGED"}, {"HASSASIYET", "SENSITIVITY"}, {"HAVA", "AIR"}, {"HAVALI", "AIR"}, {"HER", "EVERY"},
    {"HESAPLANIYOR", "CALCULATING"}, {"HIZ", "SPEED"}, {"HIZI", "RATE"}, {"HIZLI", "FAST"}, {"HURDA", "SCRAP"}, {"HURDALIK", "JUNKYARD"},
    {"IC", "INNER"}, {"ICI", "INTERIOR"}, {"ICIN", "FOR"}, {"ILE", "WITH"}, {"ILERI", "SPEED"}, {"INC", "INCH"}, {"INCE", "THIN"},
    {"INDIRIR", "REDUCES"}, {"IPTAL", "DELETE"}, {"ISCILIK", "LABOUR"}, {"ISI", "HEAT"}, {"ISIK", "LIGHT"}, {"ISINMA", "HEAT"},
    {"ISLAK", "WET"}, {"JANT", "WHEELS"}, {"KADEME", "STAGE"}, {"KADEMELI", "STAGED"}, {"KALABILIR", "MAY APPEAR"}, {"KALAN", "LEFT"},
    {"KALIN", "THICK"}, {"KALKIS", "LAUNCH"}, {"KAM", "CAM"}, {"KANADI", "WING"}, {"KANAL", "SPLINE"}, {"KANAT", "WING"},
    {"KAPAGI", "HEAD"}, {"KAPAK", "HEAD"}, {"KAPALI", "OFF"}, {"KAPLAMA", "COVER"}, {"KAPLAMALI", "COATED"}, {"KAPORTA", "BODY"},
    {"KAPUT", "BONNET"}, {"KARBON", "CARBON"}, {"KARE", "FRAME"}, {"KAREDE", "FRAME"}, {"KARISIM", "BLEND"}, {"KARMA", "MIXED"},
    {"KARSI", "ONCOMING"}, {"KARTER", "SUMP"}, {"KASA", "CASE"}, {"KATALIZOR", "CATALYST"}, {"KATEGORILER", "CATEGORIES"}, {"KATKISI", "ADDITIVE"},
    {"KAVRAMA", "BITE"}, {"KAVRAR", "ENGAGES"}, {"KAYBETTIN", "YOU LOSE"}, {"KAYDEDILIR", "SAVED"}, {"KAYDIR", "SLIP"}, {"KAYIT", "SAVE"},
    {"KAYMA", "SLIP"}, {"KAYMALI", "SLIPPER"}, {"KAYNAK", "WELDED"}, {"KAZANDIN", "YOU WIN"}, {"KELEBEGI", "THROTTLE BODY"},
    {"KELEBEK", "THROTTLE"}, {"KELEPIR", "BARGAIN"}, {"KENARLARDA", "AT EDGES"}, {"KESKIN", "SHARP"}, {"KEZ", "TIMES"}, {"KILAVUZ", "GUIDES"},
    {"KILITLER", "LOCKS"}, {"KIRIK", "BROKEN"}, {"KIRILDI", "BROKE"}, {"KIRMIZI", "RED"}, {"KIS", "WINTER"}, {"KISA", "SHORT"},
    {"KISAYOL", "SHORTCUT"}, {"KIT", "KIT"}, {"KITI", "KIT"}, {"KOL", "LEVER"}, {"KOLTUK", "SEAT"}, {"KOMBO", "COMBO"}, {"KOMP", "COMP"},
    {"KOMPLE", "COMPLETE"}, {"KOMPRESOR", "SUPERCHARGER"}, {"KOMPRESORLU", "SUPERCHARGED"}, {"KONIK", "CONE"}, {"KONT", "CTRL"},
    {"KONTROL", "CONTROL"}, {"KONTROLLU", "CONTROLLED"}, {"KONTROLU", "CONTROL"}, {"KOSE", "CORNER"}, {"KRANK", "CRANK"}, {"KROM", "CHROME"},
    {"KRT", "SUMP"}, {"KUCUK", "SMALL"}, {"KURU", "DRY"}, {"KUSAGI", "GIRDLE"}, {"KUSAK", "GIRDLE"}, {"KUTUSU", "BOX"}, {"KUVVETI", "FORCE"},
    {"LASTIGI", "TYRE"}, {"LASTIK", "TYRES"}, {"LASTIKLER", "TYRES"}, {"MANUEL", "MANUAL"}, {"MEKANIK", "MECHANICAL"},
    {"METANOL", "METHANOL"}, {"MI", ""}, {"MIL", "MILE"}, {"MILI", "SHAFT"}, {"MODIFIYE", "TUNING"}, {"MONTAJ", "MOUNT"}, {"MOTOR", "ENGINE"},
    {"MOTORA", "ENGINE"}, {"MOTORU", "ENGINE"}, {"NITRO", "NITROUS"}, {"ODUL", "PRIZE"}, {"ODULLU", "PRIZE"}, {"OKSIJENLI", "OXYGENATED"},
    {"OKTAN", "OCTANE"}, {"OLCEKLEME", "SCALING"}, {"OLMAZ", "NONE"}, {"OLSUN", ""}, {"OLU", "DEAD"}, {"ON", "FRONT"}, {"ONCE", "FIRST"},
    {"ONERILEN", "SUGGESTED"}, {"ONIZLEME", "PREVIEW"}, {"ORAN", "RATIO"}, {"ORANI", "RATIO"}, {"ORANLARI", "RATIOS"}, {"ORGANIK", "ORGANIC"},
    {"ORTA", "MEDIUM"}, {"OTOBAN", "HIGHWAY"}, {"OTOMATIK", "AUTOMATIC"}, {"OTOMATIKTE", "AUTO"}, {"OTURMA", "SEAT"}, {"OYUN", "GAME"},
    {"OYUNCU", "PLAYER"}, {"OZEL", "CUSTOM"}, {"PAKETI", "PACKAGE"}, {"PANELLER", "PANELS"}, {"PARA", "MONEY"}, {"PARCA", "PARTS"},
    {"PARCALAR", "PARTS"}, {"PARCASI", "PART"}, {"PAS", "RUST"}, {"PATLADI", "BLOWN"}, {"PENCERE", "WINDOW"}, {"PIKSEL", "PIXEL"},
    {"PIKSELLI", "PIXELATED"}, {"PIL", "BATTERY"}, {"PIST", "TRACK"}, {"PISTONLU", "PISTON"}, {"PLAKA", "PLATE"}, {"PLANYA", "SKIM"},
    {"PNOMATIK", "PNEUMATIC"}, {"POLISAJ", "POLISH"}, {"POMPA", "PUMP"}, {"PORTLU", "PORTED"}, {"RADYATOR", "RADIATOR"},
    {"RADYATORU", "RADIATOR"}, {"RAKIP", "RIVAL"}, {"REKOR", "RECORD"}, {"REKORU", "RECORD"}, {"RESTORASYON", "RESTORATION"},
    {"REVIZYONU", "REBUILD"}, {"ROMORK", "TRAILER"}, {"SADECE", "ONLY"}, {"SAGLAM", "OK"}, {"SANTRIFUJ", "CENTRIFUGAL"}, {"SANZ", "GBOX"},
    {"SANZIMAN", "GEARBOX"}, {"SAPLAMA", "STUDS"}, {"SAPLAMASI", "STUDS"}, {"SARI", "AMBER"}, {"SARILAR", "AMBERS"}, {"SASI", "CHASSIS"},
    {"SAT", "SELL"}, {"SATAMAZSIN", "CANT SELL"}, {"SATILAMAZ", "CANT SELL"}, {"SATILDI", "SOLD"}, {"SATILSIN", "SELL"}, {"SATIN", ""},
    {"SATIR", "ROW"}, {"SATIS", "SALE"}, {"SAYI", "INTEGER"}, {"SEC", "PICK"}, {"SECENEK", "OPTIONS"}, {"SEN", "YOU"},
    {"SENKROMEC", "SYNCHRO"}, {"SERAMIK", "CERAMIC"}, {"SERBEST", "FREE"}, {"SERT", "HARD"}, {"SERTLESTIRILMIS", "HARDENED"}, {"SES", "SOUND"},
    {"SESI", "SOUND"}, {"SESLER", "SOUNDS"}, {"SETI", "SET"}, {"SIGDIR", "FIT"}, {"SIKISTIRMA", "COMPRESSION"}, {"SILINDIR", "CYLINDER"},
    {"SINIRI", "LIMIT"}, {"SINIRSIZ", "UNLIMITED"}, {"SIRA", "ROW"}, {"SIRALI", "SEQUENTIAL"}, {"SISTEM", "SYSTEM"}, {"SISTEMI", "SYSTEM"},
    {"SIYAH", "BLACK"}, {"SOGUK", "COLD"}, {"SOGUTMA", "COOLING"}, {"SOGUTUCU", "COOLER"}, {"SOKAK", "STREET"}, {"SOKAKTA", "ON STREET"},
    {"SON", "FINAL"}, {"SONRA", "AFTER"}, {"SONU", "END"}, {"SONUC", "RESULT"}, {"SPOR", "SPORT"}, {"SPREYI", "SPRAY"},
    {"STEPNE", "SPARE"}, {"STOK", "STOCK"}, {"SU", "WATER"}, {"SUPAP", "VALVE"}, {"SURE", "TIME"}, {"SURESI", "DURATION"},
    {"SURTUNME", "DRAG"}, {"SURULEMEZ", "NOT LEGAL"}, {"SURUS", "DRIVE"}, {"SUSP", "SUSP"}, {"SUSPANSIYON", "SUSPENSION"},
    {"SUSTURUCU", "MUFFLER"}, {"TAK", "FIT"}, {"TAKILDI", "FITTED"}, {"TAKILI", "FITTED"}, {"TAKILIRSA", "IF FITTED"}, {"TAM", "FULL"},
    {"TAMAM", "DONE"}, {"TAMIR", "REPAIR"}, {"TASLAK", "DRAFT"}, {"TEK", "SINGLE"}, {"TEKIL", "INDIVIDUAL"}, {"TEKRAR", "AGAIN"},
    {"TELEFONU", "PHONE"}, {"TEMAS", "CONTACT"}, {"TEPE", "PEAK"}, {"TESISATI", "WIRING"}, {"TITANYUM", "TITANIUM"}, {"TITRESIM", "VIBRATION"},
    {"TOPARLAMAK", "RESTORE"}, {"TOPLAM", "TOTAL"}, {"TOPRAK", "GRAVEL"}, {"TORK", "TORQUE"}, {"TRAFIK", "TRAFFIC"}, {"TUM", "ALL"},
    {"TURBIN", "TURBINE"}, {"TUT", "HOLD"}, {"TUTUCU", "RETAINERS"}, {"UC", "THREE"}, {"UCRETSIZ", "FREE"}, {"UGRAS", "WORK"},
    {"URET", "BUILD"}, {"URETILDI", "BUILT"}, {"URETIM", "FABRICATION"}, {"UYGULANDI", "APPLIED"}, {"UYGULANIR", "APPLIES"}, {"UZUN", "LONG"},
    {"VALFI", "VALVE"}, {"VAR", "YES"}, {"VARSA", "IF ANY"}, {"VARSAYILAN", "DEFAULT"}, {"VARSAYILANLAR", "DEFAULTS"}, {"VAZGEC", "CANCEL"},
    {"VE", "AND"}, {"VIRAJ", "CURVE"}, {"VIRAJA", "CURVE"}, {"VIRAJDA", "IN CURVES"}, {"VISKOZ", "VISCOUS"}, {"VITES", "GEAR"},
    {"VOLAN", "FLYWHEEL"}, {"VOLANI", "FLYWHEEL"}, {"YA", "OR"}, {"YAG", "OIL"}, {"YAGMUR", "RAIN"}, {"YAKIN", "NEAR"},
    {"YAKIT", "FUEL"}, {"YAKLASIYOR", "AHEAD"}, {"YALNIZ", "ONLY"}, {"YAN", "SIDE"}, {"YAPISKAN", "STICKY"}, {"YARI", "SEMI"},
    {"YARIS", "RACE"}, {"YARISAMAZ", "CANT RACE"}, {"YARISI", "RACE"}, {"YARISLAR", "RACES"}, {"YARISTAN", "RACE"}, {"YARIYA", "BY HALF"},
    {"YATAK", "BEARINGS"}, {"YATAY", "LANDSCAPE"}, {"YAY", "SPRING"}, {"YAYI", "SPRINGS"}, {"YAZILIM", "SOFTWARE"}, {"YENI", "NEW"},
    {"YENILEMESINE", "REFRESH"}, {"YESIL", "GREEN"}, {"YESILDE", "ON GREEN"}, {"YETMEZ", "TOO WEAK"}, {"YETMIYOR", "NOT ENOUGH"},
    {"YIRTILMA", "TEARING"}, {"YOK", "NONE"}, {"YOKSA", "ELSE"}, {"YOL", "ROAD"}, {"YOLA", "ROAD"}, {"YOLDA", "ON ROAD"}, {"YOLU", "ROAD"},
    {"YORGUN", "TIRED"}, {"YUK", "LOAD"}, {"YUKLENDI", "LOADED"}, {"YUKSEK", "HIGH"}, {"YUKSELTME", "UPGRADE"}, {"YUKU", "LOAD"},
    {"YUZDE", "PERCENT"}, {"YUZER", "FLOATING"}, {"ZATEN", "ALREADY"}, {"KALDI", "LEFT"}, {"GALIBIYET", "WINS"}, {"GAL", "W"},
    {"EGIK", "TILT"}, {"ARAYUZ", "INTERFACE"}, {"DILI", "LANGUAGE"}, {"DIL", "LANG"}, {"MENULER", "MENUS"}, {"EKRANLARI", "SCREENS"}, {"TUM", "ALL"}, {"SKOR", "SCORE"}, {"OLU", "DEAD"}, {"SU", "WATER"}, {"KM", "KM"}, {"SECILI", "SELECTED"}, {"TEPKI", "REACTION"}, {"RAKIBI", "RIVAL"},
};

#include "LangMore.inc"
#include "LangMore2.inc"

struct Table { std::unordered_map<std::string, std::string> phrases, words; };

Table build(const Entry* p, size_t np, const Entry* w, size_t nw) {
    Table t;
    for (size_t i = 0; i < np; ++i) t.phrases[p[i].tr] = p[i].tx;
    for (size_t i = 0; i < nw; ++i) t.words[w[i].tr] = w[i].tx;
    return t;
}

// Diger diller: kelime tablosu (kMulti) + eksik kelimede Ingilizce karsilik; cumle tablosu yok (kelime duzeyi)
Table buildMulti(int col, const Table& en) {
    Table t;
    t.words = en.words;                                            // varsayilan: Ingilizce
    for (const Multi& m : kMulti) {
        const char* w[5] = {m.de, m.es, m.fr, m.it, m.pt};
        t.words[m.tr] = w[col];
    }
    return t;
}

Table buildMulti2(int col, const Table& en) {
    Table t;
    t.words = en.words;
    for (const Multi2& m : kMulti2) {
        const char* w[6] = {m.ru, m.uk, m.el, m.pl, m.nl, m.id};
        t.words[m.tr] = w[col];
    }
    return t;
}

const Table* tableFor(Lang l) {
    static const Table en = build(kEnPhrases, sizeof kEnPhrases / sizeof *kEnPhrases, kEnWords, sizeof kEnWords / sizeof *kEnWords);
    static const Table more[5] = {buildMulti(0, en), buildMulti(1, en), buildMulti(2, en), buildMulti(3, en), buildMulti(4, en)};
    switch (l) {
    case Lang::EN: return &en;
    case Lang::DE: return &more[0];
    case Lang::ES: return &more[1];
    case Lang::FR: return &more[2];
    case Lang::IT: return &more[3];
    case Lang::PT: return &more[4];
    case Lang::RU: case Lang::UK: case Lang::EL: case Lang::PL: case Lang::NL: case Lang::ID: {
        static const Table extra[6] = {buildMulti2(0, en), buildMulti2(1, en), buildMulti2(2, en), buildMulti2(3, en), buildMulti2(4, en), buildMulti2(5, en)};
        return &extra[(int)l - (int)Lang::RU];
    }
    default: return nullptr;
    }
}

std::string translateWords(const Table& t, const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    size_t i = 0;
    while (i < s.size()) {
        if (std::isalpha((unsigned char)s[i])) {
            size_t j = i;
            while (j < s.size() && std::isalpha((unsigned char)s[j])) ++j;
            const std::string w = s.substr(i, j - i);
            auto it = t.words.find(w);
            if (it == t.words.end()) out += w;
            else if (!it->second[0]) { if (j < s.size() && s[j] == ' ' && (out.empty() || out.back() == ' ')) ++j; }   // atlanan kelime
            else out += it->second;
            i = j;
        } else out += s[i++];
    }
    return out;
}

} // namespace

const char* langName(Lang l) {
    static const char* n[(int)Lang::Count] = {"TURKCE", "ENGLISH", "DEUTSCH", "ESPANOL", "FRANCAIS", "ITALIANO", "PORTUGUES",
                                              "РУССКИЙ", "УКРАЇНСЬКА", "ΕΛΛΗΝΙΚΑ", "POLSKI", "NEDERLANDS", "INDONESIA"};
    return n[(int)l];
}

const std::string& translate(Lang l, const std::string& s) {
    static std::unordered_map<std::string, std::string> cache[(int)Lang::Count];
    if (l == Lang::TR) return s;
    const Table* t = tableFor(l);
    if (!t) return s;
    auto& c = cache[(int)l];
    auto it = c.find(s);
    if (it != c.end()) return it->second;
    if (c.size() > 6000) c.clear();                              // dinamik metinler (sayilar) onbellegi sisirmesin
    auto p = t->phrases.find(s);
    std::string r = p != t->phrases.end() ? p->second : translateWords(*t, s);
    return c.emplace(s, std::move(r)).first->second;
}

} // namespace zk
