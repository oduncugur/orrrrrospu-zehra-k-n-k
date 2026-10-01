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
#include <cstring>

namespace zk {

App::App(const std::string& saveDir) {
    if (!saveDir.empty()) {
        savePath_ = saveDir;
        if (savePath_.back() != '/' && savePath_.back() != '\\') savePath_ += '/';
        settingsPath_ = savePath_ + "ayarlar.cfg";
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
    applySettings();
    setScreen(std::make_unique<GarageScreen>(*this));
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
    windSpeed_ = 0.0f; nos_ = false; rain_ = false;
}
void App::goGarage() {
    setVoice(1, nullptr);
    if (activeEvent >= 0) { activeEvent = -1; setScreen(std::make_unique<LeagueScreen>(*this)); return; }   // etkinlikten donus
    setScreen(std::make_unique<GarageScreen>(*this));
}
void App::goLeague() { activeEvent = -1; setScreen(std::make_unique<LeagueScreen>(*this)); }
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
    if (true) { goLeague(); return; }                 // kariyer artik lig ekranindan
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
void App::goRestore() { setScreen(std::make_unique<RestoreScreen>(*this)); }
void App::goGallery() { setScreen(std::make_unique<GalleryScreen>(*this)); }
void App::goDyno() { setScreen(std::make_unique<DynoScreen>(*this)); }
void App::goSettings() { setScreen(std::make_unique<SettingsScreen>(*this)); }
void App::goRoad() {
    const OwnedCar& oc = career.car();
    setScreen(std::make_unique<RoadScreen>(*this, oc.carId, &oc.tune));
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
    screen_->update(std::min(dt, 0.1));
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
    renderer_.present(sw_, sh_);
}

void App::pointerDown(int id, float px, float py) {
    float x, y; renderer_.toVirtual(sw_, sh_, px, py, x, y);
    screen_->pointerDown(id, x, y);
}
void App::pointerMove(int id, float px, float py) {
    float x, y; renderer_.toVirtual(sw_, sh_, px, py, x, y);
    screen_->pointerMove(id, x, y);
}
void App::pointerUp(int id) { screen_->pointerUp(id); }
void App::key(Key k, bool down) { screen_->key(k, down); }
bool App::back() {
    if (auto* g = dynamic_cast<GarageScreen*>(screen_.get()); g && !g->modal() && !pending_) return false;   // garajda geri = cikis (onay penceresi acik degilse)
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
        v.tireAudio.render(out, frames, slip, v.gain.load() * tireVol);
    }
    // Ruzgar: beyaz gurultu, iki kutuplu alcak geciren (kesim hizla 450 -> ~2000 Hz), genlik ~ hiz^2, yavas dalga
    if (const float ws = windSpeed_.load(); ws > 8.0f && tireVol > 0.0f) {
        const float u = std::min(1.0f, (ws - 8.0f) / 55.0f);
        const float a = 1.0f - std::exp(-6.2831853f * (450.0f + 26.0f * ws) / kSampleRate);
        const float amp = u * u * 0.55f * tireVol;
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
    const bool nosOn = nos_.load(), rainOn = rain_.load();
    if (clunkT_ >= 0.0f || nosOn || nosEnv_ > 1e-4f || rainOn) {
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
