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

    // Silindirler arasi sabit dolum/yanma farki (+-%9): gercek motorun "tirtikli" alt harmonikleri buradan gelir
    auto cylGain = [&]() { return 1.0 + 0.09 * noise(); };
    if (rotary) {
        // Eksantrik mil devri = RPM; her rotor tur basina bir kez atesler -> 720 deg'de 2n olay
        for (int k = 0; k < 2 * n; ++k)
            events_.push_back({k * 720.0 / (2 * n), 0, 0.0, cylGain(), false});
    } else if (e_.layout == Layout::V6_90Odd) {
        // 90 derece odd-fire V6: 90/150 degisen aralik
        double a = 0.0;
        for (int k = 0; k < n; ++k) { events_.push_back({a, bankOf(order[k]), 0.0, cylGain(), false}); a += (k % 2 == 0) ? 90.0 : 150.0; }
    } else {
        for (int k = 0; k < n; ++k)
            events_.push_back({k * 720.0 / n, bankOf(order[k]), headerDelay(order[k]), cylGain(), false});
    }
    // Supap kapanma tiklari (emme ve egzoz): silindir basina 2 olay / cevrim
    if (!rotary) {
        const size_t nf = events_.size();
        for (size_t k = 0; k < nf; ++k) {
            events_.push_back({std::fmod(events_[k].angle + 230.0, 720.0), 0, 0.0, 0.8 + 0.2 * noise(), true});
            events_.push_back({std::fmod(events_[k].angle + 470.0, 720.0), 0, 0.0, 0.8 + 0.2 * noise(), true});
        }
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
    // Sicak egzoz gazinda ses hizi ~520 m/s (500-600 C)
    for (int b = 0; b < 2; ++b) {
        dlLen_[b] = std::clamp((int)(2.0 * pipeLen_[b] / 520.0 * fs_), 16, 8000);
        dl_[b].assign(dlLen_[b], 0.0f);
    }
    // Motor blogu / kapak cinlamasi (dokum demir ~1.1 kHz, aluminyum kapak ~2.6 kHz), kisa sonumlu
    const double blockScale = std::clamp(2.0 / std::max(0.7, e_.displacementL), 0.5, 1.6);
    blockA_.setBandpass(900.0 + 300.0 * blockScale, 6.0, fs_);
    blockB_.setBandpass(2200.0 + 500.0 * blockScale, 8.0, fs_);
    valveBp_.setBandpass(4200.0, 3.0, fs_);
    // Susturucu hacim rezonansi (Helmholtz, ~120-320 Hz), dusuk Q: tonal degil govdeli
    for (int b = 0; b < 2; ++b)
        cavity_[b].setBandpass((v.exhaust == Exhaust::StraightPipe ? 320.0 : 160.0) * (b ? 1.12 : 1.0), 1.2, fs_);
    // Ton dengesi filtreleri: blok govdesi (hacimle iner), V8 egzoz rezonanslari 80 / 160 Hz (Q 4) + alt bas 70 Hz,
    // VTEC emme bandi ~1.35 kHz (0.8-2.2), gaz acilisi emme nefesi ~520 Hz (300-800)
    body_.setBandpass(std::clamp(210.0 - 18.0 * e_.displacementL, 110.0, 190.0), 1.8, fs_);
    v8a_.setBandpass(80.0, 4.0, fs_); v8b_.setBandpass(160.0, 4.0, fs_); v8sub_.setBandpass(70.0, 0.9, fs_);
    vtecBp_.setBandpass(1350.0, 0.9, fs_);
    intakeBp_.setBandpass(520.0, 1.2, fs_);
}

void ProceduralEngineAudio::Biquad::setBandpass(double f, double q, double fs) {
    const double w0 = 2.0 * kPi * f / fs, alpha = std::sin(w0) / (2.0 * q), a0 = 1.0 + alpha;
    b0 = alpha / a0; b2 = -alpha / a0; a1 = -2.0 * std::cos(w0) / a0; a2 = (1.0 - alpha) / a0;
}

float ProceduralEngineAudio::noise() {
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
    return (float)((rng_ & 0xFFFFFF) / 8388608.0 - 1.0);
}

void ProceduralEngineAudio::firePulse(double start, int bank, double amp, double tau, bool pop, bool mech) {
    for (Pulse& p : pulses_) {
        if (!p.active) {
            p = {start, amp, tau, bank, pop, true, mech};
            return;
        }
    }
}

void ProceduralEngineAudio::render(float* out, int n) {
    const double dt = 1.0 / fs_;
    const bool rotary = e_.layout == Layout::Rotary2 || e_.layout == Layout::Rotary3 || e_.layout == Layout::Rotary4;
    const bool raceCam = v_.exhaust == Exhaust::StraightPipe || e_.induction == Induction::ITB;
    const bool turbo = turboKit_ || e_.induction == Induction::Turbo || e_.induction == Induction::TwinTurbo;
    const bool sc = e_.induction == Induction::Supercharger;
    const bool dogbox = gearboxTable()[v_.gearbox].type == Gearbox::Dogbox;
    const double popProb = v_.exhaust == Exhaust::StraightPipe ? 0.12 : v_.exhaust == Exhaust::Sport ? 0.06 : 0.03;   // stokta da hafif patirti
    // Yanma ayrisma (combustion crack) payi: yuksek sikistirma / yaris motoru daha sert
    const double crack = raceCam ? 0.45 : 0.3;

    for (int i = 0; i < n; ++i) {
        double rpm = std::max(0.0, in_.rpm);
        // Yaris kami rolantide "lope": devir ve dolum dalgalanir
        if (raceCam && rpm < 1500) { lopePhase_ += dt * 2.1; rpm *= 1.0 + 0.05 * std::sin(2 * kPi * lopePhase_); }
        const double thr = std::clamp(in_.throttle, 0.0, 1.0);
        const double load = 0.3 + 0.7 * thr;

        // VTEC gecisi: yumusatilmis ama hizli (40 ms)
        const bool vtecOn = e_.variableCam && rpm > e_.camSwitchRpm && thr > 0.4;
        vtecMix_ += ((vtecOn ? 1.0 : 0.0) - vtecMix_) * std::min(1.0, dt / 0.04);

        // Bant sinirli gurultu (beyaz gurultu yerine ~4 kHz alcak gecirilmis): dijital cizirti yok
        nLp_ += (noise() - nLp_) * 0.45;

        // ---- krank acisi ve olaylar (alt-ornek hassasiyetli zamanlama) ----
        const double prev = theta_;
        const double dTheta = rpm / 60.0 * 360.0 * dt;
        theta_ += dTheta;
        for (const Event& ev : events_) {
            double a = ev.angle;
            while (a <= prev) a += cycleDeg_;
            if (a > theta_) continue;
            const double frac = dTheta > 0.0 ? (a - prev) / dTheta : 0.0;      // olayin ornek icindeki yeri
            const double tEv = t_ - dt + frac * dt;
            if (ev.valve) {                                                     // supap tiki: kisa mekanik vurus
                firePulse(tEv, 0, 0.35 * ev.gain * (0.6 + 0.4 * (1.0 - thr)), 0.00012, false, true);
                continue;
            }
            // Cevrimden cevrime yanma degiskenligi + zamanlama sapmasi (rolantide daha fazla)
            const double idleF = rpm < 1500 ? 1.0 : 0.4;
            const double jitter = 0.00012 * idleF * noise();
            double amp = load * ev.gain * (1.0 + (0.06 + 0.08 * idleF) * noise());
            if (raceCam && rpm < 1500 && noise() > 0.8) amp *= 0.55;           // zayif cevrim (kacan atesleme)
            bool pop = false;
            if (in_.fuelCut) {
                amp = 0.06;                                                     // yakitsiz: sadece pompalama
                if (noise() > 0.7) { amp = 1.8; pop = true; }                  // kesicide yanmamis yakit patlar
            } else if (thr < 0.05 && rpm > 2800) {
                amp = 0.14;
                const double p = in_.antiLag ? 0.5 : popProb;
                if ((noise() * 0.5 + 0.5) < p) { amp = 2.0; pop = true; }
            }
            firePulse(tEv + ev.delay + jitter, ev.bank, amp, pulseTau_ * (pop ? 2.2 : 1.0), pop, false);
            // Yanma blogu uyarir (mekanik vuruntu/tikirti), yukle artar
            firePulse(tEv, 0, crack * amp * (pop ? 0.4 : 1.0), 0.00018, false, true);
        }
        if (theta_ >= cycleDeg_ * 8) theta_ -= cycleDeg_ * 8;

        // ---- darbe uyarimi: iki ustel fark (yumusak yukselis, bant sinirli) ----
        double exc[2] = {0.0, 0.0}, mech = 0.0;
        for (Pulse& p : pulses_) {
            if (!p.active) continue;
            const double age = t_ - p.start;
            if (age < 0.0) continue;
            if (age > p.tau * 10.0 + 0.0005) { p.active = false; continue; }
            const double rise = p.mech ? 0.00005 : 0.00022;
            const double env = std::exp(-age / p.tau) - std::exp(-age / rise);
            if (p.mech) { mech += p.amp * env * nLp_; continue; }
            // Egzoz blowdown: basinc darbesi + turbulansli (gurultulu) kuyruk
            const double body = p.pop ? (0.25 + 0.9 * nLp_) : (0.82 + 0.3 * nLp_);
            exc[p.bank] += p.amp * env * body;
        }
        if (rotary) exc[0] *= 1.3;

        // ---- egzoz: zayif geri beslemeli boru (sonumlu) + susturucu hacim rezonansi ----
        double ex = 0.0;
        for (int b = 0; b < banks_; ++b) {
            float& slot = dl_[b][dlPos_[b]];
            loopLp_[b] += (slot - loopLp_[b]) * 0.18;           // boru ici kayip: yuksek frekans hizla sonumlenir
            const double y = exc[b] - 0.32 * loopLp_[b];         // acik uc yansimasi (zayif: metalik cinlama yok)
            slot = (float)y;
            dlPos_[b] = (dlPos_[b] + 1) % dlLen_[b];
            ex += y + 0.9 * cavity_[b].run(y);
        }

        // ---- susturucu (2 kutuplu alcak gecis), VTEC parlakligi ----
        const double fc = mufflerHz_ * (1.0 + 0.8 * vtecMix_) * (0.6 + 0.4 * thr + rpm / 20000.0);
        const double a = 1.0 - std::exp(-2.0 * kPi * fc * dt);
        lp1_ += (ex - lp1_) * a; lp2_ += (lp1_ - lp2_) * a;
        double sig = lp2_ * 0.92 + (ex - lp2_) * (0.05 + 0.12 * vtecMix_);

        // ---- mekanik katman: blok/kapak cinlamasi + supap tiklari ----
        sig += 0.55 * blockA_.run(mech) + 0.35 * blockB_.run(mech) + 0.25 * valveBp_.run(mech);

        // ---- emme: ITB homurtusu / plenum ugultusu, atesleme frekansiyla modulasyonlu ----
        const double fFire = rpm * std::max(1, e_.cylinders) / 120.0;
        {
            const double f = std::min(0.45, 2.0 * std::sin(kPi * intakeHz_ * (1.0 + 0.4 * vtecMix_) * dt));
            bpLow_ += f * bpBand_;
            const double high = nLp_ - bpLow_ - 0.9 * bpBand_;
            bpBand_ += f * high;
            const double env = 0.5 + 0.5 * std::max(0.0, std::sin(2 * kPi * fFire * t_));
            const double g = (e_.induction == Induction::ITB ? 0.5 : 0.14) * (1.0 + 1.0 * vtecMix_);
            sig += bpBand_ * g * thr * std::pow(rpm / e_.redline, 1.5) * env;
        }

        // ---- turbo: mil hizi, islik (dar bant gurultu + az ton), flutter ----
        if (turbo) {
            const double target = in_.boost >= 0.0 ? in_.boost
                                : thr * std::clamp((rpm - 0.35 * e_.redline) / (0.3 * e_.redline), 0.0, 1.0);
            spool_ += (target - spool_) * std::min(1.0, dt / (target > spool_ ? 0.6 : 0.35));
            const double fw = 2500.0 + 6000.0 * spool_;
            whPhase_ += fw * dt;
            const double f = std::min(0.9, 2.0 * std::sin(kPi * fw * dt));
            tbLow_ += f * tbBand_;
            tbBand_ += f * (nLp_ - tbLow_ - 0.08 * tbBand_);
            // Islik: kompresor kanat gecis frekansi (dar bant gurultu + ton), emme hisirtisi (genis bant)
            sig += spool_ * spool_ * (0.07 * tbBand_ + 0.035 * std::sin(2 * kPi * whPhase_)) + 0.03 * spool_ * thr * nLp_;
            // Blow-off valf: gaz kesilince basincli hava bosalir ("pssht"), yuksek geciren gurultu
            if (prevThr_ > 0.5 && thr < 0.2 && spool_ > 0.3 && bovT_ < 0.0) { bovT_ = 0.0; bovAmp_ = spool_; }
            if (bovT_ >= 0.0) {
                bovT_ += dt;
                const double n = noise();
                bovHp_ = n - bovPrev_; bovPrev_ = n;
                const double env = bovAmp_ * std::min(1.0, bovT_ / 0.01) * std::exp(-bovT_ / 0.18);
                sig += 0.22 * env * bovHp_;
                if (bovT_ > 0.8) bovT_ = -1.0;
            }
            if (prevThr_ > 0.6 && thr < 0.2 && spool_ > 0.4) flutterT_ = 0.0;   // kompresor surge
            if (flutterT_ >= 0.0) {
                flutterT_ += dt;
                const double lambda = 5.0, fFl = 22.0;
                const double env = std::exp(-lambda * flutterT_);
                sig += 0.35 * env * std::max(0.0, std::sin(2 * kPi * fFl * flutterT_)) * nLp_;
                if (env < 0.01) flutterT_ = -1.0;
            }
        }
        if (sc) {   // Roots/vida kompresor uguldamasi: kasnak orani * lob sayisi
            scPhase_ += rpm / 60.0 * 2.4 * 4.0 * dt;
            sig += 0.015 * thr * (rpm / e_.redline) * std::sin(2 * kPi * scPhase_) * (0.7 + 0.3 * nLp_);
        }
        if (dogbox && in_.inGear) {  // duz disli inlemesi: f = mil devri * dis sayisi
            gwPhase_ += rpm / 60.0 * 23.0 * dt;
            sig += 0.012 * (0.3 + thr) * (rpm / e_.redline) * std::sin(2 * kPi * gwPhase_) * (0.8 + 0.2 * nLp_);
        }
        prevThr_ = thr;

        // ---- v6 ton dengesi ----
        {
            const bool v8 = e_.layout == Layout::V8Cross;
            // Blok govdesi +4 dB (bant ekleme), V8: alt bas + 80 / 160 Hz egzoz rezonansi
            sig += 0.6 * body_.run(sig);
            if (v8) {
                sig += 0.9 * v8sub_.run(sig) + 0.35 * v8a_.run(sig) + 0.25 * v8b_.run(sig);
                // Cross-plane gurleme: rolantide 6-9 Hz genlik dalgasi (%25), devir arttikca kaybolur
                tremPh_ += dt * (6.0 + 3.0 * std::min(1.0, rpm / 3000.0));
                const double depth = 0.25 * std::clamp(1.0 - (rpm - 900.0) / 2500.0, 0.0, 1.0);
                sig *= 1.0 - depth * (0.5 + 0.5 * std::sin(2 * kPi * tremPh_));
            }
            // VTEC: emme bandi +5 dB ve biraz daha sert surus (3. 5. harmonik)
            if (vtecMix_ > 0.01) { sig += 0.8 * vtecMix_ * vtecBp_.run(sig); sig = sig * (1.0 - 0.3 * vtecMix_) + 0.3 * vtecMix_ * std::tanh(sig * 2.2) / 2.2 * 1.6; }
            // Gaz acilisi: emme nefesi ("vuuh", 300-800 Hz, ~60 ms)
            if (thr > 0.5 && tipPrev_ < 0.2) tipT_ = 0.0;
            tipPrev_ = thr;
            if (tipT_ >= 0.0) {
                tipT_ += dt;
                const double env = std::min(1.0, tipT_ / 0.015) * std::exp(-tipT_ / 0.06);
                sig += 0.55 * env * intakeBp_.run(nLp_ * 2.0);
                if (tipT_ > 0.4) tipT_ = -1.0;
            }
            // Kesici: her kesmenin basinda 40-70 Hz kisa bas vurusu (15 ms)
            if (in_.fuelCut && !prevCut_) thumpT_ = 0.0;
            prevCut_ = in_.fuelCut;
            if (thumpT_ >= 0.0) {
                thumpT_ += dt;
                sig += 0.35 * std::exp(-thumpT_ / 0.015) * std::sin(2 * kPi * 55.0 * thumpT_);
                if (thumpT_ > 0.1) thumpT_ = -1.0;
            }
            // Ust tiz: 2.5 kHz ustu ~-7 dB (synth parlakligi yok)
            const double ah = 1.0 - std::exp(-2.0 * kPi * 2500.0 * dt);
            hfLp_ += (sig - hfLp_) * ah;
            sig = hfLp_ + (sig - hfLp_) * 0.45;
        }
        // ---- DC engelleme + son alcak gecis (~9 kHz) + yumusak sinirlama ----
        const double hp = sig - dcIn_ + 0.995 * dc_;
        dcIn_ = sig; dc_ = hp;
        outLp_ += (hp - outLp_) * 0.72;
        out[i] = (float)std::tanh(outLp_ * 1.25);
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
