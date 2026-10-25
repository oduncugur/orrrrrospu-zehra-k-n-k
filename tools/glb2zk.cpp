// ZEHRA KINIK - glTF binary (.glb) -> oyun ici arac modeli (.inc) donusturucu. Bagimsiz (kutuphane yok).
// Kullanim: glb2zk <girdi.glb> <cikti.inc> <aracId> <uzunluk_m> <genislik_m> <yukseklik_m> [secenekler]
//   --flip       : on / arka ters (otomatik tahmin yanlissa)
//   --budget N   : govde ucgen butcesi (varsayilan 9000; tekerler ayri ~4 x 500)
//   --credit "Ad - Yazar (CC-BY)" : jenerik satiri
// Isleyis: sahne dugumleri (matris / TRS) -> dunya ucgenleri; Y yukari -> oyun (x ileri, y sol, z yukari); en uzun
// yatay eksen = boy; on: kabin (en yuksek bolge) arkaya yakin varsayimi + far / stop malzeme adlari; gercek olcuye
// olcekleme; malzeme adi / rengi / saydamligindan oyun malzemesi; lastik malzemesinden (yoksa koselerdeki alt
// kumelerden) 4 teker merkezi; teker ucgenleri ayri parca (oyunda doner); vertex kumeleme ile butceye indirme.
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <queue>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

// ------------------------------------------------------------------ minimal JSON
struct J {
    enum T { Null, Bool, Num, Str, Arr, Obj } t = Null;
    double n = 0; bool b = false; std::string s;
    std::vector<J> a, ov; std::vector<std::string> ok;   // dizi; nesne: anahtar / deger ayri (eksik tip ile pair olmaz)
    const J* get(const char* k) const { if (t != Obj) return nullptr; for (size_t i = 0; i < ok.size(); ++i) if (ok[i] == k) return &ov[i]; return nullptr; }
    double num(const char* k, double d) const { const J* j = get(k); return j && j->t == Num ? j->n : d; }
    int inum(const char* k, int d) const { return (int)num(k, d); }
};
struct JP {
    const char* p; const char* e;
    void ws() { while (p < e && std::isspace((unsigned char)*p)) ++p; }
    std::string str() {
        std::string r; ++p;
        while (p < e && *p != '"') {
            if (*p == '\\') { ++p; char c = *p++; if (c == 'u') { p += 4; r += '?'; } else r += c == 'n' ? '\n' : c == 't' ? '\t' : c; }
            else r += *p++;
        }
        ++p; return r;
    }
    J val() {
        ws(); J j;
        if (*p == '{') { j.t = J::Obj; ++p; ws(); if (*p == '}') { ++p; return j; }
            for (;;) { ws(); std::string k = str(); ws(); ++p; J v = val(); j.ok.push_back(k); j.ov.push_back(std::move(v)); ws(); if (*p == ',') { ++p; continue; } ++p; break; } }
        else if (*p == '[') { j.t = J::Arr; ++p; ws(); if (*p == ']') { ++p; return j; }
            for (;;) { j.a.push_back(val()); ws(); if (*p == ',') { ++p; continue; } ++p; break; } }
        else if (*p == '"') { j.t = J::Str; j.s = str(); }
        else if (!std::strncmp(p, "true", 4)) { j.t = J::Bool; j.b = true; p += 4; }
        else if (!std::strncmp(p, "false", 5)) { j.t = J::Bool; p += 5; }
        else if (!std::strncmp(p, "null", 4)) { p += 4; }
        else { j.t = J::Num; char* end; j.n = std::strtod(p, &end); p = end; }
        return j;
    }
};

// ------------------------------------------------------------------ matematik
struct V3 { double x = 0, y = 0, z = 0; };
using M4 = std::array<double, 16>;   // sutun oncelikli (glTF)
M4 ident() { M4 m{}; m[0] = m[5] = m[10] = m[15] = 1; return m; }
M4 mul(const M4& a, const M4& b) { M4 r{}; for (int c = 0; c < 4; ++c) for (int rr = 0; rr < 4; ++rr) { double s = 0; for (int k = 0; k < 4; ++k) s += a[k * 4 + rr] * b[c * 4 + k]; r[c * 4 + rr] = s; } return r; }
V3 xf(const M4& m, V3 p) { return {m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12], m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13], m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]}; }
M4 nodeMatrix(const J& n) {
    if (const J* m = n.get("matrix")) { M4 r; for (int i = 0; i < 16; ++i) r[i] = m->a[i].n; return r; }
    M4 T = ident(), R = ident(), S = ident();
    if (const J* t = n.get("translation")) { T[12] = t->a[0].n; T[13] = t->a[1].n; T[14] = t->a[2].n; }
    if (const J* q = n.get("rotation")) {
        const double x = q->a[0].n, y = q->a[1].n, z = q->a[2].n, w = q->a[3].n;
        R[0] = 1 - 2 * (y * y + z * z); R[1] = 2 * (x * y + z * w); R[2] = 2 * (x * z - y * w);
        R[4] = 2 * (x * y - z * w); R[5] = 1 - 2 * (x * x + z * z); R[6] = 2 * (y * z + x * w);
        R[8] = 2 * (x * z + y * w); R[9] = 2 * (y * z - x * w); R[10] = 1 - 2 * (x * x + y * y);
    }
    if (const J* s = n.get("scale")) { S[0] = s->a[0].n; S[5] = s->a[1].n; S[10] = s->a[2].n; }
    return mul(T, mul(R, S));
}

// Oyun malzemeleri (LowPolyModel.h ile ayni sira)
enum { MatPaint = 0, MatGlass, MatTire, MatRim, MatLight, MatTail, MatTrim, MatDark, MatPlate, MatChrome, MatIndicator };
struct SrcMat { std::string name; double r = 0.8, g = 0.8, b = 0.8, a = 1; bool blend = false, emissive = false; };
struct Tri { V3 p[3]; int src; int mat = 0; int wheel = -1; };

static std::string lower(std::string s) { for (char& c : s) c = (char)std::tolower((unsigned char)c); return s; }
static bool has(const std::string& s, std::initializer_list<const char*> ks) {   // kelime bazli: kelime anahtarla baslar
    std::vector<std::string> w; std::string cur;
    for (char c : s) { if (std::isalpha((unsigned char)c)) cur += c; else { if (!cur.empty()) w.push_back(cur); cur.clear(); } }
    if (!cur.empty()) w.push_back(cur);
    for (auto k : ks) for (auto& x : w) if (x.rfind(k, 0) == 0) return true;
    return false;
}

int main(int argc, char** argv) {
    if (argc < 7) { std::fprintf(stderr, "kullanim: glb2zk in.glb out.inc id L W H [--flip] [--budget N] [--credit s]\n"); return 1; }
    const std::string in = argv[1], out = argv[2];
    const int id = std::atoi(argv[3]);
    const double L = std::atof(argv[4]), W = std::atof(argv[5]), H = std::atof(argv[6]);
    bool flip = false; int budget = 9000; std::string credit, texavg; double wbase = 0;
    for (int i = 7; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--flip")) flip = true;
        else if (!std::strcmp(argv[i], "--budget") && i + 1 < argc) budget = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--credit") && i + 1 < argc) credit = argv[++i];
        else if (!std::strcmp(argv[i], "--texavg") && i + 1 < argc) texavg = argv[++i];
        else if (!std::strcmp(argv[i], "--wb") && i + 1 < argc) wbase = std::atof(argv[++i]);   // dingil mesafesi (eksik tekerleri kopyalamak icin)   // "resim r g b a" satirlari (doku ortalama rengi)
    }
    // ---- GLB oku
    std::ifstream f(in, std::ios::binary);
    if (!f) { std::fprintf(stderr, "acilamadi: %s\n", in.c_str()); return 1; }
    std::string buf((std::istreambuf_iterator<char>(f)), {});
    auto u32 = [&](size_t o) { uint32_t v; std::memcpy(&v, buf.data() + o, 4); return v; };
    if (buf.size() < 20 || u32(0) != 0x46546C67) { std::fprintf(stderr, "glb degil\n"); return 1; }
    const uint32_t jl = u32(12);
    JP jp{buf.data() + 20, buf.data() + 20 + jl};
    const J g = jp.val();
    const char* bin = nullptr; size_t binLen = 0;
    if (20 + jl + 8 <= buf.size()) { binLen = u32(20 + jl); bin = buf.data() + 20 + jl + 8; }
    const J* accs = g.get("accessors"); const J* views = g.get("bufferViews"); const J* meshes = g.get("meshes");
    const J* nodes = g.get("nodes"); const J* mats = g.get("materials");
    if (!accs || !views || !meshes || !nodes || !bin) { std::fprintf(stderr, "eksik glTF\n"); return 1; }
    auto accData = [&](int ai, int& count, int& comps, int& ctype, size_t& stride) -> const char* {
        const J& A = accs->a[ai];
        count = A.inum("count", 0); ctype = A.inum("componentType", 5126);
        const std::string ty = A.get("type")->s;
        comps = ty == "SCALAR" ? 1 : ty == "VEC2" ? 2 : ty == "VEC3" ? 3 : 4;
        const J& BV = views->a[A.inum("bufferView", 0)];
        const size_t csz = ctype == 5126 || ctype == 5125 ? 4 : ctype == 5123 || ctype == 5122 ? 2 : 1;
        stride = (size_t)BV.inum("byteStride", 0); if (!stride) stride = csz * comps;
        return bin + BV.inum("byteOffset", 0) + A.inum("byteOffset", 0);
    };
    std::map<int, std::array<double, 4>> texCol;                      // resim indeksi -> ortalama renk (0..1)
    if (!texavg.empty()) { std::ifstream tf(texavg); int im; double r, gg, b, a; while (tf >> im >> r >> gg >> b >> a) texCol[im] = {r, gg, b, a}; }
    const J* texs = g.get("textures");
    std::vector<SrcMat> smat;
    if (mats) for (const J& m : mats->a) {
        SrcMat s; if (const J* n = m.get("name")) s.name = lower(n->s);
        if (const J* pbr = m.get("pbrMetallicRoughness")) {
            if (const J* c = pbr->get("baseColorFactor")) { s.r = c->a[0].n; s.g = c->a[1].n; s.b = c->a[2].n; s.a = c->a[3].n; }
            if (const J* bt = pbr->get("baseColorTexture"); bt && texs) {          // doku: ortalama renk x carpan
                const int ti = bt->inum("index", -1);
                if (ti >= 0 && ti < (int)texs->a.size()) {
                    const auto it = texCol.find(texs->a[ti].inum("source", -1));
                    if (it != texCol.end()) { s.r *= it->second[0]; s.g *= it->second[1]; s.b *= it->second[2]; s.a *= it->second[3]; }
                }
            }
        }
        if (const J* am = m.get("alphaMode")) s.blend = am->s == "BLEND";
        if (const J* ef = m.get("emissiveFactor")) s.emissive = ef->a[0].n + ef->a[1].n + ef->a[2].n > 0.3;
        smat.push_back(s);
    }
    smat.push_back({"(yok)"});
    // ---- sahne dolasimi
    std::vector<Tri> tris;
    std::vector<std::string> meshNames;
    std::function<void(int, const M4&)> walk;
    std::vector<int> parentSeen(nodes->a.size(), 0);
    walk = [&](int ni, const M4& parent) {
        const J& N = nodes->a[ni];
        const M4 m = mul(parent, nodeMatrix(N));
        const std::string nodeName = N.get("name") ? lower(N.get("name")->s) : "";
        if (const J* mi = N.get("mesh")) {
            const J& M = meshes->a[(int)mi->n];
            for (const J& P : M.get("primitives")->a) {
                if (P.inum("mode", 4) != 4) continue;
                const J* at = P.get("attributes"); if (!at || !at->get("POSITION")) continue;
                int cnt, comps, ct; size_t st;
                const char* pd = accData((int)at->get("POSITION")->n, cnt, comps, ct, st);
                std::vector<V3> pos(cnt);
                for (int i = 0; i < cnt; ++i) { float v[3]; std::memcpy(v, pd + i * st, 12); pos[i] = xf(m, {v[0], v[1], v[2]}); }
                std::vector<uint32_t> idx;
                if (const J* ia = P.get("indices")) {
                    int ic, icp, ict; size_t ist; const char* id2 = accData((int)ia->n, ic, icp, ict, ist);
                    idx.resize(ic);
                    for (int i = 0; i < ic; ++i) {
                        if (ict == 5125) { uint32_t v; std::memcpy(&v, id2 + i * ist, 4); idx[i] = v; }
                        else if (ict == 5123) { uint16_t v; std::memcpy(&v, id2 + i * ist, 2); idx[i] = v; }
                        else idx[i] = (uint8_t)id2[i * ist];
                    }
                } else { idx.resize(cnt); for (int i = 0; i < cnt; ++i) idx[i] = i; }
                int sm = P.inum("material", (int)smat.size() - 1);
                if (!nodeName.empty() && smat[sm].name.find(nodeName) == std::string::npos) {   // dugum adi da siniflandirmaya katilir
                    SrcMat c = smat[sm]; c.name += " | " + nodeName; smat.push_back(c); sm = (int)smat.size() - 1;
                }
                for (size_t i = 0; i + 2 < idx.size(); i += 3) tris.push_back({{pos[idx[i]], pos[idx[i + 1]], pos[idx[i + 2]]}, sm});
            }
        }
        if (const J* ch = N.get("children")) for (const J& c : ch->a) walk((int)c.n, m);
    };
    {
        std::vector<char> isChild(nodes->a.size(), 0);
        for (const J& N : nodes->a) if (const J* ch = N.get("children")) for (const J& c : ch->a) isChild[(int)c.n] = 1;
        const J* scenes = g.get("scenes");
        if (scenes && !scenes->a.empty() && scenes->a[g.inum("scene", 0)].get("nodes"))
            for (const J& r : scenes->a[g.inum("scene", 0)].get("nodes")->a) walk((int)r.n, ident());
        else for (size_t i = 0; i < nodes->a.size(); ++i) if (!isChild[i]) walk((int)i, ident());
    }
    if (tris.empty()) { std::fprintf(stderr, "ucgen yok\n"); return 1; }
    // ---- eksenler: glTF Y yukari. Yatayda en uzun eksen boy.
    V3 lo{1e9, 1e9, 1e9}, hi{-1e9, -1e9, -1e9};
    for (auto& t : tris) for (auto& p : t.p) { lo.x = std::min(lo.x, p.x); lo.y = std::min(lo.y, p.y); lo.z = std::min(lo.z, p.z); hi.x = std::max(hi.x, p.x); hi.y = std::max(hi.y, p.y); hi.z = std::max(hi.z, p.z); }
    const bool lenX = (hi.x - lo.x) >= (hi.z - lo.z);
    const double cx = 0.5 * (lo.x + hi.x), cz = 0.5 * (lo.z + hi.z), y0 = lo.y;
    const double srcLen = lenX ? hi.x - lo.x : hi.z - lo.z;
    const double k = L / srcLen;
    // oyun: x ileri, y sol, z yukari
    for (auto& t : tris) for (auto& p : t.p) {
        const double fwd = (lenX ? p.x - cx : p.z - cz) * k, side = (lenX ? -(p.z - cz) : (p.x - cx)) * k, up = (p.y - y0) * k;
        p = {fwd, side, up};
    }
    // ---- malzeme siniflandirma
    double areaTot = 0; std::map<int, double> areaBy;
    auto area = [](const Tri& t) { const V3 u{t.p[1].x - t.p[0].x, t.p[1].y - t.p[0].y, t.p[1].z - t.p[0].z}, v{t.p[2].x - t.p[0].x, t.p[2].y - t.p[0].y, t.p[2].z - t.p[0].z};
        const double x = u.y * v.z - u.z * v.y, y = u.z * v.x - u.x * v.z, z = u.x * v.y - u.y * v.x; return 0.5 * std::sqrt(x * x + y * y + z * z); };
    for (auto& t : tris) { const double a = area(t); areaBy[t.src] += a; areaTot += a; }
    int paintSrc = -1; double best = 0;
    for (auto& [s, a] : areaBy) {
        const SrcMat& m = smat[s]; const double sat = std::max({m.r, m.g, m.b}) - std::min({m.r, m.g, m.b});
        const bool cand = !has(m.name, {"glass", "window", "tire", "tyre", "rubber", "rim", "wheel", "light", "lamp", "interior", "chrome", "black", "plastic"});
        if (cand && (a * (0.5 + sat) > best)) { best = a * (0.5 + sat); paintSrc = s; }
    }
    for (auto& t : tris) {
        const SrcMat& m = smat[t.src];
        const std::string& n = m.name;
        const double lum = 0.3 * m.r + 0.59 * m.g + 0.11 * m.b, sat = std::max({m.r, m.g, m.b}) - std::min({m.r, m.g, m.b});
        int mt;
        if (has(n, {"glass", "window", "windshield", "screen", "cam"}) || (m.blend && m.a < 0.85 && !has(n, {"light", "lamp"}))) mt = MatGlass;
        else if (has(n, {"tire", "tyre", "rubber", "lastik"})) mt = MatTire;
        else if (has(n, {"wheel", "rim", "alloy", "jant", "hub", "spoke", "disc", "brake", "caliper"})) mt = lum < 0.1 ? MatTire : MatRim;
        else if (has(n, {"tail", "brake_light", "rear_light", "stop"}) || (has(n, {"light", "lamp"}) && m.r > 0.5 && m.g < 0.3)) mt = MatTail;
        else if (has(n, {"indicator", "blinker", "turn", "signal", "orange"})) mt = MatIndicator;
        else if (has(n, {"light", "lamp", "head", "far", "fara"}) || m.emissive) mt = MatLight;   // far / stop: konuma gore sonra
        else if (has(n, {"chrome", "metal", "exhaust", "mirror_glass"})) mt = MatChrome;
        else if (has(n, {"plate", "license", "plaka"})) mt = MatPlate;
        else if (t.src == paintSrc || has(n, {"paint", "body", "carpaint", "kaporta", "primary"})) mt = MatPaint;
        else if (has(n, {"chassis", "bump", "boot", "secondary", "door", "hood", "bonnet", "fender", "trunk"}) && lum > 0.7 && areaBy[t.src] > 0.3) mt = MatPaint;
        else if (has(n, {"interior", "seat", "dash", "grill", "grille", "black", "plastic", "trim", "rubber_trim", "under"}) || lum < 0.12) mt = MatDark;
        else if (lum > 0.6 && sat < 0.1) mt = MatChrome;
        else if (sat > 0.25 && m.r > m.g && m.r > 0.4) mt = MatTail;
        else mt = MatTrim;
        t.mat = mt;
    }
    // ---- adsiz malzemeler (ornek "model001_material049"): konum / boyut / saydamliktan siniflandir
    {
        struct BB { V3 a{1e9, 1e9, 1e9}, b{-1e9, -1e9, -1e9}; };
        std::map<int, BB> bb;
        for (auto& t : tris) { BB& x = bb[t.src]; for (auto& p : t.p) { x.a.x = std::min(x.a.x, p.x); x.a.y = std::min(x.a.y, p.y); x.a.z = std::min(x.a.z, p.z); x.b.x = std::max(x.b.x, p.x); x.b.y = std::max(x.b.y, p.y); x.b.z = std::max(x.b.z, p.z); } }
        double zl = 1e9; for (auto& [s2, x] : bb) zl = std::min(zl, x.a.z);
        auto generic = [&](const std::string& n) {
            return !has(n, {"glass", "window", "tire", "tyre", "rubber", "rim", "wheel", "light", "lamp", "paint", "body", "chrome", "plate", "interior", "seat",
                            "primary", "secondary", "chassis", "fara", "bump", "boot", "door", "hood", "trunk", "head", "tail", "exhaust", "mirror"});
        };
        std::map<int, int> cls;
        for (auto& [si, x] : bb) {
            const SrcMat& m = smat[si];
            if (!generic(m.name)) continue;
            const double lum = 0.3 * m.r + 0.59 * m.g + 0.11 * m.b, a = areaBy[si];
            const double z0 = x.a.z - zl, z1 = x.b.z - zl, xs = x.b.x - x.a.x, ym = std::max(std::fabs(x.a.y), std::fabs(x.b.y));
            int c;
            if (m.blend && (m.a < 0.6 || (lum < 0.1 && z0 > 0.75))) c = MatGlass;               // saydam / koyu saydam cam
            else if (z1 < 0.75 && xs < 0.8 && std::min(std::fabs(x.a.x), std::fabs(x.b.x)) > 0.55 && std::min(std::fabs(x.a.y), std::fabs(x.b.y)) > 0.5)
                c = lum < 0.02 ? MatTire : MatRim;                                               // koselerde, alcak: teker
            else if (a < 0.25 && x.a.x > 0.40 * L && z0 > 0.3) c = MatLight;                     // on uc, kucuk: far
            else if (a < 0.25 && x.b.x < -0.40 * L && z0 > 0.3) c = MatTail;                     // arka uc, kucuk: stop
            else if (ym < 0.70 && x.b.x < 0.30 * L && x.a.x > -0.30 * L) c = MatDark;            // kabin ici
            else if (a > 1.0 && xs > 0.45 * L) c = MatPaint;                                     // boyunca buyuk yuzey: kaporta
            else c = lum < 0.08 ? MatDark : MatTrim;
            cls[si] = c;
        }
        for (auto& t : tris) if (auto it = cls.find(t.src); it != cls.end()) t.mat = it->second;
    }
    // ---- on / arka: kabin (z en yuksek bolge) merkezi arkaya yakin; far / stop malzemeleriyle dogrula
    {
        double sTop = 0, wTop = 0, fl = 0, fn = 0, tl = 0, tn = 0, zMax = 0;
        for (auto& t : tris) for (auto& p : t.p) zMax = std::max(zMax, p.z);
        for (auto& t : tris) for (auto& p : t.p) {
            if (p.z > zMax * 0.85) { sTop += p.x; wTop += 1; }
            if (t.mat == MatLight) { fl += p.x; fn += 1; }
            if (t.mat == MatTail) { tl += p.x; tn += 1; }
        }
        double vote = 0;                                      // > 0: +x on dogru
        if (wTop > 0) vote += (sTop / wTop < 0 ? 1.0 : -1.0); // kabin arkada (orta-motorlularda zayif)
        (void)fl; (void)fn; (void)tl; (void)tn;               // isik adlari iki uctadir (far / stop sonra konumdan): yalniz kabin
        if ((vote < 0) != flip) for (auto& t : tris) for (auto& p : t.p) { p.x = -p.x; p.y = -p.y; }   // 180 derece cevir (aynalama yok)
    }
    // ---- isiklar: on yarida far, arka yarida stop (malzeme adindan "light" ise)
    for (auto& t : tris) if (t.mat == MatLight || t.mat == MatTail) {
        const double cxm = (t.p[0].x + t.p[1].x + t.p[2].x) / 3.0;
        if (std::fabs(cxm) > 0.30 * L) t.mat = cxm > 0 ? MatLight : MatTail;
    }
    // ---- olcu: boy zaten L; genislik / yukseklik ayri olcek (sinirli: model orantisi korunur, en fazla %8 sapma duzeltilir)
    {
        double wy = 0, hz = 0;
        for (auto& t : tris) for (auto& p : t.p) { if (t.mat != MatGlass) wy = std::max(wy, std::fabs(p.y)); hz = std::max(hz, p.z); }
        const double sy = std::clamp(W * 0.5 / std::max(wy, 1e-3), 0.92, 1.08), sz = std::clamp(H / std::max(hz, 1e-3), 0.92, 1.08);
        for (auto& t : tris) for (auto& p : t.p) { p.y *= sy; p.z *= sz; }
    }
    // ---- tekerler: lastik ucgenlerini 4 koseye ayir; yoksa alt %40 + koseler
    struct Wh { double x = 0, y = 0, z = 0, r = 0; int n = 0; };
    Wh wh[4];
    auto corner = [](const V3& p) { return (p.x > 0 ? 0 : 2) + (p.y > 0 ? 0 : 1); };   // on sol, on sag, arka sol, arka sag
    {
        bool tire = false;
        for (auto& t : tris) if (t.mat == MatTire) { tire = true; break; }
        std::vector<V3> pts[4];
        for (auto& t : tris) {
            const bool use = tire ? t.mat == MatTire : (t.mat == MatRim);
            if (!use) continue;
            for (auto& p : t.p) pts[corner(p)].push_back(p);
        }
        for (int c = 0; c < 4; ++c) {
            if (pts[c].empty()) continue;
            V3 a{1e9, 1e9, 1e9}, b{-1e9, -1e9, -1e9};
            for (auto& p : pts[c]) { a.x = std::min(a.x, p.x); a.y = std::min(a.y, p.y); a.z = std::min(a.z, p.z); b.x = std::max(b.x, p.x); b.y = std::max(b.y, p.y); b.z = std::max(b.z, p.z); }
            wh[c] = {0.5 * (a.x + b.x), 0.5 * (a.y + b.y), 0.5 * (a.z + b.z), 0.25 * ((b.x - a.x) + (b.z - a.z)), (int)pts[c].size()};
        }
    }
    bool wheelsOk = true;
    for (auto& w : wh) wheelsOk &= w.n > 0 && w.r > 0.15 && w.r < 0.6;
    for (int c = 0; c < 4; ++c) std::fprintf(stderr, "teker %d: n %d merkez %.2f %.2f %.2f r %.2f\n", c, wh[c].n, wh[c].x, wh[c].y, wh[c].z, wh[c].r);
    {   // zemin: en alt nokta z = 0
        double zmin = 1e9; for (auto& t : tris) for (auto& p : t.p) zmin = std::min(zmin, p.z);
        for (auto& t : tris) for (auto& p : t.p) p.z -= zmin;
        for (auto& w : wh) w.z -= zmin;
    }
    {   // Tekerler tek kosede ust uste (GTA tarzi "dummy" yerlesim): bulunan tekeri eksik koselere kopyala / aynala
        int found = -1, nFound = 0;
        for (int c = 0; c < 4; ++c) if (wh[c].n > 0 && wh[c].r > 0.15 && wh[c].r < 0.6) { found = c; ++nFound; }
        if (nFound == 1 && wbase > 0.5) {
            const Wh src = wh[found];
            std::vector<Tri> tmpl;                                         // kaynak tekerin ucgenleri (silindir ici)
            for (auto& t : tris) {
                bool in = t.mat != MatGlass;
                for (auto& p : t.p) in &= std::hypot(p.x - src.x, p.z - src.z) < src.r * 1.06 && std::fabs(p.y - src.y) < 0.20;
                if (in) tmpl.push_back(t);
            }
            const double fx = src.x > 0 ? src.x : src.x + wbase, rx = fx - wbase, ay = std::fabs(src.y);
            for (int c = 0; c < 4; ++c) {
                if (c == found) continue;
                const double tx = c < 2 ? fx : rx, ty = (c % 2 == 0) ? ay : -ay;
                const bool mir = (ty > 0) != (src.y > 0);
                for (Tri t : tmpl) {
                    for (auto& p : t.p) { p.x += tx - src.x; p.y = mir ? -(p.y - src.y) + ty : p.y - src.y + ty; }
                    if (mir) std::swap(t.p[1], t.p[2]);
                    tris.push_back(t);
                }
                wh[c] = {tx, ty, src.z, src.r, (int)tmpl.size()};
            }
            wheelsOk = true;
        }
    }
    if (!wheelsOk) {   // eksik koseler: yere degen noktalar (teker tabani) -> merkez; yaricap: bulunan tekerden ya da boyasiz en yuksek
        wheelsOk = true;
        double rKnown = 0; for (auto& w : wh) if (w.n > 0 && w.r > 0.15 && w.r < 0.6) rKnown = w.r;
        for (int c = 0; c < 4; ++c) {
            if (wh[c].n > 0 && wh[c].r > 0.15 && wh[c].r < 0.6) continue;
            double sx = 0, sy = 0; int n = 0;
            for (auto& t : tris) for (auto& p : t.p)
                if (p.z < 0.035 && corner(p) == c && std::fabs(p.x) > 0.2 * L && std::fabs(p.y) > 0.18 * W) { sx += p.x; sy += p.y; ++n; }
            if (n < 3) { wheelsOk = false; continue; }
            Wh w; w.x = sx / n; w.y = sy / n; w.n = n;
            double top = 0;
            for (auto& t : tris) if (t.mat != MatPaint && t.mat != MatGlass) for (auto& p : t.p)
                if (std::fabs(p.x - w.x) < 0.05 && std::fabs(p.y - w.y) < 0.12 && p.z < 0.9) top = std::max(top, p.z);
            w.r = rKnown > 0 ? rKnown : std::clamp(top * 0.5, 0.28, 0.38); w.z = w.r;
            wh[c] = w;
        }
    }
    if (wheelsOk) {
        for (auto& t : tris) {
            const V3 c{(t.p[0].x + t.p[1].x + t.p[2].x) / 3, (t.p[0].y + t.p[1].y + t.p[2].y) / 3, (t.p[0].z + t.p[1].z + t.p[2].z) / 3};
            const Wh& w = wh[corner(c)];
            const double d = std::hypot(c.x - w.x, c.z - w.z);
            bool inside = d < w.r * 1.02 && std::fabs(c.y - w.y) < 0.20;
            for (auto& p : t.p) inside &= std::hypot(p.x - w.x, p.z - w.z) < w.r * 1.06;
            if (inside && t.mat != MatGlass) {                   // silindirin ici: teker (adi ne olursa olsun)
                t.wheel = corner(c);
                const SrcMat& sm2 = smat[t.src];
                const double l2 = 0.3 * sm2.r + 0.59 * sm2.g + 0.11 * sm2.b;
                if (t.mat != MatTire && t.mat != MatRim && t.mat != MatChrome) t.mat = l2 < 0.15 ? MatTire : MatRim;
                // dis halka (yaricapin %72'si disi): lastik
                bool ring = true;
                for (auto& p : t.p) ring &= std::hypot(p.x - w.x, p.z - w.z) > w.r * 0.72;
                if (ring) t.mat = MatTire;
            }
        }
    }
    // ---- butceye indirme: quadric hata metrigi ile kenar birlestirme (Garland-Heckbert). Yuzey sekli korunur;
    // acik kenarlar ve malzeme sinirlari agir cezali (far / cam kenari kaymaz); yuz normali donen birlestirme reddedilir.
    auto qem = [&](std::vector<Tri>& v, int target) {
        if ((int)v.size() <= target) return;
        struct Q { double a[10] = {0}; void add(const Q& o, double w = 1) { for (int i = 0; i < 10; ++i) a[i] += o.a[i] * w; } };
        auto plane = [](double nx, double ny, double nz, double d) { Q q; double p[4] = {nx, ny, nz, d}; int k = 0; for (int i = 0; i < 4; ++i) for (int j = i; j < 4; ++j) q.a[k++] = p[i] * p[j]; return q; };
        auto err = [](const Q& q, const V3& p) {
            const double x = p.x, y = p.y, z = p.z; const double* a = q.a;
            return a[0] * x * x + 2 * a[1] * x * y + 2 * a[2] * x * z + 2 * a[3] * x + a[4] * y * y + 2 * a[5] * y * z + 2 * a[6] * y + a[7] * z * z + 2 * a[8] * z + a[9];
        };
        // kaynak: noktalari kaynakla (1e-5 m)
        std::vector<V3> P; std::vector<std::array<int, 3>> F; std::vector<int> FM; std::vector<char> alive;
        {
            std::map<std::array<long long, 3>, int> id;
            auto vi = [&](const V3& p) { std::array<long long, 3> k{std::llround(p.x * 1e5), std::llround(p.y * 1e5), std::llround(p.z * 1e5)};
                auto it = id.find(k); if (it != id.end()) return it->second; P.push_back(p); return id[k] = (int)P.size() - 1; };
            for (auto& t : v) { std::array<int, 3> f{vi(t.p[0]), vi(t.p[1]), vi(t.p[2])}; if (f[0] == f[1] || f[1] == f[2] || f[0] == f[2]) continue; F.push_back(f); FM.push_back(t.mat); }
            alive.assign(F.size(), 1);
        }
        const int nv = (int)P.size();
        std::vector<Q> QV(nv);
        std::vector<std::vector<int>> VF(nv);
        auto fnorm = [&](int f, double& nx, double& ny, double& nz, double& ar) {
            const V3 &a = P[F[f][0]], &b = P[F[f][1]], &c = P[F[f][2]];
            const double ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z, wx = c.x - a.x, wy = c.y - a.y, wz = c.z - a.z;
            nx = uy * wz - uz * wy; ny = uz * wx - ux * wz; nz = ux * wy - uy * wx; const double l = std::sqrt(nx * nx + ny * ny + nz * nz);
            ar = 0.5 * l; if (l > 1e-12) { nx /= l; ny /= l; nz /= l; }
        };
        for (size_t f = 0; f < F.size(); ++f) {
            double nx, ny, nz, ar; fnorm((int)f, nx, ny, nz, ar);
            const Q q = plane(nx, ny, nz, -(nx * P[F[f][0]].x + ny * P[F[f][0]].y + nz * P[F[f][0]].z));
            for (int k : F[f]) { QV[k].add(q, std::max(ar, 1e-6)); VF[k].push_back((int)f); }
        }
        // acik / malzeme siniri kenarlari: kenara dik duzlem, agir
        {
            std::map<std::pair<int, int>, std::vector<int>> E;
            for (size_t f = 0; f < F.size(); ++f) for (int e = 0; e < 3; ++e) { int a = F[f][e], b = F[f][(e + 1) % 3]; E[{std::min(a, b), std::max(a, b)}].push_back((int)f); }
            for (auto& [e, fs] : E) {
                bool border = fs.size() == 1;
                if (fs.size() == 2 && FM[fs[0]] != FM[fs[1]]) border = true;
                if (!border) continue;
                double nx, ny, nz, ar; fnorm(fs[0], nx, ny, nz, ar);
                const V3 &a = P[e.first], &b = P[e.second];
                double ex = b.x - a.x, ey = b.y - a.y, ez = b.z - a.z;
                double px = ey * nz - ez * ny, py = ez * nx - ex * nz, pz = ex * ny - ey * nx; const double l = std::sqrt(px * px + py * py + pz * pz);
                if (l < 1e-12) continue; px /= l; py /= l; pz /= l;
                const Q q = plane(px, py, pz, -(px * a.x + py * a.y + pz * a.z));
                const double w = 1000.0 * std::max(ar, 1e-4);
                QV[e.first].add(q, w); QV[e.second].add(q, w);
            }
        }
        std::vector<int> ver(nv, 0), rep(nv); for (int i = 0; i < nv; ++i) rep[i] = i;
        auto find = [&](int x) { while (rep[x] != x) x = rep[x] = rep[rep[x]]; return x; };
        struct C { double cost; int a, b, va, vb; V3 p; bool operator<(const C& o) const { return cost > o.cost; } };
        std::priority_queue<C> pq;
        auto push = [&](int a, int b) {
            Q q = QV[a]; q.add(QV[b]);
            const V3 m{0.5 * (P[a].x + P[b].x), 0.5 * (P[a].y + P[b].y), 0.5 * (P[a].z + P[b].z)};
            V3 best = m; double bc = err(q, m);
            for (const V3& c : {P[a], P[b]}) { const double e = err(q, c); if (e < bc) { bc = e; best = c; } }
            pq.push({bc, a, b, ver[a], ver[b], best});
        };
        for (size_t f = 0; f < F.size(); ++f) for (int e = 0; e < 3; ++e) { int a = F[f][e], b = F[f][(e + 1) % 3]; if (a < b) push(a, b); }
        int faces = (int)F.size();
        while (faces > target && !pq.empty()) {
            C c = pq.top(); pq.pop();
            if (find(c.a) != c.a || find(c.b) != c.b || ver[c.a] != c.va || ver[c.b] != c.vb) continue;
            // normal donmesi kontrolu: a / b komsu yuzleri yeni konumla
            bool bad = false;
            for (int vtx : {c.a, c.b}) for (int f : VF[vtx]) {
                if (!alive[f]) continue;
                int idx[3]; for (int k = 0; k < 3; ++k) idx[k] = find(F[f][k]);
                const bool hasA = idx[0] == c.a || idx[1] == c.a || idx[2] == c.a, hasB = idx[0] == c.b || idx[1] == c.b || idx[2] == c.b;
                if (hasA && hasB) continue;                                  // bu yuz silinecek
                double n0x, n0y, n0z, ar0; { V3 a = P[idx[0]], b = P[idx[1]], d = P[idx[2]];
                    const double ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z, wx = d.x - a.x, wy = d.y - a.y, wz = d.z - a.z;
                    n0x = uy * wz - uz * wy; n0y = uz * wx - ux * wz; n0z = ux * wy - uy * wx; ar0 = 0; }
                V3 q3[3]; for (int k = 0; k < 3; ++k) q3[k] = (idx[k] == c.a || idx[k] == c.b) ? c.p : P[idx[k]];
                const double ux = q3[1].x - q3[0].x, uy = q3[1].y - q3[0].y, uz = q3[1].z - q3[0].z, wx = q3[2].x - q3[0].x, wy = q3[2].y - q3[0].y, wz = q3[2].z - q3[0].z;
                const double n1x = uy * wz - uz * wy, n1y = uz * wx - ux * wz, n1z = ux * wy - uy * wx;
                const double d0 = std::sqrt(n0x * n0x + n0y * n0y + n0z * n0z), d1 = std::sqrt(n1x * n1x + n1y * n1y + n1z * n1z);
                if (d1 < 1e-14 || (d0 > 1e-14 && (n0x * n1x + n0y * n1y + n0z * n1z) / (d0 * d1) < 0.2)) { bad = true; break; }
                (void)ar0;
                if (bad) break;
            }
            if (bad) continue;
            // birlestir: b -> a
            P[c.a] = c.p; QV[c.a].add(QV[c.b]); rep[c.b] = c.a; ver[c.a]++; ver[c.b]++;
            for (int f : VF[c.b]) VF[c.a].push_back(f);
            std::vector<int> nb;
            for (int f : VF[c.a]) {
                if (!alive[f]) continue;
                int idx[3]; for (int k = 0; k < 3; ++k) idx[k] = F[f][k] = find(F[f][k]);
                if (idx[0] == idx[1] || idx[1] == idx[2] || idx[0] == idx[2]) { alive[f] = 0; --faces; continue; }
                for (int k = 0; k < 3; ++k) if (idx[k] != c.a) nb.push_back(idx[k]);
            }
            std::sort(nb.begin(), nb.end()); nb.erase(std::unique(nb.begin(), nb.end()), nb.end());
            for (int n : nb) push(std::min(c.a, n), std::max(c.a, n));
        }
        std::vector<Tri> o;
        for (size_t f = 0; f < F.size(); ++f) if (alive[f]) {
            Tri t; for (int k = 0; k < 3; ++k) t.p[k] = P[find(F[f][k])]; t.src = 0; t.mat = FM[f]; o.push_back(t);
        }
        v.swap(o);
    };
    auto decimate = [&](std::vector<Tri>& v, int target) {
        qem(v, target);
        if ((int)v.size() <= target) return;
        for (double cell = 0.004; cell < 0.2; cell *= 1.25) {
            std::vector<Tri> o;
            std::unordered_map<long long, V3> rep;
            auto key = [&](const V3& p, int m) {
                const long long ix = (long long)std::floor(p.x / cell) + 2000, iy = (long long)std::floor(p.y / cell) + 2000, iz = (long long)std::floor(p.z / cell) + 2000;
                return ((ix * 4096 + iy) * 4096 + iz) * 16 + m;
            };
            std::unordered_map<long long, std::pair<V3, int>> acc;
            for (auto& t : v) for (auto& p : t.p) { auto& a = acc[key(p, t.mat)]; a.first.x += p.x; a.first.y += p.y; a.first.z += p.z; a.second++; }
            for (auto& t : v) {
                Tri n = t; long long kk[3];
                for (int i = 0; i < 3; ++i) { kk[i] = key(t.p[i], t.mat); const auto& a = acc[kk[i]]; n.p[i] = {a.first.x / a.second, a.first.y / a.second, a.first.z / a.second}; }
                if (kk[0] == kk[1] || kk[1] == kk[2] || kk[0] == kk[2]) continue;
                o.push_back(n);
            }
            if ((int)o.size() <= target) { v.swap(o); return; }
        }
    };
    std::vector<Tri> body, whs[4];
    for (auto& t : tris) (t.wheel >= 0 ? whs[t.wheel] : body).push_back(t);
    decimate(body, budget);
    for (auto& w : whs) decimate(w, 600);
    // ---- yaz
    std::ofstream o(out);
    std::vector<std::array<float, 3>> verts; std::vector<std::array<int, 4>> ts;
    std::map<std::array<long long, 3>, int> vmap;
    auto vid = [&](const V3& p) {
        const std::array<long long, 3> q{std::llround(p.x * 2000), std::llround(p.y * 2000), std::llround(p.z * 2000)};
        auto it = vmap.find(q); if (it != vmap.end()) return it->second;
        verts.push_back({(float)p.x, (float)p.y, (float)p.z}); return vmap[q] = (int)verts.size() - 1;
    };
    auto emit = [&](const std::vector<Tri>& v) { for (auto& t : v) ts.push_back({vid(t.p[0]), vid(t.p[1]), vid(t.p[2]), t.mat}); };
    emit(body);
    size_t wb[4], we[4];
    for (int c = 0; c < 4; ++c) { wb[c] = ts.size(); emit(whs[c]); we[c] = ts.size(); }
    o << "// glb2zk: " << in << " -> arac " << id << "  (" << ts.size() << " ucgen, " << verts.size() << " nokta)\n";
    o << "// " << credit << "\n";
    o << "static const float kImpV_" << id << "[][3] = {\n";
    for (auto& v : verts) { char b[96]; std::snprintf(b, sizeof b, "{%.4ff,%.4ff,%.4ff},", v[0], v[1], v[2]); o << b; }
    o << "};\nstatic const int kImpT_" << id << "[][4] = {\n";
    for (auto& t : ts) o << "{" << t[0] << "," << t[1] << "," << t[2] << "," << t[3] << "},";
    o << "};\nstatic const ImportedWheel kImpW_" << id << "[4] = {\n";
    for (int c = 0; c < 4; ++c) { char b[160]; std::snprintf(b, sizeof b, "{%.4ff,%.4ff,%.4ff,%.4ff,%zu,%zu},\n", wh[c].x, wh[c].y, wh[c].z, wh[c].r, wb[c], we[c]); o << b; }
    o << "};\n";
    std::printf("arac %d: %zu ucgen (govde %zu), %zu nokta, tekerler %s, kaynak boy %.2f -> %.2f m\n", id, ts.size(), body.size(), verts.size(),
                wheelsOk ? "tamam" : "BULUNAMADI", srcLen, L);
    std::map<int, int> mc; for (auto& t : ts) mc[t[3]]++;
    static const char* mn[] = {"boya", "cam", "lastik", "jant", "far", "stop", "trim", "koyu", "plaka", "krom", "sinyal"};
    for (auto& [m, c] : mc) std::printf("  %-7s %d\n", mn[m], c);
    std::map<int, std::pair<int, int>> srcStat;
    for (auto& t : tris) { auto& e = srcStat[t.src]; e.first++; e.second = t.mat; }
    for (auto& [si, st] : srcStat) {
        const SrcMat& m = smat[si];
        V3 a{1e9, 1e9, 1e9}, b{-1e9, -1e9, -1e9};
        for (auto& t : tris) if (t.src == si) for (auto& p : t.p) { a.x = std::min(a.x, p.x); a.y = std::min(a.y, p.y); a.z = std::min(a.z, p.z); b.x = std::max(b.x, p.x); b.y = std::max(b.y, p.y); b.z = std::max(b.z, p.z); }
        std::printf("    %3d [%.38s] %5d ucg alan %6.3f rgb %.2f %.2f %.2f a%.2f%s x[%5.2f %5.2f] y[%5.2f %5.2f] z[%4.2f %4.2f] -> %s\n", si, m.name.c_str(), st.first,
                    areaBy[si], m.r, m.g, m.b, m.a, m.blend ? " BLEND" : "", a.x, b.x, a.y, b.y, a.z, b.z, mn[st.second]);
    }
    return wheelsOk ? 0 : 2;
}
