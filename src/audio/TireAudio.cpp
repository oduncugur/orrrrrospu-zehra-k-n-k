// ZEHRA KINIK - Gercek Studyolu Lastik ve Drift Sesi Motoru
#include "TireAudio.h"
#include "TireSkidSample.h"

#include <algorithm>
#include <cmath>

namespace zk {

namespace {
constexpr float kTwoPi = 6.28318530717958647692f;

inline float clampf(float val, float minVal, float maxVal) {
    return std::max(minVal, std::min(maxVal, val));
}
}

TireAudio::TireAudio(int sampleRate)
    : fs_(sampleRate > 0 ? sampleRate : 44100),
      dt_(1.0f / (float)fs_),
      playPos_(0.0),
      env_(0.0f), lockEnv_(0.0f), chirpEnv_(0.0f), chirpPhase_(0.0f),
      curRate_(1.0f) {}

bool TireAudio::silent(double slip) const {
    return slip < 0.55 && env_ < 0.0002f && lockEnv_ < 0.0002f;
}

void TireAudio::render(float* out, int n, double slip, float gain, double lockSpeed) {
    if (gain <= 0.0f && env_ < 0.0001f) return;

    const float s = (float)std::max(0.0, slip);
    const float lock = (float)std::max(0.0, lockSpeed);
    const bool isLocked = lock > 1.0f;

    // 1. Fizik Hedefleri
    float targetEnv = 0.0f;
    float targetLock = 0.0f;
    float targetChirp = 0.0f;
    float targetRate = 1.0f;

    if (isLocked) {
        // Fren Kilidi: Hiza bagli olarak 1.25x'ten 0.80x'e inen gercekci kizaklama
        targetLock = clampf((lock - 1.0f) / 3.0f, 0.0f, 1.0f);
        targetEnv = clampf((lock - 1.0f) / 1.8f, 0.0f, 1.0f);
        targetRate = 0.80f + 0.45f * clampf(lock / 25.0f, 0.0f, 1.0f);
    } else if (s > 0.65f) {
        targetEnv = clampf((s - 0.65f) / 1.35f, 0.0f, 1.0f);

        if (s <= 6.5f) {
            // Viraj / Drift: Kayma hizi arttikca sesin perdesi 0.85x'ten 1.22x'e cikar
            const float u = (s - 0.65f) / 5.85f;
            targetRate = 0.85f + 0.37f * std::sin(u * 1.5707963f);
            if (s < 3.2f) {
                // Virajda tutun-kop dalgalanmasi
                targetChirp = clampf((3.2f - s) / 2.0f, 0.0f, 0.75f);
            }
        } else {
            // Burnout (>6.5 m/s): Yuksek tekerlek devriyle 1.22x - 1.38x hizinda doner
            const float t = clampf((s - 6.5f) / 10.0f, 0.0f, 1.0f);
            targetRate = 1.22f + 0.16f * t;
        }
    }

    // 2. Ornekleme Katsayilari (Hizli giris 25ms, yumusak cikis 85ms ile kesilme jitter'i onlenir)
    const float kEnvAttack  = 1.0f - std::exp(-1.0f / (0.025f * fs_));
    const float kEnvRelease = 1.0f - std::exp(-1.0f / (0.085f * fs_));
    const float kParam      = 1.0f - std::exp(-1.0f / (0.035f * fs_));

    for (int i = 0; i < n; ++i) {
        const float kE = (targetEnv > env_) ? kEnvAttack : kEnvRelease;
        env_ += (targetEnv - env_) * kE;
        lockEnv_ += (targetLock - lockEnv_) * kParam;
        chirpEnv_ += (targetChirp - chirpEnv_) * kParam;
        curRate_ += (targetRate - curRate_) * kParam;

        if (env_ < 0.0001f) continue;

        // Civildama LFO (Viraj girisi tutun-kop: kesilme sikligi 14.5 Hz'den 4.5 Hz'e indirildi)
        chirpPhase_ += 4.5f * dt_;
        if (chirpPhase_ >= 1.0f) chirpPhase_ -= 1.0f;
        const float cMod = 0.5f + 0.5f * std::sin(chirpPhase_ * kTwoPi);
        const float chirpGain = 1.0f - chirpEnv_ * (0.60f * cMod);

        // Gercek PCM Ornek Oynatimi (Kesintisiz Loop + Dogrusal Enterpolasyon)
        int idx0 = (int)playPos_;
        int idx1 = idx0 + 1;
        if (idx1 >= kRealSkidCount) idx1 = 0;
        float frac = (float)(playPos_ - idx0);

        float s0 = kRealSkidSamples[idx0] * (1.0f / 32768.0f);
        float s1 = kRealSkidSamples[idx1] * (1.0f / 32768.0f);
        float sample = (1.0f - frac) * s0 + frac * s1;

        // Ilerleme
        playPos_ += curRate_;
        while (playPos_ >= kRealSkidCount) {
            playPos_ -= kRealSkidCount;
        }

        out[i] += sample * chirpGain * env_ * gain * 0.72f;
    }
}

} // namespace zk
