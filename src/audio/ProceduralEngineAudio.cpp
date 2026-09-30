#include "ProceduralEngineAudio.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace zk {

static constexpr double kPi = 3.14159265358979323846;

static std::vector<int> parseOrder(const char* s) {
    std::vector<int> v; int cur = 0; bool any = false;
    for (const char* p = s; ; ++p) {
        if (*p >= '0' && *p <= '9') { cur = cur * 10 + (*p - '0'); any = true; }
        else { if (any) v.push_back(cur); cur = 0; any = false; if (!*p) break; }
    }
    return v;
}

ProceduralEngineAudio::ProceduralEngineAudio(const VehicleDef& v, int sampleRate)
    : v_(v), e_(engineTable()[v.engine]), fs_(sampleRate) {
    rng_ = 0x9E3779B9u ^ (uint32_t)(v.id * 2654435761u);
    const std::vector<int> order = parseOrder(e_.firingOrder);
    const int n = (int)order.size();
    const bool rotary = e_.layout == Layout::Rotary2 || e_.layout == Layout::Rotary3 || e_.layout == Layout::Rotary4;
    banks_ = (e_.bankRule == 'S') ? 1 : 2;

    // Silindir basina header gecikmesi (esitsiz boy header: boxer rumble)
    auto headerDelay = [&](int cyl) {
        if (!e_.unequalHeaders) return 0.0;
        static const double d[] = {0.0, 1.4e-3, 0.35e-3, 1.1e-3, 0.7e-3, 0.2e-3};
        return d[(cyl - 1) % 6];
    };
    auto bankOf = [&](int cyl) {
        if (banks_ == 1) return 0;
        if (e_.bankRule == 'O') return (cyl % 2) ? 0 : 1;
        return (cyl <= e_.cylinders / 2) ? 0 : 1;
    };

    if (rotary) {
        // Eksantrik mil devri = RPM; her rotor tur basina bir kez atesler -> 720 deg'de 2n olay
        for (int k = 0; k < 2 * n; ++k)
            events_.push_back({k * 720.0 / (2 * n), 0, 0.0});
    } else if (e_.layout == Layout::V6_90Odd) {
        // 90 derece odd-fire V6: 90/150 degisen aralik
        double a = 0.0;
        for (int k = 0; k < n; ++k) { events_.push_back({a, bankOf(order[k]), 0.0}); a += (k % 2 == 0) ? 90.0 : 150.0; }
    } else {
        for (int k = 0; k < n; ++k)
            events_.push_back({k * 720.0 / n, bankOf(order[k]), headerDelay(order[k])});
    }

    // Tini: silindir hacmi buyudukce darbe uzun ve bas; rotari kisa ve keskin (port "brap")
    const double perCylL = e_.displacementL / std::max(1, e_.cylinders);
    pulseTau_ = (rotary ? 0.00045 : 0.0006 + 0.0012 * perCylL);
    const double exhBase = v.exhaust == Exhaust::StraightPipe ? 5200.0 : v.exhaust == Exhaust::Sport ? 2600.0 : 1500.0;
    mufflerHz_ = exhBase * (e_.valvesPerCyl >= 4 ? 1.15 : 0.9) * (rotary ? 1.4 : 1.0);
    intakeHz_ = e_.induction == Induction::ITB ? 2300.0 : 850.0;
    // Boru boyu: arac uzunluguna yakin; V motorda banklar farkli boy (X/H boru gecikmesi)
    pipeLen_[0] = 0.55 * v.lengthM + 0.3;
    pipeLen_[1] = pipeLen_[0] * (e_.layout == Layout::V8Cross ? 1.13 : 1.05);
    for (int b = 0; b < 2; ++b) {
        dlLen_[b] = std::clamp((int)(2.0 * pipeLen_[b] / 343.0 * fs_), 16, 8000);
        dl_[b].assign(dlLen_[b], 0.0f);
    }
}

float ProceduralEngineAudio::noise() {
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
    return (float)((rng_ & 0xFFFFFF) / 8388608.0 - 1.0);
}

void ProceduralEngineAudio::firePulse(const Event& ev, double amp, double tau, bool pop) {
    for (Pulse& p : pulses_) {
        if (!p.active) {
            p = {t_ + ev.delay, amp, tau, ev.bank, pop, true};
            return;
        }
    }
}

void ProceduralEngineAudio::render(float* out, int n) {
    const double dt = 1.0 / fs_;
    const bool rotary = e_.layout == Layout::Rotary2 || e_.layout == Layout::Rotary3 || e_.layout == Layout::Rotary4;
    const bool raceCam = v_.exhaust == Exhaust::StraightPipe || e_.induction == Induction::ITB;
    const bool turbo = e_.induction == Induction::Turbo || e_.induction == Induction::TwinTurbo;
    const bool sc = e_.induction == Induction::Supercharger;
    const bool dogbox = gearboxTable()[v_.gearbox].type == Gearbox::Dogbox;
    const double popProb = v_.exhaust == Exhaust::StraightPipe ? 0.12 : v_.exhaust == Exhaust::Sport ? 0.05 : 0.015;

    for (int i = 0; i < n; ++i) {
        double rpm = std::max(0.0, in_.rpm);
        // Yaris kami rolantide "lope": devir ve dolum dalgalanir
        if (raceCam && rpm < 1500) { lopePhase_ += dt * 2.1; rpm *= 1.0 + 0.05 * std::sin(2 * kPi * lopePhase_); }
        const double thr = std::clamp(in_.throttle, 0.0, 1.0);

        // VTEC gecisi: yumusatilmis ama hizli (40 ms)
        const bool vtecOn = e_.variableCam && rpm > e_.camSwitchRpm && thr > 0.4;
        vtecMix_ += ((vtecOn ? 1.0 : 0.0) - vtecMix_) * std::min(1.0, dt / 0.04);

        // ---- krank acisi ve atesleme olaylari ----
        const double prev = theta_;
        theta_ += rpm / 60.0 * 360.0 * dt;
        for (const Event& ev : events_) {
            double a = ev.angle;
            while (a <= prev) a += cycleDeg_;
            if (a > theta_) continue;
            const bool overrun = thr < 0.05 && rpm > 2800;
            double amp = 0.3 + 0.7 * thr;
            amp *= 1.0 + 0.08 * noise() + (raceCam && rpm < 1500 ? 0.25 * noise() : 0.0);
            bool pop = false;
            if (in_.fuelCut) {
                amp = 0.05;                                         // yakitsiz: sadece pompalama
                if (noise() > 0.7) { amp = 1.8; pop = true; }        // kesicide yanmamis yakit patlar
            } else if (overrun) {
                amp = 0.12;
                const double p = in_.antiLag ? 0.5 : popProb;
                if ((noise() * 0.5 + 0.5) < p) { amp = 2.2; pop = true; }
            }
            firePulse(ev, amp, pulseTau_ * (pop ? 2.5 : 1.0), pop);
        }
        if (theta_ >= cycleDeg_ * 8) theta_ -= cycleDeg_ * 8;

        // ---- darbe uyarimi (bank basina) ----
        double exc[2] = {0.0, 0.0};
        for (Pulse& p : pulses_) {
            if (!p.active) continue;
            const double age = t_ - p.start;
            if (age < 0.0) continue;
            if (age > p.tau * 12.0) { p.active = false; continue; }
            const double attack = std::min(1.0, age / 0.00015);
            double s = p.amp * attack * std::exp(-age / p.tau);
            s *= p.pop ? (0.3 + noise()) : (0.75 + 0.25 * noise());
            exc[p.bank] += s;
        }
        if (rotary) { exc[0] *= 1.3; }

        // ---- egzoz boru rezonansi: dalga kilavuzu ----
        double ex = 0.0;
        for (int b = 0; b < banks_; ++b) {
            float& slot = dl_[b][dlPos_[b]];
            loopLp_[b] += (slot - loopLp_[b]) * 0.35;           // boru ici kayip (alcak gecis)
            const double y = exc[b] - 0.55 * loopLp_[b];         // acik uc: ters isaretli yansima
            slot = (float)y;
            dlPos_[b] = (dlPos_[b] + 1) % dlLen_[b];
            ex += y;
        }

        // ---- susturucu (2 kutuplu alcak gecis), VTEC parlakligi ----
        const double fc = mufflerHz_ * (1.0 + 0.9 * vtecMix_) * (0.6 + 0.4 * thr + rpm / 20000.0);
        const double a = 1.0 - std::exp(-2.0 * kPi * fc * dt);
        lp1_ += (ex - lp1_) * a; lp2_ += (lp1_ - lp2_) * a;
        double sig = lp2_ * 0.9 + (ex - lp2_) * (0.05 + 0.15 * vtecMix_);

        // ---- emme: ITB homurtusu / plenum ugultusu, atesleme frekansiyla modulasyonlu ----
        const double fFire = rpm * std::max(1, e_.cylinders) / 120.0;
        {
            const double f = std::min(0.45, 2.0 * std::sin(kPi * intakeHz_ * (1.0 + 0.5 * vtecMix_) * dt));
            const double x = noise();
            bpLow_ += f * bpBand_;
            const double high = x - bpLow_ - 0.6 * bpBand_;
            bpBand_ += f * high;
            const double env = 0.55 + 0.45 * std::sin(2 * kPi * fFire * t_);
            const double g = (e_.induction == Induction::ITB ? 0.45 : 0.12) * (1.0 + 1.2 * vtecMix_);
            sig += bpBand_ * g * thr * std::pow(rpm / e_.redline, 1.5) * env;
        }

        // ---- turbo: mil hizi, islik, flutter ----
        if (turbo) {
            const double target = in_.boost >= 0.0 ? in_.boost
                                : thr * std::clamp((rpm - 0.35 * e_.redline) / (0.3 * e_.redline), 0.0, 1.0);
            spool_ += (target - spool_) * std::min(1.0, dt / (target > spool_ ? 0.6 : 0.35));
            whPhase_ += (2500.0 + 7000.0 * spool_) * dt;
            sig += 0.05 * spool_ * spool_ * std::sin(2 * kPi * whPhase_);
            if (prevThr_ > 0.6 && thr < 0.2 && spool_ > 0.4) flutterT_ = 0.0;   // kompresor surge
            if (flutterT_ >= 0.0) {
                flutterT_ += dt;
                const double lambda = 5.0, fFl = 22.0;
                const double env = std::exp(-lambda * flutterT_);
                sig += 0.35 * env * std::max(0.0, std::sin(2 * kPi * fFl * flutterT_)) * noise();
                if (env < 0.01) flutterT_ = -1.0;
            }
        }
        if (sc) {   // Roots/vida kompresor uguldamasi: kasnak orani * lob sayisi
            scPhase_ += rpm / 60.0 * 2.4 * 4.0 * dt;
            sig += 0.03 * thr * (rpm / e_.redline) * std::sin(2 * kPi * scPhase_);
        }
        if (dogbox && in_.inGear) {  // duz disli inlemesi: f = mil devri * dis sayisi
            gwPhase_ += rpm / 60.0 * 23.0 * dt;
            sig += 0.02 * (0.3 + thr) * (rpm / e_.redline) * std::sin(2 * kPi * gwPhase_);
        }
        prevThr_ = thr;

        // ---- DC engelleme + yumusak sinirlama ----
        const double hp = sig - dcIn_ + 0.995 * dc_;
        dcIn_ = sig; dc_ = hp;
        out[i] = (float)std::tanh(hp * 1.6);
        t_ += dt;
    }
}

std::string ProceduralEngineAudio::signature() const {
    char buf[256];
    std::snprintf(buf, sizeof buf, "%s %s | atesleme %s | %d olay/%g deg | bank %d | tau %.2f ms | boru %.2f m | susturucu %.0f Hz",
                  layoutName(e_.layout), inductionName(e_.induction), e_.firingOrder, (int)events_.size(),
                  cycleDeg_, banks_, pulseTau_ * 1000.0, pipeLen_[0], mufflerHz_);
    return buf;
}

std::vector<float> renderRevDemo(const VehicleDef& v, int fs, double seconds) {
    const EngineDef& e = engineTable()[v.engine];
    ProceduralEngineAudio synth(v, fs);
    const int total = (int)(seconds * fs);
    std::vector<float> out(total);
    const int block = 256;
    const double idle = (e.layout == Layout::V8Cross && e.displacementL > 5.0) ? 700 : 850;
    const double riseRate = std::clamp(1800.0 + 9.0 * e.powerHp, 2500.0, 9000.0); // rpm/s (bosta)
    double rpm = idle;
    bool limiterCut = false;
    for (int i = 0; i < total; i += block) {
        const double t = (double)i / fs;
        EngineAudioInput in;
        if (t < 1.5) { in.throttle = 0.0; rpm += (idle - rpm) * 0.1; }
        else if (t < 4.6) {
            in.throttle = 1.0;
            if (rpm < e.redline) rpm += riseRate * (1.0 - 0.35 * rpm / e.redline) * block / fs;
            // devir kesici sekmesi
            if (rpm >= e.redline) limiterCut = true;
            if (limiterCut) { rpm -= 1800.0 * block / fs; if (rpm < e.redline - 250) limiterCut = false; }
            in.fuelCut = limiterCut;
        } else {
            in.throttle = 0.0;
            rpm = std::max(idle, rpm - 3200.0 * block / fs);
        }
        in.rpm = rpm; in.inGear = t > 1.5; in.boost = -1.0;
        synth.setInput(in);
        synth.render(out.data() + i, std::min(block, total - i));
    }
    // normalize (-1 dBFS)
    float peak = 1e-6f;
    for (float s : out) peak = std::max(peak, std::fabs(s));
    const float g = 0.89f / peak;
    for (float& s : out) s *= g;
    // yumusak giris/cikis
    const int fade = fs / 100;
    for (int i = 0; i < fade && i < total; ++i) { out[i] *= (float)i / fade; out[total - 1 - i] *= (float)i / fade; }
    return out;
}

bool writeWav16(const std::string& path, const std::vector<float>& x, int fs) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const uint32_t dataBytes = (uint32_t)x.size() * 2, riff = 36 + dataBytes, fmtLen = 16, rate = fs, byteRate = fs * 2;
    const uint16_t pcm = 1, ch = 1, align = 2, bits = 16;
    std::fwrite("RIFF", 1, 4, f); std::fwrite(&riff, 4, 1, f); std::fwrite("WAVEfmt ", 1, 8, f);
    std::fwrite(&fmtLen, 4, 1, f); std::fwrite(&pcm, 2, 1, f); std::fwrite(&ch, 2, 1, f);
    std::fwrite(&rate, 4, 1, f); std::fwrite(&byteRate, 4, 1, f); std::fwrite(&align, 2, 1, f); std::fwrite(&bits, 2, 1, f);
    std::fwrite("data", 1, 4, f); std::fwrite(&dataBytes, 4, 1, f);
    for (float s : x) { const int16_t v = (int16_t)std::lrint(std::clamp(s, -1.0f, 1.0f) * 32767.0f); std::fwrite(&v, 2, 1, f); }
    std::fclose(f);
    return true;
}

} // namespace zk
