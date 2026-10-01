#include "game/Settings.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace zk {

const std::vector<int>& Settings::fpsOptions() {
    static const std::vector<int> v{30, 45, 60, 75, 90, 120, 144, 165, 240, 0};
    return v;
}
const std::vector<int>& Settings::volumeOptions() {
    static const std::vector<int> v{0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
    return v;
}
const std::vector<int>& Settings::hapticOptions() {
    static const std::vector<int> v{0, 25, 50, 75, 100};
    return v;
}
const std::vector<int>& Settings::tiltSensOptions() {
    static const std::vector<int> v{50, 75, 100, 125, 150, 175, 200};
    return v;
}

namespace {
int indexOf(const std::vector<int>& opts, int cur) {
    for (size_t i = 0; i < opts.size(); ++i) if (opts[i] == cur) return (int)i;
    int best = 0;
    for (size_t i = 1; i < opts.size(); ++i)
        if (std::abs(opts[i] - cur) < std::abs(opts[best] - cur)) best = (int)i;
    return best;
}
// Listedeki en yakin gecerli deger (elle duzenlenmis dosya icin)
int snap(const std::vector<int>& opts, int v) { return opts[indexOf(opts, v)]; }
} // namespace

int stepOption(const std::vector<int>& opts, int cur, int dir) {
    const int i = std::clamp(indexOf(opts, cur) + (dir > 0 ? 1 : dir < 0 ? -1 : 0), 0, (int)opts.size() - 1);
    return opts[i];
}

std::string Settings::serialize() const {
    std::ostringstream o;
    o << "# ZEHRA KINIK ayarlar (elle duzenlenebilir; gecersiz deger en yakin gecerliye cekilir)\n";
    o << "version=" << kVersion << "\n";
    o << "fps_cap=" << fpsCap << "        # 0 = sinirsiz; 30 45 60 75 90 120 144 165 240\n";
    o << "vsync=" << (int)vsync << "          # 0 kapali, 1 acik, 2 uyarlamali\n";
    o << "show_fps=" << (showFps ? 1 : 0) << "\n";
    o << "fullscreen=" << (fullscreen ? 1 : 0) << "\n";
    o << "integer_scale=" << (integerScale ? 1 : 0) << "\n";
    o << "render_scale=" << renderScale << "\n";
    o << "language=" << language << "\n";
    o << "hints_seen=" << hintsSeen << "      # ilk giris ipuclari (0: hepsini yeniden goster)\n";
    o << "road_portrait=" << (roadPortrait ? 1 : 0) << "    # acik yol: 0 yatay, 1 dikey\n";
    o << "master_volume=" << masterVol << "\n";
    o << "engine_volume=" << engineVol << "\n";
    o << "tire_volume=" << tireVol << "\n";
    o << "haptics=" << haptics << "\n";
    o << "tilt_steer=" << (tiltSteer ? 1 : 0) << "\n";
    o << "tilt_sensitivity=" << tiltSens << "\n";
    o << "assist=" << (assist ? 1 : 0) << "\n";
    o << "manual_gears=" << (manualGears ? 1 : 0) << "\n";
    o << "auto_clutch=" << (autoClutch ? 1 : 0) << "      # H-desen: 0 oyuncu debriyaji, 1 otomatik (odul %75)\n";
    o << "mph=" << (mph ? 1 : 0) << "\n";
    return o.str();
}

Settings Settings::parse(const std::string& text) {
    Settings s;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        const size_t hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq);
        k.erase(std::remove_if(k.begin(), k.end(), [](char c) { return c == ' ' || c == '\t' || c == '\r'; }), k.end());
        char* end = nullptr;
        const long lv = std::strtol(line.c_str() + eq + 1, &end, 10);
        if (end == line.c_str() + eq + 1) continue;                  // sayi yok: varsayilan kalir
        const int v = (int)std::clamp(lv, -100000L, 100000L);
        if (k == "fps_cap") s.fpsCap = snap(fpsOptions(), v);
        else if (k == "vsync") s.vsync = (VSync)std::clamp(v, 0, 2);
        else if (k == "show_fps") s.showFps = v != 0;
        else if (k == "fullscreen") s.fullscreen = v != 0;
        else if (k == "integer_scale") s.integerScale = v != 0;
        else if (k == "render_scale") s.renderScale = std::clamp(v, 1, 3);
        else if (k == "language") s.language = std::clamp(v, 0, 6);
        else if (k == "hints_seen") s.hintsSeen = std::max(0, v);
        else if (k == "road_portrait") s.roadPortrait = v != 0;
        else if (k == "master_volume") s.masterVol = snap(volumeOptions(), v);
        else if (k == "engine_volume") s.engineVol = snap(volumeOptions(), v);
        else if (k == "tire_volume") s.tireVol = snap(volumeOptions(), v);
        else if (k == "haptics") s.haptics = snap(hapticOptions(), v);
        else if (k == "tilt_steer") s.tiltSteer = v != 0;
        else if (k == "tilt_sensitivity") s.tiltSens = snap(tiltSensOptions(), v);
        else if (k == "assist") s.assist = v != 0;
        else if (k == "manual_gears") s.manualGears = v != 0;
        else if (k == "auto_clutch") s.autoClutch = v != 0;
        else if (k == "mph") s.mph = v != 0;
    }
    return s;
}

bool Settings::save(const std::string& path) const {
    const std::string tmp = path + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f << serialize();
        f.flush();
        if (!f) return false;
    }
#ifdef _WIN32
    std::remove(path.c_str());
#endif
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}

Settings Settings::load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return Settings{};
    std::stringstream ss; ss << f.rdbuf();
    return parse(ss.str());
}

} // namespace zk
