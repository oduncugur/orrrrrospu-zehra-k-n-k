// Ayarlar (ayarlar.cfg) ve FPS sinirlayici testleri
#include "app/FramePacer.h"
#include "game/Settings.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using namespace zk;

static int failures = 0;
#define CHECK(cond, msg) do { if (cond) std::printf("  GECTI  %s\n", msg); else { std::printf("  KALDI  %s\n", msg); ++failures; } } while (0)

static bool same(const Settings& a, const Settings& b) {
    return a.fpsCap == b.fpsCap && a.vsync == b.vsync && a.showFps == b.showFps && a.fullscreen == b.fullscreen &&
           a.integerScale == b.integerScale && a.masterVol == b.masterVol && a.engineVol == b.engineVol &&
           a.tireVol == b.tireVol && a.haptics == b.haptics && a.tiltSteer == b.tiltSteer && a.tiltSens == b.tiltSens &&
           a.assist == b.assist && a.manualGears == b.manualGears && a.mph == b.mph;
}

// Sanal saatle kare dongusu: her kare 'work' ns surer, sonra pacer'in dedigi kadar uyunur. Ortalama kare suresi (ns).
static double simulate(FramePacer& p, int64_t& now, int frames, int64_t period, int64_t work, int64_t* minFrame = nullptr) {
    const int64_t start = now;
    int64_t prev = now, mn = INT64_MAX;
    for (int i = 0; i < frames; ++i) {
        now += work;
        now += p.wait(now, period);
        if (i > 0) mn = std::min(mn, now - prev);
        prev = now;
    }
    if (minFrame) *minFrame = mn;
    return double(now - start) / frames;
}

int main() {
    std::printf("[1] Varsayilanlar\n");
    const Settings d;
    CHECK(d.vsync == VSync::On && d.showFps && !d.fullscreen && d.masterVol == 100 && d.assist && !d.mph, "varsayilanlar makul");
    CHECK(d.fpsCap == Settings::kDefaultFpsCap, "FPS siniri platform varsayilani");
    CHECK(Settings::parse("").fpsCap == d.fpsCap && same(Settings::parse(""), d), "bos dosya = varsayilanlar");

    std::printf("[2] Yaz / oku (gidis-donus)\n");
    Settings s;
    s.fpsCap = 144; s.vsync = VSync::Adaptive; s.showFps = false; s.fullscreen = true; s.integerScale = true;
    s.masterVol = 70; s.engineVol = 40; s.tireVol = 0; s.haptics = 25; s.tiltSteer = false; s.tiltSens = 175;
    s.assist = false; s.manualGears = true; s.mph = true;
    CHECK(same(Settings::parse(s.serialize()), s), "tum alanlar korunur");
    s.fpsCap = 0;
    CHECK(Settings::parse(s.serialize()).fpsCap == 0, "SINIRSIZ (0) korunur");

    std::printf("[3] Elle duzenlenmis / bozuk dosya\n");
    const Settings e = Settings::parse(
        "fps_cap=100\r\nvsync = 7\nmaster_volume=55  # yorum\nengine_volume=-40\ntire_volume=abc\n"
        "haptics=999\ntilt_sensitivity=10\nbilinmeyen=5\nmph=1\n=3\nshow_fps\n");
    CHECK(e.fpsCap == 90, "fps_cap=100 -> en yakin gecerli (90)");
    CHECK(e.vsync == VSync::Adaptive, "vsync=7 -> 2 (uyarlamali)");
    CHECK(e.masterVol == 50, "master_volume=55 -> 50 (bosluk + yorum tolere)");
    CHECK(e.engineVol == 0, "negatif ses -> 0");
    CHECK(e.tireVol == d.tireVol, "sayi olmayan deger -> varsayilan");
    CHECK(e.haptics == 100 && e.tiltSens == 50, "aralik disi -> uca cekilir");
    CHECK(e.mph && e.showFps == d.showFps, "gecerli alan okunur, eksik '=' satiri yok sayilir");

    std::printf("[4] Secenek adimi\n");
    const auto& f = Settings::fpsOptions();
    CHECK(stepOption(f, 60, +1) == 75 && stepOption(f, 60, -1) == 45, "60 -> 75 / 45");
    CHECK(stepOption(f, 30, -1) == 30, "alt uc: 30'da kalir");
    CHECK(stepOption(f, 240, +1) == 0 && stepOption(f, 0, +1) == 0, "240 -> SINIRSIZ, sonra durur");
    CHECK(stepOption(f, 0, -1) == 240, "SINIRSIZ -> 240");
    CHECK(stepOption(Settings::volumeOptions(), 100, +1) == 100, "ses %100'de durur");
    Settings c; c.fpsCap = 60; CHECK(c.frameNs() == 16666666, "60 FPS = 16.67 ms");
    c.fpsCap = 0; CHECK(c.frameNs() == 0, "sinirsiz = 0");

    std::printf("[5] Dosya (atomik kayit)\n");
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "zk_settings_test";
    std::filesystem::create_directories(dir);
    const std::string path = (dir / "ayarlar.cfg").string();
    std::filesystem::remove(path);
    CHECK(same(Settings::load(path), d), "dosya yoksa varsayilanlar");
    s.fpsCap = 120;
    CHECK(s.save(path) && same(Settings::load(path), s), "kaydet + oku");
    s.fpsCap = 30;
    CHECK(s.save(path) && Settings::load(path).fpsCap == 30, "uzerine yaz (Windows'ta da)");
    CHECK(!std::filesystem::exists(path + ".tmp"), "gecici dosya kalmaz");
    std::filesystem::remove_all(dir);

    std::printf("[6] FPS sinirlayici (sanal saat)\n");
    {
        FramePacer p; int64_t now = 1'000'000'000;
        const int64_t per = 1'000'000'000 / 60;
        int64_t mn = 0;
        const double avg = simulate(p, now, 600, per, 3'000'000, &mn);
        std::printf("    60 FPS, 3 ms is: ort. %.3f ms, en kisa kare %.3f ms\n", avg / 1e6, mn / 1e6);
        CHECK(std::fabs(avg - per) / per < 0.005, "ortalama kare suresi hedefte (%0.5)");
        CHECK(mn >= per - 1, "hic bir kare hedeften kisa degil");
        const double avg2 = simulate(p, now, 200, 1'000'000'000 / 144, 1'000'000);
        CHECK(std::fabs(avg2 - 1e9 / 144) / (1e9 / 144) < 0.01, "sinir degisince yeni hedefe oturur (144)");
        const double slow = simulate(p, now, 100, per, 25'000'000);
        CHECK(std::fabs(slow - 25e6) < 1e3, "is > hedef: bekleme yok");
        // Tek bir takilma (80 ms) sonrasi ani kare patlamasi olmamali (yeniden esleme)
        now += 80'000'000; p.wait(now, per);
        int64_t mn2 = 0;
        simulate(p, now, 60, per, 2'000'000, &mn2);
        CHECK(mn2 >= per - 1, "takilma sonrasi telafi patlamasi yok");
        int64_t t = now;
        CHECK(p.wait(t, 0) == 0 && simulate(p, t, 50, 0, 2'000'000) == 2e6, "sinirsiz: hic bekleme yok");
    }

    std::printf("%s (%d hata)\n", failures ? "BASARISIZ" : "TUMU GECTI", failures);
    return failures ? 1 : 0;
}
