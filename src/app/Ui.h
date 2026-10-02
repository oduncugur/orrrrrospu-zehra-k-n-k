// ZEHRA KINIK - Ortak arayuz yardimcilari (tus, para bicimi)
#pragma once
#include <algorithm>
#include <cmath>
#include "app/Renderer.h"

#include <cctype>
#include <cstdio>
#include <string>

namespace zk {

struct Rect {
    float x0, y0, x1, y1;
    bool hit(float x, float y) const { return x >= x0 && x <= x1 && y >= y0 && y <= y1; }
    float cx() const { return (x0 + x1) * 0.5f; }
    float cy() const { return (y0 + y1) * 0.5f; }
};

// Arac sergi stüdyosu (garaj / galeri): koyu duvar, zemin, spot isik havuzu ve tavan isik seritleri. floorY: zemin cizgisi
inline void studio(Renderer& r, float y0, float y1, float floorY) {
    r.gradientV(0, y0, 360, floorY, {0.07f, 0.08f, 0.11f}, {0.13f, 0.14f, 0.18f});
    r.gradientV(0, floorY, 360, y1, {0.20f, 0.21f, 0.25f}, {0.10f, 0.10f, 0.13f});
    for (int k = 0; k < 3; ++k) r.rect(60.0f + k * 90.0f, y0 + 6, 120.0f + k * 90.0f, y0 + 9, {0.85f, 0.88f, 0.95f, 0.5f});   // tavan lambalari
    const int N = 28;                                                                             // spot havuzu (elips, 3 kat)
    for (int k = 0; k < 3; ++k) {
        const float rx = 165.0f - k * 40.0f, ry = (y1 - floorY) * (0.55f - k * 0.13f), cy = floorY + (y1 - floorY) * 0.22f;
        for (int i = 0; i < N; ++i) {
            const float a0 = 6.2831853f * i / N, a1 = 6.2831853f * (i + 1) / N;
            r.tri(180, cy, 180 + rx * std::cos(a0), cy + ry * std::sin(a0), 180 + rx * std::cos(a1), cy + ry * std::sin(a1), {0.75f, 0.78f, 0.85f, 0.06f});
        }
    }
    r.rect(0, floorY, 360, floorY + 1, {0.30f, 0.32f, 0.38f});
}
// Dugme: dikey renk gecisi (ust acik), ince kenar, alt golge; yazi golgeli (okunurluk)
inline void button(Renderer& r, const Rect& b, const std::string& label, Color bg, float scale = 2, Color fg = {1, 1, 1}) {
    auto mul = [&](float k) { return Color{std::min(1.0f, bg.r * k), std::min(1.0f, bg.g * k), std::min(1.0f, bg.b * k), bg.a}; };
    r.rect(b.x0 + 1, b.y0 + 2, b.x1 + 1, b.y1 + 2, {0, 0, 0, 0.35f * bg.a});           // dusen golge
    r.gradientV(b.x0, b.y0, b.x1, b.y1, mul(1.18f), mul(0.92f));
    r.rect(b.x0, b.y0, b.x1, b.y0 + 1, mul(1.45f));                                   // ust parlak kenar
    r.rect(b.x0, b.y1 - 3, b.x1, b.y1, mul(0.6f));                                    // alt kalinlik
    r.rect(b.x0, b.y0, b.x0 + 1, b.y1, mul(0.75f)); r.rect(b.x1 - 1, b.y0, b.x1, b.y1, mul(0.75f));
    const float ty = b.cy() - 3.5f * scale - 1.0f, maxW = b.x1 - b.x0 - 6;   // uzun ceviri: kuculur / kisaltilir
    r.textFit(b.cx() + scale * 0.5f, ty + scale * 0.5f, label, scale, maxW, {0, 0, 0, 0.45f}, true);
    r.textFit(b.cx(), ty, label, scale, maxW, fg, true);
}

// Vektor ikonlar (s: yaricap olcegi). Ortak gorsel dil: ana ekranlarda ikon + etiket kutucuklari.
enum Icon { IconParts, IconSetup, IconGauge, IconPaint, IconDyno, IconRoad, IconGallery, IconStreet, IconSettings, IconMap, IconCash, IconSell, IconRepair };
inline void thickLine(Renderer& r, float x0, float y0, float x1, float y1, float w, Color c) {
    const float dx = x1 - x0, dy = y1 - y0, l = std::max(1e-3f, std::sqrt(dx * dx + dy * dy)), nx = -dy / l * w * 0.5f, ny = dx / l * w * 0.5f;
    r.tri(x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, c); r.tri(x0 + nx, y0 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny, c);
}
inline void ring(Renderer& r, float cx, float cy, float rad, float w, Color c, float a0 = 0, float a1 = 6.2831853f, int seg = 24) {
    for (int i = 0; i < seg; ++i) {
        const float t0 = a0 + (a1 - a0) * i / seg, t1 = a0 + (a1 - a0) * (i + 1) / seg;
        thickLine(r, cx + std::cos(t0) * rad, cy + std::sin(t0) * rad, cx + std::cos(t1) * rad, cy + std::sin(t1) * rad, w, c);
    }
}
inline void icon(Renderer& r, int id, float cx, float cy, float s, Color c) {
    const float w = std::max(2.0f, s * 0.16f);
    switch (id) {
    case IconParts:                                                      // anahtar: sap + agiz (cember, ucu acik)
        thickLine(r, cx - s * 0.75f, cy + s * 0.75f, cx + s * 0.15f, cy - s * 0.15f, w * 1.6f, c);
        r.circle(cx + s * 0.42f, cy - s * 0.42f, s * 0.42f, 16, c);
        r.tri(cx + s * 0.42f, cy - s * 0.42f, cx + s * 0.95f, cy - s * 0.6f, cx + s * 0.6f, cy - s * 0.95f, {0.1f, 0.1f, 0.13f});
        r.circle(cx + s * 0.48f, cy - s * 0.48f, s * 0.17f, 10, {0.1f, 0.1f, 0.13f});
        break;
    case IconSetup:                                                      // ayar surguleri
        for (int k = 0; k < 3; ++k) {
            const float x = cx + (k - 1) * s * 0.6f, ky = cy + s * (k == 0 ? 0.3f : k == 1 ? -0.35f : 0.1f);
            thickLine(r, x, cy - s * 0.8f, x, cy + s * 0.8f, w * 0.7f, c);
            r.rect(x - s * 0.22f, ky - s * 0.13f, x + s * 0.22f, ky + s * 0.13f, c);
        }
        break;
    case IconGauge:                                                      // kadran
        ring(r, cx, cy + s * 0.1f, s * 0.8f, w, c, 2.6f, 2.6f + 4.2f, 20);
        thickLine(r, cx, cy + s * 0.1f, cx + s * 0.5f, cy - s * 0.4f, w, c);
        r.circle(cx, cy + s * 0.1f, s * 0.14f, 10, c);
        break;
    case IconPaint:                                                      // boya damlasi
        r.circle(cx, cy + s * 0.25f, s * 0.5f, 18, c);
        r.tri(cx - s * 0.47f, cy + s * 0.1f, cx + s * 0.47f, cy + s * 0.1f, cx, cy - s * 0.8f, c);
        break;
    case IconDyno:                                                       // guc egrisi
        thickLine(r, cx - s * 0.8f, cy + s * 0.75f, cx + s * 0.8f, cy + s * 0.75f, w * 0.7f, c);
        thickLine(r, cx - s * 0.8f, cy + s * 0.75f, cx - s * 0.8f, cy - s * 0.8f, w * 0.7f, c);
        thickLine(r, cx - s * 0.7f, cy + s * 0.5f, cx - s * 0.15f, cy - s * 0.1f, w, c);
        thickLine(r, cx - s * 0.15f, cy - s * 0.1f, cx + s * 0.35f, cy - s * 0.55f, w, c);
        thickLine(r, cx + s * 0.35f, cy - s * 0.55f, cx + s * 0.75f, cy - s * 0.35f, w, c);
        break;
    case IconRoad:                                                       // perspektif yol
        r.tri(cx - s * 0.25f, cy - s * 0.8f, cx + s * 0.25f, cy - s * 0.8f, cx + s * 0.9f, cy + s * 0.8f, c);
        r.tri(cx - s * 0.25f, cy - s * 0.8f, cx + s * 0.9f, cy + s * 0.8f, cx - s * 0.9f, cy + s * 0.8f, c);
        for (int k = 0; k < 3; ++k) r.rect(cx - s * 0.05f * (1 + k), cy - s * 0.6f + k * s * 0.5f, cx + s * 0.05f * (1 + k), cy - s * 0.35f + k * s * 0.5f, {0.1f, 0.1f, 0.12f});
        break;
    case IconGallery:                                                    // araba
        r.rect(cx - s * 0.9f, cy - s * 0.05f, cx + s * 0.9f, cy + s * 0.4f, c);
        r.tri(cx - s * 0.5f, cy - s * 0.05f, cx - s * 0.25f, cy - s * 0.45f, cx + s * 0.3f, cy - s * 0.45f, c);
        r.tri(cx - s * 0.5f, cy - s * 0.05f, cx + s * 0.3f, cy - s * 0.45f, cx + s * 0.55f, cy - s * 0.05f, c);
        r.circle(cx - s * 0.5f, cy + s * 0.45f, s * 0.22f, 12, {0.08f, 0.08f, 0.1f}); r.circle(cx + s * 0.5f, cy + s * 0.45f, s * 0.22f, 12, {0.08f, 0.08f, 0.1f});
        break;
    case IconStreet:                                                     // hilal (gece)
        r.circle(cx, cy, s * 0.75f, 20, c);
        r.circle(cx + s * 0.35f, cy - s * 0.2f, s * 0.62f, 20, {0.1f, 0.1f, 0.13f});
        break;
    case IconSettings:                                                   // disli
        for (int k = 0; k < 8; ++k) { const float a = 0.785398f * k; thickLine(r, cx + std::cos(a) * s * 0.45f, cy + std::sin(a) * s * 0.45f, cx + std::cos(a) * s * 0.85f, cy + std::sin(a) * s * 0.85f, w * 1.4f, c); }
        r.circle(cx, cy, s * 0.6f, 18, c); r.circle(cx, cy, s * 0.25f, 12, {0.1f, 0.1f, 0.13f});
        break;
    case IconMap:                                                        // konum isareti
        r.circle(cx, cy - s * 0.25f, s * 0.5f, 18, c);
        r.tri(cx - s * 0.45f, cy - s * 0.05f, cx + s * 0.45f, cy - s * 0.05f, cx, cy + s * 0.85f, c);
        r.circle(cx, cy - s * 0.25f, s * 0.2f, 12, {0.1f, 0.1f, 0.13f});
        break;
    case IconCash: r.textCentered(cx, cy - 7, "$", 2, c); break;
    case IconSell: r.textCentered(cx, cy - 7, "%", 2, c); break;
    case IconRepair:                                                     // tornavida + anahtar (capraz)
        thickLine(r, cx - s * 0.7f, cy - s * 0.7f, cx + s * 0.7f, cy + s * 0.7f, w * 1.3f, c);
        thickLine(r, cx + s * 0.7f, cy - s * 0.7f, cx - s * 0.7f, cy + s * 0.7f, w * 1.3f, c);
        break;
    default: break;
    }
}
// Kutucuk: ikon ustte, etiket altta (ana menu izgarasi). badge: sag ustte kucuk uyari / bilgi (bos: yok)
inline void tile(Renderer& r, const Rect& b, int ic, const std::string& label, Color bg, const std::string& badge = "") {
    button(r, b, "", bg, 1);
    icon(r, ic, b.cx(), b.y0 + (b.y1 - b.y0) * 0.40f, std::min(b.x1 - b.x0, b.y1 - b.y0) * 0.22f, {0.95f, 0.95f, 0.97f});
    r.textFit(b.cx(), b.y1 - 17, label, 1, b.x1 - b.x0 - 6, {1, 1, 1}, true);
    if (!badge.empty()) {
        const float bw = r.textWidth(badge, 1) + 8;
        r.rect(b.x1 - bw - 2, b.y0 + 3, b.x1 - 3, b.y0 + 15, {0.9f, 0.2f, 0.15f});
        r.text(b.x1 - bw + 2, b.y0 + 6, badge, 1, {1, 1, 1});
    }
}

inline std::string money(long v) {
    std::string d = std::to_string(v < 0 ? -v : v), o;
    for (size_t i = 0; i < d.size(); ++i) { if (i && (d.size() - i) % 3 == 0) o += ','; o += d[i]; }
    return std::string(v < 0 ? "-$" : "$") + o;
}

inline std::string upper(const std::string& s) { std::string o = s; for (char& c : o) c = (char)std::toupper((unsigned char)c); return o; }

const Color kUiBg{0.07f, 0.08f, 0.10f}, kUiPanel{0.12f, 0.13f, 0.16f}, kUiBtn{0.2f, 0.22f, 0.28f},
            kUiOrange{0.85f, 0.35f, 0.08f}, kUiGreen{0.1f, 0.55f, 0.2f}, kUiRed{0.65f, 0.12f, 0.12f},
            kUiGold{0.95f, 0.75f, 0.2f}, kUiText{0.8f, 0.8f, 0.85f}, kUiDim{0.55f, 0.55f, 0.6f};

} // namespace zk
