// ZEHRA KINIK - Platformdan bagimsiz garaj uygulamasi (Android + masaustu ortak)
//  * 360x640 dahili sanal tampon -> en yakin komsu ile piksel-keskin olcekleme (letterbox)
//  * PS1 tarzi donen low-poly arac, HUD (5x7 bitmap font), arac secimi, analog gaz kizagi
//  * Bosta devirlenen motor (PowertrainCore) + gercek zamanli ProceduralEngineAudio
// Platform katmani yalnizca pencere/GL baglami, ses cihazi ve girdiyi saglar.
#pragma once
#include "audio/ProceduralEngineAudio.h"
#include "sim/PowertrainCore.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace zk {

class GarageApp {
public:
    static constexpr int kVW = 360, kVH = 640;
    static constexpr int kSampleRate = 48000;

    GarageApp();
    bool initGraphics();                    // GL baglami aktifken cagrilir
    void shutdownGraphics();                // GL kaynaklari baglamla birlikte yok olur
    void resize(int pixelW, int pixelH);
    void update(double dt);
    void render();                          // varsayilan framebuffer'a cizer (swap platformda)

    // Girdi: pencere piksel koordinatlari
    void pointerDown(int id, float px, float py);
    void pointerMove(int id, float px, float py);
    void pointerUp(int id);
    void setThrottleKey(bool held) { throttleKey_ = held; }
    void selectRelative(int delta) { selectCar(carIdx_ + delta); }

    // Ses geri cagrisi (ses thread'i): mono float
    void renderAudio(float* out, int frames);

    // Test/ekran goruntusu
    bool readPixelsRGB(std::vector<unsigned char>& rgb, int& w, int& h);

private:
    void selectCar(int idx);
    void uploadMesh();
    void rect(float x0, float y0, float x1, float y1, float r, float g, float b);
    void text(float x, float y, const std::string& s, float scale, float r, float g, float b);
    void toVirtual(float px, float py, float& x, float& y) const;
    void viewport(int& vx, int& vy, int& vw, int& vh) const;

    int sw_ = 1, sh_ = 1;
    unsigned p3d_ = 0, p2d_ = 0, pBlit_ = 0, fbo_ = 0, fboTex_ = 0, fboDepth_ = 0, vbo3d_ = 0, vao3d_ = 0,
             vbo2d_ = 0, vao2d_ = 0, vboQuad_ = 0, vaoQuad_ = 0;
    int uMvp_ = -1, uModel_ = -1;
    int meshVerts_ = 0, meshCarId_ = -1;
    bool gl_ = false;
    std::vector<float> ui_;

    int carIdx_ = 0;
    std::unique_ptr<PowertrainCore> pt_;
    float throttle_ = 0.0f; int throttlePointer_ = -1; bool throttleKey_ = false;
    float spin_ = 0.0f;
    double simAcc_ = 0.0;

    std::mutex audioLock_;
    std::unique_ptr<ProceduralEngineAudio> synth_;
    std::atomic<float> aRpm_{900}, aThr_{0};
    std::atomic<bool> aCut_{false};
};

} // namespace zk
