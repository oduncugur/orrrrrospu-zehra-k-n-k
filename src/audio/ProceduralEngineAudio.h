// ZEHRA KINIK - Faz 2: Prosedurel motor ses sentezi (SIFIR .wav dosyasi)
//
// Her aracin sesi yalnizca katalog verisinden uretilir:
//  * Atesleme olaylari: gercek atesleme sirasi ve silindir duzenine gore krank acisi (4 zaman: 720 deg).
//    f_fire = RPM * silindir / 120 Hz. Rotari: eksantrik mil turu basina rotor sayisi kadar atesleme.
//  * Bank ayrimi: crossplane V8'de bank basina esitsiz aralik -> "homurtu"; flatplane'de esit -> "ciglik".
//  * Boxer esitsiz header: silindir basina dt gecikme -> "Subaru rumble".
//  * Egzoz: bank basina dalga kilavuzu (delay line + kayipli geri besleme) = boru rezonansi, susturucu alcak geciren.
//  * VTEC/kam gecisi: gecis devrinde parlaklik ve emme sesinde ani sicrama.
//  * ITB emme homurtusu, turbo islik + flutter sin(wt)*e^(-lambda t), kompresor uguldamasi,
//    duz disli (dogbox) inlemesi f = mil devri * dis sayisi, gaz kesmede patlamalar (crackle).
// render() gercek zamanli ses geri cagrisi icin tasarlanmistir: bellek ayirmaz, kilit kullanmaz.
#pragma once
#include "garage/VehicleCatalog.h"
#include <cstdint>
#include <string>
#include <vector>

namespace zk {

struct EngineAudioInput {
    double rpm      = 900.0;
    double throttle = 0.0;   // 0..1
    double boost    = 0.0;   // 0..1 turbo mil hizi hedefi (sim'den); <0 ise dahili tahmin
    bool   inGear   = false; // dogbox inlemesi icin
    bool   fuelCut  = false; // devir kesici / vites ateslemesi kesme
    bool   antiLag  = false; // rolling anti-lag: gaz kesmede zorla patlama
};

class ProceduralEngineAudio {
public:
    ProceduralEngineAudio(const VehicleDef& v, int sampleRate = 44100);
    void setInput(const EngineAudioInput& in) { in_ = in; }
    void setTurboKit(bool on) { turboKit_ = on; }   // sonradan takilan turbo kiti (NA motor)
    void render(float* out, int n);
    std::string signature() const;   // sesin parametrik ozeti (debug)

private:
    struct Event { double angle; int bank; double delay; double gain; bool valve; };
    struct Pulse { double start; double amp; double tau; int bank; bool pop; bool active; bool mech; };
    struct Biquad {                       // RBJ band-geciren (sabit katsayi)
        double b0 = 0, b2 = 0, a1 = 0, a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        void setBandpass(double f, double q, double fs);
        double run(double x) { const double y = b0 * x + b2 * x2 - a1 * y1 - a2 * y2; x2 = x1; x1 = x; y2 = y1; y1 = y; return y; }
    };

    float noise();
    void  firePulse(double start, int bank, double amp, double tau, bool pop, bool mech);

    VehicleDef v_; EngineDef e_;
    int    fs_;
    bool   turboKit_ = false;
    double bovAmp_ = 0.0, bovT_ = -1.0, bovHp_ = 0.0, bovPrev_ = 0.0;
    double cycleDeg_ = 720.0;
    std::vector<Event> events_;
    int    banks_ = 1;
    double theta_ = 0.0, t_ = 0.0;
    double pulseTau_, mufflerHz_, intakeHz_, pipeLen_[2];
    Pulse  pulses_[192] = {};
    std::vector<float> dl_[2]; int dlPos_[2] = {0, 0}; int dlLen_[2];
    double loopLp_[2] = {0, 0};
    double lp1_ = 0, lp2_ = 0, dc_ = 0, dcIn_ = 0;
    double bpLow_ = 0, bpBand_ = 0;
    double spool_ = 0.0, flutterT_ = -1.0, prevThr_ = 0.0, whPhase_ = 0.0, scPhase_ = 0.0, gwPhase_ = 0.0;
    double vtecMix_ = 0.0, lopePhase_ = 0.0;
    double nLp_ = 0.0, outLp_ = 0.0, tbLow_ = 0.0, tbBand_ = 0.0;
    Biquad blockA_, blockB_, valveBp_, cavity_[2];
    // v6 ton dengesi (Gemini dinleme geri bildirimi): blok govdesi, V8 alt bas + egzoz rezonanslari, VTEC emme bandi,
    // emme "vuuh"u (gaz acilisi), kesici bas vurusu, ust tiz kisma
    Biquad body_, v8a_, v8b_, v8sub_, vtecBp_, intakeBp_;
    double hfLp_ = 0.0, thumpT_ = -1.0, tipT_ = -1.0, tremPh_ = 0.0;
    bool prevCut_ = false; double tipPrev_ = 0.0;
    uint32_t rng_;
    EngineAudioInput in_;
};

// Onizleme: rolanti -> tam gaz devir tarama -> devir kesici -> gaz kesme (patlama/flutter) -> rolanti
std::vector<float> renderRevDemo(const VehicleDef& v, int sampleRate, double seconds = 7.0);
bool writeWav16(const std::string& path, const std::vector<float>& mono, int sampleRate);

} // namespace zk
