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

// Yatay (640x360)
const Rect kCopyL{342, 66, 612, 110}, kPasteL{342, 122, 612, 166};
const Rect kYesL{342, 264, 470, 304}, kNoL{484, 264, 612, 304};
const Rect kBackSL{24, 286, 298, 328};
}

void SaveCodeScreen::render(Renderer& r) {
    const bool land = app_.settings.menuLandscape;
    r.begin(land ? 640 : 360, land ? 360 : 640, kUiBg);
    if (land) {
        r.rect(0, 0, 640, 44, kUiPanel);
        r.text(12, 10, "KAYIT KODU", 2, kUiGold);
        r.text(170, 12, "KARIYERI BASKA CIHAZA TASI", 1, kUiText);

        // Sol panel: aciklama ve mevcut durum
        r.rect(12, 52, 310, 344, {0.08f, 0.09f, 0.13f});
        r.text(24, 68, "1) BU CIHAZDA: KODU KOPYALA", 1, {1, 1, 1});
        r.text(24, 94, "2) KODU KENDINE GONDER (MESAJ/NOT)", 1, {1, 1, 1});
        r.text(24, 120, "3) DIGER CIHAZDA KODU KOPYALA", 1, {1, 1, 1});
        r.text(24, 146, "4) AYARLAR > KAYIT KODU > YAPISTIR", 1, {1, 1, 1});
        r.text(24, 180, "YUKLEME MEVCUT KARIYERIN YERINE GECER!", 1, {1.0f, 0.5f, 0.35f});

        const Career& c = app_.career;
        char b[96];
        std::snprintf(b, sizeof b, "BU KAYIT: %zu ARAC  %s  %d GALIBIYET", c.cars.size(), money(c.money).c_str(), c.wins);
        r.text(24, 214, b, 1, kUiGold);
        button(r, kBackSL, "< AYARLAR", kUiBtn, 2);

        // Sag panel: eylemler
        r.rect(326, 52, 628, 344, {0.08f, 0.09f, 0.13f});
        const bool clip = app_.onSetClipboard && app_.onGetClipboard;
        button(r, kCopyL, "KODU KOPYALA", clip ? kUiGreen : Color{0.3f, 0.3f, 0.32f}, 2);
        button(r, kPasteL, "PANODAN YAPISTIR", clip ? Color{0.15f, 0.35f, 0.6f} : Color{0.3f, 0.3f, 0.32f}, 2);
        if (!clip) r.textCentered(477, 182, "BU PLATFORMDA PANO YOK", 1, {1.0f, 0.4f, 0.3f});

        if (!pending_.empty()) {
            Career n;
            if (Career::parse(pending_, n)) {
                r.rect(342, 178, 612, 316, {0.12f, 0.14f, 0.20f});
                std::snprintf(b, sizeof b, "KODDAKI KAYIT: %zu ARAC  %s", n.cars.size(), money(n.money).c_str());
                r.text(354, 190, b, 1, kUiGold);
                std::snprintf(b, sizeof b, "%d YARIS  %d GALIBIYET  %d UN", n.races, n.wins, n.rep);
                r.text(354, 208, b, 1, kUiText);
                r.text(354, 226, "MEVCUT KAYDIN YERINE YUKLENSIN MI?", 1, {1.0f, 0.5f, 0.35f});
                button(r, kYesL, "YUKLE", kUiRed, 2);
                button(r, kNoL, "VAZGEC", kUiBtn, 2);
            }
        }
        if (msgT_ > 0) { r.rect(160, 318, 480, 348, {0.02f, 0.02f, 0.04f, 0.92f}); r.textCentered(320, 326, msg_, 2, kUiGold); }
    } else {
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
}

void SaveCodeScreen::paste() {
    std::string text;
    if (!app_.onGetClipboard || !readSaveCode(app_.onGetClipboard(), text)) { msg_ = "PANODA KAYIT KODU YOK"; msgT_ = 2.0; return; }
    Career n;
    if (!Career::parse(text, n)) { msg_ = "KOD BOZUK / EKSIK"; msgT_ = 2.0; return; }
    pending_ = text;
}

void SaveCodeScreen::pointerDown(int, float x, float y) {
    const bool land = app_.settings.menuLandscape;
    const Rect yesR = land ? kYesL : kYes;
    const Rect noR = land ? kNoL : kNo;
    const Rect backR = land ? kBackSL : kBackS;
    const Rect copyR = land ? kCopyL : kCopy;
    const Rect pasteR = land ? kPasteL : kPaste;

    if (!pending_.empty()) {
        if (yesR.hit(x, y)) {
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
        if (noR.hit(x, y)) { pending_.clear(); return; }
    }
    if (backR.hit(x, y)) { app_.goSettings(); return; }
    if (copyR.hit(x, y) && app_.onSetClipboard) {
        app_.saveCareer();
        app_.onSetClipboard(makeSaveCode(app_.career.serialize()));
        msg_ = "KOD PANODA"; msgT_ = 2.0;
        return;
    }
    if (pasteR.hit(x, y)) paste();
}

void SaveCodeScreen::key(Key k, bool down) {
    if (!down) return;
    if (k == Key::Back) { if (!pending_.empty()) pending_.clear(); else app_.goSettings(); }
}

} // namespace zk
