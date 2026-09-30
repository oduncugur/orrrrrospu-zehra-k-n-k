// ZEHRA KINIK - Kare hizi sinirlayici (FPS cap). Saf mantik: bekleme suresini hesaplar, uyumayi platform yapar
// (masaustu SDL_DelayPrecise, Android nanosleep). Son tarih tabanli: kucuk gecikmeler sonraki karelerde telafi
// edilir (ortalama hiz hedefe oturur); bir kareden fazla geride kalinirsa yeniden eslenir (ani kare patlamasi yok).
#pragma once
#include <cstdint>

namespace zk {

class FramePacer {
public:
    // Kare sonunda (swap sonrasi) cagrilir. periodNs <= 0: sinir yok. Donus: uyunacak ns (0 = hemen devam).
    int64_t wait(int64_t nowNs, int64_t periodNs) {
        if (periodNs <= 0) { period_ = 0; return 0; }
        if (periodNs != period_) { period_ = periodNs; next_ = nowNs; }  // yeni sinir: bu kareden itibaren
        const int64_t w = next_ - nowNs;
        if (w < -periodNs) { next_ = nowNs + periodNs; return 0; }   // cok geride: telafi etme, yeniden esle
        next_ += periodNs;
        return w > 0 ? w : 0;
    }

private:
    int64_t period_ = 0, next_ = 0;
};

} // namespace zk
