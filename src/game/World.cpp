#include "World.h"
#include "game/Career.h"
#include "game/League.h"

#include <algorithm>
#include <array>
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
    const int nC = Career::kCities;
    // Sehir merkezleri: aralik = 4 km sehir + otoban (gercek km x 15 m), kuzey-guney dalga
    std::vector<double> cx(nC), cy(nC);
    for (int c = 0; c < nC; ++c) {
        cx[c] = c == 0 ? 0.0 : cx[c - 1] + halfU(c - 1) + halfU(c) + 2500.0 + Career::legKm(c - 1) * 55.0;   // otoban: gercek km x 55 m
        cy[c] = 3000.0 * std::sin(c * 1.3) + 900.0 * std::sin(c * 2.9);
    }
    std::vector<int> west(nC), east(nC);
    for (int c = 0; c < nC; ++c) {
        const double ax = c > 0 ? cx[c] - cx[c - 1] : 1.0, ay = c > 0 ? cy[c] - cy[c - 1] : 0.0;
        const double bx = c + 1 < nC ? cx[c + 1] - cx[c] : 1.0, by = c + 1 < nC ? cy[c + 1] - cy[c] : 0.0;
        double dx = ax / std::hypot(ax, ay) + bx / std::hypot(bx, by), dy = ay / std::hypot(ax, ay) + by / std::hypot(bx, by);
        const double dl = std::hypot(dx, dy); dx /= dl; dy /= dl;
        const double px = -dy, py = dx;
        const int style = c;
        const double hU = halfU(style), hV = halfV(style), half = std::max(hU, hV);
        cities.push_back({Career::cityName(c), cx[c], cy[c], dx, dy, half * 1.1 + 150.0, 0.0, style, hU, hV});
        auto P = [&](double u, double v) { return std::make_pair(cx[c] + u * dx + v * px, cy[c] + u * dy + v * py); };
        auto land = [&](double u, double v) { return surfaceLocal(style, u, v) == 0; };
        auto wet = [&](double u, double v) { return surfaceLocal(style, u, v) == 1; };
        // Doku: aralik (m), titresim, eksik baglanti orani, egrilik, bina yuksekligi
        static const double kSp[10] = {230, 260, 270, 280, 300, 290, 380, 340, 270, 280};
        static const double kJit[10] = {0.30, 0.22, 0.25, 0.20, 0.12, 0.18, 0.38, 0.28, 0.10, 0.22};
        static const double kDrop[10] = {0.14, 0.12, 0.14, 0.12, 0.08, 0.12, 0.34, 0.26, 0.06, 0.12};
        static const double kTall[10] = {85, 40, 50, 35, 75, 35, 9, 14, 50, 45};
        const double sp = kSp[style];
        const int nu = (int)(2.0 * hU / sp) + 1, nv = (int)(2.0 * hV / sp) + 1, n = std::max(nu, nv);
        const int midJ = nv / 2;
        std::vector<std::vector<int>> g(n, std::vector<int>(n, -1));
        std::vector<std::vector<std::pair<double, double>>> uv(n, std::vector<std::pair<double, double>>(n));
        for (int i = 0; i < nu; ++i)
            for (int j = 0; j < nv; ++j) {
                const int key = c * 7919 + i * 131 + j;
                double u = -hU + i * sp, v = -hV + j * sp;
                if (j != midJ) { u += (hashW(key * 3) - 0.5) * 2.0 * kJit[style] * sp; v += (hashW(key * 5) - 0.5) * 2.0 * kJit[style] * sp; }
                else v += 60.0 * std::sin(u / 700.0 + c);                    // otoban caddesi hafif kivrimli
                uv[i][j] = {u, v};
                if (!land(u, v)) continue;
                const auto q = P(u, v);
                g[i][j] = addNode(q.first, q.second);
            }
        // Kenarlar: komsu dugumler; bogaz / nehir koprusu yalniz secili sira / sutunlarda (bosluk atlanir)
        auto isBridgeRow = [&](int j) { return j == midJ || (style == 0 && j == midJ + 4) || (style == 3 && j % 3 == 0); };
        auto isBridgeCol = [&](int i) { return style == 3 && i % 2 == 0; };
        std::vector<char> art;                                           // bu sehrin kenarlari: cadde mi
        const int firstEdge = (int)edges.size();
        auto link = [&](int i0, int j0, int i1, int j1, bool highway, bool avenue) {
            const int a = g[i0][j0], b = g[i1][j1];
            if (a < 0 || b < 0) return;
            for (int e : nodes[a].edges) if ((edges[e].a == b || edges[e].b == b)) return;
            const double u0 = uv[i0][j0].first, v0 = uv[i0][j0].second, u1 = uv[i1][j1].first, v1 = uv[i1][j1].second;
            const int key = c * 104729 + i0 * 977 + j0 * 31 + i1 * 7 + j1;
            if (!highway && !avenue && hashW(key) < kDrop[style]) return;   // eksik baglanti: duzensiz bloklar
            std::vector<std::pair<double, double>> pts = {P(u0, v0)};
            bool bridge = false;
            for (int k = 1; k < 8; ++k) if (wet(u0 + (u1 - u0) * k / 8.0, v0 + (v1 - v0) * k / 8.0)) bridge = true;
            if (!highway && !avenue && !bridge) {                             // organik: ortasi hafif kivrik
                const double L = std::hypot(u1 - u0, v1 - v0), off = (hashW(key * 3) - 0.5) * 0.30 * kJit[style] / 0.2 * L * 0.25;
                const double mu = 0.5 * (u0 + u1) - (v1 - v0) / L * off, mv = 0.5 * (v0 + v1) + (u1 - u0) / L * off;
                if (land(mu, mv)) pts.push_back(P(mu, mv));
            }
            pts.push_back(P(u1, v1));
            const int e = addEdge(a, b, pts, highway || avenue, c);
            edges[e].bridge = bridge;
            if (highway) edges[e].hw = 8.0;
        };
        // Sira (dogu-bati) ve sutun (kuzey-guney) baglantilari; kopru sira / sutunlarda suyun ustunden atlanir
        for (int j = 0; j < n; ++j) {
            int last = -1;
            for (int i = 0; i < n; ++i) {
                if (g[i][j] < 0) continue;
                if (last >= 0 && (i - last == 1 || (isBridgeRow(j) && i - last <= 4))) link(last, j, i, j, j == midJ, false);
                last = i;
            }
        }
        for (int i = 0; i < n; ++i) {
            int last = -1;
            for (int j = 0; j < n; ++j) {
                if (g[i][j] < 0) continue;
                if (last >= 0 && (j - last == 1 || (isBridgeCol(i) && j - last <= 3)))
                    link(i, last, i, j, false, (style == 4 && i == nu / 2) || (style == 8 && i == nu / 2));   // Ankara / Mersin: kuzey-guney bulvari
                last = j;
            }
        }
        // Sehre ozgu caddeler: Ankara capraz bulvarlar, Ankara / Konya halka yollari
        if (style == 4)
            for (int k = 0; k + 1 < n; ++k) {                                    // capraz bulvarlar merkezden (dugum yoksa atlanir)
                const int ci = nu / 2 - nv / 2;
                if (k + ci >= 0 && k + 1 + ci < n) { link(k + ci, k, k + 1 + ci, k + 1, false, true); link(k + ci, nv - 1 - k, k + 1 + ci, nv - 2 - k, false, true); }
            }
        if (style == 4 || style == 5) {
            for (double R : style == 4 ? std::vector<double>{1500.0} : std::vector<double>{650.0, 1350.0}) {
                std::vector<std::pair<double, std::pair<int, int>>> ring;
                for (int i = 0; i < n; ++i) for (int j = 0; j < n; ++j)
                    if (g[i][j] >= 0 && std::fabs(std::hypot(uv[i][j].first, uv[i][j].second * hU / hV) - R * hU / 2000.0) < sp * 0.55)
                        ring.push_back({std::atan2(uv[i][j].second, uv[i][j].first), {i, j}});
                std::sort(ring.begin(), ring.end());
                for (size_t k = 0; k < ring.size(); ++k) {
                    const auto& A = ring[k].second; const auto& Bq = ring[(k + 1) % ring.size()].second;
                    if (std::abs(A.first - Bq.first) <= 1 && std::abs(A.second - Bq.second) <= 1) link(A.first, A.second, Bq.first, Bq.second, false, true);
                }
            }
        }
        // Sahil caddesi: iki ucu da suya yakin kenarlar genis cadde olur
        for (int e = firstEdge; e < (int)edges.size(); ++e) {
            WorldEdge& E = edges[e];
            if (E.highway || E.bridge) continue;
            auto nearWater = [&](int nd) {
                const double ddx = nodes[nd].x - cx[c], ddy = nodes[nd].y - cy[c];
                const double u = ddx * dx + ddy * dy, v = ddx * px + ddy * py;
                for (int k = 0; k < 8; ++k) if (wet(u + sp * 0.7 * std::cos(k * 0.785), v + sp * 0.7 * std::sin(k * 0.785))) return true;
                return false;
            };
            if (style != 3 && nearWater(E.a) && nearWater(E.b)) { E.highway = true; E.hw = 6.0; }
        }
        (void)art;
        // Baglanti: otoban caddesinin bati ucundan ulasilamayan dugumler yalniz kalir (yollari silinmez; kopru / cadde korunur)
        int wN = -1, eN = -1;
        for (int i = 0; i < n; ++i) if (g[i][midJ] >= 0) { if (wN < 0) wN = g[i][midJ]; eN = g[i][midJ]; }
        west[c] = wN; east[c] = eN;
        // Binalar: sokaga 14-90 m uzaklikta, sokak yonune donuk; su / park / dag yok
        std::vector<std::array<double, 5>> segs;                          // u0 v0 u1 v1 (yerel), genislik
        for (int e = firstEdge; e < (int)edges.size(); ++e)
            for (size_t k = 0; k + 1 < edges[e].pts.size(); ++k) {
                auto L = [&](const std::pair<double, double>& q) { const double ddx = q.first - cx[c], ddy = q.second - cy[c]; return std::make_pair(ddx * dx + ddy * dy, ddx * px + ddy * py); };
                const auto a = L(edges[e].pts[k]), b = L(edges[e].pts[k + 1]);
                segs.push_back({a.first, a.second, b.first, b.second, edges[e].hw});
            }
        const double cellH = 120.0;                                       // kaba izgara (en yakin sokak aramasi)
        const int nh = (int)(2.0 * half / cellH) + 3;   // half = buyuk yari boy
        std::vector<std::vector<int>> hash((size_t)nh * nh);
        auto hk = [&](double u, double v) { return std::clamp((int)((u + half + cellH) / cellH), 0, nh - 1) * nh + std::clamp((int)((v + half + cellH) / cellH), 0, nh - 1); };
        for (size_t k = 0; k < segs.size(); ++k) {
            const auto& sg = segs[k];
            const double L = std::hypot(sg[2] - sg[0], sg[3] - sg[1]);
            for (double t = 0; t <= L; t += cellH * 0.5) { const double f = L > 0 ? t / L : 0; const int h = hk(sg[0] + (sg[2] - sg[0]) * f, sg[1] + (sg[3] - sg[1]) * f); if (hash[h].empty() || hash[h].back() != (int)k) hash[h].push_back((int)k); }
        }
        const double bsp = style == 0 ? 44.0 : style >= 6 && style <= 7 ? 60.0 : 50.0;
        for (double u = -hU; u <= hU; u += bsp)
            for (double v = -hV; v <= hV; v += bsp) {
                const int key = c * 900001 + (int)((u + half) / bsp) * 997 + (int)((v + half) / bsp);
                const double bu = u + (hashW(key) - 0.5) * bsp * 0.3, bv = v + (hashW(key * 3) - 0.5) * bsp * 0.3;
                if (!land(bu, bv)) continue;
                const bool park = hashW(key * 7) < (style >= 6 && style <= 7 ? 0.45 : 0.18);   // bos arsa: agaclik / park
                double best = 1e9, dirU = 1, dirV = 0, bw = 4, nu = bu, nv = bv;
                const int h0 = hk(bu, bv);
                for (int di = -1; di <= 1; ++di) for (int dj = -1; dj <= 1; ++dj) {
                    const int h = h0 + di * nh + dj;
                    if (h < 0 || h >= (int)hash.size()) continue;
                    for (int k : hash[h]) {
                        const auto& sg = segs[k];
                        const double ex2 = sg[2] - sg[0], ey2 = sg[3] - sg[1], L2 = ex2 * ex2 + ey2 * ey2;
                        const double t = L2 > 0 ? std::clamp(((bu - sg[0]) * ex2 + (bv - sg[1]) * ey2) / L2, 0.0, 1.0) : 0.0;
                        const double d = std::hypot(bu - sg[0] - ex2 * t, bv - sg[1] - ey2 * t) - sg[4];
                        if (d < best) { best = d; const double l = std::sqrt(L2) + 1e-9; dirU = ex2 / l; dirV = ey2 / l; bw = sg[4]; nu = sg[0] + ex2 * t; nv = sg[1] + ey2 * t; }
                    }
                }
                (void)bw;
                const double hs = (style >= 6 && style <= 7 ? 7.0 : 9.0) + 7.0 * hashW(key * 11), hd = (style >= 6 && style <= 7 ? 6.0 : 8.0) + 6.0 * hashW(key * 13);
                if (park) {                                               // agac kumesi (yola en az 6 m)
                    if (best < 10.0 || best > 95.0) continue;
                    const int nt = 4 + (int)(hashW(key * 29) * 5);
                    for (int k = 0; k < nt; ++k) {
                        const double a = hashW(key * 31 + k) * 6.2832, rr = 3.0 + std::min(best - 6.0, 16.0) * hashW(key * 37 + k);
                        const auto q = P(bu + rr * std::cos(a), bv + rr * std::sin(a));
                        trees.push_back({q.first, q.second, (float)hashW(key * 41 + k), c});
                    }
                    continue;
                }
                if (best < hd + 7.0 || best > 95.0) continue;
                const double wx = dirU * dx + dirV * px, wy = dirU * dy + dirV * py;   // sokak yonu (dunya)
                const auto q = P(bu, bv);
                const double centre = std::max(0.0, 1.0 - std::hypot(bu / hU, bv / hV) / 1.2);
                const double ht = 6.0 + (kTall[style] * centre * centre + 6.0) * (0.35 + 0.65 * hashW(key * 17));
                const double side = (nu - bu) * -dirV + (nv - bv) * dirU;   // yol, binanin yerel v ekseninde hangi yonde
                buildings.push_back({q.first, q.second, hs, hd, wx, wy, ht, c, (float)hashW(key * 19), best - hd, side >= 0 ? 1.0f : -1.0f});
            }
        // Ozel noktalar: 2 bulusma, 6 benzinlik, 1 hurdalik (sokak kenarinda, karada)
        std::vector<int> streets;
        for (int e = firstEdge; e < (int)edges.size(); ++e) if (!edges[e].highway && !edges[e].bridge) streets.push_back(e);
        const double hdg = std::atan2(dy, dx);
        int placed = 0;
        for (size_t k = 0; k < streets.size() && placed < 9; ++k) {
            const int e = streets[(k * 53 + c * 17) % streets.size()];
            const RoadPath& fp = edges[e].fwd();
            const RoadPoint q = fp.at(fp.length() * 0.5);
            const double off = -(fp.halfWidthAt(q.s) + 20.0);
            const double wxp = q.x - off * std::sin(q.heading), wyp = q.y + off * std::cos(q.heading);
            if (surfaceAt(wxp, wyp) != 0) continue;
            const int type = placed < 2 ? WPoiMeet : placed < 8 ? WPoiGas : WPoiJunk;
            pois.push_back({type, wxp, wyp, q.heading, c, 0, type == WPoiMeet ? std::string("BULUSMA: ") + cities[c].name : type == WPoiGas ? "BENZINLIK" : "HURDALIK"});
            ++placed;
        }
        (void)hdg;
    }
    // Sehirlerarasi otoban: sehir cikisi -> sonraki sehir girisi (dalgali, 450 m yaricapli koseler); kenarinda koyler, agaclar
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
        {   // karsi yonde ikinci dinlenme tesisi (uzun otobanda)
            const RoadPoint q2 = fp.at(fp.length() * 0.68);
            const double off2 = fp.halfWidthAt(q2.s) + 22.0;
            pois.push_back({WPoiGas, q2.x - off2 * std::sin(q2.heading), q2.y + off2 * std::cos(q2.heading), q2.heading + kPiW, c, 1, "DINLENME TESISI"});
        }
        // Yol kenari: agac kumeleri (iki yanda), her 1.4-2.6 km'de bir koy (6-14 ev + bahce agaclari)
        const double Lh = fp.length();
        int key = 7000000 + c * 100000;
        for (double s = 300.0; s < Lh - 300.0; s += 13.0) {
            const RoadPoint p = fp.at(s);
            for (int side = 0; side < 2; ++side) {
                ++key;
                if (hashW(key) < 0.35) continue;
                const double off3 = (side ? 1.0 : -1.0) * (p.hw + 14.0 + 70.0 * hashW(key * 3));
                trees.push_back({p.x - off3 * std::sin(p.heading), p.y + off3 * std::cos(p.heading), (float)hashW(key * 5), -1});
            }
        }
        for (double s = 900.0 + 500.0 * hashW(c * 17); s < Lh - 900.0; s += 1400.0 + 1200.0 * hashW((int)s + c)) {
            const RoadPoint p = fp.at(s);
            const double side = hashW((int)s * 3 + c) < 0.5 ? 1.0 : -1.0;
            const double vo = side * (p.hw + 60.0 + 80.0 * hashW((int)s * 7 + c));        // koy merkezi yoldan 60-140 m
            const double vx0 = p.x - vo * std::sin(p.heading), vy0 = p.y + vo * std::cos(p.heading);
            const double ux = std::cos(p.heading), uy = std::sin(p.heading);
            const int nh = 6 + (int)(hashW((int)s * 11 + c) * 9);
            for (int h = 0; h < nh; ++h) {
                ++key;
                const double a = hashW(key * 13) * 6.2832, rr = 12.0 + 55.0 * hashW(key * 17);
                const double hx = vx0 + rr * std::cos(a), hy = vy0 + rr * std::sin(a);
                const double rot = (hashW(key * 19) - 0.5) * 0.5;                                  // evler yola yakin dogrultuda
                const double cu = std::cos(p.heading + rot), su = std::sin(p.heading + rot);
                buildings.push_back({hx, hy, 4.0 + 3.0 * hashW(key * 23), 3.5 + 2.5 * hashW(key * 29), cu, su, 4.0 + 4.5 * hashW(key * 31), -1,
                                     (float)hashW(key * 37), 6.0, 1.0f});
                for (int t = 0; t < 2; ++t) trees.push_back({hx + (hashW(key * 41 + t) - 0.5) * 22.0, hy + (hashW(key * 43 + t) - 0.5) * 22.0, (float)hashW(key * 47 + t), -1});
            }
            (void)ux; (void)uy;
        }
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
    // Simge yapilar (sehir yerel normal koordinatlarinda: 4 x 4 km kutuda, sehrin boyuna olceklenir)
    for (int c = 0; c < nC; ++c) {
        const WorldCity& C = cities[c];
        const double px = -C.dirY, py = C.dirX, su = C.hu / 2000.0, sv = C.hv / 2000.0;
        auto add = [&](int type, double u, double v, double r, double h, const char* name) {
            u *= su; v *= sv;
            landmarks.push_back({type, C.x + u * C.dirX + v * px, C.y + u * C.dirY + v * py, r, h, std::atan2(C.dirY, C.dirX), c, name});
        };
        switch (C.style) {
        case 0: {
            add(LmTower, -380, 620, 9, 67, "GALATA KULESI");
            const double vb = -1050, ub = 220.0 * std::sin(vb / 620.0 + 0.6) + 90.0 * std::sin(vb / 230.0);
            add(LmMaidenTower, ub, vb, 8, 27, "KIZ KULESI");
            for (double sgn : {-1.0, 1.0}) {                                // otoban koprusu kuleleri (Bogaz iki kiyisi)
                const double v0 = 60.0 * std::sin(0.0 + c) / sv, uc = 220.0 * std::sin(v0 / 620.0 + 0.6) + 90.0 * std::sin(v0 / 230.0);
                add(LmPylon, uc + sgn * 140.0, 0.0 + 30.0 / sv, 3, 165, "BOGAZ KOPRUSU");
                add(LmPylon, uc + sgn * 140.0, 0.0 - 30.0 / sv, 3, 165, "BOGAZ KOPRUSU");
            }
            add(LmMosque, -900, -500, 45, 40, "CAMI");
            break;
        }
        case 1: add(LmClock, 100, 300, 5, 24, "SAAT KULESI"); break;
        case 2: add(LmMosque, 0, 250, 50, 38, "ULU CAMI"); add(LmMountain, 0, -3600, 3800, 1700, "ULUDAG"); break;
        case 3: add(LmClock, -200, 420, 5, 26, "SAAT KULESI"); break;
        case 4: add(LmMausoleum, -700, 350, 70, 24, "ANITKABIR"); add(LmTvTower, 450, -650, 9, 125, "ATAKULE"); break;
        case 5: add(LmGreenDome, 380, 160, 22, 30, "MEVLANA"); break;
        case 6:
            for (int k = 0; k < 140; ++k) {                                  // peri bacalari: yesil vadilerde
                const double u = -1800 + 3600 * hashW(k * 7 + 3), v = -1300 + 2600 * hashW(k * 11 + 5);
                if (std::sin(u / 330.0) * std::sin(v / 290.0) <= 0.55 && std::hypot(u / 1800.0, v / 1300.0) < 1.0) continue;
                add(LmFairy, u, v, 6 + 6 * hashW(k * 13), 14 + 16 * hashW(k * 17), "PERI BACASI");
            }
            for (int k = 0; k < 12; ++k) add(LmBalloon, -1500 + 3000 * hashW(k * 23), -1100 + 2200 * hashW(k * 29), 9, 120 + 260 * hashW(k * 31), "BALON");
            break;
        case 7: add(LmCastle, 150, 250, 40, 22, "NIGDE KALESI"); break;
        case 8: add(LmSkyscraper, -200, 100, 22, 175, "METROPOL KULESI"); break;
        default: add(LmMinaret, -300, -150, 4, 38, "YIVLI MINARE"); add(LmGate, 200, 0, 20, 12, "HADRIAN KAPISI"); break;
        }
    }
    // Hiz kameralari: sehirlerarasi otobanda kenar basina 2 (120 km/h), sehir otoban caddesinde 2 (70 km/h)
    for (int e = 0; e < (int)edges.size(); ++e) {
        const WorldEdge& E = edges[e];
        if (!E.highway || E.bridge) continue;
        if (E.city < 0) {
            for (double f : {0.35, 0.72}) { const RoadPoint q = E.fwd().at(E.fwd().length() * f); cameras.push_back({q.x, q.y, q.heading, 120.0 / 3.6, -1}); }
        } else if (E.hw >= 8.0 && hashW(e * 7) < 0.12) {
            const RoadPoint q = E.fwd().at(E.fwd().length() * 0.5);
            cameras.push_back({q.x, q.y, q.heading, 70.0 / 3.6, E.city});
        }
    }
    // Garaj: her sehirde merkeze en yakin sokakta; toplanabilirler: sehir basina 5 nadir parca + sehir disinda ahir
    for (int c = 0; c < nC; ++c) {
        const WorldCity& C = cities[c];
        int best = -1; double bd = 1e18;
        std::vector<int> streets;
        for (int e = 0; e < (int)edges.size(); ++e) {
            const WorldEdge& E = edges[e];
            if (E.city != c || E.highway || E.bridge) continue;
            streets.push_back(e);
            const double d = std::hypot(E.pts.front().first - C.x, E.pts.front().second - C.y);
            if (d > 250.0 && d < bd) { bd = d; best = e; }
        }
        if (best >= 0) {
            const RoadPath& fp = edges[best].fwd();
            const RoadPoint q = fp.at(fp.length() * 0.5);
            const double off = -(fp.halfWidthAt(q.s) + 20.0);
            pois.push_back({WPoiGarage, q.x - off * std::sin(q.heading), q.y + off * std::cos(q.heading), q.heading, c, 0, std::string("GARAJ: ") + C.name});
        }
        for (int k = 0; k < 5 && !streets.empty(); ++k) {
            const int e = streets[(size_t)(hashW(c * 131 + k * 17) * streets.size()) % streets.size()];
            const RoadPath& fp = edges[e].fwd();
            const RoadPoint q = fp.at(fp.length() * (0.2 + 0.6 * hashW(c * 7 + k)));
            const double off = -(fp.halfWidthAt(q.s) + 3.5);
            collect.push_back({q.x - off * std::sin(q.heading), q.y + off * std::cos(q.heading), 0, c, (int)collect.size()});
        }
        {   const double u = C.hu * 0.92, v = C.hv * 1.12;                      // ahir: sehrin kuzey dogusunda, yesil alanda
            collect.push_back({C.x + u * C.dirX - v * C.dirY, C.y + u * C.dirY + v * C.dirX, 1, c, (int)collect.size()}); }
    }
    // Pert araclar: sehirlerarasi otoban kenarinda kenar basina 2 + her sehirde 1 sokakta; ozel yapim parca kasalari: 9
    // (gizli: sehrin uzak sokaklari / otoban banketi)
    {
        int seed = 1;
        for (int e = 0; e < (int)edges.size(); ++e) {
            const WorldEdge& E = edges[e];
            if (!(E.highway && E.city < 0)) continue;
            for (double f : {0.22, 0.61}) {
                const RoadPath& fp = E.fwd(); const RoadPoint q = fp.at(fp.length() * f);
                const double off = -(fp.halfWidthAt(q.s) + 9.0);
                collect.push_back({q.x - off * std::sin(q.heading), q.y + off * std::cos(q.heading), 2, -1, (int)collect.size(), seed++, q.heading + 0.7});
            }
        }
        for (int c = 0; c < nC; ++c) {
            std::vector<int> streets;
            for (int e = 0; e < (int)edges.size(); ++e) if (edges[e].city == c && !edges[e].highway && !edges[e].bridge) streets.push_back(e);
            if (streets.empty()) continue;
            const int e = streets[(size_t)(hashW(c * 977 + 5) * streets.size()) % streets.size()];
            const RoadPath& fp = edges[e].fwd(); const RoadPoint q = fp.at(fp.length() * 0.4);
            const double off = -(fp.halfWidthAt(q.s) + 5.0);
            collect.push_back({q.x - off * std::sin(q.heading), q.y + off * std::cos(q.heading), 2, c, (int)collect.size(), seed++, q.heading - 0.4});
        }
        for (int k = 0; k < 9; ++k) {                                       // ozel yapim kasalari
            const int c = k % nC;
            std::vector<int> streets;
            for (int e = 0; e < (int)edges.size(); ++e) if (edges[e].city == c && !edges[e].highway && !edges[e].bridge) streets.push_back(e);
            int bestE = -1; double bd = -1;
            for (int t = 0; t < 40 && !streets.empty(); ++t) {             // merkezden en uzak sokaklardan biri
                const int e = streets[(size_t)(hashW(k * 331 + t * 7) * streets.size()) % streets.size()];
                const double d = std::hypot(edges[e].pts.front().first - cities[c].x, edges[e].pts.front().second - cities[c].y);
                if (d > bd) { bd = d; bestE = e; }
            }
            if (bestE < 0) continue;
            const RoadPath& fp = edges[bestE].fwd(); const RoadPoint q = fp.at(fp.length() * 0.7);
            const double off = -(fp.halfWidthAt(q.s) + 4.0);
            collect.push_back({q.x - off * std::sin(q.heading), q.y + off * std::cos(q.heading), 3, c, (int)collect.size(), k, 0.0});
        }
    }
    // Simge yapilarin uzerindeki binalar kaldirilir
    buildings.erase(std::remove_if(buildings.begin(), buildings.end(), [&](const WorldBuilding& b) {
        for (const WorldLandmark& l : landmarks) if (l.type != LmMountain && l.type != LmBalloon && std::hypot(b.cx - l.x, b.cy - l.y) < l.r + 30.0) return true;
        return false; }), buildings.end());
    // Ozel parsellerin uzerindeki binalar kaldirilir (benzinlik / bulusma / hurdalik / yaris tabelasi)
    buildings.erase(std::remove_if(buildings.begin(), buildings.end(), [&](const WorldBuilding& b) {
        for (const WorldPoi& q : pois) if (std::hypot(b.cx - q.x, b.cy - q.y) < (q.type == WPoiRace ? 14.0 : 48.0)) return true;
        return false; }), buildings.end());
    minX = minY = 1e18; maxX = maxY = -1e18;
    for (const WorldEdge& e : edges) { minX = std::min(minX, e.minX); minY = std::min(minY, e.minY); maxX = std::max(maxX, e.maxX); maxY = std::max(maxY, e.maxY); }
}

// Sehir sekilleri (yerel: u otoban yonu dogu, v sol / kuzey; metre). 0 kara (sehir), 1 su, 2 yesil (park / dag / disi)
// Sehir yari boylari (m) x 1.3: daha buyuk sehirler, daha cok sokak
double World::halfU(int style) { static const double h[10] = {5000, 3000, 3500, 2500, 3500, 2750, 2000, 1500, 3500, 3500}; return 1.3 * h[std::clamp(style, 0, 9)]; }
double World::halfV(int style) { static const double h[10] = {3500, 1250, 1750, 2000, 3000, 2750, 1500, 1250, 1500, 2250}; return 1.3 * h[std::clamp(style, 0, 9)]; }

static int surfaceRaw(int style, double u, double v);
int World::surfaceLocal(int style, double u, double v) {
    if (std::fabs(u) > halfU(style) || std::fabs(v) > halfV(style)) return 2;
    u *= 2000.0 / halfU(style); v *= 2000.0 / halfV(style);              // sekiller 4 x 4 km'lik normal kutuda tanimli
    const int r = surfaceRaw(style, u, v);
    // Sehir kenari: duzensiz, kivrimli sinir (kutu kenarinda duz kesilmez; su kenarda kalir)
    const double eu = 1700.0 + 210.0 * std::sin(v / 330.0 + style * 1.7) + 70.0 * std::sin(v / 110.0 + style);
    const double ev = 1700.0 + 210.0 * std::sin(u / 360.0 + style * 2.3) + 70.0 * std::sin(u / 130.0 + style);
    if (r == 0 && (std::fabs(u) > eu || std::fabs(v) > ev)) return 2;
    return r;
}
static int surfaceRaw(int style, double u, double v) {
    const double th = std::atan2(v, u), r = std::hypot(u, v);
    auto blob = [&](double a, double b, double seed) {                  // duzensiz kenarli elips
        const double k = 1.0 + 0.13 * std::sin(3 * th + seed) + 0.07 * std::sin(5 * th + 2 * seed) + 0.04 * std::sin(9 * th + seed);
        return (u / a) * (u / a) + (v / b) * (v / b) < k * k;
    };
    if (std::fabs(u) > 2000.0 || std::fabs(v) > 2000.0) return 2;
    switch (style) {
    case 0: {   // Istanbul: kivrimli Bogaz (kuzey-guney) iki yakayi ayirir, guneyde Marmara, bati yakada Halic
        const double marmara = -1250.0 + 140.0 * std::sin(u / 420.0) + 80.0 * std::sin(u / 170.0);
        if (v < marmara) return 1;
        if (v > -1400.0 && std::fabs(u - (220.0 * std::sin(v / 620.0 + 0.6) + 90.0 * std::sin(v / 230.0))) < 120.0 + 40.0 * (v < -900 ? 1 : 0)) return 1;
        if (u > -1100.0 && u < -170.0 && std::fabs(v - (420.0 + 120.0 * std::sin(u / 260.0))) < 55.0) return 1;   // Halic
        return blob(1950.0, 1900.0, 0.4) ? 0 : 2;
    }
    case 1: {   // Izmit: guneyde korfez, kiyi boyunca ince uzun sehir
        if (v < -380.0 + 160.0 * std::sin(u / 600.0) + 60.0 * std::sin(u / 210.0)) return 1;
        return v < 950.0 + 200.0 * std::sin(u / 700.0) ? 0 : 2;
    }
    case 2: {   // Bursa: guneyde Uludag etegi (yerlesim yok), dogu-bati uzanan sehir
        if (v < -950.0 + 220.0 * std::sin(u / 520.0) + 90.0 * std::sin(u / 190.0)) return 2;
        return v < 1150.0 + 160.0 * std::sin(u / 650.0) ? 0 : 2;
    }
    case 3: {   // Eskisehir: ortadan kivrilarak gecen Porsuk Cayi
        if (std::fabs(v - (190.0 * std::sin(u / 430.0) + 60.0 * std::sin(u / 150.0))) < 32.0) return 1;
        return blob(1950.0, 1600.0, 1.3) ? 0 : 2;
    }
    case 4: return blob(2000.0, 1850.0, 2.1) ? 0 : 2;                   // Ankara: genis yayla sehri
    case 5: return r < 210.0 ? 2 : blob(1900.0, 1900.0, 0.9) ? 0 : 2;   // Konya: ortada Alaaddin tepesi (park)
    case 6: {   // Kapadokya: vadiler arasinda dagnik kasabalar
        if (!blob(1800.0, 1300.0, 2.7)) return 2;
        return std::sin(u / 330.0) * std::sin(v / 290.0) > 0.55 ? 2 : 0;
    }
    case 7: return blob(1500.0, 1250.0, 1.7) ? 0 : 2;                   // Nigde: kucuk kasaba
    case 8: {   // Mersin: guneyde deniz, kiyiya paralel uzun sehir
        if (v < -750.0 + 70.0 * std::sin(u / 500.0)) return 1;
        return v < 1150.0 + 150.0 * std::sin(u / 640.0) ? 0 : 2;
    }
    default: {  // Antalya: guneyde dalgali kiyi (falez), sahil boyunca
        if (v < -850.0 + 320.0 * std::sin(u / 900.0 + 0.4) + 90.0 * std::sin(u / 260.0)) return 1;
        return v < 1250.0 && blob(2000.0, 1900.0, 0.2) ? 0 : 2;
    }
    }
}

int World::surfaceAt(double x, double y) const {
    for (const WorldCity& C : cities) {
        const double ddx = x - C.x, ddy = y - C.y;
        if (std::fabs(ddx) > C.hu + C.hv + 800.0 || std::fabs(ddy) > C.hu + C.hv + 800.0) continue;
        return surfaceLocal(C.style, ddx * C.dirX + ddy * C.dirY, -ddx * C.dirY + ddy * C.dirX);
    }
    return 2;
}

int World::cityAt(double x, double y) const {
    for (size_t c = 0; c < cities.size(); ++c) {                           // kare sehir: yerel eksenlerde +-2 km (+ 150 m)
        const WorldCity& C = cities[c];
        const double dx = x - C.x, dy = y - C.y;
        const double u = dx * C.dirX + dy * C.dirY, v = -dx * C.dirY + dy * C.dirX;
        if (std::fabs(u) < C.hu + 150.0 && std::fabs(v) < C.hv + 150.0) return (int)c;
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
