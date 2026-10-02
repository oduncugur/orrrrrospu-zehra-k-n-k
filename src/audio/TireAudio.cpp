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
    hissBp_.highpass(500.0, 0.6, fs_);       // kavurma hisirtisi: 0.5-~2.5 kHz (gercek kayitlarda merkez ~1-2 kHz)
    hissLp_.lowpass(2200.0, 0.7, fs_);
    rumbleLp_.lowpass(450.0, 0.8, fs_);      // alt ugultu: 80-450 Hz
    rumbleHp_.highpass(80.0, 0.7, fs_);
    crackHp_.highpass(2500.0, 0.7, fs_);     // citirti: keskin
}

double TireAudio::rnd() {
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
    return (rng_ & 0xFFFF) / 32768.0 - 1.0;
}

void TireAudio::render(float* out, int n, double slip, float gain) {
    // Ciglik: kucuk-orta kaymada; buyuk kaymada (burnout) yerini kavurmaya birakir
    const double sq = std::clamp((slip - 1.0) / 3.5, 0.0, 1.0) * (1.0 - 0.8 * std::clamp((slip - 8.0) / 8.0, 0.0, 1.0));
    const double burnT = std::clamp((slip - 7.0) / 10.0, 0.0, 1.0);
    // Ton perdesi: hafif kaymada ~650 Hz, artan kaymada ~1150 Hz'e cikar
    const double f0 = 650.0 + 45.0 * std::min(slip, 11.0);
    if (std::fabs(f0 - lastF_) > 3.0) { lastF_ = f0; toneBand_.bandpass(f0, 4.0, fs_); }
    const double kEnv = 1.0 - std::exp(-1.0 / (0.025 * fs_)), kBurn = 1.0 - std::exp(-1.0 / (0.10 * fs_));
    const double kW = 1.0 - std::exp(-1.0 / (0.25 * fs_));     // yavas perde gezinmesi (~0.6 Hz)
    const double kW2 = 1.0 - std::exp(-1.0 / (0.012 * fs_));   // hizli duzensiz titreme (~13 Hz)
    const double kT = 1.0 - std::exp(-1.0 / (0.06 * fs_));     // siddet dalgalanmasi
    const double kR = 1.0 - std::exp(-1.0 / (0.004 * fs_));    // kukreme puruzu
    const double chirpDecay = std::exp(-1.0 / (0.09 * fs_)), crackDecay = std::exp(-1.0 / (0.0008 * fs_));
    const double roughHz = 25.0 + 2.5 * std::min(slip, 18.0);  // lastik / dis blogu frekansi
    for (int i = 0; i < n; ++i) {
        env_ += (sq - env_) * kEnv;
        burn_ += (burnT - burn_) * kBurn;
        const double x = rnd();
        wander_ += (rnd() - wander_) * kW;
        wander2_ += (rnd() - wander2_) * kW2;
        trem_ += (rnd() - trem_) * kT;
        double o = 0;
        // ---- ton (ciglik): perde = f0 * (1 + %6 gezinme + %1.5 titreme); 2. ve 3. harmonik zayif
        const double tone = env_ + 0.5 * chirpEnv_;
        if (tone > 1e-4) {
            const double f = f0 * (1.0 + 0.06 * wander_ * 4.0 + 0.015 * wander2_ * 8.0) * (chirpEnv_ > 0.01 ? 1.0 + 0.12 * chirp_ : 1.0);
            ph_ += f / fs_; ph_ -= std::floor(ph_);
            const double s = std::sin(2 * kPi * ph_) + 0.28 * std::sin(4 * kPi * ph_ + 0.7) + 0.10 * std::sin(6 * kPi * ph_ + 1.9);
            const double am = std::clamp(0.75 + 3.0 * trem_, 0.25, 1.25);
            const double breath = toneBand_.run(x) * 0.9;           // tonun bandinda hisirti
            o += 0.42 * tone * am * (s + breath);
        }
        // ---- burnout kavurmasi
        if (burn_ > 1e-4) {
            roughPh_ += roughHz / fs_; roughPh_ -= std::floor(roughPh_);
            rough_ += (rnd() - rough_) * kR;
            const double block = 0.55 + 0.45 * std::sin(2 * kPi * roughPh_) + 0.6 * rough_;   // dis bloklari + puruz
            hissOne_ += (hissLp_.run(hissBp_.run(x)) - hissOne_) * 0.35;   // 3. kutup: ust tiz kirpilir
            const double hiss = hissOne_ * 1.6;
            const double rumble = rumbleHp_.run(rumbleLp_.run(x));
            if (rnd() > 1.0 - 2.0 * 220.0 / fs_) crackEnv_ = 0.5 + 0.5 * std::fabs(rnd());       // citirti (~220/s)
            const double crack = crackHp_.run(crackEnv_ * rnd()); crackEnv_ *= crackDecay;
            if (chirpEnv_ < 0.02 && rnd() > 1.0 - 2.0 * 2.5 / fs_) { chirpEnv_ = 0.7 + 0.3 * std::fabs(rnd()); chirp_ = rnd(); }   // ara ciglik
            o += burn_ * (0.75 * hiss * std::clamp(block, 0.2, 1.6) + 1.3 * rumble * std::clamp(block, 0.3, 1.5) + 0.2 * crack);
        }
        chirpEnv_ *= chirpDecay;
        out[i] += (float)(o * gain);
    }
}

} // namespace zk
