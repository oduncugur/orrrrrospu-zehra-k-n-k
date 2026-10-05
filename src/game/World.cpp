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

const RoadPath& WorldEdge::fwd() const {
    if (!fwd_) fwd_ = std::make_unique<RoadPath>(RoadPath::polyline(pts, hw, lanes, lanes, cornerR));
    return *fwd_;
}
const RoadPath& WorldEdge::rev() const {
    if (!rev_) { std::vector<std::pair<double, double>> rp(pts.rbegin(), pts.rend()); rev_ = std::make_unique<RoadPath>(RoadPath::polyline(rp, hw, lanes, lanes, cornerR)); }
    return *rev_;
}

const World& World::get() { static const World w; return w; }
World::World() { build(); }

int World::addNode(double x, double y) { nodes.push_back({x, y, {}}); return (int)nodes.size() - 1; }

int World::addEdge(int a, int b, const std::vector<std::pair<double, double>>& pts, bool highway, int city) {
    WorldEdge e;
    e.pts = pts; e.hw = highway ? 8.0 : 4.0; e.lanes = highway ? 2 : 1; e.cornerR = highway ? 450.0 : 8.0;
    e.a = a; e.b = b; e.highway = highway; e.city = city;
    e.len = 0; e.minX = e.minY = 1e18; e.maxX = e.maxY = -1e18;
    for (size_t i = 0; i < pts.size(); ++i) {
        if (i) e.len += std::hypot(pts[i].first - pts[i - 1].first, pts[i].second - pts[i - 1].second);
        e.minX = std::min(e.minX, pts[i].first - e.hw); e.minY = std::min(e.minY, pts[i].second - e.hw);
        e.maxX = std::max(e.maxX, pts[i].first + e.hw); e.maxY = std::max(e.maxY, pts[i].second + e.hw);
    }
    if (highway && city < 0) { e.minX -= 400; e.minY -= 400; e.maxX += 400; e.maxY += 400; }   // yuvarlatilmis koseler kutudan tasabilir
    edges.push_back(std::move(e));
    const int id = (int)edges.size() - 1;
    nodes[a].edges.push_back(id); nodes[b].edges.push_back(id);
    return id;
}

void World::build() {
    const int nC = Career::kCities, G = kGrid, mid = G / 2;
    const double half = kBlock * (G - 1) * 0.5;                          // 2 km
    // Sehir merkezleri: aralik = 4 km sehir + otoban (gercek km x 15 m), kuzey-guney dalga
    std::vector<double> cx(nC), cy(nC);
    for (int c = 0; c < nC; ++c) {
        cx[c] = c == 0 ? 0.0 : cx[c - 1] + 2.0 * half + 1200.0 + Career::legKm(c - 1) * 15.0;
        cy[c] = 3000.0 * std::sin(c * 1.3) + 900.0 * std::sin(c * 2.9);
    }
    std::vector<int> west(nC), east(nC);
    for (int c = 0; c < nC; ++c) {
        const double ax = c > 0 ? cx[c] - cx[c - 1] : 1.0, ay = c > 0 ? cy[c] - cy[c - 1] : 0.0;
        const double bx = c + 1 < nC ? cx[c + 1] - cx[c] : 1.0, by = c + 1 < nC ? cy[c + 1] - cy[c] : 0.0;
        double dx = ax / std::hypot(ax, ay) + bx / std::hypot(bx, by), dy = ay / std::hypot(ax, ay) + by / std::hypot(bx, by);
        const double dl = std::hypot(dx, dy); dx /= dl; dy /= dl;
        const double px = -dy, py = dx;
        cities.push_back({Career::cityName(c), cx[c], cy[c], dx, dy, half * 1.42 + 150.0, 0.0});
        auto P = [&](double u, double v) { return std::make_pair(cx[c] + u * dx + v * px, cy[c] + u * dy + v * py); };
        std::vector<std::vector<int>> g(G, std::vector<int>(G));
        for (int i = 0; i < G; ++i)
            for (int j = 0; j < G; ++j) { const auto q = P((i - mid) * kBlock, (j - mid) * kBlock); g[i][j] = addNode(q.first, q.second); }
        // Orta dogu-bati sirasi otoban caddesi, orta kuzey-guney sutunu bulvar (2+2); digerleri 1+1 sokak
        for (int j = 0; j < G; ++j)
            for (int i = 0; i + 1 < G; ++i)
                addEdge(g[i][j], g[i + 1][j], {{nodes[g[i][j]].x, nodes[g[i][j]].y}, {nodes[g[i + 1][j]].x, nodes[g[i + 1][j]].y}}, j == mid, c);
        for (int i = 0; i < G; ++i)
            for (int j = 0; j + 1 < G; ++j)
                addEdge(g[i][j], g[i][j + 1], {{nodes[g[i][j]].x, nodes[g[i][j]].y}, {nodes[g[i][j + 1]].x, nodes[g[i][j + 1]].y}}, i == mid, c);
        west[c] = g[0][mid]; east[c] = g[G - 1][mid];
        // Ozel bloklar (sehre gore kaydirilmis): 2 bulusma, 6 benzinlik, 1 hurdalik; digerleri 3 x 3 bina parseli
        const int B = G - 1;
        auto cell = [&](int k) { return std::make_pair((k * 5 + c * 3 + 1) % B, (k * 7 + c * 5 + 2) % B); };
        std::vector<std::pair<int, int>> meet = {cell(0), cell(1)}, gas = {cell(2), cell(3), cell(4), cell(5), cell(6), cell(7)}, junk = {cell(8)};
        auto isIn = [](const std::vector<std::pair<int, int>>& v, int i, int j) { for (auto& q : v) if (q.first == i && q.second == j) return true; return false; };
        const double hdg = std::atan2(dy, dx);
        for (int i = 0; i < B; ++i)
            for (int j = 0; j < B; ++j) {
                const double u = (i - mid + 0.5) * kBlock, v = (j - mid + 0.5) * kBlock;
                const double vs = v + (j < mid ? 1.0 : -1.0) * (kBlock * 0.5 - 22.0);   // sokaga bakan kenar
                const auto q = P(u, vs);
                if (isIn(meet, i, j)) { pois.push_back({WPoiMeet, q.first, q.second, hdg, c, 0, std::string("BULUSMA: ") + cities[c].name}); continue; }
                if (isIn(gas, i, j)) { pois.push_back({WPoiGas, q.first, q.second, hdg, c, 0, "BENZINLIK"}); continue; }
                if (isIn(junk, i, j)) { pois.push_back({WPoiJunk, q.first, q.second, hdg, c, 0, "HURDALIK"}); continue; }
                const double centre = 1.0 - std::hypot(u, v) / (half * 1.3);   // merkeze yakin: yuksek
                for (int a = 0; a < 3; ++a)
                    for (int b = 0; b < 3; ++b) {
                        const int key = c * 100000 + i * 1000 + j * 10 + a * 3 + b;
                        if (hashW(key * 7) < 0.15) continue;                    // bos parsel
                        const double cell3 = kBlock / 3.0;
                        const double su = u + (a - 1) * cell3, sv = v + (b - 1) * cell3;
                        const double hu = cell3 * 0.5 - 9.0 - 6.0 * hashW(key * 3), hv = cell3 * 0.5 - 9.0 - 6.0 * hashW(key * 5);
                        const auto bq = P(su, sv);
                        buildings.push_back({bq.first, bq.second, std::max(10.0, hu), std::max(10.0, hv), dx, dy,
                                             7.0 + (8.0 + 70.0 * std::pow(std::max(0.0, centre), 2.0)) * (0.4 + 0.6 * hashW(key * 11)), c, (float)hashW(key * 13)});
                    }
            }
    }
    // Sehirlerarasi otoban: sehir cikisi -> sonraki sehir girisi (dalgali, 450 m yaricapli koseler)
    for (int c = 0; c + 1 < nC; ++c) {
        const WorldNode &A = nodes[east[c]], &Bn = nodes[west[c + 1]];
        std::vector<std::pair<double, double>> pts = {{A.x, A.y}, {A.x + cities[c].dirX * 400.0, A.y + cities[c].dirY * 400.0}};
        const double ex = Bn.x - cities[c + 1].dirX * 400.0, ey = Bn.y - cities[c + 1].dirY * 400.0;
        const double sx = pts.back().first, sy = pts.back().second, L = std::hypot(ex - sx, ey - sy);
        const int m = std::max(1, (int)(L / 1600.0));
        for (int k = 1; k < m; ++k) {
            const double t = (double)k / m, off = 420.0 * std::sin(k * 2.1 + c) * std::sin(kPiW * t);
            pts.push_back({sx + (ex - sx) * t - (ey - sy) / L * off, sy + (ey - sy) * t + (ex - sx) / L * off});
        }
        pts.push_back({ex, ey}); pts.push_back({Bn.x, Bn.y});
        const int e = addEdge(east[c], west[c + 1], pts, true, -1);
        const RoadPath& fp = edges[e].fwd();                                // yol ustu benzinlik: cikistan 1.2 km sonra
        const RoadPoint q = fp.at(std::min(1200.0, fp.length() * 0.3));
        const double off = -(fp.halfWidthAt(q.s) + 22.0);
        pois.push_back({WPoiGas, q.x - off * std::sin(q.heading), q.y + off * std::cos(q.heading), q.heading, c, 2, "OTOBAN BENZINLIK"});
    }
    // Yaris baslangiclari: o sehrin etkinlikleri, sehre dagilmis sokaklarin ortasinda sag kenar
    const auto& ev = leagueEvents();
    for (int c = 0; c < nC; ++c) {
        std::vector<int> streets;
        for (int e = 0; e < (int)edges.size(); ++e) if (edges[e].city == c && !edges[e].highway) streets.push_back(e);
        int k = 0;
        for (int i = 0; i < (int)ev.size(); ++i) {
            if (eventCity(i) != c || streets.empty()) continue;
            const int e = streets[(size_t)(k * 37 + 11) % streets.size()];
            ++k;
            const RoadPath& fp = edges[e].fwd();
            const RoadPoint q = fp.at(fp.length() * 0.5);
            const double off = -(fp.halfWidthAt(q.s) + 3.0);
            pois.push_back({WPoiRace, q.x - off * std::sin(q.heading), q.y + off * std::cos(q.heading), q.heading, c, i, ev[i].name});
        }
    }
    minX = minY = 1e18; maxX = maxY = -1e18;
    for (const WorldEdge& e : edges) { minX = std::min(minX, e.minX); minY = std::min(minY, e.minY); maxX = std::max(maxX, e.maxX); maxY = std::max(maxY, e.maxY); }
}

int World::cityAt(double x, double y) const {
    for (size_t c = 0; c < cities.size(); ++c) {                           // kare sehir: yerel eksenlerde +-2 km (+ 150 m)
        const WorldCity& C = cities[c];
        const double dx = x - C.x, dy = y - C.y;
        const double u = dx * C.dirX + dy * C.dirY, v = -dx * C.dirY + dy * C.dirX;
        const double half = kBlock * (kGrid - 1) * 0.5 + 150.0;
        if (std::fabs(u) < half && std::fabs(v) < half) return (int)c;
    }
    return -1;
}

bool World::nearestLeg(double x, double y, double heading, WorldLeg& out, double& s, double& lat, double maxLat) const {
    double best = 1e18;
    for (int e = 0; e < (int)edges.size(); ++e) {
        const WorldEdge& E = edges[e];
        if (x < E.minX - maxLat || x > E.maxX + maxLat || y < E.minY - maxLat || y > E.maxY + maxLat) continue;
        int hint = 0; double ss, ll;
        const RoadPath& F = E.fwd();
        F.projectGlobal(x, y, hint, ss, ll);
        const RoadPoint p = F.at(ss);
        const double d = std::hypot(x - p.x, y - p.y);
        if (d > maxLat) continue;
        const bool rev = std::cos(heading - p.heading) < 0.0;
        const double align = std::fabs(std::sin(heading - p.heading));   // kavsakta gidis yonune en uygun yol
        const double score = d + 6.0 * align + (d > p.hw ? 4.0 : 0.0);
        if (score < best) {
            best = score; out = {e, rev};
            s = rev ? F.length() - ss : ss; lat = rev ? -ll : ll;
        }
    }
    return best < 1e17;
}

std::vector<WorldLeg> World::exits(const WorldLeg& l) const {
    std::vector<WorldLeg> v;
    const int n = endNode(l);
    for (int e : nodes[n].edges) if (e != l.edge) v.push_back({e, edges[e].b == n});
    return v;
}

std::vector<WorldLeg> World::route(const WorldLeg& from, double, double tx, double ty) const {
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
            if (n == start && e == from.edge) continue;                      // ilk kavsakta geldigi yola U donusu yok
            const int m = edges[e].a == n ? edges[e].b : edges[e].a;
            const double nd = d + edges[e].len / (edges[e].highway ? 1.6 : 1.0);   // otoban / bulvar tercih (hiz)
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
