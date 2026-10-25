// ZEHRA KINIK - Hazir 3D modellerden (tools/glb2zk ile donusturulmus) arac govdeleri. LowPolyModel.cpp icinde dahil edilir.
// Her model: noktalar (x ileri, y sol, z yukari, m), ucgenler (a, b, c, malzeme), 4 teker parcasi (on sol, on sag, arka
// sol, arka sag; ucgen araligi). Lisans / yazar: docs/model_secim.md ve jenerik (credits) ekrani.
struct ImportedWheel { float x, y, z, r; size_t b, e; };
struct ImportedModel { int id; const float (*v)[3]; size_t nv; const int (*t)[4]; size_t nt; const ImportedWheel* w; };
// GENERATED-INCLUDES (glb2zk ciktilari asagiya eklenir)
#include "car_122.inc"
#include "car_5.inc"
#include "car_12.inc"
// GENERATED-INCLUDES-END
static const ImportedModel kImported[] = {
// GENERATED-TABLE
    {0, nullptr, 0, nullptr, 0, nullptr},
    {122, kImpV_122, sizeof kImpV_122 / sizeof *kImpV_122, kImpT_122, sizeof kImpT_122 / sizeof *kImpT_122, kImpW_122},
    {5, kImpV_5, sizeof kImpV_5 / sizeof *kImpV_5, kImpT_5, sizeof kImpT_5 / sizeof *kImpT_5, kImpW_5},
    {12, kImpV_12, sizeof kImpV_12 / sizeof *kImpV_12, kImpT_12, sizeof kImpT_12 / sizeof *kImpT_12, kImpW_12},
// GENERATED-TABLE-END
};
// Ikiz govdeler (ayni model): R34 GT-T -> GT-R, M3 CSL -> M3
static int importedAlias(int id) { return id == 53 ? 52 : id == 123 ? 122 : id; }
static bool buildImportedModel(const VehicleDef& v, LowPolyMesh& out) {
    const int id = importedAlias(v.id);
    for (const ImportedModel& m : kImported) {
        if (m.id != id || !m.v) continue;
        out = LowPolyMesh{}; out.paintRGB = v.paintRGB; out.outwardWinding = true;
        out.verts.reserve(m.nv); out.tris.reserve(m.nt);
        for (size_t i = 0; i < m.nv; ++i) out.verts.push_back({m.v[i][0], m.v[i][1], m.v[i][2]});
        for (size_t i = 0; i < m.nt; ++i) out.tris.push_back({m.t[i][0], m.t[i][1], m.t[i][2], m.t[i][3]});
        for (int k = 0; k < 4; ++k) out.wheels.push_back({m.w[k].x, m.w[k].y, m.w[k].z, m.w[k].r, m.w[k].b, m.w[k].e});
        return true;
    }
    return false;
}
