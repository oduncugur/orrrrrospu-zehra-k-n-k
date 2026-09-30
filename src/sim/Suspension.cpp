#include "Suspension.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace zk {

static constexpr double kPi = 3.14159265358979323846;
static constexpr double kG = 9.81;

// ------------------------------------------------------------------------------------------
// Yol profili
// ------------------------------------------------------------------------------------------
RoadProfile::RoadProfile(char isoClass, uint32_t seed) : cls_(isoClass) {
    // ISO 8608: Gd(n) = Gd(n0) * (n/n0)^-2, n0 = 0.1 dongu/m. A sinifi geometrik ortalama 16e-6 m^3,
    // her sinif 4 kat artar (A=16e-6, B=64e-6, C=256e-6, ...).
    const int idx = std::clamp(isoClass - 'A', 0, 7);
    const double Gd0 = 16e-6 * std::pow(4.0, idx);
    uint32_t r = seed * 2654435761u + 12345u;
    auto rnd = [&]() { r ^= r << 13; r ^= r >> 17; r ^= r << 5; return (r & 0xFFFFFF) / 16777216.0; };
    const int N = 70;
    const double nMin = 0.011, nMax = 3.0;              // dalga boyu ~90 m .. 0.33 m
    for (int i = 0; i < N; ++i) {
        const double n0 = nMin * std::pow(nMax / nMin, (double)i / N);
        const double n1 = nMin * std::pow(nMax / nMin, (double)(i + 1) / N);
        const double n = std::sqrt(n0 * n1), dn = n1 - n0;
        const double amp = std::sqrt(2.0 * Gd0 * std::pow(n / 0.1, -2.0) * dn);
        const double coh = 1.0 / (1.0 + std::pow(n / 0.15, 2.0));   // uzun dalgalar iki izde ortak
        const double k = 2.0 * kPi * n;
        common_.push_back({k, rnd() * 2 * kPi, amp * std::sqrt(coh)});
        left_.push_back({k, rnd() * 2 * kPi, amp * std::sqrt(1.0 - coh)});
        right_.push_back({k, rnd() * 2 * kPi, amp * std::sqrt(1.0 - coh)});
    }
}

double RoadProfile::height(double x, int side) const {
    double h = 0.0;
    for (const Wave& w : common_) h += w.amp * std::sin(w.k * x + w.phase);
    for (const Wave& w : (side < 0 ? left_ : right_)) h += w.amp * std::sin(w.k * x + w.phase);
    for (const RoadFeature& f : features_) {
        if (f.side != 0 && f.side != side) continue;
        const double u = x - f.x;
        if (u < 0.0 || u > f.length) continue;
        const double s = u / f.length;
        switch (f.type) {
        case RoadFeatureType::SpeedBump:                          // yuvarlak kasis
        case RoadFeatureType::Crest:                              // uzun tumsek (sicrama)
            h += f.height * std::sin(kPi * s);
            break;
        case RoadFeatureType::Pothole: {                          // keskin kenarli cukur
            const double edge = std::min(0.04 / f.length, 0.25);
            const double e = s < edge ? s / edge : s > 1 - edge ? (1 - s) / edge : 1.0;
            h += f.height * e;
            break;
        }
        case RoadFeatureType::Manhole:                            // rogar kapagi: hafif cukur, dik kenar
            h += f.height;
            break;
        case RoadFeatureType::ExpansionJoint:                     // dilatasyon: celik dudak + bosluk
            h += (s < 0.2 || s > 0.8) ? std::fabs(f.height) * 0.6 : -std::fabs(f.height);
            break;
        case RoadFeatureType::Cobblestone: {                      // arnavut kaldirimi: 11 cm tas
            const double ph = std::fmod(u, 0.11) / 0.11;
            h += f.height * (0.5 - 0.5 * std::cos(2 * kPi * ph)) * (0.7 + 0.3 * std::sin(u * 37.0 + side));
            break;
        }
        }
    }
    return h;
}

RoadProfile RoadProfile::preset(const std::string& name) {
    if (name == "otoban") {                                       // sehirlerarasi: B sinifi + kopru derzleri
        RoadProfile r('B', 7);
        for (double x = 60; x < 5000; x += 45) r.addFeature({RoadFeatureType::ExpansionJoint, x, 0.05, 0.012, 0});
        r.addFeature({RoadFeatureType::Pothole, 230, 0.6, -0.04, +1});
        r.addFeature({RoadFeatureType::Crest, 420, 25.0, 0.35, 0});
        return r;
    }
    if (name == "sehir") {                                        // sehir ici: C sinifi, kasis, rogar, cukur
        RoadProfile r('C', 11);
        for (double x = 90; x < 5000; x += 160) r.addFeature({RoadFeatureType::SpeedBump, x, 3.6, 0.08, 0});
        for (double x = 40; x < 5000; x += 75)  r.addFeature({RoadFeatureType::Manhole, x, 0.65, -0.018, (int)x % 2 ? 1 : -1});
        r.addFeature({RoadFeatureType::Pothole, 140, 0.9, -0.07, -1});
        r.addFeature({RoadFeatureType::Pothole, 310, 1.2, -0.05, +1});
        r.addFeature({RoadFeatureType::Cobblestone, 180, 60.0, 0.008, 0});
        return r;
    }
    if (name == "koy") {                                          // bozuk koy yolu: E sinifi, bol cukur
        RoadProfile r('E', 23);
        for (double x = 25; x < 5000; x += 37) r.addFeature({RoadFeatureType::Pothole, x, 0.8, -0.06, ((int)x / 37) % 2 ? 1 : -1});
        return r;
    }
    if (name == "dag") {                                          // touge: C sinifi + tumsekler
        RoadProfile r('C', 31);
        for (double x = 120; x < 5000; x += 210) r.addFeature({RoadFeatureType::Crest, x, 14.0, 0.45, 0});
        return r;
    }
    return RoadProfile('A', 3);                                   // drag pisti: yeni asfalt
}

std::string RoadProfile::describe() const {
    char b[96];
    std::snprintf(b, sizeof b, "ISO 8608 sinif %c, %zu ayrik ozellik", cls_, features_.size());
    return b;
}

// ------------------------------------------------------------------------------------------
// Suspansiyon
// ------------------------------------------------------------------------------------------
SuspensionSetup SuspensionSetup::fromVehicle(double mass, double wb, double track, double h, double fw,
                                             double fF, double fR, double zB, double zR) {
    SuspensionSetup s;
    s.mass = mass; s.wheelbase = wb; s.trackF = s.trackR = track; s.hCoG = h; s.frontWeight = fw;
    const double mu = std::clamp(mass * 0.035, 25.0, 60.0);     // kose basina yaysiz kutle
    auto corner = [&](double axleShare, double f) {
        SuspensionCorner c;
        const double ms = mass * axleShare * 0.5 - mu;          // kose basina yayli kutle
        c.unsprungKg = mu;
        c.springK = ms * std::pow(2 * kPi * f, 2.0);
        const double cc = 2.0 * std::sqrt(c.springK * ms);     // kritik sonum
        c.bumpC = zB * cc; c.reboundC = zR * cc;
        c.travelBump = std::clamp(0.12 / f, 0.035, 0.11);
        c.travelDroop = c.travelBump * 1.2;
        return c;
    };
    s.front = corner(fw, fF); s.rear = corner(1.0 - fw, fR);
    s.arbFront = s.front.springK * 0.8; s.arbRear = s.rear.springK * 0.45;
    return s;
}

Suspension::Suspension(const SuspensionSetup& s) : s_(s) {
    const double a = s.wheelbase * (1.0 - s.frontWeight), b = s.wheelbase * s.frontWeight;
    const double xs[4] = {a, a, -b, -b};
    const double ys[4] = {s.trackF / 2, -s.trackF / 2, s.trackR / 2, -s.trackR / 2};
    for (int c = 0; c < 4; ++c) {
        cx_[c] = xs[c]; cy_[c] = ys[c];
        const SuspensionCorner& k = c < 2 ? s.front : s.rear;
        staticFz_[c] = s.mass * kG * (c < 2 ? s.frontWeight : 1.0 - s.frontWeight) * 0.5;
        tireK_[c] = k.tireK;
        Fz_[c] = staticFz_[c];
    }
    if (s_.Iyy <= 0.0) s_.Iyy = s.mass * std::pow(0.5 * s.wheelbase, 2.0);
    if (s_.Ixx <= 0.0) s_.Ixx = s.mass * std::pow(0.4 * s.trackF, 2.0);
}

void Suspension::setTirePressure(int c, double psi) {
    tireK_[c] = 60000.0 + 5000.0 * psi;    // WheelSimulation ile ayni lastik dikey sertligi modeli
}

double Suspension::damper(const SuspensionCorner& s, double v) const {
    // v > 0: basma (bump), v < 0: uzama (rebound). Digresif: diz hizindan sonra egim duser.
    const double c = v >= 0.0 ? s.bumpC : s.reboundC;
    const double av = std::fabs(v);
    const double f = av <= s.kneeVel ? c * av : c * s.kneeVel + c * s.kneeRatio * (av - s.kneeVel);
    return v >= 0.0 ? f : -f;
}

void Suspension::step(double dt, const RoadProfile& road, double x, double ax, double ay) {
    double mUnsprung = 0.0;
    for (int c = 0; c < 4; ++c) mUnsprung += (c < 2 ? s_.front : s_.rear).unsprungKg;
    const double ms = s_.mass - mUnsprung;

    double Fs[4], s[4], ds[4];
    for (int c = 0; c < 4; ++c) {
        const SuspensionCorner& k = c < 2 ? s_.front : s_.rear;
        const double zc = z_ + cx_[c] * th_ + cy_[c] * ph_;       // govde kose yer degistirmesi
        const double vc = vz_ + cx_[c] * wth_ + cy_[c] * wph_;
        s[c] = zu_[c] - zc;                                        // + basma
        ds[c] = vu_[c] - vc;
        const double Fstat = staticFz_[c] - k.unsprungKg * kG;
        double F = Fstat + k.springK * s[c] + damper(k, ds[c]);
        bumpStop_[c] = s[c] > k.travelBump;
        if (bumpStop_[c]) { const double e = s[c] - k.travelBump; F += k.bumpStopK * (e + 40.0 * e * e); }
        if (s[c] < -k.travelDroop) F += 400000.0 * (s[c] + k.travelDroop);  // amortisor tam uzamada: tekerlegi kaldirir
        Fs[c] = F;
    }
    // Viraj denge cubuklari
    const double arbF = s_.arbFront * (s[0] - s[1]), arbR = s_.arbRear * (s[2] - s[3]);
    Fs[0] += arbF; Fs[1] -= arbF; Fs[2] += arbR; Fs[3] -= arbR;

    // Govde (yayli kutle): heave, pitch (burun yukari +), roll (sol yukari +)
    double Fsum = 0.0, My = 0.0, Mx = 0.0;
    for (int c = 0; c < 4; ++c) { Fsum += Fs[c]; My += Fs[c] * cx_[c]; Mx += Fs[c] * cy_[c]; }
    My += ms * ax * s_.hCoG;      // hizlanmada burun kalkar (yuk arkaya)
    Mx += ms * ay * s_.hCoG;      // sola donuste sag taraf coker
    zAcc_ = (Fsum - ms * kG) / ms;
    vz_ += zAcc_ * dt;            wth_ += My / s_.Iyy * dt;     wph_ += Mx / s_.Ixx * dt;
    z_ += vz_ * dt;               th_ += wth_ * dt;             ph_ += wph_ * dt;

    // Yaysiz kutleler + lastik (yalnizca basi; yerden kesilebilir)
    for (int c = 0; c < 4; ++c) {
        const SuspensionCorner& k = c < 2 ? s_.front : s_.rear;
        const int side = cy_[c] > 0 ? -1 : +1;                     // y+ = sol
        const double xc = x + cx_[c];
        // Lastik zarflama: temas izi boyunca 3 nokta ortalamasi (keskin kenarlari yumusatir)
        const double zr = (road.height(xc - 0.08, side) + road.height(xc, side) + road.height(xc + 0.08, side)) / 3.0;
        const double zr0 = road.height(0.0 + cx_[c], side);        // statik denge referansi (x=0)
        const double defl = staticFz_[c] / tireK_[c] + (zr - zr0) - zu_[c];
        double Ft = 0.0;
        if (defl > 0.0) Ft = std::max(0.0, tireK_[c] * defl - k.tireC * vu_[c]);
        const double acc = (Ft - Fs[c] - k.unsprungKg * kG) / k.unsprungKg;
        vu_[c] += acc * dt;
        zu_[c] += vu_[c] * dt;
        Fz_[c] = Ft;
        travel_[c] = s[c];
        peakFz_[c] = std::max(peakFz_[c], Ft);
        airTime_[c] = Ft <= 0.0 ? airTime_[c] + dt : 0.0;
        const bool air = airTime_[c] >= kAirMin;                   // kisa temas kayiplari sayilmaz
        if (air && !wasAir_[c]) ++airEvents_[c];
        if (bumpStop_[c] && !wasStop_[c]) ++stopEvents_[c];
        wasAir_[c] = air; wasStop_[c] = bumpStop_[c];
    }
}

} // namespace zk
