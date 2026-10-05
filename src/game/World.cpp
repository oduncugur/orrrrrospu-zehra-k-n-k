#include "World.h"
#include "game/Career.h"
#include "game/League.h"

#include <algorithm>
#include <cmath>
#include <queue>

namespace zk {

namespace {
constexpr double kPiW = 3.14159265358979323846;
double hashW(int i) { unsigned x = (unsigned)i * 2654435761u; x ^= x >> 13; x *= 0x5bd1e995u; x ^= x >> 15; return (x & 0xFFFF) / 65535.0; }
} // namespace

const World& World::get() { static const World w; return w; }
World::World() { build(); }

int World::addNode(double x, double y) { nodes.push_back({x, y, {}}); return (int)nodes.size() - 1; }

int World::addEdge(int a, int b, const std::vector<std::pair<double, double>>& pts, bool highway, int city) {
    const double hw = highway ? 8.0 : 4.0;
    const int lanes = highway ? 2 : 1;
    std::vector<std::pair<double, double>> rp(pts.rbegin(), pts.rend());
    WorldEdge e{RoadPath::polyline(pts, hw, lanes, lanes, highway ? 450.0 : 8.0), RoadPath::polyline(rp, hw, lanes, lanes, highway ? 450.0 : 8.0),
                a, b, highway, city, 1e18, 1e18, -1e18, -1e18};
    for (const RoadPoint& p : e.fwd.points()) {
        e.minX = std::min(e.minX, p.x - hw); e.minY = std::min(e.minY, p.y - hw);
        e.maxX = std::max(e.maxX, p.x + hw); e.maxY = std::max(e.maxY, p.y + hw);
    }
    edges.push_back(std::move(e));
    const int id = (int)edges.size() - 1;
    nodes[a].edges.push_back(id); nodes[b].edges.push_back(id);
    return id;
}

void World::build() {
    const int nC = Career::kCities;
    // Sehir merkezleri: gercek mesafe x 25 (110 km -> 2.75 km), kuzey-guney dalga
    std::vector<double> cx(nC), cy(nC);
    for (int c = 0; c < nC; ++c) {
        cx[c] = c == 0 ? 0.0 : cx[c - 1] + Career::legKm(c - 1) * 25.0;
        cy[c] = 1600.0 * std::sin(c * 1.3) + 500.0 * std::sin(c * 2.9);
    }
    std::vector<int> west(nC), east(nC);                                   // otobanin sehre giris / cikis kavsagi
    for (int c = 0; c < nC; ++c) {
        const double ax = c > 0 ? cx[c] - cx[c - 1] : 1.0, ay = c > 0 ? cy[c] - cy[c - 1] : 0.0;
        const double bx = c + 1 < nC ? cx[c + 1] - cx[c] : 1.0, by = c + 1 < nC ? cy[c + 1] - cy[c] : 0.0;
        double dx = ax / std::hypot(ax, ay) + bx / std::hypot(bx, by), dy = ay / std::hypot(ax, ay) + by / std::hypot(bx, by);
        const double dl = std::hypot(dx, dy); dx /= dl; dy /= dl;
        const double px = -dy, py = dx;                                     // sol
        const double R = kBlock * (kGrid - 1) * 0.5;
        cities.push_back({Career::cityName(c), cx[c], cy[c], dx, dy, R * 1.45 + 120.0, 0.0});
        auto P = [&](double u, double v) { return std::make_pair(cx[c] + u * dx + v * px, cy[c] + u * dy + v * py); };
        // Kavsak izgarasi (5 x 5); orta satir (j = 2) otoban caddesi
        int g[kGrid][kGrid];
        for (int i = 0; i < kGrid; ++i)
            for (int j = 0; j < kGrid; ++j) { const auto q = P((i - 2) * kBlock, (j - 2) * kBlock); g[i][j] = addNode(q.first, q.second); }
        for (int j = 0; j < kGrid; ++j)
            for (int i = 0; i + 1 < kGrid; ++i)
                addEdge(g[i][j], g[i + 1][j], {{nodes[g[i][j]].x, nodes[g[i][j]].y}, {nodes[g[i + 1][j]].x, nodes[g[i + 1][j]].y}}, j == 2, c);
        for (int i = 0; i < kGrid; ++i)
            for (int j = 0; j + 1 < kGrid; ++j)
                addEdge(g[i][j], g[i][j + 1], {{nodes[g[i][j]].x, nodes[g[i][j]].y}, {nodes[g[i][j + 1]].x, nodes[g[i][j + 1]].y}}, false, c);
        west[c] = g[0][2]; east[c] = g[kGrid - 1][2];
        // Bloklar: 4 x 4 hucre; bazilari ozel (bulusma, benzinlik, hurdalik), digerleri 2 x 2 bina parseli
        for (int i = 0; i + 1 < kGrid; ++i)
            for (int j = 0; j + 1 < kGrid; ++j) {
                const double u = (i - 1.5) * kBlock, v = (j - 1.5) * kBlock;
                // Ozel parseller (benzinlik / bulusma / hurdalik): sokaga bakan kenarda (sokak merkezinden ~20 m)
                const auto q = P(u, v + (j < 2 ? 1.0 : -1.0) * (kBlock * 0.5 - 20.0));
                const double hdg = std::atan2(dy, dx);
                if (i == 0 && j == 0) { pois.push_back({WPoiMeet, q.first, q.second, hdg, c, 0, std::string("BULUSMA: ") + cities[c].name}); continue; }
                if (i == 3 && j == 3) { pois.push_back({WPoiGas, q.first, q.second, hdg, c, 0, "BENZINLIK"}); continue; }
                if (i == 0 && j == 3) { pois.push_back({WPoiGas, q.first, q.second, hdg, c, 1, "BENZINLIK"}); continue; }
                if (i == 3 && j == 0) { pois.push_back({WPoiJunk, q.first, q.second, hdg, c, 0, "HURDALIK"}); continue; }
                const double centre = 1.0 - std::hypot(u, v) / (kBlock * 2.2);   // merkeze yakin: yuksek
                for (int a = 0; a < 2; ++a)
                    for (int b = 0; b < 2; ++b) {
                        const int key = c * 1000 + i * 100 + j * 10 + a * 2 + b;
                        if (hashW(key * 7) < 0.12) continue;                     // bos parsel (otopark / bahce)
                        const double mU = (j == 1 || j == 2) && false ? 12.0 : 9.0;
                        const double su = u + (a - 0.5) * kBlock * 0.5, sv = v + (b - 0.5) * kBlock * 0.5;
                        const double hu = kBlock * 0.25 - mU - 2.0 * hashW(key * 3), hv = kBlock * 0.25 - mU - 2.0 * hashW(key * 5);
                        const auto bq = P(su, sv);
                        buildings.push_back({bq.first, bq.second, std::max(8.0, hu), std::max(8.0, hv), dx, dy,
                                             8.0 + (12.0 + 38.0 * std::max(0.0, centre)) * hashW(key * 11), c, (float)hashW(key * 13)});
                    }
            }
    }
    // Sehirlerarasi otoban: sehir cikisi -> sonraki sehir girisi (dalgali, 450 m yaricapli koseler)
    for (int c = 0; c + 1 < nC; ++c) {
        const WorldNode &A = nodes[east[c]], &B = nodes[west[c + 1]];
        std::vector<std::pair<double, double>> pts = {{A.x, A.y}, {A.x + cities[c].dirX * 350.0, A.y + cities[c].dirY * 350.0}};
        const double ex = B.x - cities[c + 1].dirX * 350.0, ey = B.y - cities[c + 1].dirY * 350.0;
        const double sx = pts.back().first, sy = pts.back().second, L = std::hypot(ex - sx, ey - sy);
        const int m = std::max(1, (int)(L / 1600.0));
        for (int k = 1; k < m; ++k) {
            const double t = (double)k / m, off = 380.0 * std::sin(k * 2.1 + c) * std::sin(kPiW * t);
            pts.push_back({sx + (ex - sx) * t - (ey - sy) / L * off, sy + (ey - sy) * t + (ex - sx) / L * off});
        }
        pts.push_back({ex, ey}); pts.push_back({B.x, B.y});
        const int e = addEdge(east[c], west[c + 1], pts, true, -1);
        // Yol ustu benzinlik: cikistan 1.2 km sonra, sag tarafta
        const RoadPath& fp = edges[e].fwd;
        const RoadPoint q = fp.at(std::min(1200.0, fp.length() * 0.3));
        const double off = -(fp.halfWidthAt(q.s) + 22.0);
        pois.push_back({WPoiGas, q.x - off * std::sin(q.heading), q.y + off * std::cos(q.heading), q.heading, c, 2, "OTOBAN BENZINLIK"});
    }
    // Yaris baslangiclari: o sehrin etkinlikleri, dikey sokaklarin ortasinda sag kenar
    const auto& ev = leagueEvents();
    for (int c = 0; c < nC; ++c) {
        int k = 0;
        for (int i = 0; i < (int)ev.size(); ++i) {
            if (eventCity(i) != c) continue;
            int found = -1, seen = 0;
            for (int e = 0; e < (int)edges.size(); ++e)
                if (edges[e].city == c && !edges[e].highway) { if (seen == (k * 3 + 1) % 36) { found = e; break; } ++seen; }
            ++k;
            if (found < 0) continue;
            const RoadPath& fp = edges[found].fwd;
            const RoadPoint q = fp.at(fp.length() * 0.5);
            const double off = -(fp.halfWidthAt(q.s) + 3.0);
            pois.push_back({WPoiRace, q.x - off * std::sin(q.heading), q.y + off * std::cos(q.heading), q.heading, c, i, ev[i].name});
        }
    }
    minX = minY = 1e18; maxX = maxY = -1e18;
    for (const WorldNode& n : nodes) { minX = std::min(minX, n.x); minY = std::min(minY, n.y); maxX = std::max(maxX, n.x); maxY = std::max(maxY, n.y); }
    for (const WorldEdge& e : edges) { minX = std::min(minX, e.minX); minY = std::min(minY, e.minY); maxX = std::max(maxX, e.maxX); maxY = std::max(maxY, e.maxY); }
}

int World::cityAt(double x, double y) const {
    for (size_t c = 0; c < cities.size(); ++c) if (std::hypot(x - cities[c].x, y - cities[c].y) < cities[c].r) return (int)c;
    return -1;
}

bool World::nearestLeg(double x, double y, double heading, WorldLeg& out, double& s, double& lat, double maxLat) const {
    double best = 1e18;
    for (int e = 0; e < (int)edges.size(); ++e) {
        const WorldEdge& E = edges[e];
        if (x < E.minX - maxLat || x > E.maxX + maxLat || y < E.minY - maxLat || y > E.maxY + maxLat) continue;
        int hint = 0; double ss, ll;
        E.fwd.projectGlobal(x, y, hint, ss, ll);
        const RoadPoint p = E.fwd.at(ss);
        const double d = std::hypot(x - p.x, y - p.y);
        if (d > maxLat) continue;
        const bool rev = std::cos(heading - p.heading) < 0.0;
        // Yon uyumu: kavsakta arac gittigi yone en uygun yolu secer (dik yolu cezalandir)
        const double align = std::fabs(std::sin(heading - p.heading));
        const double score = d + 6.0 * align + (d > p.hw ? 4.0 : 0.0);
        if (score < best) {
            best = score; out = {e, rev};
            s = rev ? E.fwd.length() - ss : ss; lat = rev ? -ll : ll;
        }
    }
    return best < 1e17;
}

std::vector<WorldLeg> World::exits(const WorldLeg& l) const {
    std::vector<WorldLeg> v;
    const int n = endNode(l);
    for (int e : nodes[n].edges) {
        if (e == l.edge) continue;
        v.push_back({e, edges[e].b == n});                                 // n'den cikis yonu
    }
    return v;
}

std::vector<WorldLeg> World::route(const WorldLeg& from, double, double tx, double ty) const {
    // Hedef: hedef noktaya en yakin dugum; Dijkstra (kenar uzunlugu) bulunulan yolun ucundan
    int target = 0; double bd = 1e18;
    for (int i = 0; i < (int)nodes.size(); ++i) { const double d = std::hypot(nodes[i].x - tx, nodes[i].y - ty); if (d < bd) { bd = d; target = i; } }
    const int start = endNode(from);
    std::vector<double> dist(nodes.size(), 1e18);
    std::vector<WorldLeg> via(nodes.size(), {-1, false});
    using QE = std::pair<double, int>;
    std::priority_queue<QE, std::vector<QE>, std::greater<QE>> q;
    dist[start] = 0; q.push({0, start});
    while (!q.empty()) {
        const auto [d, n] = q.top(); q.pop();
        if (d > dist[n]) continue;
        if (n == target) break;
        for (int e : nodes[n].edges) {
            const int m = edges[e].a == n ? edges[e].b : edges[e].a;
            const double nd = d + edges[e].fwd.length();
            if (nd < dist[m]) { dist[m] = nd; via[m] = {e, edges[e].b == n}; q.push({nd, m}); }
        }
    }
    std::vector<WorldLeg> legs;
    for (int n = target; n != start && via[n].edge >= 0; ) {
        legs.push_back(via[n]);
        const WorldEdge& E = edges[via[n].edge];
        n = via[n].rev ? E.b : E.a;
    }
    legs.push_back(from);
    std::reverse(legs.begin(), legs.end());
    return legs;
}

double World::fuelPriceAt(int city, int week) const {
    const int c = std::max(0, city);
    return 42.90 + 0.45 * c + 1.2 * std::sin(week * 1.7 + c * 0.9);
}

} // namespace zk
