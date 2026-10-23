// ZEHRA KINIK - Basarimlar ekrani: 21 hedef, kazanilanlar altin, odul sagda.
#include "Screens.h"
#include "Ui.h"
#include "game/Achievements.h"

#include <algorithm>
#include <cstdio>

namespace zk {

namespace {
const Rect kBackAP{8, 600, 352, 634};
const Rect kBackAL{500, 6, 632, 36};
constexpr float kY0 = 66;
}

void AchievementsScreen::render(Renderer& r) {
    const Career& c = app_.career;
    const auto& a = achievements();
    const bool land = landscape();
    const float W = land ? 640.0f : 360.0f;
    const float H = land ? 360.0f : 640.0f;
    r.begin((int)W, (int)H, kUiBg);

    int n = 0; for (uint32_t m = c.achieved; m; m >>= 1) n += m & 1u;
    char b[64];
    std::snprintf(b, sizeof b, "%d / %zu TAMAMLANDI", n, a.size());

    if (land) {
        // --- YATAY MOD (640x360) ---
        r.rect(0, 0, 640, 42, kUiPanel);
        r.rect(0, 40, 640, 42, {0.2f, 0.22f, 0.28f});
        r.text(12, 10, "BASARIMLAR", 2, kUiGold);
        r.text(140, 14, b, 1, kUiText);
        r.rect(300, 16, 480, 24, {0.15f, 0.15f, 0.18f});
        r.rect(300, 16, 300 + 180.0f * n / (float)a.size(), 24, kUiGold);
        button(r, kBackAL, "< KARIYER", kUiBtn, 1);

        // 2 Kolon: Sol (0..10), Sag (11..20)
        const size_t half = (a.size() + 1) / 2;
        const float rowH = 27.0f;
        for (size_t i = 0; i < a.size(); ++i) {
            const bool rightCol = i >= half;
            const size_t row = rightCol ? i - half : i;
            const float x0 = rightCol ? 324.0f : 8.0f;
            const float x1 = rightCol ? 632.0f : 316.0f;
            const float y = 46.0f + row * rowH;
            const bool done = (c.achieved >> i) & 1u;

            r.rect(x0, y, x1, y + rowH - 3, done ? Color{0.20f, 0.16f, 0.05f} : kUiPanel);
            if (done) r.rect(x0, y, x0 + 3, y + rowH - 3, kUiGold);

            const float ty = y + (rowH - 3) * 0.5f - 4;
            r.text(x0 + 8, ty, a[i].name, 1, done ? kUiGold : Color{0.85f, 0.85f, 0.9f});
            const float dx = x0 + 8 + r.textWidth(a[i].name, 1) + 6;
            r.textFit(dx, ty, a[i].desc, 1, x1 - 56 - dx, kUiDim);
            const std::string rw = done ? "TAMAM" : money(a[i].reward);
            r.text(x1 - r.textWidth(rw, 1) - 6, ty, rw, 1, done ? Color{0.4f, 1.0f, 0.5f} : kUiGold);
        }
    } else {
        // --- DIKEY MOD (360x640) ---
        r.rect(0, 0, 360, 58, kUiPanel);
        r.text(8, 8, "BASARIMLAR", 2, kUiGold);
        r.text(8, 34, b, 1, kUiText);
        r.rect(180, 36, 352, 42, {0.15f, 0.15f, 0.18f});
        r.rect(180, 36, 180 + 172.0f * n / (float)a.size(), 42, kUiGold);
        const float kRowH = std::min(25.0f, (594.0f - kY0) / (float)a.size());
        for (size_t i = 0; i < a.size(); ++i) {
            const float y = kY0 + i * kRowH;
            const bool done = (c.achieved >> i) & 1u;
            r.rect(8, y, 352, y + kRowH - 3, done ? Color{0.20f, 0.16f, 0.05f} : kUiPanel);
            if (done) r.rect(8, y, 11, y + kRowH - 3, kUiGold);
            const float ty = y + (kRowH - 3) * 0.5f - 4;
            r.text(16, ty, a[i].name, 1, done ? kUiGold : Color{0.85f, 0.85f, 0.9f});
            const float dx = 16 + r.textWidth(a[i].name, 1) + 8;
            r.textFit(dx, ty, a[i].desc, 1, 284 - dx, kUiDim);
            const std::string rw = done ? "TAMAM" : money(a[i].reward);
            r.text(344 - r.textWidth(rw, 1), ty, rw, 1, done ? Color{0.4f, 1.0f, 0.5f} : kUiGold);
        }
        button(r, kBackAP, "< KARIYER", kUiBtn, 2);
    }
}

void AchievementsScreen::pointerDown(int, float x, float y) {
    const bool land = landscape();
    const Rect& backBtn = land ? kBackAL : kBackAP;
    if (backBtn.hit(x, y)) app_.goLeague();
}

void AchievementsScreen::key(Key k, bool down) {
    if (down && (k == Key::Back || k == Key::Enter)) app_.goLeague();
}

} // namespace zk