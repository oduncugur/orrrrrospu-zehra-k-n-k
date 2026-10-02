// ZEHRA KINIK - Satin alinabilir kadranlar: analog (ibreli devir + hiz), dijital (rakam + segment devir seridi +
// vites isigi), ikisi birden; turbo basinc gostergesi. Yol / drag HUD'u ve kadran dukkani onizlemesi ortak kullanir.
#pragma once
#include "Renderer.h"

#include <string>

namespace zk {

enum GaugeStyle { GaugeStock = 0, GaugeAnalog = 1, GaugeDigital = 2, GaugeBoth = 3, GaugeStyles = 4 };
const char* gaugeName(int style);

struct GaugeData {
    float rpm = 0, redline = 7000, shiftRpm = 6700;
    float speed = 0, speedMax = 260;        // gosterim biriminde
    const char* unit = "KM/H";
    std::string gear = "N";
    Color gearCol{1.0f, 0.62f, 0.05f};
    float boost = 0, boostMax = 0;          // bar (boostMax <= 0: atmosferik)
    double t = 0;                           // zaman (vites isigi yanip sonmesi)
};

void drawAnalogTach(Renderer& r, float cx, float cy, float R, const GaugeData& d);
void drawAnalogSpeedo(Renderer& r, float cx, float cy, float R, const GaugeData& d);
void drawDigitalCluster(Renderer& r, float x0, float y0, float x1, float y1, const GaugeData& d, bool withTach);
void drawBoostGauge(Renderer& r, float cx, float cy, float R, const GaugeData& d);
// Tam kume: stil + turbo gostergesi, verilen kutuya yerlesir (yatay kutu)
void drawGaugeCluster(Renderer& r, float x0, float y0, float x1, float y1, int style, bool boostGauge, const GaugeData& d);

} // namespace zk
