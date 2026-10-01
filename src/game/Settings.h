// ZEHRA KINIK - Oyun ayarlari (goruntu, ses, kontrol, birimler). Kariyer kaydindan ayri dosya: ayarlar.cfg
// Metin "anahtar=deger" satirlari; elle duzenlenebilir. Bilinmeyen anahtar yok sayilir, gecersiz deger
// en yakin gecerli degere cekilir, eksik anahtar varsayilani korur (eski/yeni surum uyumu).
#pragma once
#include <string>
#include <vector>

namespace zk {

enum class VSync { Off = 0, On = 1, Adaptive = 2 };   // Adaptive: gec kalan karede yirtilmaya izin (swap interval -1)

struct Settings {
    static constexpr int kVersion = 1;

    // Goruntu
    int   fpsCap = kDefaultFpsCap;   // kare/saniye siniri; 0 = sinirsiz
    VSync vsync = VSync::On;
    bool  showFps = true;            // FPS / fizik suresi gostergesi
    bool  fullscreen = false;        // yalniz masaustu
    bool  integerScale = false;      // piksel-keskin tam sayi olcek (kenarlarda siyah bant olabilir)
    int   renderScale = 2;           // ic cozunurluk carpani: 1 retro (piksel), 2 normal, 3 yuksek
    bool  roadPortrait = false;      // acik yol ekrani: false yatay (640x360), true dikey (360x640)
    // Ses (yuzde, 0..100)
    int   masterVol = 100, engineVol = 100, tireVol = 100;
    // Kontrol
    int   haptics = 100;             // titresim gucu (%); 0 = kapali
    bool  tiltSteer = true;          // telefon egimiyle direksiyon (Android, acik yol)
    int   tiltSens = 100;            // egim hassasiyeti (%), 50..200
    bool  assist = true;             // acik yol surus yardimi (ESP + otomatik debriyaj) varsayilani
    bool  manualGears = false;       // (eski) acik yolda manuel vites varsayilani; artik vites kolu sanziman tipinden
    // H-desen manuelde debriyaj: false = oyuncu (analog pedal), true = otomatik. Otomatigin bedeli (denge):
    // virajli bolumde vites degistirilemez, debriyaj gec birakilir, yol yarislarinda odul %75.
    bool  autoClutch = false;
    static constexpr double kAutoClutchPrize = 0.75;
    // Birim / oyun
    bool  mph = false;               // hiz birimi: km/h ya da mph
    int   language = 0;              // arayuz dili (app/Lang.h: 0 TR, 1 EN, ...)
    int   hintsSeen = 0;             // gosterilmis ilk-giris ipuclari (bit)

#ifdef __ANDROID__
    static constexpr int kDefaultFpsCap = 60;    // pil ve isinma
#else
    static constexpr int kDefaultFpsCap = 0;     // masaustu: dikey esitleme zaten sinirlar
#endif

    // Secenek listeleri (arayuz bunlar arasinda gezinir)
    static const std::vector<int>& fpsOptions();          // 30 .. 240, 0 = sinirsiz
    static const std::vector<int>& volumeOptions();       // 0, 10, .. 100
    static const std::vector<int>& hapticOptions();       // 0, 25, 50, 75, 100
    static const std::vector<int>& tiltSensOptions();     // 50 .. 200

    double speedFactor() const { return mph ? 2.2369362920544 : 3.6; }   // m/s -> gosterim birimi
    const char* speedUnit() const { return mph ? "MPH" : "KM/H"; }
    // Kare suresi hedefi (ns); 0 = sinir yok
    long long frameNs() const { return fpsCap > 0 ? 1000000000LL / fpsCap : 0; }

    std::string serialize() const;
    static Settings parse(const std::string& text);        // her zaman gecerli bir sonuc dondurur
    bool save(const std::string& path) const;             // atomik: gecici dosya + yeniden adlandirma
    static Settings load(const std::string& path);        // dosya yoksa varsayilanlar
};

// Listede bir sonraki / onceki deger (dir = +1 / -1, uclarda durur). Deger listede yoksa en yakinina oturur.
int stepOption(const std::vector<int>& opts, int cur, int dir);

} // namespace zk
