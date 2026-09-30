// ZEHRA KINIK - Ortak arayuz yardimcilari (tus, para bicimi)
#pragma once
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

inline void button(Renderer& r, const Rect& b, const std::string& label, Color bg, float scale = 2, Color fg = {1, 1, 1}) {
    r.rect(b.x0, b.y0, b.x1, b.y1, bg);
    r.rect(b.x0, b.y1 - 3, b.x1, b.y1, {bg.r * 0.6f, bg.g * 0.6f, bg.b * 0.6f});   // alt golge
    r.textCentered(b.cx(), b.cy() - 3.5f * scale, label, scale, fg);
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
