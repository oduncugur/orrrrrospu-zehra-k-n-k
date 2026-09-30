#pragma once
// Lastik sesi: kaucugun yolda yapis-kay (stick-slip) titresimi -> TONAL ciglik (harmonikli),
// perdesi kayma hizi ile hafif yukselir, rastgele perde/genlik titrer. Buyuk patinajda (burnout)
// ustune yanma/hirlama gurultusu eklenir. Saf gurultu "ruzgar" gibi duyuldugu icin kullanilmaz.
#include <cstdint>

namespace zk {

class TireAudio {
public:
    explicit TireAudio(int sampleRate = 44100) : fs_(sampleRate) {}
    bool silent(double slip) const { return slip <= 1.5 && env_ < 1e-4; }
    // slip: tekerlek cevre hizi - arac hizi (m/s). out'a EKLER.
    void render(float* out, int n, double slip, float gain);

private:
    int fs_;
    double ph_ = 0, jit_ = 0, amJ_ = 0, rum_ = 0, burn_ = 0, env_ = 0;
    uint32_t rng_ = 0x1234567u;
};

} // namespace zk
