#include "TireAudio.h"

#include <algorithm>
#include <cmath>

namespace zk {

namespace { constexpr double kPi = 3.14159265358979; }

// RBJ biquad'lari
void TireAudio::Bq::bandpass(double f, double q, int fs) {
    const double w = 2 * kPi * f / fs, al = std::sin(w) / (2 * q), a0 = 1 + al;
    b0 = al / a0; b1 = 0; b2 = -al / a0; a1 = -2 * std::cos(w) / a0; a2 = (1 - al) / a0;
}
void TireAudio::Bq::lowpass(double f, double q, int fs) {
    const double w = 2 * kPi * f / fs, al = std::sin(w) / (2 * q), c = std::cos(w), a0 = 1 + al;
    b0 = (1 - c) / 2 / a0; b1 = (1 - c) / a0; b2 = b0; a1 = -2 * c / a0; a2 = (1 - al) / a0;
}
void TireAudio::Bq::highpass(double f, double q, int fs) {
    const double w = 2 * kPi * f / fs, al = std::sin(w) / (2 * q), c = std::cos(w), a0 = 1 + al;
    b0 = (1 + c) / 2 / a0; b1 = -(1 + c) / a0; b2 = b0; a1 = -2 * c / a0; a2 = (1 - al) / a0;
}
double TireAudio::Bq::run(double x) {
    const double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1; x1 = x; y2 = y1; y1 = y;
    return y;
}

TireAudio::TireAudio(int sampleRate) : fs_(sampleRate) {
    scrubHp_.highpass(900.0, 0.7, fs_);      // surtunme hisirtisi: 0.9-4.2 kHz
    scrubLp_.lowpass(4200.0, 0.7, fs_);
    rumble_.bandpass(170.0, 1.8, fs_);       // dis blogu ezilmesi 110-240 Hz
    thump_.bandpass(85.0, 3.5, fs_);         // gövde vuruntusu
    f1_.bandpass(880.0, 4.0, fs_);           // gövde / kusak: kalin "havlama"
    f2_.bandpass(1850.0, 4.8, fs_);          // dis blogu bosalmasi: asil ciglik
    f3_.bandpass(3300.0, 3.0, fs_);          // asfalt isirmasi
    outLp_.lowpass(4400.0, 0.7, fs_);
    outHp_.highpass(130.0, 0.7, fs_);
}

double TireAudio::rnd() {
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
    return (rng_ & 0xFFFF) / 32768.0 - 1.0;
}

void TireAudio::render(float* out, int n, double slip, float gain, double lockSpeed) {
    const bool locked = lockSpeed > 1.0;
    // Siddetler: ciglik (hafif-orta kayma), burnout (buyuk kayma), kilit (fren)
    const double sq = locked ? 0.0 : std::clamp((slip - 1.0) / 3.5, 0.0, 1.0);
    const double burnT = locked ? 0.0 : std::clamp((slip - 7.0) / 10.0, 0.0, 1.0);
    const double lockT = locked ? std::clamp(lockSpeed / 4.0, 0.0, 1.0) : 0.0;
    const double uT = std::clamp((slip - 1.0) / 14.0, 0.0, 1.0);          // kayma yogunlugu 0..1
    const double kEnv = 1.0 - std::exp(-1.0 / (0.03 * fs_)), kBurn = 1.0 - std::exp(-1.0 / (0.12 * fs_));
    const double kU = 1.0 - std::exp(-1.0 / (0.08 * fs_));
    // Doku hizi (asfalt): kayma hizina bagli 45-135 Hz; kilitte arac hizi dustukce yavaslar
    const double tex = locked ? 40.0 + 70.0 * std::clamp(lockSpeed / 25.0, 0.0, 1.0) : std::clamp(45.0 + 0.9 * slip * 6.0, 45.0, 135.0);
    if (std::fabs(tex - lastTex_) > 2.0) { lastTex_ = tex; road_.bandpass(tex, 2.2, fs_); }
    const double peakDecay = std::exp(-1.0 / (0.05 * fs_));
    const double crackDecay = std::exp(-1.0 / ((0.0025 - 0.0017 * burn_) * fs_));
    static const double kRatio[3] = {1.00, 1.065, 0.925};
    for (int i = 0; i < n; ++i) {
        env_ += (sq - env_) * kEnv;
        burn_ += (burnT - burn_) * kBurn;
        lockEnv_ += (lockT - lockEnv_) * kEnv;
        u_ += (uT - u_) * kU;
        const double act = std::max({env_, burn_, lockEnv_});
        if (act < 1e-4) continue;
        const double u = u_, lk = lockEnv_ > 0.05 ? 1.0 : 0.0;
        // Perde: tutunma tepesinde en yuksek (~950 Hz), burnoutta ~750'ye iner; kilit sert ve parlak
        const double f0 = lk ? 920.0 + 120.0 * std::clamp(lockSpeed / 30.0, 0.0, 1.0)
                             : 600.0 + 340.0 * std::sin(kPi * 0.85 * std::pow(u, 0.65));
        // Ayarlar kayma yogunluguna gore (hafif -> sert -> burnout); kilit ayri
        const double mid = std::clamp(u / 0.4, 0.0, 1.0), hi = std::clamp((u - 0.4) / 0.4, 0.0, 1.0);
        const double kfb = lk ? 1.35 : 0.75 + 0.40 * mid + 0.15 * hi;
        const double drive = 1.2 + 1.3 * (lk ? 0.8 : u);
        const double jDepth = lk ? 0.045 : 0.035 + 0.020 * mid + 0.055 * hi;
        const double jFc = lk ? 200.0 : 140.0 + 40.0 * mid + 70.0 * hi;
        const double dAM = lk ? 0.55 : 0.65 - 0.20 * mid + 0.30 * hi;
        const double kJ = 1.0 - std::exp(-2.0 * kPi * jFc / fs_);
        // Asfalt dokusu: yarim dalga dogrultulmus bant gurultusu, tepe izleyiciyle 0..1
        const double rb = road_.run(rnd());
        roadPeak_ = std::max(std::fabs(rb), roadPeak_ * peakDecay);
        const double mRoad = std::max(0.0, rb) / std::max(roadPeak_, 1e-6);
        const double amod = (1.0 - dAM) + dAM * mRoad;
        // Yavas kayma (+-%1.5)
        drift_ += (rnd() - drift_) * (1.0 - std::exp(-2.0 * kPi * 3.0 / fs_));
        // 3 ses: geri beslemeli FM + tanh (yapis-kay testeresi), bagimsiz hizli titreme
        double tonal = 0;
        for (int v = 0; v < 3; ++v) {
            jit_[v] += (rnd() - jit_[v]) * kJ;
            const double f = f0 * kRatio[v] * (1.0 + jDepth * std::clamp(jit_[v] * 3.0, -1.0, 1.0) + 0.015 * std::clamp(drift_ * 4.0, -1.0, 1.0));
            ph_[v] += f / fs_; ph_[v] -= std::floor(ph_[v]);
            y_[v] = std::tanh(drive * std::sin(2 * kPi * ph_[v] + kfb * y_[v]));
            tonal += y_[v] * (v == 0 ? 0.45 : 0.30);
        }
        tonal *= amod;
        // Surtunme hisirtisi: yapis-kay dongusuyle atar (duz ruzgar degil)
        const double scrubRaw = scrubLp_.run(scrubHp_.run(rnd()));
        const double scrub = scrubRaw * (0.4 + 0.6 * std::min(1.5, 0.5 * std::fabs(tonal) + mRoad));
        // Citirti tanecikleri (burnout / kilit)
        const double grains = 250.0 + 650.0 * burn_;
        if (rnd() > 1.0 - 2.0 * grains / fs_) crackEnv_ = 0.5 + 0.5 * std::fabs(rnd());
        const double crack = crackEnv_ * rnd(); crackEnv_ *= crackDecay;
        // Karisim: hafif 78/22/0, sert 58/37/5, burnout 22/48/30, kilit 48/42/10
        double wT, wN, wC;
        if (lk) { wT = 0.48; wN = 0.42; wC = 0.10; }
        else {
            wT = 0.78 - 0.20 * mid; wN = 0.22 + 0.15 * mid; wC = 0.05 * mid;
            wT += (0.22 - wT) * burn_; wN += (0.48 - wN) * burn_; wC += (0.30 - wC) * burn_;
        }
        const double exc = wT * tonal + wN * scrub * 2.5 + wC * crack * 1.5;
        // Gövde tinilari (sabit): tizlik burada sinirli kalir
        const double f3g = lk ? 0.40 : 0.28;
        double o = 1.0 * f1_.run(exc) + 0.63 * f2_.run(exc) + f3g * f3_.run(exc);
        // Dis blogu ezilmesi (burnout): 110-240 Hz + 85 Hz vuruntu, doku ile ogutulur
        if (burn_ > 1e-3) o += burn_ * (rumble_.run(rnd()) * 1.2 + thump_.run(rnd()) * 0.8) * (0.5 + 0.5 * mRoad);
        o = outHp_.run(outLp_.run(o));
        out[i] += (float)(o * act * (lk ? 2.8 : 1.6) * gain);
    }
}

} // namespace zk
