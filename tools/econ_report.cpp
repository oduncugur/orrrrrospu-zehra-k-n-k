// Ekonomi raporu: lig etkinlikleri, rakip arac / paket / ET / endeks, odul ve arac fiyatlari (denge analizi)
#include "game/Career.h"
#include "game/League.h"
#include "garage/VehicleCatalog.h"
#include <cstdio>

using namespace zk;

int main() {
    const Career c = Career::newGame();
    const VehicleDef* me = findVehicle(c.car().carId);
    std::printf("BASLANGIC: %s  para %ld  ET %.2f  endeks %.0f  fiyat %d\n", me->fullName().c_str(), c.money,
                tableEt(me->id, 0), performanceIndex(*me, Tune{}), carPrice(*me));
    for (int p = 1; p <= 2; ++p) std::printf("  sahin paket %d: ET %.2f endeks %.0f\n", p, tableEt(me->id, p), performanceIndex(*me, opponentPreset(p)));
    for (const EventDef& e : leagueEvents()) {
        if (e.rival < 0) { std::printf("L%d %-26s %-9s esdeger rakip      odul %6d\n", e.league, e.name, eventModeName(e.mode), e.prize); continue; }
        const RivalDef& r = rivals()[e.rival];
        const VehicleDef* v = findVehicle(r.carId);
        std::printf("L%d r%-2d id%-3d %-24.24s p%d h%.2f [%3.0f %3.0f %3.0f] odul %6d\n", e.league, e.rival, v->id, v->fullName().c_str(), r.preset,
                    r.handicap, performanceIndex(*v, opponentPreset(0)), performanceIndex(*v, opponentPreset(1)),
                    performanceIndex(*v, opponentPreset(2)), e.prize);
    }
}
