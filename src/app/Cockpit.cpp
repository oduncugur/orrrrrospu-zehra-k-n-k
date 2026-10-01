#include "Cockpit.h"
#include "Ui.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace zk {

namespace {
// ---- yerlesim: yatay (640x360) kaydiricilar kenarlarda boydan boya; dikey (360x640) alt kosede ----
struct PadLayout {
    Rect brake, clutch, thr, lever;
    float colX[3], rowTop, rowMid, rowBot;
    Rect seqUp, seqDn;
    float autoY[3];   // P N D
};
// Kaydiricilar ekranin alt ~%57'sinde (basparmak az yol alir), 64 px genis; vites alani buyuk (kullanici geri bildirimi)
const PadLayout kLand{{72, 150, 136, 356}, {4, 150, 68, 356}, {572, 150, 636, 356}, {404, 214, 566, 356},
                   {432, 484, 536}, 236, 286, 336, {404, 214, 566, 282}, {404, 288, 566, 356}, {238, 286, 334}};
const PadLayout kPort{{72, 440, 136, 636}, {4, 440, 68, 636}, {292, 440, 356, 636}, {142, 470, 286, 636},
                   {166, 214, 262}, 492, 553, 614, {142, 470, 286, 550}, {142, 556, 286, 636}, {494, 553, 612}};const char* const kAutoName[3] = {"P", "N", "D"};

float sliderValue(const Rect& r, float y) { return std::clamp((r.y1 - 6 - y) / (r.y1 - r.y0 - 12), 0.0f, 1.0f); }
const PadLayout& lay(bool portrait) { return portrait ? kPort : kLand; }
// Pedal sirasi gercek arabadaki gibi soldan: debriyaj, fren, gaz. Debriyaj yoksa fren en solda.
Rect brakeRect(const PadLayout& L, bool clutch) { return clutch ? L.brake : L.clutch; }
} // namespace



void Cockpit::configure(Lever lever, int gears, bool clutchPedal) {
    lever_ = lever; gears_ = std::max(1, gears);
    clutchPedal_ = clutchPedal && lever == Lever::HPattern;
    touches_.clear();
    thrUi_ = brakeUi_ = clutchUi_ = 0;
    autoPos_ = AutoPos::D;
    setKnobGear(1);
}

void Cockpit::setKnobGear(int g) {
    const PadLayout& L = lay(portrait_);
    g = std::clamp(g, 0, gears_);
    knobGear_ = g;
    if (g == 0) { knobY_ = L.rowMid; knobX_ = std::clamp(knobX_, L.colX[0], L.colX[2]); return; }
    knobX_ = L.colX[(g - 1) / 2]; knobY_ = (g % 2) ? L.rowTop : L.rowBot;
}

Cockpit::Ctl Cockpit::hit(float x, float y) const {
    const PadLayout& L = lay(portrait_);
    if (L.thr.hit(x, y)) return Ctl::Throttle;
    if (brakeRect(L, clutchPedal_).hit(x, y)) return Ctl::Brake;
    if (clutchPedal_ && L.clutch.hit(x, y)) return Ctl::Clutch;
    if (lever_ == Lever::HPattern && L.lever.hit(x, y)) return Ctl::Shifter;
    if (lever_ == Lever::Automatic && L.lever.hit(x, y)) return Ctl::Auto;
    if (lever_ == Lever::Sequential) {
        if (L.seqUp.hit(x, y)) return Ctl::Up;
        if (L.seqDn.hit(x, y)) return Ctl::Down;
    }
    return Ctl::None;
}
bool Cockpit::overControl(float x, float y) const { return hit(x, y) != Ctl::None; }

// H-desen: gercek kol gibi. Surukleme sirasinda kol parmagi kanal (H) icinde izler; vites ancak kol yuvanin
// dibine (%60 yol) oturunca takilir (titresim); parmak kalkinca en yakin yuvaya oturur, orta bantta birakilirsa bos.
// Eskiden surukleme yolundaki her yuva (bos dahil) aninda istek olarak gidiyordu: 2->3 gecisinde araya bos / citirti
// girip vites "tam oturmuyordu" (kullanici geri bildirimi).
void Cockpit::shifterFromPoint(float x, float y, bool release) {
    const PadLayout& L = lay(portrait_);
    const float half = (L.rowBot - L.rowTop) * 0.5f, midBand = half * 0.30f;
    int col = 0;
    for (int c = 1; c < 3; ++c) if (std::fabs(x - L.colX[c]) < std::fabs(x - L.colX[col])) col = c;
    int target = col * 2 + (y < L.rowMid ? 1 : 2);
    while (target > gears_ && target > 2) target -= 2;                 // olmayan sutun: en yakin var olan vitese
    if (target > gears_) target = gears_;
    const float depth = std::fabs(y - L.rowMid);
    if (release) {
        knobDrag_ = false;
        if (depth < midBand) { knobGear_ = 0; knobX_ = std::clamp(x, L.colX[0], L.colX[2]); knobY_ = L.rowMid; return; }
        if (target != knobGear_) ++seatedEv_;
        setKnobGear(target);
        return;
    }
    // Gorsel kol: orta bantta yatay serbest, kanalda sutuna kilitli
    knobDrag_ = true;
    if (depth < midBand) { dragX_ = std::clamp(x, L.colX[0], L.colX[2]); dragY_ = std::clamp(y, L.rowTop, L.rowBot); }
    else { dragX_ = L.colX[(target - 1) / 2]; dragY_ = std::clamp(y, L.rowTop, L.rowBot); }
    if (depth > half * 0.60f && target != knobGear_) { setKnobGear(target); ++seatedEv_; }   // yuvaya oturdu
}

void Cockpit::autoFromPoint(float y) {
    const PadLayout& L = lay(portrait_);
    int best = 0;
    for (int i = 1; i < 3; ++i) if (std::fabs(y - L.autoY[i]) < std::fabs(y - L.autoY[best])) best = i;
    autoPos_ = (AutoPos)best;
}

bool Cockpit::pointerDown(int id, float x, float y) {
    const PadLayout& L = lay(portrait_);
    const Ctl c = hit(x, y);
    if (c == Ctl::None) return false;
    touches_.push_back({id, c});
    switch (c) {
    case Ctl::Throttle: thrUi_ = sliderValue(L.thr, y); break;
    case Ctl::Brake: brakeUi_ = sliderValue(brakeRect(L, clutchPedal_), y); break;
    case Ctl::Clutch: clutchUi_ = sliderValue(L.clutch, y); break;
    case Ctl::Shifter: shifterFromPoint(x, y); lastShX_ = x; lastShY_ = y; break;
    case Ctl::Auto: autoFromPoint(y); break;
    case Ctl::Up: shift_ = +1; break;
    case Ctl::Down: shift_ = -1; break;
    default: break;
    }
    return true;
}

void Cockpit::pointerMove(int id, float x, float y) {
    const PadLayout& L = lay(portrait_);
    for (const Touch& t : touches_) {
        if (t.id != id) continue;
        if (t.ctl == Ctl::Throttle) thrUi_ = sliderValue(L.thr, y);
        else if (t.ctl == Ctl::Brake) brakeUi_ = sliderValue(brakeRect(L, clutchPedal_), y);
        else if (t.ctl == Ctl::Clutch) clutchUi_ = sliderValue(L.clutch, y);
        else if (t.ctl == Ctl::Shifter) { shifterFromPoint(x, y); lastShX_ = x; lastShY_ = y; }
        else if (t.ctl == Ctl::Auto) autoFromPoint(y);
    }
}

void Cockpit::pointerUp(int id) {
    for (size_t i = 0; i < touches_.size(); ++i) {
        if (touches_[i].id != id) continue;
        switch (touches_[i].ctl) {               // ayak pedaldan kalkti
        case Ctl::Throttle: thrUi_ = 0; break;
        case Ctl::Brake: brakeUi_ = 0; break;
        case Ctl::Clutch: clutchUi_ = 0; break;
        case Ctl::Shifter: shifterFromPoint(lastShX_, lastShY_, true); break;   // birakinca en yakin yuvaya otur
        default: break;
        }
        touches_.erase(touches_.begin() + i);
        return;
    }
}

void Cockpit::key(Key k, bool down) {
    switch (k) {
    case Key::Throttle: kThr_ = down; break;
    case Key::Brake: kBrake_ = down; break;
    case Key::Clutch: kClutch_ = down; break;
    default: break;
    }
    if (!down) return;
    if (lever_ == Lever::HPattern) {
        if (k >= Key::Gear0 && k <= Key::Gear6) setKnobGear((int)k - (int)Key::Gear0);
        if (k == Key::ShiftUp) setKnobGear(knobGear_ + 1);
        if (k == Key::ShiftDown) setKnobGear(knobGear_ - 1);
    } else if (lever_ == Lever::Sequential) {
        if (k == Key::ShiftUp) shift_ = +1;
        if (k == Key::ShiftDown) shift_ = -1;
    } else {
        if (k == Key::ShiftUp) autoPos_ = (AutoPos)std::min(2, (int)autoPos_ + 1);
        if (k == Key::ShiftDown) autoPos_ = (AutoPos)std::max(0, (int)autoPos_ - 1);
        if (k == Key::Gear0) autoPos_ = AutoPos::N;
        if (k >= Key::Gear1 && k <= Key::Gear6) autoPos_ = AutoPos::D;
    }
}

void Cockpit::update(double dt) {
    // Klavye analog rampalari: gaz/fren ~0.15 s'de dolar, debriyaj hizli basilir yavas birakilir (kalkis)
    const float d = (float)dt;
    keyThr_ = kThr_ ? std::min(1.0f, keyThr_ + d / 0.15f) : std::max(0.0f, keyThr_ - d / 0.08f);
    keyBrake_ = kBrake_ ? std::min(1.0f, keyBrake_ + d / 0.15f) : std::max(0.0f, keyBrake_ - d / 0.08f);
    keyClutch_ = kClutch_ ? std::min(1.0f, keyClutch_ + d / 0.08f) : std::max(0.0f, keyClutch_ - d / 0.45f);
}

void Cockpit::render(Renderer& r, int gear, bool grind) const {
    const PadLayout& L = lay(portrait_);
    auto slider = [&](const Rect& rc, float v, const char* label, Color fillC) {
        r.rect(rc.x0, rc.y0, rc.x1, rc.y1, {0.10f, 0.10f, 0.12f, 0.6f});
        const float y = rc.y1 - 6 - (rc.y1 - rc.y0 - 12) * v;
        r.rect(rc.x0 + 4, y, rc.x1 - 4, rc.y1 - 4, fillC);
        r.rect(rc.x0, y - 2, rc.x1, y + 2, {1, 1, 1, 0.9f});
        for (size_t i = 0; label[i]; ++i) r.textCentered(rc.cx(), rc.y0 + 8 + i * 16.0f, std::string(1, label[i]), 2, {1, 1, 1, 0.9f});
    };
    slider(brakeRect(L, clutchPedal_), (float)brake(), "FREN", {0.85f, 0.15f, 0.15f, 0.85f});
    if (clutchPedal_) {
        // Kavrama noktasi (ClutchSpec: pedal 0.62 = tutmaya baslar, 0.32 = tam kavrar) belirgin bant + yazi;
        // pedal bandin icindeyken dolgu sariya doner (debriyaj tutuyor)
        const float c = (float)clutch();
        const bool biting = c > 0.32f && c < 0.62f;
        slider(L.clutch, c, "", biting ? Color{1.0f, 0.78f, 0.1f, 0.9f} : Color{0.25f, 0.55f, 0.95f, 0.85f});
        const float span = L.clutch.y1 - L.clutch.y0 - 12;
        const float yTop = L.clutch.y1 - 6 - span * 0.62f, yBot = L.clutch.y1 - 6 - span * 0.32f;
        r.rect(L.clutch.x0, yTop, L.clutch.x1, yBot, {1.0f, 0.8f, 0.1f, biting ? 0.35f : 0.22f});
        r.rect(L.clutch.x0, yTop - 1, L.clutch.x1, yTop + 1, {1.0f, 0.85f, 0.2f});     // tutmaya basladigi yer
        r.rect(L.clutch.x0, yBot - 1, L.clutch.x1, yBot + 1, {1.0f, 0.85f, 0.2f});
        r.textCentered(L.clutch.cx(), (yTop + yBot) * 0.5f - 4, "KAVRAMA", 1, {1.0f, 0.95f, 0.7f});
        r.textCentered(L.clutch.cx(), L.clutch.y0 + 6, "DEBRIYAJ", 1, {1, 1, 1, 0.9f});
        r.textCentered(L.clutch.cx(), L.clutch.y1 - 14, "BIRAK", 1, {0.8f, 0.8f, 0.85f, 0.8f});
        r.textCentered(L.clutch.cx(), L.clutch.y0 + 18, "BAS", 1, {0.8f, 0.8f, 0.85f, 0.8f});
    }
    slider(L.thr, (float)throttle(), "GAZ", {0.95f, 0.55f, 0.1f, 0.9f});

    r.rect(L.lever.x0, L.lever.y0, L.lever.x1, L.lever.y1, {0.10f, 0.10f, 0.13f, 0.45f});   // yari saydam: yolu kapatmasin
    if (lever_ == Lever::HPattern) {
        r.rect(L.colX[0] - 2, L.rowMid - 2, L.colX[2] + 2, L.rowMid + 2, {0.35f, 0.35f, 0.4f});
        for (int c = 0; c < 3; ++c) {
            const bool has = c * 2 + 1 <= gears_;
            r.rect(L.colX[c] - 2, L.rowTop, L.colX[c] + 2, L.rowBot, has ? Color{0.35f, 0.35f, 0.4f} : Color{0.2f, 0.2f, 0.22f});
            if (has) r.text(L.colX[c] - 2, L.rowTop - 9, std::to_string(c * 2 + 1), 1, {0.85f, 0.85f, 0.85f});
            if (c * 2 + 2 <= gears_) r.text(L.colX[c] - 2, L.rowBot + 3, std::to_string(c * 2 + 2), 1, {0.85f, 0.85f, 0.85f});
        }
        // Takili vites yuvasi vurgusu + kol (surukleme sirasinda parmagi izler)
        if (knobGear_ > 0) r.circle(L.colX[(knobGear_ - 1) / 2], (knobGear_ % 2) ? L.rowTop : L.rowBot, 11, 14, {0.3f, 0.8f, 0.4f, 0.35f});
        const float kx = knobDrag_ ? dragX_ : knobX_, ky = knobDrag_ ? dragY_ : knobY_;
        r.circle(kx, ky, 10, 14, {0.05f, 0.05f, 0.06f, 0.6f});
        r.circle(kx, ky, 8, 14, grind ? Color{1.0f, 0.15f, 0.1f} : Color{0.92f, 0.92f, 0.95f});
    } else if (lever_ == Lever::Automatic) {
        r.rect(L.lever.cx() - 2, L.autoY[0], L.lever.cx() + 2, L.autoY[2], {0.35f, 0.35f, 0.4f});
        for (int i = 0; i < 3; ++i) {
            const bool on = (int)autoPos_ == i;
            r.text(L.lever.x0 + 8, L.autoY[i] - 7, kAutoName[i], 2, on ? kUiGold : kUiDim);
        }
        r.rect(L.lever.cx() - 12, L.autoY[(int)autoPos_] - 6, L.lever.cx() + 12, L.autoY[(int)autoPos_] + 6, {0.92f, 0.92f, 0.95f});
        if (autoPos_ == AutoPos::D && gear > 0) r.text(L.lever.x1 - 18, L.autoY[2] - 7, std::to_string(gear), 2, {0.6f, 0.9f, 1.0f});
    } else {
        button(r, L.seqUp, "+", Color{0.22f, 0.24f, 0.3f, 0.85f}, 3);
        button(r, L.seqDn, "-", Color{0.22f, 0.24f, 0.3f, 0.85f}, 3);
    }
}

} // namespace zk
