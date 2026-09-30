#include "TireAudio.h"

#include <algorithm>
#include <cmath>

namespace zk {

void TireAudio::render(float* out, int n, double slip, float gain) {
    const double target = std::clamp((slip - 1.5) / 6.0, 0.0, 1.0);
    const double burnT = std::clamp((slip - 12.0) / 15.0, 0.0, 1.0);
    const double f0 = 620.0 + 18.0 * std::min(slip, 20.0);
    const double kEnv = 1.0 - std::exp(-1.0 / (0.015 * fs_)), kBurn = 1.0 - std::exp(-1.0 / (0.045 * fs_));
    const double kJit = 1.0 - std::exp(-1.0 / (0.025 * fs_)), kAm = 1.0 - std::exp(-1.0 / (0.0057 * fs_));
    const double kRum = 1.0 - std::exp(-1.0 / (0.0011 * fs_));
    for (int i = 0; i < n; ++i) {
        env_ += (target - env_) * kEnv;
        burn_ += (burnT - burn_) * kBurn;
        rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
        const double x = ((rng_ & 0xFFFF) / 32768.0 - 1.0);
        jit_ += (x - jit_) * kJit;                         // yavas perde kaymasi
        amJ_ += (x - amJ_) * kAm;                          // hizli genlik titremesi ("cirp")
        ph_ += f0 * (1.0 + 0.9 * jit_) / fs_; if (ph_ > 1.0) ph_ -= 1.0;
        const double w = 6.283185307179586 * ph_;
        const double tone = std::sin(w) + 0.55 * std::sin(2 * w) + 0.3 * std::sin(3 * w + 0.7) + 0.12 * std::sin(5 * w);
        const double am = std::clamp(0.75 + 3.0 * amJ_, 0.2, 1.3);
        rum_ += (x - rum_) * kRum;                         // burnout: alcak geciren gurultu (hirlama)
        out[i] += (float)(((0.22 * tone * am * (1.0 - 0.5 * burn_) + 0.03 * x) * env_ + 1.4 * rum_ * burn_) * gain);
    }
}

} // namespace zk
