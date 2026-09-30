#include "TireAudio.h"

#include <algorithm>
#include <cmath>

namespace zk {

namespace { constexpr double kPi = 3.14159265358979; }

TireAudio::TireAudio(int sampleRate) : fs_(sampleRate) {
    // Kaucuk sirt/yanak modlari: oranlar inharmonik, ust modlar daha sonumlu
    const double ratio[kModes] = {1.0, 1.47, 2.09, 2.73, 3.61};
    const double q[kModes]     = {22, 18, 14, 11, 8};
    const double g[kModes]     = {1.0, 0.7, 0.45, 0.3, 0.18};
    for (int i = 0; i < kModes; ++i) { m_[i].f = ratio[i]; m_[i].q = q[i]; m_[i].g = g[i]; }
}

double TireAudio::rnd() {
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
    return (rng_ & 0xFFFF) / 32768.0 - 1.0;
}

void TireAudio::render(float* out, int n, double slip, float gain) {
    // Ciglik: kucuk-orta kaymada (kalkis, viraj, fren); cok buyuk kaymada kaucuk erir, ciglik azalir
    const double sq = std::clamp((slip - 1.0) / 4.0, 0.0, 1.0) * (1.0 - 0.75 * std::clamp((slip - 9.0) / 10.0, 0.0, 1.0));
    const double burnT = std::clamp((slip - 9.0) / 12.0, 0.0, 1.0);
    // Temel mod ~ 700-1100 Hz; kayma arttikca yapis-kay hizlanir
    const double f0 = 700.0 + 22.0 * std::min(slip, 18.0);
    if (std::fabs(f0 - lastF_) > 2.0) {
        lastF_ = f0;
        for (Res& r : m_) {
            const double w = 2 * kPi * std::min(f0 * r.f, 0.45 * fs_) / fs_, al = std::sin(w) / (2 * r.q), a0 = 1 + al;
            r.b0 = al / a0; r.a1 = -2 * std::cos(w) / a0; r.a2 = (1 - al) / a0;
        }
    }
    const double kEnv = 1.0 - std::exp(-1.0 / (0.02 * fs_)), kBurn = 1.0 - std::exp(-1.0 / (0.08 * fs_));
    const double kJit = 1.0 - std::exp(-1.0 / (0.03 * fs_)), kAm = 1.0 - std::exp(-1.0 / (0.008 * fs_));
    const double kFl = 1.0 - std::exp(-1.0 / (0.05 * fs_));
    // Burnout ugultusu: ~250-1800 Hz bant (iki kutuplu alcak + yuksek geciren)
    const double lpA = 1.0 - std::exp(-2 * kPi * 900.0 / fs_), hpA = 1.0 - std::exp(-2 * kPi * 150.0 / fs_);
    const double hsA = 1.0 - std::exp(-2 * kPi * 3500.0 / fs_), crackDecay = std::exp(-1.0 / (0.0006 * fs_));
    for (int i = 0; i < n; ++i) {
        env_ += (sq - env_) * kEnv;
        burn_ += (burnT - burn_) * kBurn;
        const double x = rnd();
        jit_ += (x - jit_) * kJit;
        amJ_ += (rnd() - amJ_) * kAm;
        // Yapis-kay darbeleri: periyodu %35 titreyen gevseme osilatoru (temel modun biraz altinda)
        ph_ += f0 * 0.93 * (1.0 + 1.2 * jit_ + 0.35 * x) / fs_;
        double exc = 0.08 * x;                               // surekli surtunme tabani
        if (ph_ >= 1.0) { ph_ -= std::floor(ph_); exc += 1.0 + 0.4 * rnd(); }
        double sig = 0;
        for (Res& r : m_) {
            const double y = r.b0 * exc - r.a1 * r.y1 - r.a2 * r.y2;
            r.y2 = r.y1; r.y1 = y; sig += y * r.g;
        }
        const double am = std::clamp(0.7 + 2.5 * amJ_, 0.15, 1.3);
        double o = 6.0 * sig * am * env_;
        if (burn_ > 1e-4) {
            // Burnout = kaucugun parcalanmasi: (1) "kizartma" cizirtisi: 1-6 kHz hisirti, gaz dalgali
            // (2) citirti: kopan kaucuk/tas taneleri -> rastgele kisa darbeler (Poisson ~350/s)
            // (3) alt ugultu: 150-900 Hz yuvarlanma kavurmasi
            // (4) aralikli ciglik: yukaridaki modlar yavas acilip kapanan kapi ile tekrar uyarilir
            flut_ += (rnd() - flut_) * kFl;
            r1_ += (x - r1_) * lpA; r2_ += (r1_ - r2_) * lpA;          // alcak: ~1.1 kHz
            rb_ += (r2_ - rb_) * hpA;
            const double hiss = x - r1_;                                 // ~>1 kHz
            hs_ += (hiss - hs_) * hsA;                                   // ust sinir ~6 kHz
            double crack = 0;
            if (rnd() > 1.0 - 2.0 * 350.0 / fs_) crackEnv_ = 0.6 + 0.4 * rnd();
            crack = crackEnv_ * rnd(); crackEnv_ *= crackDecay;
            const double gate = std::clamp(0.5 + 4.0 * flut_, 0.0, 1.0);
            hs2_ += (hs_ + 0.5 * crack - hs2_) * hsA;                  // 2. kutup: cizirti yumusak
            o += burn_ * (1.1 * hs2_ * std::clamp(0.75 + 2.5 * flut_, 0.35, 1.3)
                        + 1.1 * (r2_ - rb_) + 2.5 * sig * gate);
        }
        out[i] += (float)(o * gain);
    }
}

} // namespace zk
