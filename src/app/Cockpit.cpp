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
const PadLayout kLand{{4, 40, 58, 356}, {62, 40, 116, 356}, {582, 40, 636, 356}, {494, 250, 578, 356},
                   {508, 536, 564}, 264, 303, 342, {494, 250, 578, 300}, {494, 306, 578, 356}, {266, 303, 340}};
const PadLayout kPort{{4, 420, 58, 636}, {62, 420, 116, 636}, {302, 420, 356, 636}, {214, 530, 298, 636},
                   {228, 256, 284}, 544, 583, 622, {214, 530, 298, 580}, {214, 586, 298, 636}, {546, 583, 620}};
const char* const kAutoName[3] = {"P", "N", "D"};

float sliderValue(const Rect& r, float y) { return std::clamp((r.y1 - 6 - y) / (r.y1 - r.y0 - 12), 0.0f, 1.0f); }
const PadLayout& lay(bool portrait) { return portrait ? kPort : kLand; }
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
    if (L.brake.hit(x, y)) return Ctl::Brake;
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

// H-desen: kol yalniz bos sirasinda yana kayar; sutunda yukari/asagi. Olmayan vitese girmez.
void Cockpit::shifterFromPoint(float x, float y) {
    const PadLayout& L = lay(portrait_);
    const bool inNeutralRow = std::fabs(knobY_ - L.rowMid) < 5.0f;
    if (inNeutralRow) knobX_ = std::clamp(x, L.colX[0], L.colX[2]);
    int col = 0;
    for (int c = 1; c < 3; ++c) if (std::fabs(knobX_ - L.colX[c]) < std::fabs(knobX_ - L.colX[col])) col = c;
    const float ny = std::clamp(y, L.rowTop, L.rowBot);
    if (std::fabs(ny - L.rowMid) >= 5.0f) {
        const int target = col * 2 + (ny < L.rowMid ? 1 : 2);
        if (target > gears_) { knobY_ = L.rowMid; return; }
        knobX_ = L.colX[col];
    }
    knobY_ = ny;
    if (knobY_ <= L.rowTop + 6) knobGear_ = col * 2 + 1;
    else if (knobY_ >= L.rowBot - 6) knobGear_ = col * 2 + 2;
    else if (std::fabs(knobY_ - L.rowMid) < 12) knobGear_ = 0;
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
    case Ctl::Brake: brakeUi_ = sliderValue(L.brake, y); break;
    case Ctl::Clutch: clutchUi_ = sliderValue(L.clutch, y); break;
    case Ctl::Shifter: shifterFromPoint(x, y); break;
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
        else if (t.ctl == Ctl::Brake) brakeUi_ = sliderValue(L.brake, y);
        else if (t.ctl == Ctl::Clutch) clutchUi_ = sliderValue(L.clutch, y);
        else if (t.ctl == Ctl::Shifter) shifterFromPoint(x, y);
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
    slider(L.brake, (float)brake(), "FREN", {0.85f, 0.15f, 0.15f, 0.85f});
    if (clutchPedal_) {
        slider(L.clutch, (float)clutch(), "DEBRIYAJ", {0.25f, 0.55f, 0.95f, 0.85f});
        const float span = L.clutch.y1 - L.clutch.y0 - 12;                // isirma bolgesi (pedal 0.32 .. 0.62)
        r.rect(L.clutch.x1 - 4, L.clutch.y1 - 6 - span * 0.62f, L.clutch.x1, L.clutch.y1 - 6 - span * 0.32f, {1.0f, 0.8f, 0.1f});
    }
    slider(L.thr, (float)throttle(), "GAZ", {0.95f, 0.55f, 0.1f, 0.9f});

    r.rect(L.lever.x0, L.lever.y0, L.lever.x1, L.lever.y1, {0.10f, 0.10f, 0.13f, 0.78f});
    if (lever_ == Lever::HPattern) {
        r.rect(L.colX[0] - 2, L.rowMid - 2, L.colX[2] + 2, L.rowMid + 2, {0.35f, 0.35f, 0.4f});
        for (int c = 0; c < 3; ++c) {
            const bool has = c * 2 + 1 <= gears_;
            r.rect(L.colX[c] - 2, L.rowTop, L.colX[c] + 2, L.rowBot, has ? Color{0.35f, 0.35f, 0.4f} : Color{0.2f, 0.2f, 0.22f});
            if (has) r.text(L.colX[c] - 2, L.rowTop - 9, std::to_string(c * 2 + 1), 1, {0.85f, 0.85f, 0.85f});
            if (c * 2 + 2 <= gears_) r.text(L.colX[c] - 2, L.rowBot + 3, std::to_string(c * 2 + 2), 1, {0.85f, 0.85f, 0.85f});
        }
        r.circle(knobX_, knobY_, 8, 12, grind ? Color{1.0f, 0.15f, 0.1f} : Color{0.92f, 0.92f, 0.95f});
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
