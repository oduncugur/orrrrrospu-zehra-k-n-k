#include "Screens.h"
#include "Gauges.h"
#include "Ui.h"
#include "app/Hints.h"
#include "app/Looks.h"
#include "garage/VehicleCatalog.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace zk {

namespace {
// ---- yerlesim (sanal 640x360) ----
constexpr float kPx = 22.0f;                         // piksel / metre (yakin serit)
constexpr float kWorldTop = 30, kHorizon = 150, kWall0 = 180, kFar0 = 186, kFarGround = 211, kBarrier0 = 214,
                kNear0 = 218, kNearGround = 252, kWorldBottom = 262;
constexpr float kClutch[4] = {4, 150, 58, 356}, kThrottle[4] = {582, 150, 636, 356};   // alt yari: basparmak az yol alir
constexpr float kBrake[4] = {436, 268, 480, 354};
constexpr float kShift[4] = {484, 250, 578, 356};   // vites alani buyutuldu
constexpr float kColX[3] = {500, 531, 562};
constexpr float kRowTop = 266, kRowMid = 303, kRowBot = 340;
constexpr float kPadDn[4] = {484, 262, 528, 350}, kPadUp[4] = {534, 262, 578, 350};
constexpr float kStageBtn[4] = {250, 120, 390, 156};
constexpr float kLcDn[4] = {352, 62, 382, 88}, kLcUp[4] = {484, 62, 514, 88};   // 2-step kalkis devri (yesilden once)
constexpr float kDist[4] = {140, 62, 244, 88};                                      // yaris mesafesi (yesilden once)
const double kDistM[3] = {DragRace::kQuarterMile, DragRace::kHalfMile, DragRace::kMile};
const char* const kDistName[3] = {"1/4 MIL", "1/2 MIL", "1 MIL"};
constexpr float kAgain[4] = {120, 262, 250, 296}, kGarage[4] = {260, 262, 390, 296}, kGraph[4] = {400, 262, 520, 296};
constexpr float kCamLead = 8.0f;                     // oyuncu arac merkezi, ekranin solundan 8 m sagda

bool in(const float* r, float x, float y) { return x >= r[0] && x <= r[2] && y >= r[1] && y <= r[3]; }
std::string up(const std::string& s) { std::string o = s; for (char& c : o) c = (char)std::toupper((unsigned char)c); return o; }
std::string upperS(const std::string& s) { return up(s); }
float sliderValue(const float* r, float y) { return std::clamp((r[3] - 6 - y) / (r[3] - r[1] - 12), 0.0f, 1.0f); }
float hash01(int i) { unsigned x = (unsigned)i * 2654435761u; x ^= x >> 13; x *= 0x5bd1e995u; x ^= x >> 15; return (x & 0xFFFF) / 65535.0f; }
std::string sec(double t) { if (t < 0) return "--.---"; char b[16]; std::snprintf(b, sizeof b, "%6.3f", t); return b; }

const Color kDim{0.18f, 0.16f, 0.12f}, kAmber{1.0f, 0.62f, 0.05f}, kGreen{0.1f, 1.0f, 0.25f}, kRed{1.0f, 0.1f, 0.1f};
} // namespace

DragScreen::DragScreen(App& app, int playerCar, int opponentCar, const Tune* playerTune, const Tune* opponentTune, bool career)
    : app_(app), seed_(1234), career_(career) {
    app_.hint(HintDrag, hintTexts()[HintDrag]);
    carIds_[0] = playerCar; carIds_[1] = opponentCar;
    if (playerTune) { tunes_[0] = *playerTune; hasTune_[0] = true; }
    if (opponentTune) { tunes_[1] = *opponentTune; hasTune_[1] = true; }
    app_.setVoiceTuned(0, playerCar, hasTune_[0] ? &tunes_[0] : nullptr);
    app_.setVoiceTuned(1, opponentCar, hasTune_[1] ? &tunes_[1] : nullptr);
    restart();
}

void DragScreen::restart() {
    seed_ = seed_ * 1103515245u + 12345u;
    race_ = std::make_unique<DragRace>(carIds_[0], carIds_[1], app_.treePro ? TreeType::Pro : TreeType::Sportsman, seed_, true,
                                       hasTune_[0] ? &tunes_[0] : nullptr, hasTune_[1] ? &tunes_[1] : nullptr);
    race_->setOpponentHandicap(app_.activeEvent >= 0 || app_.activeTour || app_.activeMeet ? app_.eventHandicap : 1.6);
    race_->setLength(kDistM[std::clamp(app_.settings.dragDist, 0, 2)]);   // rakip insan gibi hata yapar
    if (career_ && app_.career.car().carId == carIds_[0]) race_->lane(0).sim->setNosFill(app_.career.car().nosFill);   // tupte kalan
    rewarded_ = false; prize_ = 0;
    tel_.clear(); telT_ = telAcc_ = 0; showGraph_ = std::getenv("ZK_GRAPH") != nullptr;
    run_.clear(); runAcc_ = 0; ghostSaved_ = false; ghost_.clear(); ghostEt_ = 0;
    if (auto it = app_.ghosts.find(carIds_[0]); it != app_.ghosts.end()) { ghost_ = it->second.d; ghostEt_ = it->second.et; }
    smoke_.clear(); ticker_.clear(); touches_.clear();
    clutchUi_ = throttleUi_ = 0; brakeBtn_ = false;
    knobX_ = kColX[0]; knobY_ = kRowTop; pendingGear_ = 1; pendingPaddle_ = 0;
    finishedT_ = 0; t_ = 0;
    hapGear_ = 1; hapFlat_ = 0; hapLeft_ = hapBroke_ = hapRed_ = false; hapLimiterT_ = 0;
    race_->setPlayerAutopilot(autopilot_);
    race_->setPlayerTractionControl(app_.settings.assist);       // yalniz aracta TC varsa (fabrika / ECU kiti)
}

DragScreen::Ctl DragScreen::hit(float x, float y) const {
    if (in(kClutch, x, y)) return Ctl::Clutch;
    if (in(kThrottle, x, y)) return Ctl::Throttle;
    if (in(kBrake, x, y)) return Ctl::Brake;
    const Gearbox box = race_->lane(0).sim->gearboxType();
    if (box == Gearbox::HPattern && in(kShift, x, y)) return Ctl::Shifter;
    if (box == Gearbox::Dogbox || box == Gearbox::DCT) {
        if (in(kPadUp, x, y)) return Ctl::PaddleUp;
        if (in(kPadDn, x, y)) return Ctl::PaddleDown;
    }
    return Ctl::None;
}

// H-desen (kolay gecis): dokunulan/surtulen noktaya en yakin vites yuvasi secilir ve kol oraya oturur; orta siranin
// yakininda bos (N). Eskiden kol bos sirasindan yana kaydirilip tam isabetle itilmek zorundaydi (telefonda zordu).
// Gercek kol gibi (Cockpit ile ayni): surukleme sirasinda kol kanalda parmagi izler, vites yuvanin dibine oturunca
// ya da parmak kalkinca takilir; aradan gecerken bos / baska vites istegi gitmez.
void DragScreen::shifterFromPoint(float x, float y, bool release) {
    const int gears = race_->lane(0).sim->powertrain().gearCount();
    const float half = (kRowBot - kRowTop) * 0.5f, midBand = half * 0.30f;
    int col = 0;
    for (int c = 1; c < 3; ++c) if (std::fabs(x - kColX[c]) < std::fabs(x - kColX[col])) col = c;
    int target = col * 2 + (y < kRowMid ? 1 : 2);
    while (target > gears && target > 2) target -= 2;                // olmayan sutun: en yakin var olan vites
    if (target > gears) target = gears;
    const float depth = std::fabs(y - kRowMid);
    auto seat = [&](int g) {
        if (g != pendingGear_) app_.haptic(18, 160);
        pendingGear_ = g;
        if (g == 0) { knobX_ = std::clamp(x, kColX[0], kColX[2]); knobY_ = kRowMid; }
        else { knobX_ = kColX[(g - 1) / 2]; knobY_ = (g % 2) ? kRowTop : kRowBot; }
    };
    if (release) { knobDrag_ = false; seat(depth < midBand ? 0 : target); return; }
    knobDrag_ = true;
    if (depth < midBand) { dragX_ = std::clamp(x, kColX[0], kColX[2]); dragY_ = std::clamp(y, kRowTop, kRowBot); }
    else { dragX_ = kColX[(target - 1) / 2]; dragY_ = std::clamp(y, kRowTop, kRowBot); }
    if (depth > half * 0.60f && target != pendingGear_) seat(target);
}

// Kalkis devri: 250 rpm adim; kariyer aracinda araca kaydedilir (yaris icindeki sim ayni Tune'u okur)
void DragScreen::adjustLaunch(int delta) {
    const VehicleSim& s = *race_->lane(0).sim;
    const int cur = (int)std::lround(s.defaultLaunchRpm());
    const int lo = (int)(s.engineSpec().idleRpm + 600.0), hi = (int)(s.engineSpec().redlineRpm - 200.0);
    tunes_[0].launchRpm = std::clamp((cur + delta) / 250 * 250, lo / 250 * 250 + 250, hi / 250 * 250);
    race_->setPlayerLaunchRpm(tunes_[0].launchRpm);
    if (career_ && app_.career.car().carId == carIds_[0]) {
        app_.career.cars[app_.career.current].tune.launchRpm = tunes_[0].launchRpm;
        app_.saveCareer();
    }
    app_.haptic(12, 120);
}

void DragScreen::pointerDown(int id, float x, float y) {
    const RacePhase ph = race_->phase();
    if (ph == RacePhase::Finished && finishedT_ > 1.5) {
        if (in(kAgain, x, y)) { restart(); return; }
        if (in(kGarage, x, y)) { app_.goGarage(); return; }
        if (in(kGraph, x, y)) { showGraph_ = !showGraph_; return; }
    }
    if (ph == RacePhase::Burnout && in(kStageBtn, x, y)) { race_->skipBurnout(); return; }
    if ((ph == RacePhase::Burnout || ph == RacePhase::Staging) && in(kDist, x, y)) {   // mesafe: 1/4 -> 1/2 -> 1 mil (yaris yeniden kurulur)
        app_.settings.dragDist = (app_.settings.dragDist + 1) % 3; app_.saveSettings();
        restart();
        return;
    }
    if ((ph == RacePhase::Burnout || ph == RacePhase::Staging || ph == RacePhase::Tree) && hasTune_[0] && launchControlAvailable(tunes_[0])
        && race_->lane(0).sim->gearboxType() != Gearbox::TorqueConverter && (in(kLcDn, x, y) || in(kLcUp, x, y))) {
        adjustLaunch(in(kLcUp, x, y) ? +250 : -250);
        return;
    }
    const Ctl c = hit(x, y);
    touches_.push_back({id, c});
    switch (c) {
    case Ctl::Clutch: clutchUi_ = sliderValue(kClutch, y); break;
    case Ctl::Throttle: throttleUi_ = sliderValue(kThrottle, y); break;
    case Ctl::Brake: brakeBtn_ = true; break;
    case Ctl::Shifter: shifterFromPoint(x, y); lastShX_ = x; lastShY_ = y; break;
    case Ctl::PaddleUp: pendingPaddle_ = +1; break;
    case Ctl::PaddleDown: pendingPaddle_ = -1; break;
    default: break;
    }
}

void DragScreen::pointerMove(int id, float x, float y) {
    for (const Touch& t : touches_) {
        if (t.id != id) continue;
        if (t.ctl == Ctl::Clutch) clutchUi_ = sliderValue(kClutch, y);
        else if (t.ctl == Ctl::Throttle) throttleUi_ = sliderValue(kThrottle, y);
        else if (t.ctl == Ctl::Shifter) { shifterFromPoint(x, y); lastShX_ = x; lastShY_ = y; }
    }
}

void DragScreen::pointerUp(int id) {
    for (size_t i = 0; i < touches_.size(); ++i) {
        if (touches_[i].id != id) continue;
        switch (touches_[i].ctl) {
        case Ctl::Clutch: clutchUi_ = 0.0f; break;      // ayak pedaldan kalkti
        case Ctl::Throttle: throttleUi_ = 0.0f; break;
        case Ctl::Brake: brakeBtn_ = false; break;
        case Ctl::Shifter: shifterFromPoint(lastShX_, lastShY_, true); break;   // birakinca yuvaya otur
        default: break;
        }
        touches_.erase(touches_.begin() + i);
        return;
    }
}

void DragScreen::key(Key k, bool down) {
    switch (k) {
    case Key::Throttle: keyThr_ = down; break;
    case Key::Clutch: keyClutch_ = down; break;
    case Key::Brake: keyBrake_ = down; break;
    default: break;
    }
    if (!down) return;
    const Gearbox box = race_->lane(0).sim->gearboxType();
    const int gear = race_->lane(0).sim->powertrain().gear();
    auto setKnob = [&](int g) {
        pendingGear_ = g;
        if (g == 0) { knobY_ = kRowMid; return; }
        knobX_ = kColX[(g - 1) / 2]; knobY_ = (g % 2) ? kRowTop : kRowBot;
    };
    if (k >= Key::Gear0 && k <= Key::Gear6 && box == Gearbox::HPattern) {
        const int g = (int)k - (int)Key::Gear0;
        if (g <= race_->lane(0).sim->powertrain().gearCount()) setKnob(g);
    }
    if (k == Key::ShiftUp || k == Key::ShiftDown) {
        const int d = k == Key::ShiftUp ? 1 : -1;
        if (box == Gearbox::HPattern) setKnob(std::clamp(gear + d, 0, race_->lane(0).sim->powertrain().gearCount()));
        else pendingPaddle_ = d;
    }
    if (k == Key::Enter) {
        if (race_->phase() == RacePhase::Burnout) race_->skipBurnout();
        else if (race_->phase() == RacePhase::Finished && finishedT_ > 1.5) restart();
    }
    if (k == Key::Back) app_.goGarage();
}

void DragScreen::update(double dt) {
    t_ += dt;
    // Klavye analog rampalari: debriyaj yavas birakilir (kalkis icin), gaz hizli
    float& kc = keyClutchVal_; float& kt = keyThrVal_;
    kc = keyClutch_ ? std::min(1.0f, kc + (float)dt / 0.08f) : std::max(0.0f, kc - (float)dt / 0.35f);
    kt = keyThr_ ? std::min(1.0f, kt + (float)dt / 0.12f) : std::max(0.0f, kt - (float)dt / 0.08f);
    pc_.clutch = std::max(clutchUi_, kc);
    pc_.throttle = std::max(throttleUi_, kt);
    pc_.brake = (keyBrake_ || brakeBtn_) ? 1.0 : 0.0;
    const Gearbox box = race_->lane(0).sim->gearboxType();
    pc_.requestedGear = box == Gearbox::HPattern ? pendingGear_ : -1;
    pc_.paddle = pendingPaddle_;
    pendingPaddle_ = 0;
    race_->advance(dt, pc_);
    {   // Hayalet izi: kalkistan bitise 20 Hz mesafe; bitiste en iyiyse saklanir
        const LaneState& L0 = race_->lane(0);
        if (L0.left && !L0.slip.finished && run_.size() < 1200 && race_->isQuarter())
            while (runAcc_ <= race_->clock() - L0.leaveTime) { run_.push_back((float)L0.sim->distance()); runAcc_ += 0.05; }
        if (L0.slip.finished && !ghostSaved_) {
            ghostSaved_ = true;
            const TimeSlip& sl = L0.slip;
            if (!sl.redLight && !sl.broke && sl.quarter > 0 && race_->isQuarter()) {   // hayalet yalniz 1/4 mil
                App::Ghost& g = app_.ghosts[carIds_[0]];
                if (g.et <= 0 || sl.quarter < g.et) {
                    run_.push_back(402.336f);
                    g.d = run_; g.et = sl.quarter;
                    app_.saveGhosts();
                    ticker_.push_back(ghostEt_ > 0 ? "YENI EN IYI KOSU: HAYALET GUNCELLENDI" : "HAYALET KAYDEDILDI: SONRAKI YARISTA KENDINLE YARIS");
                    tickerT_ = 3.0;
                }
            }
        }
    }
    if (race_->phase() == RacePhase::Run && !race_->lane(0).slip.finished && tel_.size() < 2400) {   // telemetri
        telT_ += dt; telAcc_ += dt;
        if (telAcc_ >= 1.0 / 30.0 || tel_.empty()) {
            telAcc_ = 0;
            const VehicleSim& ps = *race_->lane(0).sim;
            double slip = 0;
            for (int i = 0; i < 4; ++i) { const WheelSimulation& w = ps.wheel(i); if (w.Fz() > 100) slip = std::max(slip, w.omega() * w.rEff() - ps.speed()); }
            tel_.push_back({(float)telT_, (float)ps.speed(), (float)const_cast<VehicleSim&>(ps).powertrain().rpm(), (float)(race_->lane(1).slip.finished ? -1.0 : race_->lane(1).sim->speed()),
                            (float)slip, const_cast<VehicleSim&>(ps).powertrain().gear()});
        }
    }
    {
        const int g = race_->lane(0).sim->powertrain().gear();
        if (g != lastGear_) { if (lastGear_ > -2) app_.sfxShift(); lastGear_ = g; }
        app_.nitrousSound(race_->lane(0).sim->nitrousActive());
    }
    // Debriyajsiz vites girmedi: kol gercek vitese geri seker (eskiden kol yeni viteste kalip debriyaja basilinca
    // vites aniden giriyordu; kol ile gercek vites ayrisiyordu)
    if (box == Gearbox::HPattern && race_->lane(0).grind) {
        const int g = race_->lane(0).sim->powertrain().gear();
        pendingGear_ = g;
        if (g == 0) { knobY_ = kRowMid; }
        else { knobX_ = kColX[(g - 1) / 2]; knobY_ = (g % 2) ? kRowTop : kRowBot; }
        app_.haptic(60, 200);
    }
    for (auto& e : race_->lane(0).sim->drainFailEvents()) { ticker_.push_back("SEN: " + e); tickerT_ = 3.0; app_.haptic(400, 255); }
    for (int l = 0; l < 2; ++l) {                                        // gorsel teker donusu (tahrikli teker: patinaj gorunur)
        const VehicleSim& s = *race_->lane(l).sim;
        spinD_[l] += (float)std::clamp(s.wheel(s.drivenLeft()).omega() * dt, -0.55, 0.55);
    }
    for (auto& e : race_->drainEvents()) {
        const bool mine = e.rfind("SEN:", 0) == 0;
        if (e == "YESIL!") { flash_ = "YESIL!"; flashColor_ = {0.2f, 1.0f, 0.3f}; flashT_ = 0.9; continue; }
        if (e == "KAZANDIN!" || e == "KAYBETTIN") { flash_ = e; flashColor_ = e == "KAZANDIN!" ? Color{0.2f, 1.0f, 0.3f} : Color{1.0f, 0.25f, 0.2f}; flashT_ = 1.4; continue; }
        if (mine && e.find("KIRMIZI") != std::string::npos) { flash_ = "KIRMIZI ISIK!"; flashColor_ = {1.0f, 0.15f, 0.1f}; flashT_ = 1.6; }
        if (mine && e.find("CITIRTISI") != std::string::npos) { flash_ = "DEBRIYAJ!"; flashColor_ = {1.0f, 0.6f, 0.1f}; flashT_ = 0.6; }
        ticker_.push_back(e); tickerT_ = 3.0;
        if (ticker_.size() > 2) ticker_.erase(ticker_.begin());
    }
    tickerT_ -= dt; flashT_ -= dt;
    if (tickerT_ <= 0 && !ticker_.empty()) { ticker_.erase(ticker_.begin()); tickerT_ = ticker_.empty() ? 0 : 2.0; }
    if (race_->phase() == RacePhase::Finished) finishedT_ += dt;
    // Kariyer: sonuc bir kez islenir ve kaydedilir (otopilotta degil)
    if (career_ && !autopilot_ && !rewarded_ && race_->phase() == RacePhase::Finished) {
        rewarded_ = true;
        const bool won = race_->winner() == 0;
        const TimeSlip& s = race_->lane(0).slip;
        const Opponent& o = app_.lastOpp;
        const double diff = o.carId == race_->lane(1).car->id && o.estEt > 0 && o.playerEt > 0 ? prizeDifficulty(o.playerEt - o.estEt) : 1.0;
        if (app_.activeEvent >= 0) {                                    // lig etkinligi
            int pink = 0;
            prize_ = app_.career.recordEvent(app_.activeEvent, won, s.finished && !s.redLight && race_->isQuarter() ? s.quarter : 0.0, 0, &pink);
            if (pink > 0) app_.eventNote = "PINK SLIP: " + upperS(findVehicle(pink)->model) + " SENIN!";
            else if (pink < 0) app_.eventNote = "PINK SLIP: ARABANI KAYBETTIN";
            else if (const int rv = leagueEvents()[app_.activeEvent].rival; *bossLine(rv, 0))   // patron: yaris sonrasi sozu
                app_.eventNote = std::string(rivals()[rv].name) + ": " + bossLine(rv, won ? 1 : 2);
        } else if (app_.activeTour) {                                   // haftalik turnuva turu
            prize_ = app_.career.recordTour(won);
            char nb[64];
            if (!won) std::snprintf(nb, sizeof nb, "TURNUVA: %d. TURDA ELENDIN", app_.career.tourRound + 1);
            else if (prize_ > 0) std::snprintf(nb, sizeof nb, "TURNUVA SAMPIYONU! +%s", money(prize_).c_str());
            else std::snprintf(nb, sizeof nb, "TURNUVA: %d. TUR KAZANILDI", app_.career.tourRound);
            app_.eventNote = nb;
        } else if (app_.activeMeet) {                                   // gece bulusmasi
            long fine = 0;
            prize_ = app_.career.recordMeet(won, &fine);
            char nb[96];
            std::snprintf(nb, sizeof nb, "%s%s", won ? ("BULUSMA KAZANILDI +" + money(prize_)).c_str() : "BULUSMA KAYBEDILDI",
                          fine > 0 ? ("  POLIS BASKINI! CEZA " + money(fine)).c_str() : "");
            app_.eventNote = nb;
        } else app_.career.recordRace(*race_->lane(1).car, won, s.finished && !s.redLight && race_->isQuarter() ? s.quarter : 0.0, &prize_, diff);
        const VehicleSim& ps = *race_->lane(0).sim;
        app_.career.recordDamage(s.broke, ps.failure().bearingDamage(), ps.failure().bearingSpun(), ps.gearboxBroken(), ps.engineStress(), ps.tireWearGained());
        if (app_.career.car().carId == carIds_[0]) app_.career.recordNosUse(ps.nitrousLeft());
        app_.saveCareer();
    }

    // ---- duman: tahrikli tekerlek kayma hizi yuksekse ----
    for (int lane = 0; lane < 2; ++lane) {
        const VehicleSim& s = *race_->lane(lane).sim;
        const VehicleDef* v = race_->lane(lane).car;
        const double wb = v->wheelbaseM, L = v->lengthM;
        const double xFront = L * 0.5 - (L - wb) * (v->frontWeight > 0.5 ? 0.54 : 0.44), xRear = xFront - wb;
        for (int side = 0; side < 2; ++side) {
            const int wi = side ? s.drivenRight() : s.drivenLeft();
            const WheelSimulation& w = s.wheel(wi);
            const double slip = std::fabs(w.omega() * w.rEff() - s.speed());
            if (slip < 3.0 || std::fabs(w.Fx()) < 500) continue;
            const float n = (float)std::min(1.0, (slip - 3.0) / 10.0);
            if (hash01((int)(t_ * 997) + lane * 31 + side) > 0.35f + 0.6f * n) continue;
            const double wx = s.distance() + (wi < 2 ? xFront : xRear);
            smoke_.push_back({(float)wx, (float)lane, 0.25f, (float)(-0.3 * s.speed() - 1.0 + 2.0 * hash01((int)(t_ * 331))),
                              0.5f + 0.8f * hash01((int)(t_ * 713) + 5), 1.6f, 0.35f});
        }
    }
    for (Smoke& p : smoke_) { p.x += p.vx * (float)dt; p.y += p.vy * (float)dt; p.vy *= 0.98f; p.size += (float)dt * 1.1f; p.life -= (float)dt; }
    smoke_.erase(std::remove_if(smoke_.begin(), smoke_.end(), [](const Smoke& p) { return p.life <= 0; }), smoke_.end());
    if (smoke_.size() > 400) smoke_.erase(smoke_.begin(), smoke_.begin() + (smoke_.size() - 400));

    camX_ = (float)race_->lane(0).sim->distance() - kCamLead;

    // ---- haptik (oyuncu serdi) ----
    {
        const LaneState& P = race_->lane(0);
        const PowertrainCore& pt = P.sim->powertrain();
        if (P.left && !hapLeft_) app_.haptic(45, 220);                       // kalkis vurusu
        if (pt.gear() != hapGear_ && pt.gear() > 0) app_.haptic(22, 150);   // vites
        if (P.slip.broke && !hapBroke_) app_.haptic(320, 255);              // aks kirildi
        if (P.slip.redLight && !hapRed_) app_.haptic(180, 200);
        hapLimiterT_ -= dt;
        if ((pt.limiterHit() || P.cutIgnition) && hapLimiterT_ <= 0) { app_.haptic(10, 90); hapLimiterT_ = 0.06; }   // kesici tirtiklamasi
        int flat = 0;
        for (int i = 0; i < 4; ++i) flat += P.sim->wheel(i).hapticPulseCount();
        if (flat > hapFlat_) app_.haptic(14, (int)std::clamp(80 + 400 * P.sim->hapticIntensity(), 80.0, 255.0));   // flat-spot turu
        hapLeft_ = P.left; hapGear_ = pt.gear() > 0 ? pt.gear() : hapGear_; hapBroke_ = P.slip.broke; hapRed_ = P.slip.redLight; hapFlat_ = flat;
    }

    // ---- ses ----
    for (int lane = 0; lane < 2; ++lane) {
        const LaneState& L = race_->lane(lane);
        const PowertrainCore& pt = L.sim->powertrain();
        float gain = 1.0f;
        if (lane == 1) {
            const double gap = std::fabs(L.sim->distance() - race_->lane(0).sim->distance());
            gain = (float)(0.45 * std::clamp(1.0 - gap / 150.0, 0.15, 1.0));
        }
        app_.voice(lane, pt.rpm(), pt.throttleEffective(), pt.limiterHit() || L.cutIgnition, pt.gear() > 0, gain);
        double slip = 0;
        for (int i = 0; i < 4; ++i) {
            const WheelSimulation& w = L.sim->wheel(i);
            if (w.Fz() > 100) slip = std::max(slip, std::fabs(w.omega() * w.rEff() - L.sim->speed()));
        }
        app_.tire(lane, slip);
    }
}

// ------------------------------------------------------------------ cizim
double DragScreen::ghostTimeAt(double d) const {
    for (size_t i = 1; i < ghost_.size(); ++i)
        if (ghost_[i] >= d) {
            const double a = ghost_[i - 1], b = ghost_[i];
            return (i - 1 + (b > a ? (d - a) / (b - a) : 0.0)) * 0.05;
        }
    return -1;
}

void DragScreen::drawCarAt(Renderer& r, int lane, float sx, float groundY, float scale, float alpha) {
    const LaneState& L = race_->lane(lane);
    const VehicleDef* v = L.car;
    const float px = kPx * scale;
    const float w = ((float)v->lengthM + 2.0f) * px, h = ((float)v->heightM + 1.6f) * px;
    const float bottom = -1.3f;                       // ortho alt siniri (m); zemin buradan ~0.41 m yukarida gorunur
    const float y = groundY - h + 0.41f * px;
    const Mat4 proj = matOrtho(-w / 2 / px, w / 2 / px, bottom, bottom + h / px, -20, 20);
    const Mat4 view = matLookAt(0, 2.2f, 10, 0, 0.9f, 0);
    const Suspension& su = L.sim->suspension();
    const Mat4 model = alpha < 1.0f ? matTranslate(0, 0, 0) : matMul(matTranslate(0, (float)su.heave(), 0), matRotZ((float)(su.pitchDeg() * 3.14159265 / 180.0)));
    Renderer::CarLook lk = lane == 0 && career_ && app_.career.car().carId == carIds_[0] ? lookOf(app_.career.car())
                                                                                        : lookOf(hasTune_[lane] ? &tunes_[lane] : nullptr);
    if (alpha < 1.0f) { lk.alpha = alpha; lk.paintOn = true; lk.paint[0] = 0.55f; lk.paint[1] = 0.85f; lk.paint[2] = 1.0f; lk.stripe = 0; }
    r.setCarLook(lk);
    r.drawCar(v->id, sx - w / 2, y, w, h, proj, view, model, alpha < 1.0f ? 0.0f : spinD_[lane]);
    if (alpha < 1.0f) return;                                            // hayalet: alev yok

    // Egzoz alevi: devir kesici / dogbox atesleme kesme
    const PowertrainCore& pt = L.sim->powertrain();
    if ((pt.limiterHit() || L.cutIgnition) && pt.rpm() > 0.6 * L.sim->engineSpec().redlineRpm && hash01((int)(t_ * 60) + lane) > 0.3f) {
        const float ex = sx - (float)v->lengthM * 0.5f * px, ey = groundY - 0.28f * px;
        const float len = (10 + 14 * hash01((int)(t_ * 120) + 7 * lane)) * scale;
        r.tri(ex, ey - 3 * scale, ex, ey + 3 * scale, ex - len, ey, {1.0f, 0.55f, 0.1f, 0.9f});
        r.tri(ex, ey - 1.5f * scale, ex, ey + 1.5f * scale, ex - len * 0.6f, ey, {1.0f, 0.95f, 0.5f, 0.95f});
    }
}

void DragScreen::drawWorld(Renderer& r) {
    const float cam = camX_;
    auto sx = [&](float wx, float factor) { return 64.0f + (wx - cam * factor) * kPx; };
    // Gokyuzu, gunes
    r.gradientV(0, kWorldTop, 640, kHorizon, {0.16f, 0.22f, 0.48f}, {0.96f, 0.60f, 0.38f});
    r.circle(470 - cam * 0.2f * 0.02f * kPx, 118, 22, 16, {1.0f, 0.86f, 0.58f});
    // Daglar (paralaks 0.04)
    for (int layer = 0; layer < 2; ++layer) {
        const float f = layer ? 0.07f : 0.04f, period = layer ? 180.0f : 260.0f;
        const Color c = layer ? Color{0.26f, 0.17f, 0.30f} : Color{0.36f, 0.24f, 0.40f};
        const float off = std::fmod(cam * f * kPx, period);
        for (int i = -1; i < 640 / (int)period + 3; ++i) {
            const float x0 = i * period - off;
            const float hgt = 30 + 30 * hash01(i + (int)(cam * f * kPx / period) + layer * 100);
            r.tri(x0, kHorizon, x0 + period * 0.5f, kHorizon - hgt, x0 + period, kHorizon, c);
        }
    }
    // Sehir silueti (0.12)
    {
        const float f = 0.12f, period = 36.0f, off = std::fmod(cam * f * kPx, period);
        const int base = (int)(cam * f * kPx / period);
        for (int i = -1; i < 20; ++i) {
            const float x0 = i * period - off, hgt = 12 + 34 * hash01(base + i + 500);
            r.rect(x0, kHorizon - hgt, x0 + period - 4, kHorizon, {0.14f, 0.12f, 0.22f});
            for (int wy = 0; wy < (int)(hgt / 8); ++wy)
                if (hash01(base + i * 7 + wy) > 0.55f) r.rect(x0 + 6, kHorizon - hgt + 4 + wy * 8, x0 + 10, kHorizon - hgt + 7 + wy * 8, {0.95f, 0.8f, 0.4f});
        }
    }
    // Tribun (0.55) + seyirci + isik direkleri
    r.rect(0, kHorizon, 640, kWall0, {0.22f, 0.22f, 0.26f});
    {
        const float f = 0.55f, period = 8.0f, off = std::fmod(cam * f * kPx, period);
        const int base = (int)(cam * f * kPx / period);
        for (int row = 0; row < 4; ++row) {
            r.rect(0, kHorizon + 4 + row * 7, 640, kHorizon + 5 + row * 7, {0.3f, 0.3f, 0.34f});
            for (int i = -1; i < 82; ++i) {
                const float h = hash01((base + i) * 13 + row);
                if (h < 0.35f) continue;
                const Color cc = h > 0.85f ? Color{0.9f, 0.2f, 0.2f} : h > 0.7f ? Color{0.2f, 0.4f, 0.9f} : h > 0.55f ? Color{0.95f, 0.85f, 0.3f} : Color{0.85f, 0.85f, 0.85f};
                const float x = i * period - off + (row % 2) * 4;
                r.rect(x, kHorizon + row * 7, x + 3, kHorizon + row * 7 + 4, cc);
            }
        }
        const float pf = 0.55f, pp = 60.0f * kPx * pf;
        const float poff = std::fmod(cam * pf * kPx, pp);
        for (int i = -1; i < 4; ++i) {
            const float x = i * pp - poff + 100;
            r.rect(x, 60, x + 3, kWall0, {0.35f, 0.35f, 0.38f});
            r.rect(x - 10, 56, x + 13, 62, {0.95f, 0.95f, 0.8f});
        }
    }
    // Duvar + reklam panolari (1.0)
    r.rect(0, kWall0, 640, kFar0, {0.72f, 0.72f, 0.68f});
    for (int i = (int)(cam / 30.0f) - 1; i < (int)(cam / 30.0f) + 3; ++i) {
        const float x = sx(i * 30.0f + 12.0f, 1.0f);
        static const char* ads[] = {"100 OKTAN", "SLICK", "ZK MOTOR", "KINIK", "DYNO"};
        static const Color adc[] = {{0.8f, 0.1f, 0.1f}, {0.1f, 0.2f, 0.6f}, {0.95f, 0.7f, 0.1f}, {0.1f, 0.5f, 0.2f}, {0.2f, 0.2f, 0.2f}};
        const int k = ((i % 5) + 5) % 5;
        r.rect(x, kWall0 - 12, x + 88, kWall0, adc[k]);
        r.textCentered(x + 44, kWall0 - 10, ads[k], 1, {1, 1, 1});
    }
    // Seritler
    r.rect(0, kFar0, 640, kBarrier0, {0.20f, 0.20f, 0.22f});
    r.rect(0, kFar0 + 12, 640, kFar0 + 20, {0.15f, 0.15f, 0.16f});           // lastik izi
    r.rect(0, kBarrier0, 640, kNear0, {0.78f, 0.78f, 0.74f});
    r.rect(0, kNear0, 640, kWorldBottom - 4, {0.23f, 0.23f, 0.25f});
    r.rect(0, kNear0 + 18, 640, kNear0 + 30, {0.17f, 0.17f, 0.18f});
    r.rect(0, kWorldBottom - 6, 640, kWorldBottom - 4, {0.9f, 0.9f, 0.9f});
    r.rect(0, kWorldBottom - 4, 640, kWorldBottom, {0.16f, 0.32f, 0.14f});
    // 10 m isaretleri
    for (int m = (int)(cam / 10.0f) * 10 - 10; m < cam + 32; m += 10) {
        const float x = sx((float)m, 1.0f);
        r.rect(x, kBarrier0, x + 2, kNear0, {0.3f, 0.3f, 0.3f});
    }
    // Baslangic ve bitis cizgisi, tabelalar
    const float start = sx(0.0f, 1.0f);
    r.rect(start - 1, kFar0, start + 2, kWorldBottom - 4, {0.95f, 0.95f, 0.95f});
    const float fin = sx((float)race_->length(), 1.0f);
    for (int i = 0; i < 16; ++i)
        for (int j = 0; j < 2; ++j)
            r.rect(fin + j * 5, kFar0 + i * 4.75f, fin + j * 5 + 5, kFar0 + (i + 1) * 4.75f, ((i + j) % 2) ? Color{0.05f, 0.05f, 0.05f} : Color{0.95f, 0.95f, 0.95f});
    struct Mark { double m; const char* label; };
    static const Mark marks[] = {{18.288, "60 FT"}, {100.584, "330 FT"}, {201.168, "1/8"}, {304.8, "1000 FT"}, {402.336, "1/4"},
                                 {804.672, "1/2"}, {1207.0, "3/4"}, {1609.344, "1 MIL"}};
    for (const Mark& mk : marks) {
        if (mk.m > race_->length() + 1.0) continue;
        const bool finish = mk.m > race_->length() - 1.0;
        const float x = sx((float)mk.m, 1.0f);
        if (x < -80 || x > 720) continue;
        r.rect(x - 1, kWall0 - 34, x + 1, kWall0, {0.3f, 0.3f, 0.3f});
        r.rect(x - 34, kWall0 - 46, x + 34, kWall0 - 32, {0.1f, 0.1f, 0.1f});
        r.textCentered(x, kWall0 - 43, finish ? "BITIS" : mk.label, 1, {1.0f, 0.85f, 0.2f});
    }
    // Pist agaci (baslangicta, bariyer uzerinde)
    {
        const float x = sx(-1.2f, 1.0f);
        const int lights = race_->treeLights();
        r.rect(x - 1, kBarrier0 - 44, x + 1, kBarrier0, {0.2f, 0.2f, 0.2f});
        r.rect(x - 6, kBarrier0 - 50, x + 6, kBarrier0 - 12, {0.12f, 0.12f, 0.12f});
        const Color c[5] = {(lights & 1) ? kAmber : kDim, (lights & 2) ? kAmber : kDim, (lights & 4) ? kAmber : kDim,
                            (lights & 8) ? kGreen : kDim, (lights & 16) ? kRed : kDim};
        for (int i = 0; i < 5; ++i) r.rect(x - 3, kBarrier0 - 47 + i * 7, x + 3, kBarrier0 - 42 + i * 7, c[i]);
    }
}

void DragScreen::drawHud(Renderer& r) {
    const LaneState& P = race_->lane(0);
    const LaneState& O = race_->lane(1);
    const PowertrainCore& pt = P.sim->powertrain();
    const EngineSpec& es = P.sim->engineSpec();
    const Gearbox box = P.sim->gearboxType();
    const RacePhase ph = race_->phase();
    char b[128];

    // ---- ust serit ----
    r.rect(0, 0, 640, kWorldTop, {0.07f, 0.07f, 0.09f, 0.95f});
    if (ph == RacePhase::Burnout || ph == RacePhase::Staging) {          // mesafe secimi (dokun: 1/4 -> 1/2 -> 1 mil)
        r.rect(kDist[0], kDist[1], kDist[2], kDist[3], {0.30f, 0.20f, 0.08f, 0.9f});
        r.textCentered((kDist[0] + kDist[2]) / 2, kDist[1] + 9, kDistName[std::clamp(app_.settings.dragDist, 0, 2)], 1, {1.0f, 0.85f, 0.3f});
    }
    if ((ph == RacePhase::Burnout || ph == RacePhase::Staging || ph == RacePhase::Tree) && hasTune_[0] && launchControlAvailable(tunes_[0])
        && box != Gearbox::TorqueConverter) {
        r.rect(kLcDn[0], kLcDn[1], kLcUp[2], kLcUp[3], {0.05f, 0.05f, 0.08f, 0.8f});
        r.rect(kLcDn[0], kLcDn[1], kLcDn[2], kLcDn[3], {0.25f, 0.27f, 0.35f});
        r.rect(kLcUp[0], kLcUp[1], kLcUp[2], kLcUp[3], {0.25f, 0.27f, 0.35f});
        r.textCentered((kLcDn[0] + kLcDn[2]) / 2, 68, "-", 2, {1, 1, 1});
        r.textCentered((kLcUp[0] + kLcUp[2]) / 2, 68, "+", 2, {1, 1, 1});
        std::snprintf(b, sizeof b, "%d", (int)std::lround(P.sim->defaultLaunchRpm()));
        r.textCentered((kLcDn[2] + kLcUp[0]) / 2, 64, "KALKIS DEVRI", 1, {0.7f, 0.75f, 0.85f});
        r.textCentered((kLcDn[2] + kLcUp[0]) / 2, 75, b, 1, tunes_[0].launchRpm > 0 ? Color{1.0f, 0.75f, 0.25f} : Color{1, 1, 1});
    }
    auto laneLine = [&](const LaneState& L, float x, const char* who, Color c) {
        const double et = L.left ? (L.slip.finished ? L.slip.quarter : race_->clock() - L.leaveTime) : -1;
        const Settings& st = app_.settings;
        std::snprintf(b, sizeof b, "%s RT %s ET %s %3.0f %s", who, L.left ? sec(L.slip.reaction).c_str() : "--.---",
                      sec(et).c_str(), L.sim->speed() * st.speedFactor(), st.speedUnit());
        r.text(x, 4, b, 1, c);
        std::snprintf(b, sizeof b, "%s", up(L.car->model).substr(0, 22).c_str());
        r.text(x, 16, b, 1, {0.65f, 0.65f, 0.7f});
    };
    laneLine(P, 64, "SEN  ", {1, 1, 1});
    laneLine(O, 400, "RAKIP", {1.0f, 0.75f, 0.55f});
    if (app_.settings.showFps) {
        std::snprintf(b, sizeof b, "%2.0fFPS %4.1fMS", app_.fps(), app_.updateMs());
        r.text(548, 16, b, 1, {0.45f, 0.5f, 0.45f});
    }

    // ---- buyuk agac (orta ust) ----
    {
        const int lights = race_->treeLights();
        r.rect(298, 34, 342, 146, {0.08f, 0.08f, 0.09f, 0.92f});
        for (int lane = 0; lane < 2; ++lane) {
            const float x = lane ? 331 : 309;
            const bool staged = race_->lane(lane).staged;
            r.circle(x, 40, 3, 8, staged || ph != RacePhase::Burnout ? Color{1, 1, 0.9f} : kDim);
            r.circle(x, 49, 3, 8, staged ? Color{1, 1, 0.9f} : kDim);
            for (int a = 0; a < 3; ++a) r.circle(x, 63 + a * 16.0f, 6, 12, (lights & (1 << a)) ? kAmber : kDim);
            r.circle(x, 113, 6, 12, (lights & 8) && !(lights & (lane ? 32 : 16)) ? kGreen : kDim);
            r.circle(x, 131, 6, 12, (lights & (lane ? 32 : 16)) ? kRed : kDim);
        }
    }

    // ---- olay kutusu (agacin solu) + kritik an flasi ----
    if (!ticker_.empty() && !(ph == RacePhase::Finished && finishedT_ > 1.5)) {
        r.rect(66, 34, 292, 38 + 12.0f * ticker_.size(), {0.02f, 0.02f, 0.04f, 0.72f});
        for (size_t i = 0; i < ticker_.size(); ++i) r.text(70, 37 + i * 12.0f, ticker_[i].substr(0, 37), 1, {1.0f, 0.95f, 0.6f});
    }
    if (flashT_ > 0 && !(ph == RacePhase::Finished && finishedT_ > 1.5)) {
        const float w = r.textWidth(flash_, 4) + 24;
        r.rect(320 - w / 2, 160, 320 + w / 2, 196, {0.02f, 0.02f, 0.04f, 0.78f});
        r.textCentered(320, 164, flash_, 4, flashColor_);
    }
    if (ph == RacePhase::Burnout) {
        r.rect(kStageBtn[0], kStageBtn[1], kStageBtn[2], kStageBtn[3], {0.1f, 0.55f, 0.2f});
        r.textCentered(320, 131, "STAGE >", 2, {1, 1, 1});
    }

    // ---- kizaklar ----
    auto slider = [&](const float* rc, float v, const char* label, Color fillC) {
        r.rect(rc[0], rc[1], rc[2], rc[3], {0.10f, 0.10f, 0.12f, 0.72f});
        const float y = rc[3] - 6 - (rc[3] - rc[1] - 12) * v;
        r.rect(rc[0] + 4, y, rc[2] - 4, rc[3] - 4, fillC);
        r.rect(rc[0], y - 2, rc[2], y + 2, {1, 1, 1});
        { const std::string lb = translate(r.lang, label);
          for (size_t i = 0; i < lb.size() && rc[1] + 24 + i * 16.0f < rc[3]; ++i) r.textCentered((rc[0] + rc[2]) / 2, rc[1] + 8 + i * 16.0f, std::string(1, lb[i]), 2, {1, 1, 1}); }
    };
    if (box == Gearbox::HPattern || box == Gearbox::Dogbox) {
        // Kavrama noktasi (ClutchSpec: pedal 0.62 tutmaya baslar, 0.32 tam kavrar): belirgin bant + yazi; pedal
        // bandin icindeyken dolgu sariya doner
        const float cv = (float)pc_.clutch;
        const bool biting = cv > 0.32f && cv < 0.62f;
        slider(kClutch, cv, "", biting ? Color{1.0f, 0.78f, 0.1f, 0.9f} : Color{0.25f, 0.55f, 0.95f, 0.85f});
        const float span = kClutch[3] - kClutch[1] - 12;
        const float y0 = kClutch[3] - 6 - span * 0.62f, y1 = kClutch[3] - 6 - span * 0.32f;
        r.rect(kClutch[0], y0, kClutch[2], y1, {1.0f, 0.8f, 0.1f, biting ? 0.35f : 0.22f});
        r.rect(kClutch[0], y0 - 1, kClutch[2], y0 + 1, {1.0f, 0.85f, 0.2f});
        r.rect(kClutch[0], y1 - 1, kClutch[2], y1 + 1, {1.0f, 0.85f, 0.2f});
        const float cx = (kClutch[0] + kClutch[2]) / 2;
        r.textCentered(cx, (y0 + y1) * 0.5f - 4, "KAVRAMA", 1, {1.0f, 0.95f, 0.7f});
        r.textCentered(cx, kClutch[1] + 6, "DEBRIYAJ", 1, {1, 1, 1});
        r.textCentered(cx, kClutch[1] + 18, "BAS", 1, {0.8f, 0.8f, 0.85f});
        r.textCentered(cx, kClutch[3] - 14, "BIRAK", 1, {0.8f, 0.8f, 0.85f});
    }
    slider(kThrottle, (float)pc_.throttle, "GAZ", {0.95f, 0.55f, 0.1f, 0.9f});

    // ---- gosterge paneli ----
    r.rect(62, kWorldBottom, 580, 360, {0.09f, 0.09f, 0.11f});
    const int gear = pt.gear();
    const float rpm = (float)pt.rpm(), red = (float)es.redlineRpm, shiftAt = (float)P.sim->shiftRpm();
    // Satin alinan kadran (kariyer araci): analog devir saati + turbo gostergesi; yazilar saga kayar
    const bool mine = app_.career.car().carId == carIds_[0];
    const int gstyle = mine ? app_.career.car().gauge : 0;
    const bool analog = gstyle == 1 || gstyle == 3, boostG = mine && app_.career.car().boostGauge && P.sim->boostMax() > 0;
    float tx = 136;
    if (analog || boostG) {
        GaugeData gd;
        gd.rpm = rpm; gd.redline = red; gd.shiftRpm = shiftAt; gd.gear = gear == 0 ? "N" : std::to_string(gear);
        gd.gearCol = P.grind ? kRed : Color{1, 1, 1}; gd.t = t_;
        boostShown_ += ((float)P.sim->boostNow() - boostShown_) * 0.15f;
        gd.boost = boostShown_; gd.boostMax = (float)P.sim->boostMax();
        float gx = 66;
        if (analog) { drawAnalogTach(r, 110, 311, 44, gd); gx = 158; }
        else { r.rect(66, 268, 128, 354, {0.14f, 0.14f, 0.17f}); r.textCentered(97, 283, gd.gear, 8, gd.gearCol); gx = 132; }
        if (boostG) { drawBoostGauge(r, gx + 28, 311, 26, gd); gx += 58; }
        tx = gx + 4;
    } else {
    r.rect(66, 268, 128, 354, {0.14f, 0.14f, 0.17f});
    r.textCentered(97, 283, gear == 0 ? "N" : std::to_string(gear), 8, P.grind ? kRed : Color{1, 1, 1});
    }
    // Devir seridi (30 segment) + vites isigi
    if (!analog)
    for (int i = 0; i < 30; ++i) {
        const float segRpm = red * 1.05f * (i + 1) / 30.0f;
        const bool lit = rpm >= segRpm - red * 1.05f / 30.0f;
        Color c = segRpm > red ? kRed : segRpm > shiftAt - 800 ? kAmber : kGreen;
        if (!lit) c = {c.r * 0.18f, c.g * 0.18f, c.b * 0.18f};
        r.rect(tx + i * 9.8f * (430 - tx) / 294.0f, 268, tx + (i * 9.8f + 8) * (430 - tx) / 294.0f, 290, c);
    }
    if (rpm > shiftAt - 150 && std::fmod(t_, 0.12) < 0.06) r.rect(tx, 264, 430, 267, {0.3f, 0.6f, 1.0f});
    std::snprintf(b, sizeof b, "%5.0f RPM", rpm);
    r.text(tx, analog ? 274 : 296, b, 2, pt.limiterHit() ? kAmber : Color{1, 1, 1});
    if (pt.vtecActive()) r.text(tx + 132, analog ? 274 : 296, "VTEC", 2, kRed);
    std::snprintf(b, sizeof b, "%3.0f %s", P.sim->speed() * app_.settings.speedFactor(), app_.settings.speedUnit());
    r.text(analog ? tx : 320, analog ? 296 : 296, b, 2, {1, 1, 1});
    // Talimat
    const char* hint = "";
    const bool autoClutch = box == Gearbox::DCT || box == Gearbox::TorqueConverter;
    if (ph == RacePhase::Burnout) hint = autoClutch ? "BURNOUT: FRENE BAS + GAZ" : "BURNOUT: GAZ + DEBRIYAJI KAYDIR";
    else if ((ph == RacePhase::Staging || ph == RacePhase::Tree) && !P.armed) hint = autoClutch ? "FRENE BAS VE TUT" : "DEBRIYAJA BAS VE TUT";
    else if (ph == RacePhase::Staging || ph == RacePhase::Tree) hint = autoClutch ? "FREN + GAZ, YESILDE FRENI BIRAK" : "DEBRIYAJ + GAZ, YESILDE BIRAK";
    else if (ph == RacePhase::Run) hint = box == Gearbox::HPattern ? "VITES: DEBRIYAJ BAS + KOL" : box == Gearbox::TorqueConverter ? "OTOMATIK: SADECE GAZ" : "VITES: + / -";
    r.text(tx, 322, hint, 1, {0.75f, 0.8f, 0.9f});
    std::snprintf(b, sizeof b, "YAG %.1f BAR  BALATA %.0fC  LASTIK %.0fC", pt.oilPressureBar(), pt.clutchTempC(),
                  P.sim->wheel(P.sim->drivenLeft()).tempC());
    r.text(tx, 336, b, 1, {0.6f, 0.65f, 0.6f});
#ifndef __ANDROID__
    r.text(136, 349, "W GAZ  S FREN  BOSLUK DEBR  1-6/N  E/Q  ESC", 1, {0.55f, 0.75f, 1.0f});
#endif
    // Fren
    r.rect(kBrake[0], kBrake[1], kBrake[2], kBrake[3], pc_.brake > 0 ? Color{0.8f, 0.15f, 0.15f} : Color{0.3f, 0.12f, 0.12f});
    r.textCentered((kBrake[0] + kBrake[2]) / 2, 307, "FREN", 1, {1, 1, 1});
    // Vites kolu / pedallar
    r.rect(kShift[0], kShift[1], kShift[2], kShift[3], {0.13f, 0.13f, 0.16f});
    if (box == Gearbox::HPattern) {
        const int gears = pt.gearCount();
        r.rect(kColX[0] - 2, kRowMid - 2, kColX[2] + 2, kRowMid + 2, {0.35f, 0.35f, 0.4f});
        for (int c = 0; c < 3; ++c) {
            r.rect(kColX[c] - 2, kRowTop, kColX[c] + 2, kRowBot, c * 2 + 1 <= gears ? Color{0.35f, 0.35f, 0.4f} : Color{0.2f, 0.2f, 0.22f});
            if (c * 2 + 1 <= gears) r.text(kColX[c] - 2, kRowTop - 8, std::to_string(c * 2 + 1), 1, {0.8f, 0.8f, 0.8f});
            if (c * 2 + 2 <= gears) r.text(kColX[c] - 2, kRowBot + 2, std::to_string(c * 2 + 2), 1, {0.8f, 0.8f, 0.8f});
        }
        { const float kx = knobDrag_ ? dragX_ : knobX_, ky = knobDrag_ ? dragY_ : knobY_;
          r.circle(kx, ky, 10, 14, {0.05f, 0.05f, 0.06f, 0.6f});
          r.circle(kx, ky, 8, 14, P.grind ? kRed : Color{0.9f, 0.9f, 0.92f}); }
    } else if (box == Gearbox::TorqueConverter) {
        r.textCentered(539, 300, "OTO", 2, {0.8f, 0.8f, 0.9f});
    } else {
        r.rect(kPadDn[0], kPadDn[1], kPadDn[2], kPadDn[3], {0.22f, 0.24f, 0.3f});
        r.rect(kPadUp[0], kPadUp[1], kPadUp[2], kPadUp[3], {0.22f, 0.24f, 0.3f});
        r.textCentered((kPadDn[0] + kPadDn[2]) / 2, 305, "-", 3, {1, 1, 1});
        r.textCentered((kPadUp[0] + kPadUp[2]) / 2, 305, "+", 3, {1, 1, 1});
    }
}

void DragScreen::drawResults(Renderer& r) {
    const LaneState& P = race_->lane(0);
    const LaneState& O = race_->lane(1);
    r.rect(110, 40, 530, 304, {0.03f, 0.03f, 0.05f, 0.96f});
    const bool won = race_->winner() == 0;
    r.textCentered(career_ ? 250 : 320, 50, won ? "KAZANDIN!" : "KAYBETTIN", 3, won ? kGreen : kRed);
    if (career_) {
        char pb[48]; std::snprintf(pb, sizeof pb, "+$%ld", prize_);
        r.text(420, 52, prize_ > 0 ? pb : "$0", 2, prize_ > 0 ? kGreen : Color{0.6f, 0.6f, 0.6f});
    }
    if (!showGraph_) {
    r.text(250, 80, "SEN", 2, {1, 1, 1});
    r.text(390, 80, "RAKIP", 2, {1.0f, 0.75f, 0.55f});
    auto row = [&](int i, const char* label, const std::string& a, const std::string& o) {
        const float y = 100 + i * 17.0f;
        r.text(126, y, label, 2, {0.7f, 0.7f, 0.75f});
        r.text(236, y, a, 2, {1, 1, 1});
        r.text(376, y, o, 2, {1, 1, 1});
    };
    const double unitK = app_.settings.speedFactor() / 3.6;          // km/h -> secili birim
    auto kmh = [unitK](double v) { char b[16]; std::snprintf(b, sizeof b, "%6.1f", v * unitK); return std::string(b); };
    row(0, "RT", P.slip.redLight ? "KIRMIZI" : sec(P.slip.reaction), O.slip.redLight ? "KIRMIZI" : sec(O.slip.reaction));
    row(1, "60 FT", sec(P.slip.sixtyFt), sec(O.slip.sixtyFt));
    row(2, "330 FT", sec(P.slip.t330), sec(O.slip.t330));
    row(3, "1/8", sec(P.slip.eighth), sec(O.slip.eighth));
    row(4, app_.settings.mph ? "1/8 MPH" : "1/8 KMH", kmh(P.slip.eighthKmh), kmh(O.slip.eighthKmh));
    row(5, "1000 FT", sec(P.slip.t1000), sec(O.slip.t1000));
    row(6, (std::string(kDistName[std::clamp(app_.settings.dragDist, 0, 2)]).substr(0, 3) + " ET").c_str(), sec(P.slip.quarter), sec(O.slip.quarter));
    row(7, "TRAP", kmh(P.slip.trapKmh), kmh(O.slip.trapKmh));
    if (P.slip.broke || O.slip.broke || P.slip.stalled || O.slip.stalled) {
        std::string n = std::string(P.slip.broke ? "SEN: AKS KIRIK " : "") + (O.slip.broke ? "RAKIP: AKS KIRIK " : "") +
                        (P.slip.stalled ? "SEN: STOP " : "") + (O.slip.stalled ? "RAKIP: STOP" : "");
        r.textCentered(320, 240, n, 1, kRed);
    }
    }
    if (showGraph_) drawGraph(r);
    r.rect(kGraph[0], kGraph[1], kGraph[2], kGraph[3], showGraph_ ? Color{0.75f, 0.45f, 0.1f} : Color{0.25f, 0.27f, 0.35f});
    r.textCentered((kGraph[0] + kGraph[2]) / 2, kGraph[1] + 10, showGraph_ ? "ZAMANLAR" : "GRAFIK", 2, {1, 1, 1});
    r.rect(kAgain[0], kAgain[1], kAgain[2], kAgain[3], {0.1f, 0.55f, 0.2f});
    r.textCentered((kAgain[0] + kAgain[2]) / 2, kAgain[1] + 10, "TEKRAR", 2, {1, 1, 1});
    r.rect(kGarage[0], kGarage[1], kGarage[2], kGarage[3], {0.25f, 0.27f, 0.35f});
    r.textCentered((kGarage[0] + kGarage[2]) / 2, kGarage[1] + 10, "GARAJ", 2, {1, 1, 1});
}

// Telemetri grafigi: oyuncu hizi (beyaz), rakip hizi (turuncu), devir (yesil, ayri eksen), patinaj (kirmizi bant),
// vites gecisleri (dikey cizgi + vites numarasi). Kalkista patinaj / gec vites / kesicide bekleme gorunur.
void DragScreen::drawGraph(Renderer& r) {
    const float x0 = 126, x1 = 514, y0 = 92, y1 = 236;
    r.rect(x0, y0, x1, y1, {0.07f, 0.08f, 0.11f});
    r.text(126, 78, "HIZ", 1, {1, 1, 1}); r.text(156, 78, "RAKIP", 1, {1.0f, 0.6f, 0.25f});
    r.text(200, 78, "DEVIR", 1, {0.35f, 0.9f, 0.4f});
    if (tel_.size() < 2) { r.textCentered((x0 + x1) / 2, (y0 + y1) / 2, "VERI YOK", 2, kUiDim); return; }
    const float tMax = std::max(1.0f, tel_.back().t);
    float vMax = 1, rMax = 1;
    for (const TelPt& p : tel_) { vMax = std::max({vMax, p.v, p.ov}); rMax = std::max(rMax, p.rpm); }   // ov -1: rakip bitirdi
    vMax *= 1.08f; rMax *= 1.08f;
    auto X = [&](float t) { return x0 + (x1 - x0) * t / tMax; };
    auto Yv = [&](float v) { return y1 - (y1 - y0) * v / vMax; };
    auto Yr = [&](float rpm) { return y1 - (y1 - y0) * rpm / rMax; };
    for (int k = 1; k < 4; ++k) r.rect(x0, y0 + (y1 - y0) * k / 4, x1, y0 + (y1 - y0) * k / 4 + 1, {1, 1, 1, 0.06f});
    for (int s = 1; s < (int)tMax + 1; ++s) r.rect(X((float)s), y0, X((float)s) + 1, y1, {1, 1, 1, 0.05f});
    auto seg = [&](float ax, float ay, float bx, float by, float w, Color c) {
        const float dx = bx - ax, dy = by - ay, l = std::max(1e-3f, std::sqrt(dx * dx + dy * dy)), nx = -dy / l * w, ny = dx / l * w;
        r.tri(ax + nx, ay + ny, bx + nx, by + ny, bx - nx, by - ny, c); r.tri(ax + nx, ay + ny, bx - nx, by - ny, ax - nx, ay - ny, c);
    };
    for (size_t i = 1; i < tel_.size(); ++i) {
        const TelPt &a = tel_[i - 1], &b = tel_[i];
        if (b.slip > 2.5f) r.rect(X(a.t), y0, X(b.t) + 0.5f, y1, {1.0f, 0.2f, 0.15f, std::min(0.35f, 0.08f + b.slip * 0.015f)});
        if (b.gear != a.gear && b.gear > 0) {
            r.rect(X(b.t), y0, X(b.t) + 1, y1, {1, 1, 1, 0.25f});
            char g[4]; std::snprintf(g, sizeof g, "%d", b.gear);
            r.text(X(b.t) + 2, y0 + 2, g, 1, {1, 1, 1, 0.8f});
        }
        seg(X(a.t), Yr(a.rpm), X(b.t), Yr(b.rpm), 0.7f, {0.35f, 0.9f, 0.4f, 0.8f});
        if (a.ov >= 0 && b.ov >= 0) seg(X(a.t), Yv(a.ov), X(b.t), Yv(b.ov), 0.8f, {1.0f, 0.6f, 0.25f, 0.85f});   // rakip bitisine kadar
        seg(X(a.t), Yv(a.v), X(b.t), Yv(b.v), 1.1f, {1, 1, 1});
    }
    char b[48];
    const double unitK = app_.settings.speedFactor();                  // m/s -> secili birim
    std::snprintf(b, sizeof b, "%.0f %s", vMax * unitK, app_.settings.mph ? "MPH" : "KMH");
    r.text(x0 + 4, y0 + 2, b, 1, kUiDim);
    std::snprintf(b, sizeof b, "%.1f S", tMax);
    r.text(x1 - r.textWidth(b, 1), y1 + 3, b, 1, kUiDim);
    r.text(x0, y1 + 3, "0", 1, kUiDim);
    // Ozet: kalkis patinaj suresi
    float spin = 0;
    for (size_t i = 1; i < tel_.size(); ++i) if (tel_[i].slip > 2.5f) spin += tel_[i].t - tel_[i - 1].t;
    std::snprintf(b, sizeof b, "PATINAJ %.2f S", spin);
    r.text(244, 78, b, 1, {1.0f, 0.3f, 0.25f});
}

void DragScreen::render(Renderer& r) {
    r.begin(640, 360, {0.05f, 0.05f, 0.06f});
    drawWorld(r);
    // Rakip (uzak serit, kucuk) sonra oyuncu (yakin serit)
    const float playerSx = 64.0f + kCamLead * kPx;
    const float oppSx = playerSx + (float)(race_->lane(1).sim->distance() - race_->lane(0).sim->distance()) * kPx;
    if (oppSx > -150 && oppSx < 790) drawCarAt(r, 1, oppSx, kFarGround, 0.8f);
    // Duman (uzak serit arabanin onunde, yakin seritten once)
    auto drawSmoke = [&](int lane) {
        for (const Smoke& p : smoke_) {
            if ((int)p.lane != lane) continue;
            const float scale = lane ? 0.8f : 1.0f;
            const float x = 64.0f + (p.x - camX_) * kPx * (lane ? 1.0f : 1.0f);
            const float y = (lane ? kFarGround : kNearGround) - p.y * kPx * scale;
            r.circle(x, y, p.size * kPx * 0.5f * scale, 10, {0.82f, 0.82f, 0.84f, 0.5f * p.life / 1.6f});
        }
    };
    drawSmoke(1);
    const LaneState& L0 = race_->lane(0);
    if (!ghost_.empty() && L0.left) {                                    // hayalet: onceki en iyi kosu (yari saydam, mavi)
        const double t = race_->clock() - L0.leaveTime;
        const size_t i = std::min(ghost_.size() - 1, (size_t)(t / 0.05));
        const double f = std::clamp(t / 0.05 - i, 0.0, 1.0);
        const double gd = i + 1 < ghost_.size() ? ghost_[i] + (ghost_[i + 1] - ghost_[i]) * f : ghost_.back();
        const float gx = playerSx + (float)(gd - L0.sim->distance()) * kPx;
        if (gx > -150 && gx < 790 && std::fabs(gx - playerSx) > 4.0f) drawCarAt(r, 0, gx, kNearGround, 1.0f, 0.32f);
    }
    drawCarAt(r, 0, playerSx, kNearGround, 1.0f);
    drawSmoke(0);
    drawHud(r);
    if (!ghost_.empty() && L0.left && !L0.slip.finished) {               // hayalete gore anlik fark
        const double tg = ghostTimeAt(L0.sim->distance());
        if (tg >= 0) {
            const double dt = (race_->clock() - L0.leaveTime) - tg;
            char b[48]; std::snprintf(b, sizeof b, "HAYALET %+.2f", dt);
            r.textCentered(440, 98, b, 2, dt <= 0 ? Color{0.4f, 1.0f, 0.5f} : Color{1.0f, 0.4f, 0.3f});
        }
    }
    if (race_->phase() == RacePhase::Finished && finishedT_ > 1.5) drawResults(r);
    r.flush2D();
}

} // namespace zk
