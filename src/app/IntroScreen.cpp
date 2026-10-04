// ZEHRA KINIK - Acilis sinematigi (yatay 640x360): gece, yagmur, islak asfalt (ters yansima), dort cekim.
// Ilk acilis: vitrin araci (E46) tam surum (~7 s). Sonraki acilislar: oyuncunun araci, kisa kahraman cekimi (~3.5 s).
// Dokun / tus: atla. Sonunda garaja (ana menu) gecer.
#include "app/Screens.h"
#include "app/App.h"
#include "app/Looks.h"
#include "app/Ui.h"

#include <algorithm>
#include <cmath>

namespace zk {

namespace {
constexpr int kW = 640, kH = 360;
constexpr int kShowcaseCar = 122;                 // BMW M3 E46 (en iyi model)
float hashI(int i) { unsigned x = (unsigned)i * 2654435761u; x ^= x >> 13; x *= 0x5bd1e995u; x ^= x >> 15; return (x & 0xFFFF) / 65535.0f; }
float ease(float x) { x = std::clamp(x, 0.0f, 1.0f); return x * x * (3.0f - 2.0f * x); }
struct P2 { float x, y, w; bool ok; };
P2 proj(const Mat4& vp, float x, float y, float z) {
    const float cx = vp.m[0] * x + vp.m[4] * y + vp.m[8] * z + vp.m[12];
    const float cy = vp.m[1] * x + vp.m[5] * y + vp.m[9] * z + vp.m[13];
    const float cw = vp.m[3] * x + vp.m[7] * y + vp.m[11] * z + vp.m[15];
    if (cw < 0.1f) return {0, 0, cw, false};
    return {(cx / cw * 0.5f + 0.5f) * kW, (1.0f - (cy / cw * 0.5f + 0.5f)) * kH, cw, true};
}
} // namespace

IntroScreen::IntroScreen(App& app) : app_(app) {
    full_ = !app_.settings.introSeen;
    if (full_ || app_.career.cars.empty()) { carId_ = kShowcaseCar; look_.paintOn = true; look_.paint[0] = 0.62f; look_.paint[1] = 0.04f; look_.paint[2] = 0.06f; look_.paintGloss = 1.3f; }
    else { carId_ = app_.career.car().carId; look_ = lookOf(app_.career.car()); }
    t_ = full_ ? 0.0 : 4.4;                                       // kisa surum: dogrudan kahraman cekimi
    dur_ = full_ ? 7.2 : 8.0;
    if (full_) app_.setVoiceTuned(0, carId_, nullptr);
    else app_.setVoiceTuned(0, carId_, &app_.career.car().tune);
    app_.setVoice(1, nullptr);
    const VehicleDef* v = findVehicle(carId_);
    halfL_ = v ? (float)v->lengthM * 0.5f : 2.25f;
    red_ = 7000.0;
    if (v) red_ = buildEngineSpec(*v).redlineRpm;
}

void IntroScreen::finish() {
    if (done_) return;
    done_ = true;
    app_.settings.introSeen = true;
    app_.saveSettings();
    app_.goGarage();
}

// Arac konumu (x ileri, m) ve hizi: yaklasma -> gecis -> yavaslayip durma
float IntroScreen::carX(double t) const {
    if (t < 4.4) return (float)(-30.0 + 13.0 * t);                // ~47 km/h ... sabit (sinematik hiz)
    const double u = std::min(1.0, (t - 4.4) / 1.6);
    return (float)(-30.0 + 13.0 * 4.4 + 13.0 * 1.6 * (u - 0.5 * u * u));   // yavaslar, durur
}

void IntroScreen::update(double dt) {
    if (done_) return;
    t_ += dt;
    // Motor sesi: yaklasmada yukselen devir, arka cekimde kesiciye vurma (alev), kahraman cekiminde gaz patlamalari
    double rpm = 1000, thr = 0.1; bool cut = false;
    if (t_ < 1.8) { rpm = 2500 + 1500 * t_; thr = 0.6; }
    else if (t_ < 3.2) { rpm = 4500 + 1600 * (t_ - 1.8); thr = 1.0; }
    else if (t_ < 4.4) { const double ph = std::fmod(t_ - 3.2, 0.4); rpm = red_ - (ph < 0.2 ? 0 : 900); thr = ph < 0.2 ? 1.0 : 0.0; cut = ph < 0.08; }
    else { const double ph = t_ - 4.4; rpm = ph < 1.6 ? red_ * (0.5 - 0.25 * ph / 1.6) : 950.0 + (std::fmod(ph, 1.1) < 0.25 ? 2600.0 : 0.0); thr = std::fmod(ph, 1.1) < 0.25 ? 0.8 : 0.05; }
    app_.voice(0, std::min(rpm, red_), thr, cut, t_ < 4.4, 1.0f);
    spin_ += (float)(std::min(13.0, (carX(t_ + dt) - carX(t_)) / std::max(dt, 1e-4)) / 0.32 * dt);
    if (t_ >= dur_) finish();
}

void IntroScreen::render(Renderer& r) {
    r.begin(kW, kH, {0.01f, 0.01f, 0.02f});
    const float t = (float)t_;
    // ---- gokyuzu: gece, mor-lacivert; uzak sehir silueti (pencereler) ----
    const float horizon = 190.0f;
    r.gradientV(0, 0, kW, horizon, {0.02f, 0.02f, 0.06f}, {0.16f, 0.08f, 0.20f});
    for (int k = 0; k < 46; ++k) {
        const float bw = 10 + 22 * hashI(k * 3), bh = 30 + 110 * hashI(k * 7) * hashI(k * 5 + 1);
        const float x = std::fmod(k * 17.3f - t * 4.0f + 1000.0f, (float)kW + 40.0f) - 20.0f;
        r.rect(x, horizon - bh, x + bw, horizon, {0.05f, 0.04f, 0.09f});
        for (int w = 0; w < 8; ++w) if (hashI(k * 31 + w) > 0.62f) {
            const float wx = x + 2 + (w % 3) * (bw - 4) / 3.0f, wy = horizon - bh + 6 + (w / 3) * 11.0f;
            if (wy < horizon - 4) r.rect(wx, wy, wx + 2, wy + 3, {1.0f, 0.78f, 0.45f, 0.75f});
        }
    }
    // ---- islak asfalt: koyu, ufukta isik yansimasi (dikey cizgiler) ----
    r.gradientV(0, horizon, kW, kH, {0.06f, 0.05f, 0.09f}, {0.015f, 0.015f, 0.025f});
    for (int k = 0; k < 30; ++k) {
        const float x = std::fmod(k * 23.7f - t * 4.0f + 1000.0f, (float)kW);
        const Color c = hashI(k) > 0.5f ? Color{1.0f, 0.6f, 0.3f, 0.10f} : Color{0.4f, 0.5f, 1.0f, 0.08f};
        r.rect(x, horizon, x + 2 + 3 * hashI(k * 9), horizon + 40 + 120 * hashI(k * 13), c);
    }
    // ---- kamera (cekime gore) ----
    const float cx = carX(t_);
    float ex, ey, ez, tx, ty, tz, fov = 0.75f;
    if (t < 1.8f) { ex = 7.0f; ey = 0.45f; ez = 2.6f; tx = cx; ty = 0.6f; tz = 0.0f; fov = 0.55f; }                         // yaklasma
    else if (t < 3.2f) { const float u = (t - 1.8f) / 1.4f, wx = cx + halfL_ - 0.8f - 1.4f * u; ex = wx + 0.6f; ey = 0.30f; ez = 3.4f; tx = wx; ty = 0.40f; tz = 0.6f; fov = 0.42f; }   // teker yani
    else if (t < 4.4f) { ex = cx - 4.6f; ey = 0.38f; ez = 1.5f; tx = cx; ty = 0.62f; tz = 0.0f; fov = 0.7f; }                // arka
    else { const float a = 0.6f + 0.55f * (t - 4.4f); ex = cx + 6.2f * std::cos(a); ey = 1.35f; ez = 6.2f * std::sin(a); tx = cx; ty = 1.05f; tz = 0.0f; fov = 0.6f; }   // kahraman
    const Mat4 P = matPerspective(fov, (float)kW / kH, 0.1f, 200.0f), V = matLookAt(ex, ey, ez, tx, ty, tz), VP = matMul(P, V);
    const Mat4 M = matTranslate(cx, 0, 0);
    // ---- yol cizgileri (perspektif): kesik beyaz serit, kenar ----
    for (int k = -6; k < 30; ++k) {
        const float x0 = std::floor(cx / 6.0f) * 6.0f + k * 6.0f;
        const P2 a = proj(VP, x0, 0, -1.9f), b = proj(VP, x0 + 3, 0, -1.9f), c = proj(VP, x0 + 3, 0, -2.05f), d = proj(VP, x0, 0, -2.05f);
        if (a.ok && b.ok && c.ok && d.ok) { r.tri(a.x, a.y, b.x, b.y, c.x, c.y, {0.8f, 0.8f, 0.75f, 0.5f}); r.tri(a.x, a.y, c.x, c.y, d.x, d.y, {0.8f, 0.8f, 0.75f, 0.5f}); }
    }
    r.setSceneLight(0.45f, 0.45f);
    // ---- islak zemin yansimasi: ters arac, yari saydam ----
    {
        Mat4 flip{}; flip.m[0] = 1; flip.m[5] = -1; flip.m[10] = 1; flip.m[15] = 1;
        Renderer::CarLook L = look_; L.alpha = 0.22f;
        r.setCarLook(L);
        r.drawCar(carId_, 0, 0, kW, kH, P, V, matMul(M, flip), spin_);
    }
    r.setSceneLight(0.45f, 0.45f);
    r.setCarLook(look_);
    r.drawCar(carId_, 0, 0, kW, kH, P, V, M, spin_);
    // ---- farlar (parlama) / stop / egzoz alevi ----
    for (float sz : {-0.55f, 0.55f}) {
        const P2 f = proj(VP, cx + halfL_ - 0.05f, 0.62f, sz);
        if (f.ok && f.w > 0.5f) {
            const float k = std::clamp(9.0f / f.w, 0.4f, 6.0f);
            r.circle(f.x, f.y, 26 * k, 18, {1.0f, 0.95f, 0.8f, 0.10f});
            r.circle(f.x, f.y, 9 * k, 14, {1.0f, 0.97f, 0.9f, 0.35f});
            r.circle(f.x, f.y, 3 * k, 10, {1.0f, 1.0f, 1.0f, 0.95f});
            r.rect(f.x - 1.5f * k, f.y + 4 * k, f.x + 1.5f * k, f.y + 60 * k, {1.0f, 0.95f, 0.8f, 0.08f});   // asfaltta far yansimasi
        }
        const P2 s = proj(VP, cx - halfL_ + 0.05f, 0.82f, sz * 1.1f);
        if (s.ok && s.w > 0.5f) { const float k = std::clamp(9.0f / s.w, 0.4f, 6.0f); r.circle(s.x, s.y, 7 * k, 12, {1.0f, 0.1f, 0.08f, 0.35f}); }
    }
    if (t > 3.2f && t < 4.4f && std::fmod(t - 3.2f, 0.4f) < 0.12f) {   // kesicide egzoz alevi
        const P2 e = proj(VP, cx - halfL_ - 0.1f, 0.3f, 0.45f);
        if (e.ok) { const float k = std::clamp(9.0f / e.w, 0.5f, 8.0f); r.circle(e.x, e.y, 14 * k, 12, {1.0f, 0.5f, 0.1f, 0.6f}); r.circle(e.x, e.y, 6 * k, 10, {0.6f, 0.75f, 1.0f, 0.9f}); }
    }
    // ---- yagmur cizgileri ----
    for (int k = 0; k < 140; ++k) {
        const float sp = 520.0f + 260.0f * hashI(k * 5);
        const float x = std::fmod(hashI(k) * (kW + 120) - t * 140.0f + 2000.0f, (float)kW + 120) - 60;
        const float y = std::fmod(hashI(k * 3) * kH + t * sp, (float)kH + 40) - 20;
        r.tri(x, y, x + 1.2f, y, x - 3.0f, y + 14.0f, {0.75f, 0.8f, 0.95f, 0.22f});
    }
    // ---- sinematik bantlar + logo ----
    r.rect(0, 0, kW, 34, {0, 0, 0, 1}); r.rect(0, kH - 34, kW, kH, {0, 0, 0, 1});
    const float lt = t - 5.0f;
    if (lt > 0) {
        if (lt < 0.25f) r.rect(0, 0, kW, kH, {1, 1, 1, 0.8f * (1.0f - lt / 0.25f)});   // flas
        const float s = 5.0f + 3.0f * std::max(0.0f, 0.15f - lt) / 0.15f;               // carpma: buyukten oturur
        const float a = ease(lt / 0.15f);
        r.textCentered(kW * 0.5f + 3, 62 + 3, "ZEHRA KINIK", s, {0, 0, 0, 0.6f * a});
        r.textCentered(kW * 0.5f, 62, "ZEHRA KINIK", s, {1.0f, 0.78f, 0.15f, a});
        if (lt > 0.5f) r.textCentered(kW * 0.5f, 112, "SOKAKLAR SENIN", 2, {1, 1, 1, ease((lt - 0.5f) / 0.4f) * 0.9f});
    }
    if (t > 0.6f) r.text(kW - 92, kH - 24, "DOKUN: GEC", 1, {1, 1, 1, 0.45f});
    // ---- giris / cikis karartma ----
    if (t_ < 0.5 && full_) r.rect(0, 0, kW, kH, {0, 0, 0, 1.0f - ease(t / 0.5f)});
    if (t_ > dur_ - 0.45) r.rect(0, 0, kW, kH, {0, 0, 0, ease((float)(t_ - (dur_ - 0.45)) / 0.45f)});
    r.flush2D();
}

void IntroScreen::pointerDown(int, float, float) { if (t_ > 0.3 || !full_) finish(); }
void IntroScreen::key(Key, bool down) { if (down) finish(); }

} // namespace zk
