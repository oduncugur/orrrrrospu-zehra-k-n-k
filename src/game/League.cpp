#include "League.h"

#include <algorithm>
#include <string>

#include <algorithm>
#include <cstdio>
#include <ctime>

namespace zk {

const char* leagueName(int l) {
    static const char* n[kLeagues] = {"MAHALLE", "SEHIR", "SEHIRLERARASI", "DAG YOLLARI", "PIST"};
    return n[std::clamp(l, 0, kLeagues - 1)];
}
double leagueIndexCap(int l) {
    static const double c[kLeagues] = {125, 220, 340, 480, 0};
    return c[std::clamp(l, 0, kLeagues - 1)];
}
const char* eventModeName(EventMode m) {
    switch (m) {
    case EventMode::Drag: return "DRAG 1/4 MIL";
    case EventMode::Road: return "YOL YARISI";
    case EventMode::Touge: return "DAG YOLU";
    case EventMode::Karma: return "KARMA";
    case EventMode::Flow: return "OTOBAN AKISI";
    case EventMode::Chase: return "POLIS KACIS";
    case EventMode::Marathon: return "MARATON";
    }
    return "";
}

const char* bossLine(int rival, int kind) {
    if (rival < 0 || rival >= (int)rivals().size() || !rivals()[rival].boss) return "";
    static const struct { const char* name; const char* l[3]; } kLines[] = {
        {"MAHALLENIN KRALI SEDAT", {"BU SOKAKLAR BENIM, CAYLAK.", "SANS ESERI... BIR DAHAKINE BAKARIZ.", "MAHALLEYE HOS GELDIN. EVINE DON."}},
        {"GECE KUSU ASLI", {"GECE BENIM SAATIM. YETISEBILIRSEN.", "ISIKLARI SEN SONDURDUN. SAYGI.", "STOPLARIMI BILE GOREMEDIN."}},
        {"BASKAN", {"BU YOLLARIN KURALINI BEN KOYARIM.", "DEMEK KURALLAR DEGISTI.", "OYLAMA BITTI. KAYBETTIN."}},
        {"DAGIN HAYALETI", {"VIRAJLARI KIMSE BENIM GIBI BILMEZ.", "SISIN ICINDEN CIKTIN... KIMSIN SEN?", "HAYALETI YAKALAYAMAZSIN."}},
        {"EFSANE", {"BUTUN O YOLLAR BURAYA CIKIYORDU.", "ARTIK EFSANE SENSIN.", "EFSANELER KOLAY OLMEZ."}},
    };
    for (const auto& k : kLines)
        if (std::string(k.name) == rivals()[rival].name) return k.l[std::clamp(kind, 0, 2)];
    return "";
}

const std::vector<RivalDef>& rivals() {
    static const std::vector<RivalDef> r = {
        // 0 MAHALLE
        {"BAKKAL CEMAL", 0, 302, 0, 1.8, false}, {"TAKSICI RIZA", 0, 282, 1, 1.8, false},
        {"KOMSU OGLU EMRE", 0, 216, 1, 1.7, false}, {"DOLMUSCU HAKAN", 0, 290, 2, 1.7, false},
        {"OKUL CIKISI BURAK", 0, 27, 0, 1.6, false}, {"MAHALLENIN KRALI SEDAT", 0, 218, 2, 1.45, true},
        // 6 SEHIR
        {"TIP-R MERT", 1, 5, 0, 1.6, false}, {"GTI ALI", 1, 152, 1, 1.6, false}, {"BMWCI SELIN", 1, 118, 0, 1.55, false},
        {"DRIFTCI TOFU", 1, 24, 1, 1.5, false}, {"KUCUK TERMINATOR", 1, 291, 1, 1.5, false}, {"INTEGRA EDA", 1, 12, 1, 1.45, false},
        {"GECE KUSU ASLI", 1, 100, 0, 1.3, true},
        // 13 SEHIRLERARASI
        {"SUPRA HAKAN", 2, 34, 1, 1.5, false}, {"GODZILLA KAAN", 2, 50, 1, 1.5, false}, {"DONER MOTOR DENIZ", 2, 78, 1, 1.45, false},
        {"RALLICI ONUR", 2, 88, 1, 1.45, false}, {"MUSTANG CAN", 2, 227, 1, 1.4, false}, {"SILVIA BERK", 2, 59, 1, 1.4, false},
        {"BASKAN", 2, 52, 2, 1.25, true},
        // 20 DAG
        {"VIRAJ USTASI ECE", 3, 17, 2, 1.4, false}, {"NSX TOLGA", 3, 19, 2, 1.4, false}, {"EXIGE EFE", 3, 308, 2, 1.35, false},
        {"ELFER ORHAN", 3, 167, 2, 1.35, false}, {"M3 KEREM", 3, 122, 2, 1.3, false}, {"MX-5 IPEK", 3, 75, 2, 1.3, false},
        {"DAGIN HAYALETI", 3, 171, 2, 1.2, true},
        // 27 PIST
        {"F40 SAMI", 4, 184, 1, 1.3, false}, {"VIPER RECEP", 4, 272, 1, 1.3, false}, {"LFA YUSUF", 4, 46, 2, 1.25, false},
        {"CARRERA GT BURCU", 4, 177, 1, 1.25, false}, {"720S KORAY", 4, 315, 1, 1.2, false}, {"EFSANE", 4, 313, 1, 1.1, true},
    };
    return r;
}

const std::vector<EventDef>& leagueEvents() {
    using M = EventMode;
    static const std::vector<EventDef> e = {
        // MAHALLE (lig 0): rakipler 0-5
        {"SANAYI SITESI DRAG", 0, M::Drag, 0, 400, 10, false, 0}, {"TAKSI DURAGI DRAG", 0, M::Drag, 1, 450, 10, false, 0},
        {"CEVRE YOLU KAPISMASI", 0, M::Road, 2, 550, 12, false, 0}, {"DOLMUS HATTI", 0, M::Drag, 3, 500, 12, false, 0},
        {"OKUL YOLU KARMA", 0, M::Karma, 4, 650, 14, false, 0}, {"GECE AKISI", 0, M::Flow, -1, 500, 10, false, 15000},
        {"ACIK DAVET DRAG", 0, M::Drag, -1, 450, 8, false, 0}, {"PATRON: MAHALLENIN KRALI", 0, M::Drag, 5, 2000, 40, false, 0},
        // SEHIR (lig 1): rakipler 6-12
        {"LIMAN DRAG", 1, M::Drag, 6, 1300, 18, false, 0}, {"SAHIL YOLU", 1, M::Road, 7, 1500, 20, false, 0},
        {"OTOPARK KAPISMASI", 1, M::Drag, 8, 1400, 18, false, 0}, {"MAKAS GECESI", 1, M::Karma, 9, 1800, 22, false, 0},
        {"KOPRU AKISI", 1, M::Flow, -1, 1500, 16, false, 30000}, {"KUCUK DEV", 1, M::Road, 10, 1600, 20, false, 0},
        {"PINK SLIP: INTEGRA", 1, M::Drag, 11, 0, 30, true, 0}, {"PATRON: GECE KUSU", 1, M::Karma, 12, 6000, 70, false, 0},
        // SEHIRLERARASI (lig 2): rakipler 13-19
        {"OTOBAN CIKISI", 2, M::Drag, 13, 3200, 30, false, 0}, {"GODZILLA GECESI", 2, M::Road, 14, 3800, 34, false, 0},
        {"ROTARI DUELLO", 2, M::Karma, 15, 4000, 34, false, 0}, {"RALLI USTASI", 2, M::Road, 16, 3600, 32, false, 0},
        {"KAS GUCU", 2, M::Drag, 17, 3400, 30, false, 0}, {"TIR PARKI AKISI", 2, M::Flow, -1, 3000, 26, false, 45000},
        {"PINK SLIP: SILVIA", 2, M::Karma, 18, 0, 45, true, 0}, {"PATRON: BASKAN", 2, M::Karma, 19, 15000, 110, false, 0},
        // DAG (lig 3): rakipler 20-26
        {"VIRAJ SINAVI", 3, M::Touge, 20, 6000, 42, false, 0}, {"MIDSHIP DUELLO", 3, M::Touge, 21, 6500, 44, false, 0},
        {"HAFIF SIKLET", 3, M::Touge, 22, 6800, 44, false, 0}, {"ELFER TIRMANISI", 3, M::Karma, 23, 7000, 46, false, 0},
        {"M3 GECESI", 3, M::Road, 24, 6200, 42, false, 0}, {"PINK SLIP: MX-5", 3, M::Touge, 25, 0, 55, true, 0},
        {"PATRON: DAGIN HAYALETI", 3, M::Touge, 26, 30000, 160, false, 0},
        // PIST (lig 4): rakipler 27-32
        {"PIST GUNU F40", 4, M::Drag, 27, 12000, 60, false, 0}, {"YILAN", 4, M::Road, 28, 14000, 64, false, 0},
        {"V10 SENFONI", 4, M::Karma, 29, 16000, 66, false, 0}, {"KARBON CAG", 4, M::Road, 30, 15000, 64, false, 0},
        {"YENI NESIL", 4, M::Drag, 31, 18000, 70, false, 0}, {"GECE AKISI REKORU", 4, M::Flow, -1, 12000, 50, false, 90000},
        {"PATRON: EFSANE", 4, M::Karma, 32, 60000, 300, false, 0},
        // Polis kacislari (sona eklenir: kayittaki etkinlik bitleri sira numarasiyla tutulur)
        {"MAHALLE KACISI", 0, M::Chase, -1, 450, 10, false, 0}, {"POLIS CEVIRMESI", 1, M::Chase, -1, 1300, 18, false, 0},
        {"OTOBAN KACISI", 2, M::Chase, -1, 3200, 30, false, 0}, {"DAG KACISI", 3, M::Chase, -1, 6000, 42, false, 0},
        {"SON KACIS", 4, M::Chase, -1, 12000, 58, false, 0},
        // Maratonlar: 18 km, yakit + benzinlik (sona eklenir)
        {"SEHIRLERARASI MARATON", 2, M::Marathon, -1, 6500, 40, false, 0}, {"PIST MARATONU", 4, M::Marathon, -1, 20000, 80, false, 0},
    };
    return e;
}

int todayIndex() { return (int)(std::time(nullptr) / 86400); }

std::vector<DailyTask> dailyTasks(int day, double playerEt) {
    uint32_t x = (uint32_t)day * 2654435761u + 977u;
    auto rnd = [&]() { x ^= x << 13; x ^= x >> 17; x ^= x << 5; return x; };
    std::vector<DailyTask> pool = {
        {TaskType::WinDrag, 2, 600, 8}, {TaskType::WinRoad, 1, 700, 8}, {TaskType::WinAny, 3, 900, 10},
        {TaskType::EtUnder, (long)std::max(800.0, (playerEt - 0.3) * 100.0), 800, 10}, {TaskType::FlowScore, 12000, 700, 8},
        {TaskType::Earn, 3000, 500, 6}, {TaskType::Restore, 4, 500, 6}, {TaskType::BuyParts, 2, 400, 5},
    };
    std::vector<DailyTask> out;
    while (out.size() < 3) {
        const DailyTask& t = pool[rnd() % pool.size()];
        bool dup = false;
        for (const DailyTask& o : out) dup |= o.type == t.type;
        if (!dup) out.push_back(t);
    }
    return out;
}

std::string taskText(const DailyTask& t) {
    char b[64];
    switch (t.type) {
    case TaskType::WinDrag: std::snprintf(b, sizeof b, "%ld DRAG YARISI KAZAN", t.target); break;
    case TaskType::WinRoad: std::snprintf(b, sizeof b, "%ld YOL YARISI KAZAN", t.target); break;
    case TaskType::WinAny: std::snprintf(b, sizeof b, "%ld YARIS KAZAN", t.target); break;
    case TaskType::EtUnder: std::snprintf(b, sizeof b, "1/4 MILI %.2f S ALTINDA BITIR", t.target / 100.0); break;
    case TaskType::FlowScore: std::snprintf(b, sizeof b, "OTOBAN AKISINDA %ld PUAN", t.target); break;
    case TaskType::Earn: std::snprintf(b, sizeof b, "$%ld KAZAN", t.target); break;
    case TaskType::Restore: std::snprintf(b, sizeof b, "%ld RESTORASYON ADIMI", t.target); break;
    case TaskType::BuyParts: std::snprintf(b, sizeof b, "%ld PARCA TAK", t.target); break;
    }
    return b;
}

} // namespace zk
