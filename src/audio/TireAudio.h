#pragma once
// Lastik sesi (v4): kaucuk yolda yapis-kay (stick-slip) yapar; her "kayma" bir darbe uretir.
// Darbe dizisi (periyodu titreyen gevseme osilatoru) lastik yanaginin/sirtinin UYUMSUZ (inharmonik)
// titresim modlarini (dar bant rezonatorler) uyarir -> sentetik duduk degil, kaucuk "cigligi".
// Buyuk patinajda (burnout) cigligin yerini genis bantli, dalgalanan "kavurma" ugultusu alir.
#include <cstdint>

namespace zk {

class TireAudio {
public:
    explicit TireAudio(int sampleRate = 44100);
    bool silent(double slip) const { return slip <= 1.0 && env_ < 1e-4 && burn_ < 1e-4; }
    // slip: tekerlek cevre hizi - arac hizi (m/s). out'a EKLER.
    void render(float* out, int n, double slip, float gain);

private:
    struct Res { double f, q, b0 = 0, a1 = 0, a2 = 0, y1 = 0, y2 = 0, g; };
    static constexpr int kModes = 5;
    int fs_;
    Res m_[kModes];
    double ph_ = 0, jit_ = 0, amJ_ = 0, env_ = 0, burn_ = 0, lastF_ = -1;
    double r1_ = 0, r2_ = 0, rb_ = 0, flut_ = 0, hs_ = 0, hs2_ = 0, crackEnv_ = 0;
    uint32_t rng_ = 0x1234567u;
    double rnd();
};

} // namespace zk
