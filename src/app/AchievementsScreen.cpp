// ZEHRA KINIK - Basarimlar ekrani: 21 hedef, kazanilanlar altin, odul sagda.
#include "Screens.h"
#include "Ui.h"
#include "game/Achievements.h"

#include <algorithm>
#include <cstdio>

namespace zk {

namespace {
const Rect kBackA{8, 600, 352, 634};
constexpr float kY0 = 66;
}

void AchievementsScreen::render(Renderer& r) {
    const Career& c = app_.career;
    const auto& a = achievements();
    r.begin(360, 640, kUiBg);
    r.rect(0, 0, 360, 58, kUiPanel);
    r.text(8, 8, "BASARIMLAR", 2, kUiGold);
    int n = 0; for (uint32_t m = c.achieved; m; m >>= 1) n += m & 1u;
    char b[64];
    std::snprintf(b, sizeof b, "%d / %zu TAMAMLANDI", n, a.size());
    r.text(8, 34, b, 1, kUiText);
    r.rect(180, 36, 352, 42, {0.15f, 0.15f, 0.18f});
    r.rect(180, 36, 180 + 172.0f * n / (float)a.size(), 42, kUiGold);
    const float kRowH = std::min(25.0f, (594.0f - kY0) / (float)a.size());   // liste uzadikca satir sigar
    for (size_t i = 0; i < a.size(); ++i) {
        const float y = kY0 + i * kRowH;
        const bool done = (c.achieved >> i) & 1u;
        r.rect(8, y, 352, y + kRowH - 3, done ? Color{0.20f, 0.16f, 0.05f} : kUiPanel);
        if (done) r.rect(8, y, 11, y + kRowH - 3, kUiGold);
        r.text(16, y + 3, a[i].name, 1, done ? kUiGold : Color{0.85f, 0.85f, 0.9f});
        r.text(16, y + kRowH - 13, a[i].desc, 1, kUiDim);
        const std::string rw = done ? "TAMAM" : money(a[i].reward);
        r.text(344 - r.textWidth(rw, 1), y + 7, rw, 1, done ? Color{0.4f, 1.0f, 0.5f} : kUiGold);
    }
    button(r, kBackA, "< KARIYER", kUiBtn, 2);
}

void AchievementsScreen::pointerDown(int, float x, float y) {
    if (kBackA.hit(x, y)) app_.goLeague();
}

void AchievementsScreen::key(Key k, bool down) {
    if (down && (k == Key::Back || k == Key::Enter)) app_.goLeague();
}

} // namespace zk
