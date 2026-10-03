// ZEHRA KINIK - Yatay (640x360) surus kontrolleri: analog gaz / fren / debriyaj kaydiricilari ve sanziman
// tipine gore vites kolu: H-desen (manuel), otomatik kol (yukaridan S-D-N-R-P; D'den ileri itince S, yanda M +/-),
// sirali +/- kolu. Dokunmatik + klavye.
// Ekrandan bagimsiz: yol ve karma yaris ekranlari kullanir; ciktilar RoadControls'a cevrilir.
#pragma once
#include "app/App.h"

#include <algorithm>
#include <vector>

namespace zk {

class Cockpit {
public:
    enum class Lever { HPattern, Automatic, Sequential };
    enum class AutoPos { P = 0, R, N, D, S, M };   // M: elle (+/- kapisi)

    // clutchPedal: oyuncu debriyaji (yalniz H-desende anlamli)
    void configure(Lever lever, int gears, bool clutchPedal);
    void setPortrait(bool p) { portrait_ = p; setKnobGear(knobGear_); }   // dikey (360x640) / yatay (640x360) yerlesim
    Lever lever() const { return lever_; }
    bool  clutchPedal() const { return clutchPedal_; }

    bool pointerDown(int id, float x, float y);   // bir kontrole dokunduysa true
    void pointerMove(int id, float x, float y);
    void pointerUp(int id);
    void key(Key k, bool down);
    void update(double dt);                        // klavye analog rampalari
    void render(Renderer& r, int gear, bool grind) const;

    // Dokunmatik gaz: yolun %85'inde tam gaz (kisa pedal)
    double throttle() const { return std::max({std::min(1.0, thrUi_ / 0.85), (double)keyThr_, (double)padThr_}); }
    double brake() const { return std::max({brakeUi_, keyBrake_, padBrk_}); }
    void setPadPedals(float thr, float brk) { padThr_ = thr; padBrk_ = brk; }   // oyun kolu tetikleri (analog 0..1)
    // Dokunmatik debriyaj: kavrama araligi (pedal 0.32-0.62) cubugun %76'sina yayilir, bos / tam basili bolgeler kisa
    static double clutchCurve(double u) {
        return u < 0.12 ? u / 0.12 * 0.32 : u < 0.88 ? 0.32 + (u - 0.12) / 0.76 * 0.30 : 0.62 + (u - 0.88) / 0.12 * 0.38;
    }
    double clutch() const { return std::max(clutchCurve(clutchUi_), (double)keyClutch_); }   // 1 = basili
    int  knobGear() const { return knobGear_; }    // H-desen: kolun gosterdigi vites (0 = bos)
    bool takeSeated() { const bool e = seatedEv_ != 0; seatedEv_ = 0; return e; }   // vites yuvaya oturdu (titresim)
    int  takeShift() { const int s = shift_; shift_ = 0; return s; }   // sirali / otomatik M: +1 / -1 darbe
    AutoPos autoPos() const { return autoPos_; }
    void setKnobGear(int g);                        // kolu bir vitese oturt (klavye, baslangic)
    bool overControl(float x, float y) const;       // nokta bir kontrolun ustunde mi (ekran dokunuslari icin)

private:
    enum class Ctl { None, Throttle, Brake, Clutch, Shifter, Up, Down, Auto, Gate };
    struct Touch { int id; Ctl ctl; };
    Ctl hit(float x, float y) const;
    void shifterFromPoint(float x, float y, bool release = false);
    void autoFromPoint(float y);

    bool portrait_ = false;
    Lever lever_ = Lever::Automatic;
    int gears_ = 5;
    bool clutchPedal_ = false;
    std::vector<Touch> touches_;
    float thrUi_ = 0, brakeUi_ = 0, clutchUi_ = 0;
    float keyThr_ = 0, keyBrake_ = 0, keyClutch_ = 0, padThr_ = 0, padBrk_ = 0;
    bool kThr_ = false, kBrake_ = false, kClutch_ = false;
    float knobX_ = 0, knobY_ = 0;
    // Surukleme: kol parmagi vites kanali icinde izler (gorsel), vites yalniz yuvaya oturunca / birakinca takilir
    bool knobDrag_ = false; float dragX_ = 0, dragY_ = 0, lastShX_ = 0, lastShY_ = 0;
    int seatedEv_ = 0;                             // yeni oturma olayi (titresim icin, okununca sifir)
    int knobGear_ = 1, shift_ = 0;
    AutoPos autoPos_ = AutoPos::D;
};

} // namespace zk
