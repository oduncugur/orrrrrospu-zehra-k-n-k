// Otoban akisi (Faz 5 olcut 6): skor kurallari (yakin gecis, kombo, hiz, apex, carpisma) + oturum + kariyer odulu
#include "game/Career.h"
#include "game/FlowScore.h"
#include "game/RoadSession.h"

#include <cmath>
#include <cstdio>

using namespace zk;
static int failures = 0;
#define CHECK(cond, msg) do { if (cond) std::printf("  GECTI  %s\n", msg); else { std::printf("  KALDI  %s\n", msg); ++failures; } } while (0)

// Oyuncu v hizla ilerler, tek bir trafik araci (sabit s0, serit lat0) yanindan gecilir. Donus: kazanilan puan
static long passOne(FlowScorer& f, double v, double lat, double carLat, bool oncoming, int uid, double& s) {
    const long before = f.score();
    const double s0 = s + 30.0, dt = 1.0 / 60.0;
    for (int i = 0; i < 60 * 3 && s < s0 + 25.0; ++i) {
        s += v * dt;
        f.update(dt, s, lat, v, false, {{uid, s0, carLat, oncoming}});
    }
    f.update(1.0 / 60.0, s, lat, v, false, {});                   // arac pencereden cikti
    return f.score() - before;
}

int main() {
    std::printf("[1] Viraj tepesi (apex) bulma\n");
    {
        // 0-100 duz, 100-140 klotoid, 140-240 yay R=200 (sol), 240-280 cikis, duz, 400-500 yay R=300 (sag)
        std::vector<double> s, k;
        for (double x = 0; x <= 600; x += 2) {
            double kk = 0;
            if (x >= 100 && x < 140) kk = (x - 100) / 40 / 200;
            else if (x >= 140 && x < 240) kk = 1.0 / 200;
            else if (x >= 240 && x < 280) kk = (280 - x) / 40 / 200;
            else if (x >= 400 && x < 500) kk = -1.0 / 300;
            s.push_back(x); k.push_back(kk);
        }
        const auto a = findApexes(s, k, 600.0);
        std::printf("    %zu tepe: ", a.size());
        for (auto& p : a) std::printf("s=%.0f R=%.0f  ", p.first, 1.0 / p.second);
        std::printf("\n");
        CHECK(a.size() == 2, "iki viraj, iki tepe");
        CHECK(a.size() == 2 && std::fabs(a[0].first - 189) < 3 && a[0].second > 0, "sol viraj tepesi yay ortasinda");
        CHECK(a.size() == 2 && std::fabs(a[1].first - 449) < 3 && a[1].second < 0, "sag viraj tepesi, isaret dogru");
    }

    std::printf("[2] Yakin gecis\n");
    {
        FlowScorer f({}, 0.9);
        double s = 0;
        // Sag seritteki (lat -1.8) araci orta cizgiye yakin (lat +0.9) gecmek: bosluk 2.7 - 1.8 = 0.9 m
        const long p1 = passOne(f, 30.0, 0.9, -1.8, false, 1, s);
        std::printf("    108 km/h, 0.9 m bosluk: +%ld (yakin %d)\n", p1, f.stats().nearMisses);
        CHECK(f.stats().nearMisses == 1 && p1 >= 100, "yakin gecis sayildi");
        // Karsi seritten rahat gecis (lat +1.8): bosluk 1.8 m -> sayilmaz
        FlowScorer g({}, 0.9); double s2 = 0;
        passOne(g, 30.0, 1.8, -1.8, false, 1, s2);
        CHECK(g.stats().nearMisses == 0, "genis bosluk yakin gecis degil");
        // Yavas (40 km/h) yakin gecis sayilmaz
        FlowScorer h({}, 0.9); double s3 = 0;
        passOne(h, 11.0, 0.9, -1.8, false, 1, s3);
        CHECK(h.stats().nearMisses == 0, "60 km/h alti sayilmaz");
    }

    std::printf("[3] Karsi seritte yakin gecis + kombo\n");
    {
        FlowScorer f({}, 0.9);
        double s = 0;
        const long a = passOne(f, 19.0, -0.9, +1.8, true, 1, s);        // gelen arac, bosluk 0.9 m (68 km/h: surekli puan yok)
        const long b = passOne(f, 19.0, -0.9, +1.8, true, 2, s);        // hemen ardindan (< 4 s)
        std::printf("    1. +%ld, 2. +%ld (kombo X%d)\n", a, b, f.combo());
        CHECK(f.stats().oncomingMisses == 2, "iki karsi yakin gecis");
        CHECK(b >= 2 * a - 2 && f.combo() == 2, "ikinci gecis kombo X2");
        for (int i = 0; i < 60 * 5; ++i) f.update(1.0 / 60.0, s += 0.5, -1.8, 20.0, false, {});
        CHECK(f.combo() == 1, "4 s gecis yoksa kombo biter");
        FlowScorer g({}, 0.9); double s2 = 0;
        const long c = passOne(g, 19.0, 0.9, -1.8, false, 1, s2);
        CHECK(a >= 2 * c - 2, "karsi seritteki gecis 2 kat");
    }

    std::printf("[4] Hiz ve karsi serit puani, yol disi\n");
    {
        FlowScorer f({}, 0.9);
        for (int i = 0; i < 600; ++i) f.update(1.0 / 60.0, i * 0.8, -1.8, 50.0, false, {});   // 180 km/h, 10 s
        std::printf("    180 km/h 10 s: %ld puan\n", f.score());
        CHECK(std::labs(f.score() - 1800) <= 2, "180 km/h = 180 puan/s");
        FlowScorer g({}, 0.9);
        for (int i = 0; i < 600; ++i) g.update(1.0 / 60.0, i * 0.8, -1.8, 50.0, true, {});
        CHECK(g.score() == 0, "yol disinda puan yok");
        FlowScorer o({}, 0.9);
        for (int i = 0; i < 600; ++i) o.update(1.0 / 60.0, i * 0.8, +1.8, 50.0, false, {});
        CHECK(o.score() > 3000 && o.stats().oncomingTime > 9.9, "karsi seritte ek puan");
    }

    std::printf("[5] Apex\n");
    {
        FlowScorer f({{100.0, 1.0 / 150.0}, {300.0, -1.0 / 150.0}}, 0.9);
        double s = 0;
        for (int i = 0; i < 900; ++i) f.update(1.0 / 60.0, s += 30.0 / 60, -1.8, 30.0, false, {});   // 30 m/s, R150: 0.61 g
        std::printf("    30 m/s R=150: %d apex, %ld puan\n", f.stats().apexes, f.score());
        CHECK(f.stats().apexes == 2, "iki viraj tepesi yuksek yanal ivmeyle gecildi");
        FlowScorer g({{100.0, 1.0 / 150.0}}, 0.9); double s2 = 0;
        for (int i = 0; i < 600; ++i) g.update(1.0 / 60.0, s2 += 15.0 / 60, -1.8, 15.0, false, {});  // 0.15 g
        CHECK(g.stats().apexes == 0, "yavas gecis apex degil");
    }

    std::printf("[6] Carpisma cezasi\n");
    {
        FlowScorer f({}, 0.9);
        for (int i = 0; i < 600; ++i) f.update(1.0 / 60.0, i * 0.8, -1.8, 50.0, false, {});
        double s = 480;
        passOne(f, 30.0, 0.9, -1.8, false, 7, s);
        const long before = f.score();
        f.crash();
        CHECK(f.score() == before - 1000 && f.combo() == 1 && f.stats().crashes == 1, "-1000 ve kombo sifir");
        FlowScorer g({}, 0.9); g.crash();
        CHECK(g.score() == 0, "skor eksiye dusmez");
    }

    std::printf("[7] Oturum: 2 dk akis (#5, YZ surucu), belirlenimci\n");
    long scores[2] = {0, 0};
    for (int run = 0; run < 2; ++run) {
        Tune t;
        RoadSession rs(RoadSession::Mode::Flow, 5, &t, 0, nullptr, 3u);
        double t0 = 0; bool nan = false, moved = false;
        double lane = -RoadSession::kLane;
        while (rs.phase() != RoadSession::Phase::Finished && t0 < 200.0) {
            // Test surucusu: onde yavas arac varsa ve karsidan gelen yoksa karsi seritten sollar
            const double ps = rs.player().s(), v = rs.player().sim().speed();
            double cap = 1e9; bool blocked = false, oncoming = false, rightClear = true;
            for (const TrafficCar& tc : rs.traffic()) {
                const double ds = tc.s - ps;
                if (!tc.oncoming && ds > 0 && ds < 25.0 + 1.2 * v) blocked = true;
                if (!tc.oncoming && std::fabs(ds) < 15.0) rightClear = false;
                if (tc.oncoming && ds > -10.0 && ds < 80.0 + 3.0 * v) oncoming = true;
            }
            if (lane < 0 && blocked && !oncoming) lane = +RoadSession::kLane;
            else if (lane > 0 && (rightClear || oncoming)) lane = -RoadSession::kLane;
            for (const TrafficCar& tc : rs.traffic()) {
                const double ds = tc.s - ps;
                if (lane < 0 && !tc.oncoming && ds > 0 && ds < 40.0) cap = std::min(cap, tc.v);
                if (lane > 0 && tc.oncoming && ds > 0 && ds < 60.0) cap = std::min(cap, 5.0);   // kafa kafaya: yavasla
            }
            rs.update(1.0 / 60.0, rs.player().aiControls(lane, 0.6, cap));
            if (rs.phase() == RoadSession::Phase::Countdown && rs.player().sim().speed() > 0.5) moved = true;
            if (!std::isfinite(rs.player().sim().speed())) nan = true;
            t0 += 1.0 / 60.0;
        }
        const FlowScorer::Stats& st = rs.flow()->stats();
        scores[run] = st.score;
        if (run == 0) {
            std::printf("    %.1f s: skor %ld, yakin %d, karsi %d, apex %d, carpisma %d, tepe %.0f km/h, %.2f km\n", t0, st.score,
                        st.nearMisses, st.oncomingMisses, st.apexes, st.crashes, st.topSpeed * 3.6, rs.player().s() / 1000.0);
            CHECK(!moved, "geri sayimda arac kipirdamadi");
            CHECK(rs.phase() == RoadSession::Phase::Finished && std::fabs(t0 - 123.0) < 0.5, "3 s geri sayim + 120 s sonra bitti");
            CHECK(!nan && rs.flowTimeLeft() == 0.0, "NaN yok, sure sifir");
            CHECK(st.score > 0 && st.topSpeed > 25.0, "skor ve hiz puani birikti");
            int near = 0;
            for (const TrafficCar& tc : rs.traffic()) if (std::fabs(tc.s - rs.player().s()) < 400.0) ++near;
            CHECK(near >= 6, "akis modunda oyuncunun yakininda yogun trafik");
        }
    }
    CHECK(scores[0] == scores[1], "ayni tohum + girdi = ayni skor");

    std::printf("[8] Kariyer odulu ve rekor kaydi\n");
    {
        Career c = Career::newGame();
        const long m0 = c.money;
        bool rec = false;
        const long p1 = c.recordFlow(12000, &rec);
        CHECK(rec && p1 == 900 && c.bestFlow == 12000 && c.money == m0 + 900, "12000 skor: 600 + %50 rekor = $900");
        const long p2 = c.recordFlow(8000, &rec);
        CHECK(!rec && p2 == 400 && c.bestFlow == 12000, "rekor degil: $400, rekor korunur");
        CHECK(c.recordFlow(10'000'000) <= Career::kFlowPrizeCap * 3 / 2, "odul tavanli");
        Career d;
        CHECK(Career::parse(c.serialize(), d) && d.bestFlow == c.bestFlow, "rekor kayitta saklanir");
    }

    std::printf(failures ? "\nSONUC: %d test KALDI\n" : "\nSONUC: tum testler gecti\n", failures);
    return failures ? 1 : 0;
}
