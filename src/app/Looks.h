// ZEHRA KINIK - Arac gorunumu: boya paleti, cila / serit / jant, parcalardan gorunur kit (aero) ve basiklik (suspansiyon).
#pragma once
#include "app/Renderer.h"
#include "game/Career.h"
#include "sim/PartTables.h"

#include <algorithm>

namespace zk {

struct PaintDef { const char* name; unsigned rgb; };
inline const PaintDef* paintPalette() {
    static const PaintDef p[16] = {
        {"BEYAZ", 0xF2F2F0}, {"SIYAH", 0x101114}, {"GUMUS", 0xB8BCC2}, {"ANTRASIT", 0x3A3D44},
        {"KIRMIZI", 0xC81E1E}, {"BORDO", 0x6E1020}, {"TURUNCU", 0xF06A10}, {"SARI", 0xF2C81A},
        {"LIME", 0x8CD228}, {"YESIL", 0x1E6E36}, {"TURKUAZ", 0x18A8A8}, {"MAVI", 0x2A6AD8},
        {"LACIVERT", 0x14224A}, {"MOR", 0x6A2CA0}, {"PEMBE", 0xE85AA0}, {"ALTIN", 0xC8A040},
    };
    return p;
}
constexpr int kPaints = 16;
inline const char* finishName(int f) { static const char* n[4] = {"PARLAK", "METALIK", "MAT", "SEDEF"}; return n[std::clamp(f, 0, 3)]; }
inline const char* stripeName(int s) { static const char* n[4] = {"YOK", "ORTA", "CIFT", "YAN"}; return n[std::clamp(s, 0, 3)]; }
inline float finishGloss(int f) { static const float g[4] = {1.0f, 1.3f, 0.12f, 1.6f}; return g[std::clamp(f, 0, 3)]; }

inline void rgbOf(unsigned c, float o[3]) { o[0] = ((c >> 16) & 255) / 255.f; o[1] = ((c >> 8) & 255) / 255.f; o[2] = (c & 255) / 255.f; }

// Parcalardan gorunur: aero kiti / kanat, suspansiyon alcalmasi (yalniz Tune; rakipler de bu kadarini gosterir)
inline Renderer::CarLook lookOf(const Tune* t) {
    Renderer::CarLook L;
    if (!t) return L;
    // Arka aero satiri -> kit bicimi (Renderer::kitMesh: 3 spoiler, 4 ducktail, 5 GT, 6 yaris, 9 ozel); drag paketi: puruzsuz
    static const int kit[10] = {0, 3, 5, 3, 4, 5, 6, 6, 0, 9};
    L.aero = kit[std::clamp(t->aero, 0, 9)];
    L.aeroFront = t->aeroFront > 0; L.aeroSide = t->aeroSide > 0; L.aeroUnder = t->aeroUnder > 0;
    L.wingH = (float)std::clamp(0.15 + t->custWingN / 2500.0, 0.15, 0.45);
    const auto& st = suspTable();
    L.drop = (float)std::clamp(st[std::clamp(t->susp, 0, (int)st.size() - 1)].lowerMm / 1000.0, -0.03, 0.06);
    return L;
}
inline Renderer::CarLook lookOf(const OwnedCar& oc) {
    Renderer::CarLook L = lookOf(&oc.tune);
    if (oc.paint >= 0 && oc.paint < kPaints) { L.paintOn = true; rgbOf(paintPalette()[oc.paint].rgb, L.paint); }
    L.paintGloss = finishGloss(oc.finish);
    if (oc.finish == 3) for (float& c : L.paint) c = std::min(1.0f, c * 1.08f + 0.03f);   // sedef: hafif acik
    L.stripe = std::clamp(oc.stripe, 0, 3);
    rgbOf(paintPalette()[std::clamp(oc.stripeCol, 0, kPaints - 1)].rgb, L.stripeCol);
    if (oc.rimCol >= 0 && oc.rimCol < kPaints) { L.rimOn = true; rgbOf(paintPalette()[oc.rimCol].rgb, L.rim); }
    return L;
}

} // namespace zk
