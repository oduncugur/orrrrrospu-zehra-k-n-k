// ZEHRA KINIK - Kayit kodu ekrani: kariyeri panoya kopyala / panodaki kodu yukle (onayli).
#include "Screens.h"
#include "Ui.h"
#include "game/SaveCode.h"

#include <cstdio>

namespace zk {

namespace {
const Rect kCopy{8, 220, 352, 266}, kPaste{8, 280, 352, 326};
const Rect kYes{8, 420, 176, 462}, kNo{184, 420, 352, 462};
const Rect kBackS{8, 596, 352, 634};
}

void SaveCodeScreen::render(Renderer& r) {
    r.begin(360, 640, kUiBg);
    r.rect(0, 0, 360, 58, kUiPanel);
    r.text(8, 8, "KAYIT KODU", 2, kUiGold);
    r.text(8, 34, "KARIYERI BASKA CIHAZA TASI", 1, kUiText);
    r.text(8, 80, "1) BU CIHAZDA: KODU KOPYALA", 1, {1, 1, 1});
    r.text(8, 96, "2) KODU KENDINE GONDER (MESAJ, NOT...)", 1, {1, 1, 1});
    r.text(8, 112, "3) DIGER CIHAZDA KODU KOPYALA", 1, {1, 1, 1});
    r.text(8, 128, "4) AYARLAR > KAYIT KODU > YAPISTIR", 1, {1, 1, 1});
    r.text(8, 152, "YUKLEME MEVCUT KARIYERIN YERINE GECER!", 1, {1.0f, 0.5f, 0.35f});
    const Career& c = app_.career;
    char b[96];
    std::snprintf(b, sizeof b, "BU KAYIT: %zu ARAC  %s  %d GALIBIYET", c.cars.size(), money(c.money).c_str(), c.wins);
    r.text(8, 180, b, 1, kUiDim);
    const bool clip = app_.onSetClipboard && app_.onGetClipboard;
    button(r, kCopy, "KODU KOPYALA", clip ? kUiGreen : Color{0.3f, 0.3f, 0.32f}, 2);
    button(r, kPaste, "PANODAN YAPISTIR", clip ? Color{0.15f, 0.35f, 0.6f} : Color{0.3f, 0.3f, 0.32f}, 2);
    if (!clip) r.textCentered(180, 340, "BU PLATFORMDA PANO YOK", 1, {1.0f, 0.4f, 0.3f});
    if (!pending_.empty()) {
        Career n;
        if (Career::parse(pending_, n)) {
            r.rect(8, 360, 352, 470, {0.08f, 0.09f, 0.13f});
            std::snprintf(b, sizeof b, "KODDAKI KAYIT: %zu ARAC  %s", n.cars.size(), money(n.money).c_str());
            r.text(16, 372, b, 1, kUiGold);
            std::snprintf(b, sizeof b, "%d YARIS  %d GALIBIYET  %d UN", n.races, n.wins, n.rep);
            r.text(16, 388, b, 1, kUiText);
            r.text(16, 404, "MEVCUT KAYDIN YERINE YUKLENSIN MI?", 1, {1.0f, 0.5f, 0.35f});
            button(r, kYes, "YUKLE", kUiRed, 2);
            button(r, kNo, "VAZGEC", kUiBtn, 2);
        }
    }
    button(r, kBackS, "< AYARLAR", kUiBtn, 2);
    if (msgT_ > 0) { r.rect(0, 540, 360, 572, {0.02f, 0.02f, 0.04f, 0.92f}); r.textCentered(180, 550, msg_, 2, kUiGold); }
}

void SaveCodeScreen::paste() {
    std::string text;
    if (!app_.onGetClipboard || !readSaveCode(app_.onGetClipboard(), text)) { msg_ = "PANODA KAYIT KODU YOK"; msgT_ = 2.0; return; }
    Career n;
    if (!Career::parse(text, n)) { msg_ = "KOD BOZUK / EKSIK"; msgT_ = 2.0; return; }
    pending_ = text;
}

void SaveCodeScreen::pointerDown(int, float x, float y) {
    if (!pending_.empty()) {
        if (kYes.hit(x, y)) {
            Career n;
            if (Career::parse(pending_, n)) {
                app_.career = n;
                app_.treePro = n.treePro;
                app_.saveCareer();
                msg_ = "KAYIT YUKLENDI"; msgT_ = 2.0;
            }
            pending_.clear();
            return;
        }
        if (kNo.hit(x, y)) { pending_.clear(); return; }
    }
    if (kBackS.hit(x, y)) { app_.goSettings(); return; }
    if (kCopy.hit(x, y) && app_.onSetClipboard) {
        app_.saveCareer();
        app_.onSetClipboard(makeSaveCode(app_.career.serialize()));
        msg_ = "KOD PANODA"; msgT_ = 2.0;
        return;
    }
    if (kPaste.hit(x, y)) paste();
}

void SaveCodeScreen::key(Key k, bool down) {
    if (!down) return;
    if (k == Key::Back) { if (!pending_.empty()) pending_.clear(); else app_.goSettings(); }
}

} // namespace zk
