// ZEHRA KINIK - Ayarlar ekrani (dikey 360x640). Satir: etiket + "< deger >".
// Dokunma: oklar azalt/artir, satirin geri kalani dongusel ileri. Klavye: W/S satir, A/D (<-/->) deger, ENTER ileri.
#include "Screens.h"
#include "Ui.h"

#include <cstdio>

namespace zk {

namespace {
using It = SettingsScreen::Item;
const Rect kDefaults{8, 596, 104, 634}, kSaveCode{108, 596, 212, 634}, kBack{216, 596, 352, 634};
constexpr float kRowH = 26, kArrowL0 = 204, kArrowL1 = 248, kArrowR0 = 308, kArrowR1 = 352;
const char* const kSections[] = {"GORUNTU", "SES", "KONTROL", "OYUN"};

const char* label(It it) {
    switch (it) {
    case It::Language: return "DIL / LANGUAGE";
    case It::FpsCap: return "FPS SINIRI";
    case It::VSync: return "DIKEY ESITLEME";
    case It::ShowFps: return "FPS GOSTERGESI";
    case It::Fullscreen: return "TAM EKRAN";
    case It::IntScale: return "OLCEKLEME";
    case It::Graphics: return "GRAFIK";
    case It::RoadView: return "YOL EKRANI";
    case It::Master: return "ANA SES";
    case It::Engine: return "MOTOR SESI";
    case It::Tire: return "LASTIK SESI";
    case It::Haptics: return "TITRESIM";
    case It::Tilt: return "EGIM DIREKSIYON";
    case It::TiltSens: return "EGIM HASSASIYET";
    case It::Assist: return "CEKIS KONT.";
    case It::Gears: return "DEBRIYAJ (H)";
    case It::Speed: return "HIZ BIRIMI";
    case It::Tree: return "DRAG AGACI";
    }
    return "";
}
const char* help(It it) {
    switch (it) {
    case It::Language: return "ARAYUZ DILI. ENGLISH: TUM MENULER VE YARIS EKRANLARI";
    case It::FpsCap: return "KARE HIZI SINIRI. DUSUK: AZ PIL VE ISINMA, YUKSEK: AKICI";
    case It::VSync: return "EKRAN YENILEMESINE KILITLER (YIRTILMA OLMAZ). ADAPTIF: GEC KALAN KAREDE BEKLEMEZ";
    case It::ShowFps: return "EKRANDA KARE HIZI VE KARE BASINA FIZIK SURESI (MS)";
    case It::Fullscreen: return "PENCERE / TAM EKRAN. KISAYOL: F11";
    case It::IntScale: return "TAM SAYI: PIKSEL-KESKIN, KENARLARDA SIYAH BANT KALABILIR";
    case It::Graphics: return "IC COZUNURLUK. RETRO: PIKSELLI (EN HIZLI), NORMAL 2X, YUKSEK 3X (KESKIN)";
    case It::RoadView: return "ACIK YOL EKRANI: YATAY (GENIS GORUS) YA DA DIKEY (TEK EL)";
    case It::Master: return "TUM SESLER";
    case It::Engine: return "MOTOR, EGZOZ, TURBO SESI";
    case It::Tire: return "LASTIK CIGLIGI VE BURNOUT SESI";
    case It::Haptics: return "TITRESIM GUCU. 0 = KAPALI";
    case It::Tilt: return "TELEFONU EGEREK DIREKSIYON; TERS: YON CEVRILIR";
    case It::TiltSens: return "YUKSEK: DAHA AZ EGIMLE TAM DIREKSIYON";
    case It::Assist: return "TC/ESP: YALNIZ ARACTA VARSA (YOKSA ECU + ELEKTRONIK PARCASI). YOLDA DA DEGISIR";
    case It::Gears: return "H-DESEN MANUEL: OTOMATIKTE VIRAJDA VITES YOK, GEC KAVRAR, ODUL %75";
    case It::Speed: return "HIZ GOSTERGESI BIRIMI";
    case It::Tree: return "PRO: 3 SARI BIRDEN, 0.4 S. SPOR: SIRALI SARILAR, 0.5 S";
    }
    return "";
}
std::string pct(int v) { return "%" + std::to_string(v); }
const char* onOff(bool b) { return b ? "ACIK" : "KAPALI"; }
// Uzun yardim metnini iki satira bol (satir basina ~57 karakter)
void wrap2(Renderer& r, float y, const std::string& s, Color c) {
    constexpr size_t kMax = 57;
    if (s.size() <= kMax) { r.text(8, y, s, 1, c); return; }
    size_t cut = s.rfind(' ', kMax);
    if (cut == std::string::npos) cut = kMax;
    r.text(8, y, s.substr(0, cut), 1, c);
    r.text(8, y + 11, s.substr(cut + 1), 1, c);
}
} // namespace

SettingsScreen::SettingsScreen(App& app) : app_(app) {
    app_.setVoice(1, nullptr);
    const std::vector<std::pair<int, It>> items = {
        {0, It::Language}, {0, It::FpsCap}, {0, It::VSync}, {0, It::ShowFps},
#ifndef __ANDROID__
        {0, It::Fullscreen},
#endif
        {0, It::IntScale}, {0, It::Graphics}, {0, It::RoadView},
        {1, It::Master}, {1, It::Engine}, {1, It::Tire},
#ifdef __ANDROID__
        {2, It::Haptics}, {2, It::Tilt}, {2, It::TiltSens},
#endif
        {2, It::Assist}, {2, It::Gears},
        {3, It::Speed}, {3, It::Tree}};
    float y = 62;
    int last = -1;
    for (const auto& [sec, it] : items) {
        int head = -1;
        if (sec != last) { head = sec; last = sec; y += 16; }
        rows_.push_back({head, it, y});
        y += kRowH + 2;
    }
}

std::string SettingsScreen::value(It it) const {
    const Settings& s = app_.settings;
    switch (it) {
    case It::Language: return langName((Lang)s.language);
    case It::FpsCap: return s.fpsCap > 0 ? std::to_string(s.fpsCap) : "SINIRSIZ";
    case It::VSync:
        if (s.vsync != VSync::Off && app_.vsyncUnavailable) return "YAZILIM";   // surucu reddetti: yazilimla sinir
        return s.vsync == VSync::Off ? "KAPALI" : s.vsync == VSync::On ? "ACIK" : "ADAPTIF";
    case It::ShowFps: return onOff(s.showFps);
    case It::Fullscreen: return onOff(s.fullscreen);
    case It::IntScale: return s.integerScale ? "TAM SAYI" : s.fillScreen ? "DOLDUR" : "SIGDIR";
    case It::Graphics: return s.renderScale == 1 ? "RETRO" : s.renderScale == 2 ? "NORMAL" : "YUKSEK";
    case It::RoadView: return s.roadPortrait ? "DIKEY" : "YATAY";
    case It::Master: return pct(s.masterVol);
    case It::Engine: return pct(s.engineVol);
    case It::Tire: return pct(s.tireVol);
    case It::Haptics: return s.haptics > 0 ? pct(s.haptics) : "KAPALI";
    case It::Tilt: return !s.tiltSteer ? onOff(false) : s.tiltInvert ? std::string("TERS") : onOff(true);
    case It::TiltSens: return pct(s.tiltSens);
    case It::Assist: return onOff(s.assist);
    case It::Gears: return s.autoClutch ? "OTOMATIK" : "OYUNCU";
    case It::Speed: return s.speedUnit();
    case It::Tree: return app_.treePro ? "PRO .4" : "SPOR .5";
    }
    return "";
}

void SettingsScreen::change(It it, int dir) {
    Settings& s = app_.settings;
    // Listede adim: dir 0 = ileri, sondaysa basa don
    auto list = [dir](const std::vector<int>& opts, int cur) {
        const int n = stepOption(opts, cur, dir == 0 ? +1 : dir);
        return dir == 0 && n == cur ? opts.front() : n;
    };
    switch (it) {
    case It::Language: s.language = (s.language + (dir < 0 ? (int)Lang::Count - 1 : 1)) % (int)Lang::Count; break;
    case It::FpsCap: s.fpsCap = list(Settings::fpsOptions(), s.fpsCap); break;
    case It::VSync: {
        const int v = ((int)s.vsync + (dir < 0 ? 2 : 1)) % 3;
        s.vsync = (VSync)v;
        break;
    }
    case It::ShowFps: s.showFps = !s.showFps; break;
    case It::Fullscreen: s.fullscreen = !s.fullscreen; break;
    case It::IntScale: {                                   // DOLDUR -> SIGDIR -> TAM SAYI -> DOLDUR
        const int m = s.integerScale ? 2 : s.fillScreen ? 0 : 1;
        const int n = (m + (dir < 0 ? 2 : 1)) % 3;
        s.fillScreen = n == 0; s.integerScale = n == 2;
        break;
    }
    case It::Graphics: s.renderScale = dir < 0 ? (s.renderScale + 1) % 3 + 1 : s.renderScale % 3 + 1; break;
    case It::RoadView: s.roadPortrait = !s.roadPortrait; break;
    case It::Master: s.masterVol = list(Settings::volumeOptions(), s.masterVol); break;
    case It::Engine: s.engineVol = list(Settings::volumeOptions(), s.engineVol); break;
    case It::Tire: s.tireVol = list(Settings::volumeOptions(), s.tireVol); break;
    case It::Haptics:
        s.haptics = list(Settings::hapticOptions(), s.haptics);
        app_.haptic(40, 200);                                  // yeni gucu hissettir
        break;
    case It::Tilt:                                   // KAPALI -> ACIK -> TERS (yon) -> KAPALI
        if (!s.tiltSteer) { s.tiltSteer = true; s.tiltInvert = false; } else if (!s.tiltInvert) s.tiltInvert = true; else { s.tiltSteer = false; s.tiltInvert = false; }
        break;
    case It::TiltSens: s.tiltSens = list(Settings::tiltSensOptions(), s.tiltSens); break;
    case It::Assist: s.assist = !s.assist; break;
    case It::Gears: s.autoClutch = !s.autoClutch; break;
    case It::Speed: s.mph = !s.mph; break;
    case It::Tree: app_.treePro = !app_.treePro; app_.saveCareer(); return;
    }
    app_.applySettings();
    app_.saveSettings();
}

void SettingsScreen::render(Renderer& r) {
    r.begin(360, 640, kUiBg);
    r.rect(0, 0, 360, 58, kUiPanel);
    r.text(8, 10, "AYARLAR", 3, kUiGold);
    r.text(8, 40, "DEGISIKLIK ANINDA UYGULANIR VE KAYDEDILIR", 1, kUiDim);
    if (app_.settings.showFps) {
        char b[32]; std::snprintf(b, sizeof b, "%3.0f FPS", app_.fps());
        r.text(352 - r.textWidth(b, 2), 10, b, 2, {0.45f, 0.8f, 0.45f});
    }
    for (size_t i = 0; i < rows_.size(); ++i) {
        const Row& row = rows_[i];
        if (row.section >= 0) r.text(8, row.y - 13, kSections[row.section], 1, kUiGold);
        const bool sel = (int)i == sel_;
        r.rect(8, row.y, 352, row.y + kRowH, sel ? Color{0.2f, 0.24f, 0.34f} : kUiPanel);
        if (sel) r.rect(8, row.y, 11, row.y + kRowH, kUiOrange);
        r.textFit(16, row.y + 7, label(row.item), 2, kArrowL0 - 20, {1, 1, 1});
        r.text(kArrowL0 + 2, row.y + 7, "<", 2, kUiDim);
        r.text(kArrowR1 - 12, row.y + 7, ">", 2, kUiDim);
        r.textFit((kArrowL0 + kArrowR1) / 2 + 2, row.y + 7, value(row.item), 2, kArrowR1 - kArrowL0 - 30, kUiGold, true);
    }
    if (sel_ >= 0 && sel_ < (int)rows_.size()) {
        const It it = rows_[sel_].item;
        if (it == It::VSync && app_.vsyncUnavailable && app_.settings.vsync != VSync::Off) {
            char b[96];
            std::snprintf(b, sizeof b, "EKRAN KARTI SURUCUSU ESITLEMEYI KAPATIYOR. OYUN YAZILIMLA %.0f FPS SINIRI UYGULUYOR", app_.displayHz);
            wrap2(r, 556, b, kUiGold);
        } else wrap2(r, 556, help(it), kUiText);
    }
#ifndef __ANDROID__
    r.text(8, 582, "W/S SATIR  <> DEGER  ENTER DEGISTIR  ESC GERI", 1, {0.55f, 0.75f, 1.0f});
#endif
    button(r, kDefaults, "VARSAYILAN", kUiBtn, 1);
    button(r, kSaveCode, "KAYIT KODU", {0.15f, 0.35f, 0.55f}, 1);
    button(r, kBack, "< GARAJ", kUiBtn, 2);
    if (msgT_ > 0) {
        r.rect(0, 250, 360, 280, {0.02f, 0.02f, 0.04f, 0.9f});
        r.textCentered(180, 258, msg_, 2, kUiGold);
    }
}

void SettingsScreen::pointerDown(int, float x, float y) {
    if (kBack.hit(x, y)) { app_.goGarage(); return; }
    if (kSaveCode.hit(x, y)) { app_.goSaveCode(); return; }
    if (kDefaults.hit(x, y)) {
        app_.settings = Settings{};
        app_.applySettings();
        app_.saveSettings();
        msg_ = "VARSAYILANLAR YUKLENDI"; msgT_ = 1.8;
        return;
    }
    for (size_t i = 0; i < rows_.size(); ++i) {
        const Row& row = rows_[i];
        if (y < row.y || y > row.y + kRowH || x < 8 || x > 352) continue;
        sel_ = (int)i;
        change(row.item, x >= kArrowL0 && x <= kArrowL1 ? -1 : x >= kArrowR0 ? +1 : 0);
        return;
    }
}

void SettingsScreen::key(Key k, bool down) {
    if (!down) return;
    const int n = (int)rows_.size();
    switch (k) {
    case Key::Throttle: sel_ = (sel_ + n - 1) % n; break;
    case Key::Brake: sel_ = (sel_ + 1) % n; break;
    case Key::Left: change(rows_[sel_].item, -1); break;
    case Key::Right: change(rows_[sel_].item, +1); break;
    case Key::Enter: change(rows_[sel_].item, 0); break;
    case Key::Back: case Key::Settings: app_.goGarage(); break;
    default: break;
    }
}

} // namespace zk
