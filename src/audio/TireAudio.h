#pragma once
// Lastik sesi (v6, yapis-kay modeli). Gercek kayitlarin yapisi:
//  * Ciglik tek temiz sinus degil: lastik dis siralari (ic / orta / dis) biraz farkli hizda kayar -> 3 hafif
//    detune ses (oran 1 / 1.065 / 0.925); her biri geri beslemeli FM + tanh ile testere benzeri (yapis: gerilme,
//    kay: ani bosalma), harmonikler ~-7 dB/oktav. Perde hizli titrer (+-%3.5-11, 140-250 Hz suzulmus gurultu).
//  * Asfalt dokusu yapis-kay dongusunu 45-135 Hz'de keser: yarim dalga dogrultulmus bant gecirgen gurultuyle
//    genlik modulasyonu (ruzgar / ugultu hissi bundan gider). Surtunme hisirtisi bu donguyle atar.
//  * Ses lastik gövdesinin sabit tinilarindan cikar: 880 / 1850 / 3300 Hz rezonator (tizlik burada sinirlanir).
//  * Perde kaymayla dogrusal artmaz: tutunma tepesinde (~950 Hz) en yuksek, burnoutta ~750 Hz'e iner ve bozulur.
//  * Burnout: duz hisirti yerine citirti tanecikleri (250-900 /s, 0.8-2.5 ms) + dis blogu ezilmesi (110-240 Hz).
// Kaynak: Gemini (Argon) ile ornek kayitlar uzerinden yapilan analiz; sayilar oradan.
#include <cstdint>

namespace zk {

class TireAudio {
public:
    explicit TireAudio(int sampleRate = 44100);
    bool silent(double slip) const { return slip <= 1.0 && env_ < 1e-4 && burn_ < 1e-4 && lockEnv_ < 1e-4; }
    // slip: tekerlek cevre hizi - arac hizi (m/s). out'a EKLER.
    // lockSpeed > 0: kilitli tekerle kayma (fren): sert, parlak ciglik + yol titresimi; burnout yok
    void render(float* out, int n, double slip, float gain, double lockSpeed = 0.0);

private:
    struct Bq { double b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
                void bandpass(double f, double q, int fs); void lowpass(double f, double q, int fs); void highpass(double f, double q, int fs);
                double run(double x); };
    int fs_;
    double env_ = 0, burn_ = 0, lockEnv_ = 0, u_ = 0;            // yumusatilmis siddetler / kayma
    double ph_[3] = {0, 0, 0}, y_[3] = {0, 0, 0}, jit_[3] = {0, 0, 0};
    double drift_ = 0, roadPeak_ = 1e-3, crackEnv_ = 0, lastTex_ = -1;
    Bq road_, scrubHp_, scrubLp_, rumble_, thump_, f1_, f2_, f3_, outLp_, outHp_;
    uint32_t rng_ = 0x1234567u;
    double rnd();
};

} // namespace zk
