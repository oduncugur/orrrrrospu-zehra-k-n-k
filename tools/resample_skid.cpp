#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>

int main() {
    std::ifstream in("C:/Users/Master Nigger/ZehraKinik/tires_squal_loop.wav", std::ios::binary);
    if (!in) { std::cerr << "Dosya acilamadi\n"; return 1; }

    char header[44];
    in.read(header, 44);

    std::vector<float> srcSamples;
    uint8_t b[3];
    while (in.read((char*)b, 3)) {
        int32_t val = (b[0]) | (b[1] << 8) | (b[2] << 16);
        if (val & 0x800000) val |= 0xFF000000;
        srcSamples.push_back(val / 8388608.0f);
    }
    std::cout << "Okunan 96kHz ornek: " << srcSamples.size() << " (" << srcSamples.size()/96000.0f << "s)\n";

    double ratio = 96000.0 / 44100.0;
    int dstCount = (int)(srcSamples.size() / ratio);
    std::vector<int16_t> dstSamples(dstCount);

    float maxAbs = 0.0f;
    for (int i = 0; i < dstCount; ++i) {
        double srcIdx = i * ratio;
        int idx0 = (int)srcIdx;
        int idx1 = (idx0 + 1 < (int)srcSamples.size()) ? idx0 + 1 : idx0;
        float frac = (float)(srcIdx - idx0);
        float s = (1.0f - frac) * srcSamples[idx0] + frac * srcSamples[idx1];
        if (std::fabs(s) > maxAbs) maxAbs = std::fabs(s);
    }

    float norm = maxAbs > 0 ? (0.92f / maxAbs) : 1.0f;
    for (int i = 0; i < dstCount; ++i) {
        double srcIdx = i * ratio;
        int idx0 = (int)srcIdx;
        int idx1 = (idx0 + 1 < (int)srcSamples.size()) ? idx0 + 1 : idx0;
        float frac = (float)(srcIdx - idx0);
        float s = ((1.0f - frac) * srcSamples[idx0] + frac * srcSamples[idx1]) * norm;
        int v = (int)(s * 32767.0f);
        if (v > 32767) v = 32767;
        if (v < -32768) v = -32768;
        dstSamples[i] = (int16_t)v;
    }

    std::cout << "44.1kHz hedef ornek: " << dstCount << "\n";

    std::ofstream out("C:/Users/Master Nigger/ZehraKinik/src/audio/TireSkidSample.h");
    out << "#pragma once\n";
    out << "// Gercek lastik kayma ornegi (44.1kHz 16-bit PCM)\n";
    out << "#include <cstdint>\n\n";
    out << "namespace zk {\n";
    out << "constexpr int kRealSkidCount = " << dstCount << ";\n";
    out << "extern const int16_t kRealSkidSamples[" << dstCount << "];\n";
    out << "} // namespace zk\n";

    std::ofstream outCpp("C:/Users/Master Nigger/ZehraKinik/src/audio/TireSkidSample.cpp");
    outCpp << "#include \"TireSkidSample.h\"\n\n";
    outCpp << "namespace zk {\n";
    outCpp << "const int16_t kRealSkidSamples[" << dstCount << "] = {\n";
    for (size_t i = 0; i < dstSamples.size(); ++i) {
        outCpp << dstSamples[i] << ",";
        if (i % 25 == 24) outCpp << "\n";
    }
    outCpp << "\n};\n} // namespace zk\n";

    std::cout << "TireSkidSample.h ve TireSkidSample.cpp basariyla olusturuldu!\n";
    return 0;
}
