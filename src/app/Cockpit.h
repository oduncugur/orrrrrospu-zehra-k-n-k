// ZEHRA KINIK - Yatay (640x360) surus kontrolleri: analog gaz / fren / debriyaj kaydiricilari ve sanziman
// tipine gore vites kolu: H-desen (manuel), otomatik P-N-D topuzu, sirali +/- kolu. Dokunmatik + klavye.
// Ekrandan bagimsiz: yol ve karma yaris ekranlari kullanir; ciktilar RoadControls'a cevrilir.
#pragma once
#include "app/App.h"

#include <algorithm>
#include <vector>

namespace zk {

class Cockpit {
public:
    enum class Lever { HPattern, Automatic, Sequential };
    enum class AutoPos { P = 0, N = 1, D = 2 };

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

    double throttle() const { return std::max(thrUi_, keyThr_); }
    double brake() const { return std::max(brakeUi_, keyBrake_); }
    double clutch() const { return std::max(clutchUi_, keyClutch_); }   // 1 = basili
    int  knobGear() const { return knobGear_; }    // H-desen: kolun gosterdigi vites (0 = bos)
    int  takeShift() { const int s = shift_; shift_ = 0; return s; }   // sirali: +1 / -1 darbe
    AutoPos autoPos() const { return autoPos_; }
    void setKnobGear(int g);                        // kolu bir vitese oturt (klavye, baslangic)
    bool overControl(float x, float y) const;       // nokta bir kontrolun ustunde mi (ekran dokunuslari icin)

private:
    enum class Ctl { None, Throttle, Brake, Clutch, Shifter, Up, Down, Auto };
    struct Touch { int id; Ctl ctl; };
    Ctl hit(float x, float y) const;
    void shifterFromPoint(float x, float y);
    void autoFromPoint(float y);

    bool portrait_ = false;
    Lever lever_ = Lever::Automatic;
    int gears_ = 5;
    bool clutchPedal_ = false;
    std::vector<Touch> touches_;
    float thrUi_ = 0, brakeUi_ = 0, clutchUi_ = 0;
    float keyThr_ = 0, keyBrake_ = 0, keyClutch_ = 0;
    bool kThr_ = false, kBrake_ = false, kClutch_ = false;
    float knobX_ = 0, knobY_ = 0;
    int knobGear_ = 1, shift_ = 0;
    AutoPos autoPos_ = AutoPos::D;
};

} // namespace zk
