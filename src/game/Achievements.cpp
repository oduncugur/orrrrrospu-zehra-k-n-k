// ZEHRA KINIK - Basarimlar
#include "game/Achievements.h"
#include "game/Career.h"
#include "game/League.h"

#include <algorithm>

namespace zk {

const std::vector<AchDef>& achievements() {
    static const std::vector<AchDef> a = {
        {"ILK KAN", "ILK YARISINI KAZAN", 300},
        {"SOKAK KURDU", "10 YARIS KAZAN", 1500},
        {"DURDURULAMAZ", "50 YARIS KAZAN", 6000},
        {"TAMIRCI CIRAGI", "ILK PARCANI TAK", 300},
        {"GARAJ SAHIBI", "3 ARACIN OLSUN", 1000},
        {"KOLEKSIYONCU", "8 ARACIN OLSUN", 5000},
        {"13'UN ALTI", "1/4 MIL 13 SANIYENIN ALTINDA", 1000},
        {"11'IN ALTI", "1/4 MIL 11 SANIYENIN ALTINDA", 3000},
        {"9'UN ALTI", "1/4 MIL 9 SANIYENIN ALTINDA", 8000},
        {"MAHALLEDEN CIKIS", "SEHIR LIGINI AC", 1500},
        {"SEHRIN HAKIMI", "SEHIRLERARASI LIGI AC", 3000},
        {"YOLLARIN KRALI", "DAG YOLLARI LIGINI AC", 5000},
        {"PISTE DAVET", "PIST LIGINI AC", 8000},
        {"SOKAK EFSANESI", "1000 UN TOPLA", 10000},
        {"CEBI DOLU", "50.000 $ BIRIKTIR", 2000},
        {"CEYREK MILYON", "TOPLAM 250.000 $ KAZAN", 10000},
        {"HURDADAN ZIRVEYE", "HURDALIK ARACIYLA YARIS KAZAN", 2500},
        {"AKIS USTASI", "OTOBANDA 20.000 SKOR YAP", 2000},
        {"PINK SLIP", "RAKIBIN ARABASINI KAZAN", 3000},
        {"BOYACI", "ARACINI BOYAT", 300},
        {"GUNUN ADAMI", "UC GUNLUK GOREVI BITIR", 800},
        {"KACAK", "POLISTEN KAC", 1500},
        // Koleksiyonlar: setin uc araci ayni anda garajda
        {"YERLI GURURU", "KOLEKSIYON: SAHIN + DOGAN + MURAT 124", 3000},
        {"HOT HATCH", "KOLEKSIYON: GOLF GTI + 205 GTI + CIVIC TYPE R", 6000},
        {"ALMAN OKULU", "KOLEKSIYON: M3 E30 + M3 E46 + 964 RS", 12000},
        {"JDM UCLUSU", "KOLEKSIYON: SUPRA + SKYLINE R34 + RX-7", 15000},
        {"KAS VE CELIK", "KOLEKSIYON: MUSTANG + VIPER + F40", 30000},
        {"SUPER YILDIZLAR", "KOLEKSIYON: CARRERA GT + 720S + F1", 60000},
    };
    return a;
}

bool achievementMet(const Career& c, int i) {
    double best = 0;
    for (const OwnedCar& o : c.cars) if (o.bestEt > 0 && (best <= 0 || o.bestEt < best)) best = o.bestEt;
    auto any = [&](auto f) { return std::any_of(c.cars.begin(), c.cars.end(), f); };
    switch (i) {
    case 0: return c.wins >= 1;
    case 1: return c.wins >= 10;
    case 2: return c.wins >= 50;
    case 3: return any([](const OwnedCar& o) { return o.paidParts > 0; });
    case 4: return c.cars.size() >= 3;
    case 5: return c.cars.size() >= 8;
    case 6: return best > 0 && best < 13.0;
    case 7: return best > 0 && best < 11.0;
    case 8: return best > 0 && best < 9.0;
    case 9: case 10: case 11: case 12: return c.leagueUnlocked() >= i - 8;
    case 13: return c.rep >= 1000;
    case 14: return c.money >= 50000;
    case 15: return c.earnings >= 250000;
    case 16: return any([](const OwnedCar& o) { return o.fromJunk && o.wins > 0; });
    case 17: return c.bestFlow >= 20000;
    case 18: {
        const auto& ev = leagueEvents();
        for (int k = 0; k < (int)ev.size(); ++k) if (ev[k].pink && c.eventWon(k)) return true;
        return false;
    }
    case 19: return any([](const OwnedCar& o) { return o.paint >= 0 || o.stripe > 0; });
    case 20: return c.dailyDone == 7;
    case 21: return c.chaseEscapes >= 1;
    case 22: case 23: case 24: case 25: case 26: case 27: {
        static const int sets[6][3] = {{217, 218, 216}, {152, 291, 5}, {118, 122, 167}, {34, 52, 78}, {227, 272, 184}, {177, 315, 313}};
        for (int id : sets[i - 22])
            if (!any([&](const OwnedCar& o) { return o.carId == id; })) return false;
        return true;
    }
    default: return false;
    }
}

} // namespace zk
