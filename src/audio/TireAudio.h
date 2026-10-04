#pragma once
// Lastik sesi (v5). Gercek kayitlarin bilinen yapisi:
//  * Viraj / fren / kalkis cigligi: dar bantli, neredeyse sinuzoidal TON (~600-1400 Hz), birkac zayif harmonik;
//    perdesi yavas gezinir (yuk / kayma), hizli duzensiz titrer (yapis-kay), siddeti dalgalanir; etrafinda
//    tonun bandinda hafif hisirti. (v4'teki darbe dizisi + rezonator "vizilti" uretiyordu.)
//  * Burnout: tonun yerini genis bantli kavurma alir: 200 Hz-5 kHz kaucuk parcalanma hisirtisi, lastik
//    donusu / dis bloklarindan gelen 25-70 Hz puruzlu genlik dalgasi (kukreme), alt ugultu, citirtilar ve
//    araya giren kisa ciglik patlamalari.
#include <cstdint>

namespace zk {

class TireAudio {
public:
    explicit TireAudio(int sampleRate = 44100);
    bool silent(double slip) const { return slip <= 1.0 && env_ < 1e-4 && burn_ < 1e-4; }
    // slip: tekerlek cevre hizi - arac hizi (m/s). out'a EKLER.
    // lockSpeed > 0: kilitli tekerle kayma (fren): ton hizla 1.2 kHz -> 400 Hz iner, burnout kavurmasi yok
    void render(float* out, int n, double slip, float gain, double lockSpeed = 0.0);

private:
    struct Bq { double b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
                void bandpass(double f, double q, int fs); void lowpass(double f, double q, int fs); void highpass(double f, double q, int fs);
                double run(double x); };
    int fs_;
    double ph_ = 0, wander_ = 0, wander2_ = 0, trem_ = 0, env_ = 0, burn_ = 0, chirp_ = 0, chirpEnv_ = 0, crackEnv_ = 0;
    double rough_ = 0, roughPh_ = 0, lastF_ = -1, hissOne_ = 0;
    Bq toneBand_, hissBp_, hissLp_, rumbleLp_, rumbleHp_, crackHp_;
    uint32_t rng_ = 0x1234567u;
    double rnd();
};

} // namespace zk
