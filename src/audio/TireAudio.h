#pragma once
// ZEHRA KINIK - Gercek Studyolu Lastik ve Drift Sesi Motoru
//
// Ses Mimarisi:
// - Gercek Formula/Drift lastik kayma kaydi (tires_squal_loop) C++ icine gomuldu (Sifir bagimlilik).
// - Kayma hizina ve kilitlemeye gore dinamik hiz/perde (pitch-shifting) ve zarf modulasyonu.
// - Viraj girisinde tutun-kop civildamasi (chirping).
// - Kesintisiz, tiklamasiz seamless loop oynatimi.
#include <cstdint>

namespace zk {

class TireAudio {
public:
    explicit TireAudio(int sampleRate = 44100);

    bool silent(double slip) const;

    void render(float* out, int n, double slip, float gain, double lockSpeed = 0.0);

private:
    int fs_;
    float dt_;

    // Oynatma pozisyonu ve hiz kontrolu
    double playPos_ = 0.0;
    float env_ = 0.0f;
    float lockEnv_ = 0.0f;
    float chirpEnv_ = 0.0f;
    float chirpPhase_ = 0.0f;

    // Dinamik hiz
    float curRate_ = 1.0f;
};

} // namespace zk
