#include "game/FlowScore.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace zk {

FlowScorer::FlowScorer(std::vector<std::pair<double, double>> apexes, double playerHalfWidth)
    : apexes_(std::move(apexes)), halfW_(playerHalfWidth) {}

void FlowScorer::award(long pts, const std::string& what, bool comboEvent) {
    if (comboEvent) {
        if (comboT_ > 0) combo_ = std::min(combo_ + 1, kMaxCombo);
        comboT_ = kComboTime;
        st_.bestCombo = std::max(st_.bestCombo, combo_);
        pts *= combo_;
    }
    st_.score += pts;
    char b[64];
    if (comboEvent && combo_ > 1) std::snprintf(b, sizeof b, "%s +%ld X%d", what.c_str(), pts, combo_);
    else std::snprintf(b, sizeof b, "%s +%ld", what.c_str(), pts);
    msgs_.push_back(b);
}

void FlowScorer::crash() {
    ++st_.crashes;
    const long pen = std::min(st_.score, kCrashPenalty);
    st_.score -= pen;
    combo_ = 1; comboT_ = 0;
    msgs_.push_back("CARPISMA -" + std::to_string(pen));
}

void FlowScorer::update(double dt, double s, double lat, double v, bool offRoad, const std::vector<Car>& cars) {
    st_.topSpeed = std::max(st_.topSpeed, v);
    // Surekli puan: yuksek hiz + karsi seritte gitme (asfalt uzerinde)
    if (!offRoad) {
        if (v > kSpeedFloor) speedAcc_ += dt * (v - kSpeedFloor) * 3.6 * 2.0;    // 180 km/h: 180 puan/s
        if (lat > 0.3 && v > 70 / 3.6) { st_.oncomingTime += dt; speedAcc_ += dt * v * 3.6; }
    }
    if (speedAcc_ >= 1.0) { const long p = (long)speedAcc_; st_.score += p; speedAcc_ -= p; }
    // Kombo penceresi
    if (comboT_ > 0) { comboT_ -= dt; if (comboT_ <= 0) { comboT_ = 0; combo_ = 1; } }
    if (offRoad && combo_ > 1) { combo_ = 1; comboT_ = 0; msgs_.push_back("KOMBO BITTI"); }

    // Yakin gecis: trafik araci ondeyken (ds > 0) arkaya gectigi karede, gecis boyunca olculen en kucuk yanal
    // boslugu kNearGap'ten azsa. Carpisan arac yeniden dogar (yeni uid) -> sayilmaz.
    for (Track& t : tracks_) t.seen = false;
    for (const Car& c : cars) {
        const double ds = c.s - s;
        const int side = ds >= 0 ? 1 : -1;
        auto it = std::find_if(tracks_.begin(), tracks_.end(), [&](const Track& t) { return t.uid == c.uid; });
        if (it == tracks_.end()) { tracks_.push_back({c.uid, side, 99.0, c.oncoming, true}); it = tracks_.end() - 1; }
        Track& tr = *it;
        tr.seen = true;
        if (std::fabs(ds) < 6.0) tr.minGap = std::min(tr.minGap, std::fabs(lat - c.lat) - (halfW_ + 0.9));
        if (tr.side == 1 && side == -1) {
            const double g = tr.minGap;
            if (g < kNearGap && g > -0.3 && v >= kMinNearV && !offRoad) {
                const double close = 1.0 + (kNearGap - std::max(g, 0.0)) / kNearGap;   // 1..2
                long pts = (long)std::lround(100.0 * close * (v * 3.6 / 100.0) * (c.oncoming ? 2.0 : 1.0));
                if (c.oncoming) ++st_.oncomingMisses; else ++st_.nearMisses;
                award(pts, c.oncoming ? "KARSI YAKIN GECIS" : "YAKIN GECIS", true);
            }
            tr.minGap = 99.0;
        }
        tr.side = side;
    }
    tracks_.erase(std::remove_if(tracks_.begin(), tracks_.end(), [](const Track& t) { return !t.seen; }), tracks_.end());

    // Apex: viraj tepesini yuksek yanal ivmeyle (v^2 k) ve asfaltta gecmek
    if (prevS_ < 0 || s < prevS_ - 30.0) {                        // ilk kare ya da geri alindi (kurtarma)
        nextApex_ = 0;
        while (nextApex_ < apexes_.size() && apexes_[nextApex_].first < s) ++nextApex_;
    } else {
        while (nextApex_ < apexes_.size() && apexes_[nextApex_].first <= s) {
            const double ay = v * v * std::fabs(apexes_[nextApex_].second) / 9.81;
            if (!offRoad && ay >= kApexG) { ++st_.apexes; award((long)std::lround(ay * 400.0), "APEX", false); }
            ++nextApex_;
        }
    }
    prevS_ = s;
}

std::vector<std::pair<double, double>> findApexes(const std::vector<double>& s, const std::vector<double>& k, double maxRadius) {
    std::vector<std::pair<double, double>> out;
    const double thr = 1.0 / maxRadius;
    const size_t n = std::min(s.size(), k.size());
    size_t i = 0;
    while (i < n) {
        if (std::fabs(k[i]) < thr) { ++i; continue; }
        const bool pos = k[i] > 0;
        size_t j = i;
        double kmax = 0;
        while (j < n && std::fabs(k[j]) >= thr && (k[j] > 0) == pos) { kmax = std::max(kmax, std::fabs(k[j])); ++j; }
        double s0 = -1, s1 = -1;
        for (size_t q = i; q < j; ++q)
            if (std::fabs(k[q]) >= kmax * 0.995) { if (s0 < 0) s0 = s[q]; s1 = s[q]; }
        out.push_back({0.5 * (s0 + s1), pos ? kmax : -kmax});
        i = j;
    }
    return out;
}

} // namespace zk
