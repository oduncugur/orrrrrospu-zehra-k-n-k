#include "App.h"
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
    if (!savePath_.empty()) career.save(savePath_);
}
App::~App() = default;

void App::applySettings() {
    const float master = settings.masterVol / 100.0f;
    engineVol_ = master * settings.engineVol / 100.0f;
    tireVol_ = master * settings.tireVol / 100.0f;
    renderer_.integerScale = settings.integerScale;
    renderer_.setRenderScale(settings.renderScale);
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
    windSpeed_ = 0.0f;
}
void App::goGarage() { setVoice(1, nullptr); setScreen(std::make_unique<GarageScreen>(*this)); }
void App::goDrag(int p, int o, bool autopilot) {
    auto s = std::make_unique<DragScreen>(*this, p, o, nullptr, nullptr, false);
    s->setAutopilot(autopilot);
    setScreen(std::move(s));
}
void App::goCareerRace() {
    const OwnedCar& oc = career.car();
    const Opponent opp = career.pickOpponentFor((uint32_t)career.races * 7919u + raceSeed_++);
    lastOpp = opp;
    setScreen(std::make_unique<DragScreen>(*this, oc.carId, opp.carId, &oc.tune, &opp.tune, true));
}
void App::goParts(int cat) { setScreen(std::make_unique<PartsScreen>(*this, cat)); }
void App::goFabricate(int cat) { setScreen(std::make_unique<FabricateScreen>(*this, cat)); }
void App::goGallery() { setScreen(std::make_unique<GalleryScreen>(*this)); }
void App::goDyno() { setScreen(std::make_unique<DynoScreen>(*this)); }
void App::goSettings() { setScreen(std::make_unique<SettingsScreen>(*this)); }
void App::goRoad() {
    const OwnedCar& oc = career.car();
    setScreen(std::make_unique<RoadScreen>(*this, oc.carId, &oc.tune));
}

void App::update(double dt) {
    if (pending_) {
        screen_ = std::move(pending_);
        if (onOrientation) onOrientation(screen_->landscape());
    }
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
