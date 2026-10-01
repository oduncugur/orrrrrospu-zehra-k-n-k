// ZEHRA KINIK - Basarimlar: kariyer istatistiklerinden kosullar, bir kez odul. Kayit: Career::achieved (bit maskesi).
#pragma once
#include <vector>

namespace zk {

struct Career;

struct AchDef { const char* name; const char* desc; long reward; };
const std::vector<AchDef>& achievements();
bool achievementMet(const Career& c, int idx);

} // namespace zk
