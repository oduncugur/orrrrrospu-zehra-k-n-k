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
