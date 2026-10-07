#include "App.h"
#include "game/Achievements.h"
#include "Ui.h"
#include "GLApi.h"
#include "Screens.h"
#include "sim/PartTables.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

namespace zk {

App::App(const std::string& saveDir) {
    if (!saveDir.empty()) {
        savePath_ = saveDir;
        if (savePath_.back() != '/' && savePath_.back() != '\\') savePath_ += '/';
        settingsPath_ = savePath_ + "ayarlar.cfg";
        ghostPath_ = savePath_ + "hayalet.zkg";
        settings = Settings::load(settingsPath_);
        savePath_ += "kariyer.zks";
        bool corrupt = false;
        career = Career::loadOrNew(savePath_, &corrupt);
        if (corrupt) startupMsg = "KAYIT BOZUK - YENI OYUN";
    } else {
        career = Career::newGame();
    }
    treePro = career.treePro;
    selectedCar = career.car().carId;
    loadGhosts();
    applySettings();
    // Acilis sinematigi (test calistirmalarinda yok; ZK_START_SCREEN=intro ile acilir)
    const bool test = std::getenv("ZK_START_SCREEN") || std::getenv("ZK_AUTOPILOT") || std::getenv("ZK_START_DRAG");
    if (test) setScreen(std::make_unique<GarageScreen>(*this));
    else setScreen(std::make_unique<IntroScreen>(*this));
}

void App::goIntro() { setScreen(std::make_unique<IntroScreen>(*this)); }

// Hayalet dosyasi: satir basina "arac ET n d0 d1 ..." (metin; bozuk satir atlanir)
void App::loadGhosts() {
    if (ghostPath_.empty()) return;
    std::ifstream f(ghostPath_);
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream in(line);
        int id = 0; size_t n = 0; Ghost g;
        if (!(in >> id >> g.et >> n) || n > 2000 || g.et <= 0) continue;
        g.d.resize(n);
        bool ok = true;
        for (size_t i = 0; i < n && ok; ++i) ok = (bool)(in >> g.d[i]);
        if (ok && n > 1) ghosts[id] = std::move(g);
    }
}
void App::saveGhosts() const {
    if (ghostPath_.empty()) return;
    std::ofstream f(ghostPath_ + ".tmp", std::ios::trunc);
    for (const auto& [id, g] : ghosts) {
        f << id << ' ' << g.et << ' ' << g.d.size();
        char b[16];
        for (float d : g.d) { std::snprintf(b, sizeof b, " %.2f", d); f << b; }
        f << '\n';
    }
    f.close();
    std::remove(ghostPath_.c_str());
    std::rename((ghostPath_ + ".tmp").c_str(), ghostPath_.c_str());
}

void App::saveCareer() {
    career.treePro = treePro;
    for (int i : career.checkAchievements())
        toast("BASARIM: " + std::string(achievements()[i].name) + "  +" + money(achievements()[i].reward));
    if (!savePath_.empty()) career.save(savePath_);
}
App::~App() = default;

void App::applySettings() {
    const float master = settings.masterVol / 100.0f;
    engineVol_ = master * settings.engineVol / 100.0f;
    tireVol_ = master * settings.tireVol / 100.0f;
    renderer_.integerScale = settings.integerScale;
    renderer_.fillScreen = settings.fillScreen && !settings.integerScale;
    renderer_.setRenderScale(settings.renderScale);
    renderer_.lang = (Lang)settings.language;
}
void App::saveSettings() { if (!settingsPath_.empty()) settings.save(settingsPath_); }

void App::haptic(int ms, int amplitude) {
    if (!onHaptic || settings.haptics <= 0) return;
    onHaptic(ms, std::clamp(amplitude * settings.haptics / 100, 1, 255));
}

bool App::initGraphics() { return renderer_.init(); }
void App::shutdownGraphics() { renderer_.shutdown(); }

void App::setScreen(std::unique_ptr<Screen> s) {
    if (!screen_) { screen_ = std::move(s); if (onOrientation) onOrientation(screen_->landscape()); return; }
    pending_ = std::move(s);   // bir sonraki karede gecis (ekran kendi metodunun icindeyken silinmesin)
    windSpeed_ = 0.0f; nos_ = false; rain_ = false; siren_ = 0.0f;
    for (Voice& v : voices_) { v.slip = 0.0f; v.lock = 0.0f; }      // yaris bitince lastik sesi menuye tasinmasin
}
bool App::backToWorld() {
    if (!worldReturn) return false;
    worldReturn = false; activeEvent = -1; activeTour = false; activeMeet = false; runPlan.active = false;
    goWorld();
    return true;
}
void App::goGarageDirect() { worldReturn = false; setVoice(1, nullptr); setScreen(std::make_unique<GarageScreen>(*this)); }
void App::goWorld() { setVoice(1, nullptr); setScreen(std::make_unique<WorldScreen>(*this)); }
void App::goGarage() {
    if (backToWorld()) return;
    setVoice(1, nullptr);
    runPlan.active = false;                                             // yarim kalan seyahat iptal (bulunulan sehirde kalinir)
    if (activeEvent >= 0) {                                                       // etkinlikten donus: o lig
        const int tab = leagueEvents()[activeEvent].league;
        activeEvent = -1; setScreen(std::make_unique<LeagueScreen>(*this, tab)); return;
    }
    if (activeTour) { activeTour = false; setScreen(std::make_unique<RegionMapScreen>(*this)); return; }   // turnuvadan donus
    if (activeMeet) { activeMeet = false; setScreen(std::make_unique<StreetScreen>(*this)); return; }       // bulusmadan donus
    setScreen(std::make_unique<GarageScreen>(*this));
}
void App::goLeague(int tab) { if (backToWorld()) return; activeEvent = -1; setScreen(std::make_unique<LeagueScreen>(*this, tab)); }
void App::goMap() { if (backToWorld()) return; activeEvent = -1; activeTour = false; activeMeet = false; setScreen(std::make_unique<RegionMapScreen>(*this)); }
void App::startTour() {
    std::string why;
    if (!career.tourStart(&why)) return;
    const OwnedCar& oc = career.car();
    const int r = career.tourRound;                                    // her tur rakip daha hizli ve keskin
    Opponent o = career.pickOpponentFor((uint32_t)career.tourWeek * 131u + (uint32_t)r * 977u + 7u, -0.12 - 0.12 * r);
    eventHandicap = std::max(1.1, 1.5 - 0.15 * r);
    lastOpp = o;
    activeEvent = -1; activeTour = true;
    saveCareer();
    setScreen(std::make_unique<DragScreen>(*this, oc.carId, o.carId, &oc.tune, &o.tune, true));
}
void App::startEvent(int idx) {
    const auto& ev = leagueEvents();
    if (idx < 0 || idx >= (int)ev.size() || !career.eventAvailable(idx)) return;
    const EventDef& e = ev[idx];
    const OwnedCar& oc = career.car();
    Opponent o;
    if (e.rival >= 0) {
        const RivalDef& r = rivals()[e.rival];
        o.carId = r.carId; o.tune = opponentPreset(r.preset); o.estEt = tableEt(r.carId, r.preset);
        o.playerEt = estimatedEt(*findVehicle(oc.carId), oc.tune);
        eventHandicap = r.handicap;
    } else {
        o = career.pickOpponentFor((uint32_t)career.races * 7919u + raceSeed_++);
        eventHandicap = 1.6;
    }
    lastOpp = o;
    activeEvent = idx;
    if (e.mode == EventMode::Drag) setScreen(std::make_unique<DragScreen>(*this, oc.carId, o.carId, &oc.tune, &o.tune, true));
    else setScreen(std::make_unique<RoadScreen>(*this, oc.carId, &oc.tune));
}
void App::goDrag(int p, int o, bool autopilot) {
    auto s = std::make_unique<DragScreen>(*this, p, o, nullptr, nullptr, false);
    s->setAutopilot(autopilot);
    setScreen(std::move(s));
}
void App::goCareerRace() {
    if (true) { goMap(); return; }                    // kariyer: bolge haritasi -> lig
    const OwnedCar& oc = career.car();
    const Opponent opp = career.pickOpponentFor((uint32_t)career.races * 7919u + raceSeed_++);
    lastOpp = opp;
    setScreen(std::make_unique<DragScreen>(*this, oc.carId, opp.carId, &oc.tune, &opp.tune, true));
}
void App::goParts(int cat) { setScreen(std::make_unique<PartsScreen>(*this, cat)); }
void App::goFabricate(int cat) { setScreen(std::make_unique<FabricateScreen>(*this, cat)); }
void App::goJunkyard() { setScreen(std::make_unique<JunkyardScreen>(*this)); }
void App::goBodyShop() { setScreen(std::make_unique<BodyShopScreen>(*this)); }
void App::goAchievements() { setScreen(std::make_unique<AchievementsScreen>(*this)); }
void App::goEcu() { setScreen(std::make_unique<EcuScreen>(*this)); }
void App::goSetup() { setScreen(std::make_unique<SetupScreen>(*this)); }
void App::goGauges() { setScreen(std::make_unique<GaugeShopScreen>(*this)); }
void App::goStreet() { activeEvent = -1; activeTour = false; activeMeet = false; setScreen(std::make_unique<StreetScreen>(*this)); }
void App::startTravel(int target, bool solo) {
    std::string why;
    if (!career.canTravel(target, &why)) return;
    runPlan = {};
    runPlan.active = true; runPlan.from = career.city; runPlan.target = target; runPlan.solo = solo;
    // Tek etap: kac sehir otesine gidiliyorsa tum ayaklarin toplami (ornek 110 + 130 = 240 km); alan en kalabalik ayaktan
    const int dir = target > career.city ? 1 : -1, leg = dir > 0 ? career.city : career.city - 1;
    runPlan.realKm = career.travelKm(target);
    int fieldN = 0;
    for (int c = std::min(career.city, target); c < std::max(career.city, target); ++c) fieldN = std::max(fieldN, Career::legField(c));
    runPlan.field = solo ? std::vector<RunEntrant>{} : career.runField(fieldN, (uint32_t)(career.races * 131 + leg * 7 + 3));
    activeEvent = -1; activeTour = false; activeMeet = false;
    setScreen(std::make_unique<RoadScreen>(*this, career.car().carId, &career.car().tune));   // runPlan: The Run etabi
}
void App::continueTravel() { setScreen(std::make_unique<RoadScreen>(*this, career.car().carId, &career.car().tune)); }
void App::nextTravelLeg(double fuelLeft) {
    if (!runPlan.active) return;
    career.arriveCity(runPlan.target);                                  // tek etap: dogrudan hedef sehir
    saveCareer();
    runPlan.active = false; runPlan.fuelL = fuelLeft;
    eventNote = std::string(Career::cityName(career.city)) + "'A VARDIN!";
}

void App::startMeet() {
    std::string why;
    if (!career.meetStart(&why)) return;
    const OwnedCar& oc = career.car();
    Opponent o = career.meetOpponent();
    eventHandicap = 1.35;
    lastOpp = o;
    activeEvent = -1; activeTour = false; activeMeet = true;
    saveCareer();
    setScreen(std::make_unique<DragScreen>(*this, oc.carId, o.carId, &oc.tune, &o.tune, true));
}
void App::goSaveCode() { setScreen(std::make_unique<SaveCodeScreen>(*this)); }
void App::goRestore() { setScreen(std::make_unique<RestoreScreen>(*this)); }
void App::goGallery() { if (backToWorld()) return; setScreen(std::make_unique<GalleryScreen>(*this)); }
void App::goDyno() { setScreen(std::make_unique<DynoScreen>(*this)); }
void App::goSettings() { setScreen(std::make_unique<SettingsScreen>(*this)); }
void App::goRoad() {
    const OwnedCar& oc = career.car();
    setScreen(std::make_unique<RoadScreen>(*this, oc.carId, &oc.tune));
}

void App::hint(int id, const char* text) {
    if (id < 0 || id > 30 || ((settings.hintsSeen >> id) & 1)) return;
    const bool test = std::getenv("ZK_START_SCREEN") || std::getenv("ZK_AUTOPILOT") || std::getenv("ZK_START_DRAG");
    if (test && !std::getenv("ZK_HINTS")) return;
    settings.hintsSeen |= 1 << id;
    saveSettings();
    hint_ = text;
}

void App::update(double dt) {
    if (!toasts_.empty() && (toastT_ += dt) > 2.6) { toasts_.erase(toasts_.begin()); toastT_ = 0; }
    if (pending_) {
        screen_ = std::move(pending_);
        if (onOrientation) onOrientation(screen_->landscape());
        fade_ = 1.0f;                                         // ekran gecisi: karartmadan acilir
    }
    fade_ = std::max(0.0f, fade_ - (float)std::min(dt, 0.05) / 0.22f);
    const auto t0 = std::chrono::steady_clock::now();
    if (hint_.empty() && !confirmBack_) screen_->update(std::min(dt, 0.1));            // ipucu karti acikken ekran durur
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    updMs_ += (ms - updMs_) * 0.05;                          // yumusatilmis
    fpsAcc_ += dt; ++fpsFrames_;
    if (fpsAcc_ >= 0.5) { fps_ = fpsFrames_ / fpsAcc_; fpsAcc_ = 0; fpsFrames_ = 0; }
}

void App::render() {
    if (!renderer_.ready()) return;
    screen_->render(renderer_);
    if (fade_ > 0.0f) renderer_.rect(0, 0, (float)renderer_.vw(), (float)renderer_.vh(), {0.02f, 0.02f, 0.03f, fade_ * fade_});
    if (!toasts_.empty()) {                                       // ust bildirim: kayarak iner, 2.6 s
        const float W = (float)renderer_.vw(), k = (float)std::min({1.0, toastT_ / 0.25, (2.6 - toastT_) / 0.25});
        const float y = -34.0f + 40.0f * std::max(0.0f, k);
        renderer_.rect(W * 0.5f - 172, y, W * 0.5f + 172, y + 30, {0.10f, 0.08f, 0.02f, 1.0f});
        renderer_.rect(W * 0.5f - 172, y + 28, W * 0.5f + 172, y + 30, {1.0f, 0.78f, 0.2f});
        renderer_.textCentered(W * 0.5f, y + 9, toasts_.front().substr(0, 42), 1, {1.0f, 0.85f, 0.35f});
    }
    if (!hint_.empty()) {                                         // ilk giris ipucu karti (ortada, ekran karartilir)
        std::vector<std::string> lines;
        for (size_t a = 0, b; a <= hint_.size(); a = b + 1) { b = hint_.find('\n', a); if (b == std::string::npos) b = hint_.size(); lines.push_back(hint_.substr(a, b - a)); }
        const float W = (float)renderer_.vw(), H = (float)renderer_.vh();
        float tw = 0; for (auto& l : lines) tw = std::max(tw, renderer_.textWidth(l, 1));
        const float cw = std::min(W - 16, std::max(240.0f, tw + 28)), ch = 44 + lines.size() * 14.0f + 20;
        const float x0 = (W - cw) * 0.5f, y0 = (H - ch) * 0.5f;
        renderer_.rect(0, 0, W, H, {0, 0, 0, 0.55f});
        renderer_.rect(x0, y0, x0 + cw, y0 + ch, {0.09f, 0.10f, 0.14f});
        renderer_.rect(x0, y0, x0 + cw, y0 + 3, {1.0f, 0.62f, 0.12f});
        renderer_.text(x0 + 14, y0 + 12, "IPUCU", 2, {1.0f, 0.78f, 0.25f});
        for (size_t i = 0; i < lines.size(); ++i) renderer_.text(x0 + 14, y0 + 40 + i * 14.0f, lines[i], 1, {0.92f, 0.92f, 0.96f});
        renderer_.textCentered(W * 0.5f, y0 + ch - 16, "DOKUN: TAMAM", 1, {0.55f, 0.75f, 1.0f});
    }
    if (confirmBack_) {                                           // ana menuye donus onayi
        const float W = (float)renderer_.vw(), H = (float)renderer_.vh(), cw = 300, ch = 130;
        const float x0 = (W - cw) * 0.5f, y0 = (H - ch) * 0.5f;
        renderer_.rect(0, 0, W, H, {0, 0, 0, 0.6f});
        renderer_.rect(x0, y0, x0 + cw, y0 + ch, {0.09f, 0.10f, 0.14f});
        renderer_.rect(x0, y0, x0 + cw, y0 + 3, {1.0f, 0.35f, 0.2f});
        renderer_.textCentered(W * 0.5f, y0 + 18, "ANA MENUYE DON?", 2, {1, 1, 1});
        renderer_.textCentered(W * 0.5f, y0 + 44, "EMIN MISIN? ILERLEME BU SURUSTE KAYBOLUR", 1, {0.8f, 0.8f, 0.85f});
        renderer_.rect(x0 + 16, y0 + 70, x0 + 140, y0 + 114, {0.55f, 0.15f, 0.12f});
        renderer_.rect(x0 + 160, y0 + 70, x0 + 284, y0 + 114, {0.2f, 0.3f, 0.45f});
        renderer_.textCentered(x0 + 78, y0 + 80, "EVET", 2, {1, 1, 1});
        renderer_.textCentered(x0 + 78, y0 + 100, "(B / GERI)", 1, {0.9f, 0.8f, 0.8f});
        renderer_.textCentered(x0 + 222, y0 + 80, "HAYIR", 2, {1, 1, 1});
        renderer_.textCentered(x0 + 222, y0 + 100, "(A / START)", 1, {0.8f, 0.85f, 0.95f});
    }
    renderer_.present(sw_, sh_);
}

void App::pointerDown(int id, float px, float py) {
    if (!hint_.empty()) { hint_.clear(); return; }               // ipucu karti: dokunus kapatir (ekrana gecmez)
    float x, y; renderer_.toVirtual(sw_, sh_, px, py, x, y);
    if (confirmBack_) {                                           // onay: EVET sol, HAYIR sag (disari dokunus = hayir)
        const float W = (float)renderer_.vw(), H = (float)renderer_.vh(), x0 = (W - 300) * 0.5f, y0 = (H - 130) * 0.5f;
        confirmBack_ = false;
        if (x >= x0 + 16 && x <= x0 + 140 && y >= y0 + 70 && y <= y0 + 114) screen_->key(Key::Back, true);
        return;
    }
    screen_->pointerDown(id, x, y);
}
void App::pointerMove(int id, float px, float py) {
    float x, y; renderer_.toVirtual(sw_, sh_, px, py, x, y);
    screen_->pointerMove(id, x, y);
}
void App::pointerUp(int id) { screen_->pointerUp(id); }
bool App::gate(int dir) {
    if (!hint_.empty() || confirmBack_) return false;
    const int type = screen_->shifter();
    auto tap = [&](Key k) { screen_->key(k, true); screen_->key(k, false); };
    if (type == 2) {                                              // otomatik / sirali: duz yukari-asagi
        if (dir == 0) { tap(Key::ShiftUp); return true; }
        if (dir == 1) { tap(Key::ShiftDown); return true; }
        return false;
    }
    if (type != 1) return false;
    const int g = screen_->shifterGear();
    if (g != 0) {                                                 // vitesteyken: yalniz ters yone itis bosa alir
        const bool top = g > 0 && (g % 2) == 1;
        if ((top && dir == 1) || (!top && dir == 0)) { tap(Key::Gear0); gateCol_ = 1; }   // bos: kol orta kanala yaylanir
        return true;
    }
    if (dir == 2) { gateCol_ = std::max(-1, gateCol_ - 1); return true; }
    if (dir == 3) { gateCol_ = std::min(2, gateCol_ + 1); return true; }
    if (gateCol_ < 0) { tap(Key::GearR); return true; }           // R kanali: yukari ya da asagi
    tap((Key)((int)Key::Gear0 + gateCol_ * 2 + (dir == 0 ? 1 : 2)));
    return true;
}
void App::padStick(float x, float y) {
    const float ax = std::fabs(x), ay = std::fabs(y), m = std::max(ax, ay);
    if (m < 0.3f) { stickDir_ = -1; return; }                     // merkeze dondu: yeni itis beklenir
    if (m < 0.65f || stickDir_ >= 0) return;
    stickDir_ = ay >= ax ? (y < 0 ? 0 : 1) : (x < 0 ? 2 : 3);
    gate(stickDir_);
}
void App::key(Key k, bool down) {
    if (!hint_.empty()) { if (down && (k == Key::Enter || k == Key::Back)) hint_.clear(); return; }
    if (confirmBack_) { if (down && k == Key::Back) { confirmBack_ = false; screen_->key(Key::Back, true); }
                        else if (down && (k == Key::Enter || k == Key::ShiftUp)) confirmBack_ = false; return; }
    screen_->key(k, down);
}
bool App::back() {
    if (!hint_.empty()) { hint_.clear(); return true; }
    if (auto* g = dynamic_cast<GarageScreen*>(screen_.get()); g && !g->modal() && !pending_) return false;   // garajda geri = cikis (onay penceresi acik degilse)
    if (confirmBack_) { confirmBack_ = false; screen_->key(Key::Back, true); return true; }   // ikinci basis: evet
    if (screen_->backLeaves()) { confirmBack_ = true; return true; }   // surus ekranlari: once sor
    screen_->key(Key::Back, true);
    return true;
}

void App::setVoice(int i, const VehicleDef* v, bool turboKit) {
    std::unique_ptr<ProceduralEngineAudio> s = v ? std::make_unique<ProceduralEngineAudio>(*v, kSampleRate) : nullptr;
    if (s) s->setTurboKit(turboKit);
    std::lock_guard<std::mutex> g(audioLock_);
    voices_[i].synth = std::move(s);
}

void App::setVoiceTuned(int i, int carId, const Tune* tune) {
    const VehicleDef* v = findVehicle(carId);
    if (!v) { setVoice(i, nullptr); return; }
    VehicleDef d = *v;
    if (tune) { d.engine = effectiveEngine(*v, tune); d.gearbox = effectiveGearbox(*v, tune); }
    setVoice(i, &d, tune && (tune->turbo > 0 || tune->superch > 0));        // ses kopyayi saklar (VehicleDef deger)
    if (tune && tune->cam == kVtecKitCam) {                                  // VTEC kiti: seste de kam gecisi
        std::lock_guard<std::mutex> g(audioLock_);
        if (voices_[i].synth) voices_[i].synth->setVtecKit(vtecKitRpm(engineTable()[d.engine].redline));
    }
}

void App::voice(int i, double rpm, double thr, bool cut, bool inGear, float gain) {
    Voice& v = voices_[i];
    v.rpm = (float)rpm; v.thr = (float)thr; v.cut = cut; v.inGear = inGear; v.gain = gain;
}

void App::renderAudio(float* out, int frames) {
    std::memset(out, 0, sizeof(float) * frames);
    if (!audioLock_.try_lock()) return;           // ses thread'i asla beklemez
    if ((int)mix_.size() < frames) mix_.resize(frames);
    const float engVol = engineVol_.load(), tireVol = tireVol_.load();
    for (Voice& v : voices_) {
        if (!v.synth) continue;
        EngineAudioInput in;
        in.rpm = v.rpm.load(); in.throttle = v.thr.load(); in.fuelCut = v.cut.load(); in.inGear = v.inGear.load();
        in.boost = -1.0;
        v.synth->setInput(in);
        v.synth->render(mix_.data(), frames);
        const float g = v.gain.load() * 0.8f * engVol;
        for (int i = 0; i < frames; ++i) out[i] += mix_[i] * g;
    }
    // Lastik sesi (TireAudio: tonal stick-slip ciglik + burnout hirlamasi)
    for (Voice& v : voices_) {
        const double slip = v.slip.load();
        if (v.tireAudio.silent(slip) || tireVol <= 0.0f) continue;
        v.tireAudio.render(out, frames, slip, v.gain.load() * tireVol, v.lock.load());
    }
    // Ruzgar: beyaz gurultu, iki kutuplu alcak geciren (kesim hizla 450 -> ~2000 Hz), genlik ~ hiz^2, yavas dalga
    if (const float ws = windSpeed_.load(); ws > 8.0f && tireVol > 0.0f) {
        const float u = std::min(1.0f, (ws - 8.0f) / 55.0f);
        const float a = 1.0f - std::exp(-6.2831853f * (450.0f + 26.0f * ws) / kSampleRate);
        const float amp = u * u * 0.32f * tireVol;
        for (int i = 0; i < frames; ++i) {
            windRng_ = windRng_ * 1664525u + 1013904223u;
            const float n = (float)(windRng_ >> 8) / 8388608.0f - 1.0f;
            windLp1_ += a * (n - windLp1_);
            windLp2_ += a * (windLp1_ - windLp2_);
            windPh_ += 6.2831853f * 0.27f / kSampleRate;
            if (windPh_ > 6.2831853f) windPh_ -= 6.2831853f;
            out[i] += windLp2_ * amp * (0.8f + 0.2f * std::sin(windPh_));
        }
    }
    // Efektler: vites "tok"u (85 Hz sonumlu govde + 3 ms metal tik), nitro tislamasi (yuksek geciren gurultu),
    // yagmur (alcak geciren gurultu + seyrek damla tiklari)
    if (clunk_.exchange(0) > 0) clunkT_ = 0.0f;
    if (applause_.exchange(0) > 0) clapT_ = 0.0f;
    const bool nosOn = nos_.load(), rainOn = rain_.load();
    const float sirenTarget = siren_.load();
    if (clunkT_ >= 0.0f || clapT_ >= 0.0f || nosOn || nosEnv_ > 1e-4f || rainOn || sirenTarget > 0.0f || sirenLv_ > 1e-4f) {
        const float dt = 1.0f / kSampleRate;
        const float aN = 1.0f - std::exp(-6.2831853f * 2500.0f * dt), aR = 1.0f - std::exp(-6.2831853f * 1100.0f * dt);
        for (int i = 0; i < frames; ++i) {
            fxRng_ = fxRng_ * 1664525u + 1013904223u;
            const float n = (float)(fxRng_ >> 8) / 8388608.0f - 1.0f;
            float o = 0.0f;
            if (clunkT_ >= 0.0f) {
                const float t = clunkT_;
                o += 0.40f * std::sin(6.2831853f * 85.0f * t) * std::exp(-t / 0.045f) * engVol;
                if (t < 0.003f) o += 0.30f * n * (1.0f - t / 0.003f) * engVol;
                clunkT_ += dt;
                if (clunkT_ > 0.3f) clunkT_ = -1.0f;
            }
            nosEnv_ += ((nosOn ? 1.0f : 0.0f) - nosEnv_) * (nosOn ? 0.0008f : 0.0003f);
            if (nosEnv_ > 1e-4f) { nosLp_ += aN * (n - nosLp_); o += (n - nosLp_) * 0.16f * nosEnv_ * tireVol; }
            if (rainOn) {
                rainLp1_ += aR * (n - rainLp1_); rainLp2_ += aR * (rainLp1_ - rainLp2_);
                o += rainLp2_ * 0.22f * tireVol;
                if (dripT_ < 0.0f && (fxRng_ & 0xFFFF) < 3) { dripT_ = 0.0f; dripF_ = 900.0f + (float)((fxRng_ >> 16) & 1023); }
                if (dripT_ >= 0.0f) {
                    o += 0.05f * std::sin(6.2831853f * dripF_ * dripT_) * std::exp(-dripT_ / 0.012f) * tireVol;
                    dripT_ += dt; if (dripT_ > 0.06f) dripT_ = -1.0f;
                }
            }
            if (clapT_ >= 0.0f) {                                        // alkis: ~180 el/s rastgele carpma, 1-3 kHz bant gurultu
                const float lv = std::min(1.0f, clapT_ / 0.25f) * std::clamp((3.5f - clapT_) / 1.2f, 0.0f, 1.0f);
                if ((fxRng_ & 0xFFFF) < 65536u * 180u / kSampleRate) clapEnv_ = std::max(clapEnv_, 0.5f + 0.5f * (float)((fxRng_ >> 16) & 255) / 255.0f);
                clapEnv_ *= 0.9965f;                                     // ~6 ms sonum
                clapHp_ = n - clapPrev_; clapPrev_ = n;                  // yuksek geciren
                clapLp_ += 0.45f * (clapHp_ - clapLp_);                  // ~4 kHz ustu kirpilir
                o += 0.55f * lv * clapEnv_ * clapLp_ * tireVol + 0.04f * lv * n * tireVol;   // carpmalar + kalabalik ugultusu
                clapT_ += dt; if (clapT_ > 3.5f) clapT_ = -1.0f;
            }
            sirenLv_ += (sirenTarget - sirenLv_) * 0.0005f;              // siren: "wail" 650-1350 Hz, 0.35 Hz tarama
            if (sirenLv_ > 1e-4f) {
                sirenT_ += dt;
                const float f = 1000.0f + 350.0f * std::sin(6.2831853f * 0.35f * sirenT_);
                sirenPh_ += 6.2831853f * f * dt;
                if (sirenPh_ > 6.2831853f) sirenPh_ -= 6.2831853f;
                o += (std::sin(sirenPh_) + 0.3f * std::sin(3.0f * sirenPh_)) * 0.10f * sirenLv_ * tireVol;
            }
            out[i] += o;
        }
    }
    audioLock_.unlock();
    for (int i = 0; i < frames; ++i) out[i] = std::clamp(out[i], -1.0f, 1.0f);
}

bool App::readPixelsRGB(std::vector<unsigned char>& rgb, int& w, int& h) {
    // ZK_SHOT_FBO: ic cozunurlukteki sanal tampondan (renderScale x 640x360 / 360x640) - pencere boyutundan bagimsiz
    const bool fbo = std::getenv("ZK_SHOT_FBO") != nullptr;
    w = fbo ? renderer_.vw() * renderer_.renderScale() : sw_; h = fbo ? renderer_.vh() * renderer_.renderScale() : sh_;
    std::vector<unsigned char> rgba((size_t)w * h * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    if (fbo) renderer_.bindTarget();
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    if (fbo) glBindFramebuffer(GL_FRAMEBUFFER, 0);
    rgb.resize((size_t)w * h * 3);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            for (int c = 0; c < 3; ++c) rgb[((size_t)y * w + x) * 3 + c] = rgba[((size_t)(h - 1 - y) * w + x) * 4 + c];
    return true;
}

} // namespace zk
