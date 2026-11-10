// ZEHRA KINIK - Manuel vites ogretimi: oyuncu debriyajli (H-desen) ilk suruste adim adim yonlendirme.
// Bir kez tamamlaninca (ayar: clutch_coach=1) bir daha cikmaz.
#pragma once

namespace zk {

struct ClutchCoach {
    int step = 0;          // 0 debriyaja bas, 1 1. vitese tak, 2 kalkis, 3 2. vites, 4 kutlama, 5 bitti
    double t = 0;
    bool justDone = false; // bu karede tamamlandi (ayari kaydet)

    // clutch: 1 = basili; gear: gercek vites (0 = bos); v: m/s
    void update(double dt, double clutch, int gear, double v) {
        justDone = false;
        switch (step) {
        case 0: if (clutch > 0.6) step = 1; break;
        case 1: if (gear == 1) step = 2; else if (clutch < 0.3 && gear == 0) step = 0; break;
        case 2: if (gear == 0) step = clutch > 0.6 ? 1 : 0; else if (v > 4.0 && clutch < 0.15) step = 3; break;
        case 3: if (gear >= 2 && clutch < 0.2) { step = 4; t = 3.0; } break;
        case 4: t -= dt; if (t <= 0) { step = 5; justDone = true; } break;
        default: break;
        }
    }
    bool active() const { return step < 5; }
    const char* text() const {
        switch (step) {
        case 0: return "1/4  DEBRIYAJA BAS (SOL PEDAL) VE TUT";
        case 1: return "2/4  DEBRIYAJ BASILIYKEN KOLU 1. VITESE TAK";
        case 2: return "3/4  GAZ VER, DEBRIYAJI YAVAS YAVAS BIRAK";
        case 3: return "4/4  DEVIR YUKSELINCE: DEBRIYAJ + 2. VITES";
        case 4: return "TAMAM! HER VITES BOYLE: DEBRIYAJ + KOL";
        default: return "";
        }
    }
};

} // namespace zk
