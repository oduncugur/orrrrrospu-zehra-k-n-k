#include "Gauges.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace zk {

namespace {
constexpr float kPi = 3.14159265f;
// Kadran acisi: sol alt (225 derece) -> sag alt (-45 derece), saat yonunde; ekran y asagi
float dialAng(float f) { return (225.0f - 270.0f * std::clamp(f, 0.0f, 1.05f)) * kPi / 180.0f; }
void radial(Renderer& r, float cx, float cy, float a, float r0, float r1, float w, Color c) {   // kalinlikli cizgi
    const float ca = std::cos(a), sa = -std::sin(a), nx = -sa * w * 0.5f, ny = ca * w * 0.5f;
    const float x0 = cx + ca * r0, y0 = cy + sa * r0, x1 = cx + ca * r1, y1 = cy + sa * r1;
    r.tri(x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, c);
    r.tri(x0 + nx, y0 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny, c);
}
void arcBand(Renderer& r, float cx, float cy, float r0, float r1, float f0, float f1, Color c) {
    const int n = std::max(2, (int)((f1 - f0) * 40));
    for (int i = 0; i < n; ++i) {
        const float a0 = dialAng(f0 + (f1 - f0) * i / n), a1 = dialAng(f0 + (f1 - f0) * (i + 1) / n);
        const float p0x = cx + std::cos(a0) * r0, p0y = cy - std::sin(a0) * r0, p1x = cx + std::cos(a0) * r1, p1y = cy - std::sin(a0) * r1;
        const float q0x = cx + std::cos(a1) * r0, q0y = cy - std::sin(a1) * r0, q1x = cx + std::cos(a1) * r1, q1y = cy - std::sin(a1) * r1;
        r.tri(p0x, p0y, p1x, p1y, q1x, q1y, c); r.tri(p0x, p0y, q1x, q1y, q0x, q0y, c);
    }
}
void face(Renderer& r, float cx, float cy, float R) {
    r.circle(cx, cy + 2, R + 3, 32, {0, 0, 0, 0.35f});                // golge
    r.circle(cx, cy, R + 3, 32, {0.55f, 0.56f, 0.6f});                 // krom cerceve
    r.circle(cx, cy, R, 32, {0.05f, 0.05f, 0.07f, 0.92f});
}
void needle(Renderer& r, float cx, float cy, float R, float f, Color c) {
    const float a = dialAng(f), ca = std::cos(a), sa = -std::sin(a), w = std::max(1.5f, R * 0.045f);
    const float tx = cx + ca * R * 0.86f, ty = cy + sa * R * 0.86f, bx = cx - ca * R * 0.14f, by = cy - sa * R * 0.14f;
    r.tri(bx - sa * w, by + ca * w, tx, ty, bx + sa * w, by - ca * w, c);
    r.circle(cx, cy, std::max(3.0f, R * 0.09f), 12, {0.75f, 0.75f, 0.78f});
}
float txtScale(float R) { return R >= 60 ? 2.0f : 1.0f; }
}

const char* gaugeName(int style) {
    static const char* const n[GaugeStyles] = {"FABRIKA", "ANALOG KADRAN", "DIJITAL EKRAN", "ANALOG + DIJITAL"};
    return n[std::clamp(style, 0, GaugeStyles - 1)];
}

void drawAnalogTach(Renderer& r, float cx, float cy, float R, const GaugeData& d) {
    face(r, cx, cy, R);
    const int kMax = (int)std::ceil(d.redline * 1.1f / 1000.0f);         // ust sinir: 1000'e yuvarli
    const float full = kMax * 1000.0f;
    arcBand(r, cx, cy, R * 0.78f, R * 0.92f, d.redline / full, 1.0f, {0.85f, 0.12f, 0.1f});   // kirmizi bolge
    for (int k = 0; k <= kMax * 2; ++k) {
        const float f = k / (kMax * 2.0f);
        const bool major = k % 2 == 0;
        radial(r, cx, cy, dialAng(f), R * (major ? 0.74f : 0.82f), R * 0.93f, major ? 2.0f : 1.0f, {0.92f, 0.92f, 0.92f});
        if (major && R >= 34) {
            const float a = dialAng(f), lr = R * 0.58f;
            r.textCentered(cx + std::cos(a) * lr, cy - std::sin(a) * lr - 3.5f, std::to_string(k / 2), 1, {0.9f, 0.9f, 0.9f});
        }
    }
    r.textRaw(cx - 10, cy + R * 0.30f, "X1000", 1, {0.55f, 0.55f, 0.6f});
    r.textCentered(cx, cy + R * 0.48f, d.gear, txtScale(R) + (R >= 40 ? 1 : 0), d.gearCol);
    const bool shift = d.rpm > d.shiftRpm - 150 && std::fmod(d.t, 0.14) < 0.07;
    if (shift) r.circle(cx, cy - R * 0.36f, R * 0.08f, 10, {0.3f, 0.6f, 1.0f});   // vites isigi
    needle(r, cx, cy, R, d.rpm / full, {1.0f, 0.35f, 0.15f});
}

void drawAnalogSpeedo(Renderer& r, float cx, float cy, float R, const GaugeData& d) {
    face(r, cx, cy, R);
    const int step = d.speedMax > 200 ? 40 : 20;
    const float full = std::ceil(d.speedMax / step) * step;
    for (int v = 0; v <= (int)full; v += step / 2) {
        const float f = v / full;
        const bool major = v % step == 0;
        radial(r, cx, cy, dialAng(f), R * (major ? 0.74f : 0.82f), R * 0.93f, major ? 2.0f : 1.0f, {0.92f, 0.92f, 0.92f});
        if (major && R >= 34 && (v / step) % (R >= 55 ? 1 : 2) == 0) {
            const float a = dialAng(f), lr = R * 0.58f;
            r.textCentered(cx + std::cos(a) * lr, cy - std::sin(a) * lr - 3.5f, std::to_string(v), 1, {0.9f, 0.9f, 0.9f});
        }
    }
    char b[16]; std::snprintf(b, sizeof b, "%.0f", d.speed);
    r.textCentered(cx, cy + R * 0.32f, b, txtScale(R), {1, 1, 1});
    r.textCentered(cx, cy + R * 0.32f + 9.0f * txtScale(R), d.unit, 1, {0.55f, 0.55f, 0.6f});
    needle(r, cx, cy, R, d.speed / full, {1.0f, 0.35f, 0.15f});
}

void drawDigitalCluster(Renderer& r, float x0, float y0, float x1, float y1, const GaugeData& d, bool withTach) {
    const float w = x1 - x0, h = y1 - y0;
    r.rect(x0 - 2, y0 - 2, x1 + 2, y1 + 2, {0.45f, 0.46f, 0.5f});
    r.rect(x0, y0, x1, y1, {0.02f, 0.05f, 0.06f, 0.94f});               // LCD
    const Color lcd{0.35f, 1.0f, 0.85f};
    float ty = y0 + 4;
    if (withTach) {                                                     // segment devir seridi + vites isigi
        const int n = 24;
        const float full = d.redline * 1.05f, sw = (w - 8) / n;
        for (int i = 0; i < n; ++i) {
            const float segRpm = full * (i + 1) / n;
            Color c = segRpm > d.redline ? Color{1.0f, 0.2f, 0.15f} : segRpm > d.shiftRpm - 800 ? Color{1.0f, 0.75f, 0.1f} : lcd;
            if (d.rpm < segRpm - full / n) c = {c.r * 0.12f, c.g * 0.12f, c.b * 0.12f};
            const float sh = h * 0.16f * (0.5f + 0.5f * (i + 1) / n);  // yukselen seritler
            r.rect(x0 + 4 + i * sw, ty + h * 0.16f - sh, x0 + 4 + (i + 1) * sw - 1.5f, ty + h * 0.16f, c);
        }
        if (d.rpm > d.shiftRpm - 150 && std::fmod(d.t, 0.14) < 0.07) r.rect(x0, y0 - 5, x1, y0 - 2, {0.3f, 0.6f, 1.0f});
        ty += h * 0.16f + 4;
    }
    const float sc = std::clamp(std::min(std::floor((y1 - ty - 4) / 9.0f), std::floor((w - 50.0f) / 30.0f)), 1.0f, 6.0f);   // hiz + birim + vites sigsin
    char b[16]; std::snprintf(b, sizeof b, "%3.0f", d.speed);
    r.text(x0 + 6, ty + 2, b, sc, lcd);
    r.text(x0 + 6 + r.textWidth(b, sc) + 4, ty + 2, d.unit, 1, {0.2f, 0.6f, 0.5f});
    r.text(x1 - 6 - r.textWidth(d.gear, sc), ty + 2, d.gear, sc, d.gearCol);
    if (withTach) { std::snprintf(b, sizeof b, "%5.0f", d.rpm); r.text(x0 + 6 + r.textWidth("000", sc) + 4, ty + 2 + 9 * sc - 9, b, 1, {0.2f, 0.6f, 0.5f}); }
}

void drawBoostGauge(Renderer& r, float cx, float cy, float R, const GaugeData& d) {
    face(r, cx, cy, R);
    const float lo = -1.0f, hi = std::max(1.0f, std::ceil(d.boostMax + 0.4f));
    auto f = [&](float b) { return (b - lo) / (hi - lo); };
    arcBand(r, cx, cy, R * 0.80f, R * 0.92f, 0.0f, f(0.0f), {0.25f, 0.45f, 0.85f});   // vakum
    arcBand(r, cx, cy, R * 0.80f, R * 0.92f, f(0.0f), 1.0f, {0.9f, 0.55f, 0.1f});   // basinc
    for (int k = (int)lo; k <= (int)hi; ++k) radial(r, cx, cy, dialAng(f((float)k)), R * 0.66f, R * 0.93f, 1.5f, {0.92f, 0.92f, 0.92f});
    char b[16]; std::snprintf(b, sizeof b, "%+.1f", d.boost);
    r.textCentered(cx, cy + R * 0.30f, b, 1, d.boost > 0 ? Color{1.0f, 0.7f, 0.2f} : Color{0.5f, 0.7f, 1.0f});
    r.textCentered(cx, cy + R * 0.30f + 9, "BAR", 1, {0.55f, 0.55f, 0.6f});
    needle(r, cx, cy, R, f(std::clamp(d.boost, lo, hi)), {1.0f, 0.35f, 0.15f});
}

void drawGaugeCluster(Renderer& r, float x0, float y0, float x1, float y1, int style, bool boostGauge, const GaugeData& d) {
    const float w = x1 - x0, h = y1 - y0;
    const bool boost = boostGauge && d.boostMax > 0.0f;
    const float R = std::min(h * 0.5f - 3.0f, w * (boost ? 0.21f : 0.24f));
    const float cy = y0 + h * 0.5f;
    switch (style) {
    case GaugeAnalog:
        drawAnalogSpeedo(r, x0 + R + 3, cy, R, d);
        drawAnalogTach(r, x1 - R - 3, cy, R, d);
        if (boost) drawBoostGauge(r, (x0 + x1) * 0.5f, cy + R * 0.35f, R * 0.5f, d);
        break;
    case GaugeDigital:
        drawDigitalCluster(r, x0 + (boost ? R * 1.1f + 6 : 0), y0 + 6, x1, y1, d, true);
        if (boost) drawBoostGauge(r, x0 + R * 0.55f + 3, cy, R * 0.55f, d);
        break;
    case GaugeBoth:
        drawAnalogTach(r, x0 + R + 3, cy, R, d);
        drawDigitalCluster(r, x0 + 2 * R + 12, y0 + h * 0.18f, x1, boost ? cy + 4 : y1 - h * 0.12f, d, false);
        if (boost) drawBoostGauge(r, x1 - R * 0.45f - 3, y1 - R * 0.45f - 2, R * 0.45f, d);
        break;
    default: break;
    }
}

} // namespace zk
