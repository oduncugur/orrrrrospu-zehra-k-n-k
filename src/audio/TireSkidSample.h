#pragma once
// Gercek lastik kayma ornegi (44.1kHz 16-bit PCM)
#include <cstdint>

namespace zk {
constexpr int kRealSkidCount = 132300;
extern const int16_t kRealSkidSamples[132300];
} // namespace zk
