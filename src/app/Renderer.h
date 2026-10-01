// ZEHRA KINIK - Renderer: piksel-keskin sanal tampon (dikey 360x640 / yatay 640x360), 2D katman (alfa),
// bitmap font, dusuk poligon arac modeli onbellegi. Android (GLES3) ve masaustu (GL 3.3) ortak.
#pragma once
#include "app/Lang.h"
#include <map>
#include <string>
#include <vector>

namespace zk {

struct Mat4 { float m[16]; };
Mat4 matMul(const Mat4& a, const Mat4& b);
Mat4 matPerspective(float fovy, float aspect, float n, float f);
Mat4 matOrtho(float l, float r, float b, float t, float n, float f);
Mat4 matLookAt(float ex, float ey, float ez, float cx, float cy, float cz);
Mat4 matRotY(float a);
Mat4 matRotZ(float a);
Mat4 matTranslate(float x, float y, float z);
Mat4 matScale(float s);

struct Color { float r, g, b, a = 1.0f; };

class Renderer {
public:
    static constexpr int kFbo = 640;   // sanal tampon dokusu (her iki yon icin)

    bool init();                       // GL baglami aktifken
    void shutdown();                   // baglam kaybi: onbellek gecersiz
    bool ready() const { return ready_; }

    // Kare: sanal cozunurlukte cizime basla / ekrana olcekle
    void begin(int vw, int vh, Color clear);
    void present(int screenW, int screenH);
    void toVirtual(int screenW, int screenH, float px, float py, float& x, float& y) const;
    int  vw() const { return vw_; }
    int  vh() const { return vh_; }
    bool integerScale = false;         // ekrana tam sayi katla olcekle (piksel-keskin; kenarlarda bant kalabilir)
    bool fillScreen = false;           // ekrani doldur: en-boy farki %35'e kadar gerilir (siyah bant yok)
    // Ic cozunurluk carpani: 1 = retro (sanal piksel), 2-3 = keskin 3B ve kenarlar (2D koordinatlar degismez)
    void setRenderScale(int s) { scale_ = s < 1 ? 1 : s > 3 ? 3 : s; }
    int  renderScale() const { return scale_; }
    void bindTarget();                 // sanal tamponu bagla (test ekran goruntusu okumasi)

    // 2D (sanal piksel, sol ust orijin). flush2D cagrilana kadar biriktirilir.
    void rect(float x0, float y0, float x1, float y1, Color c);
    void gradientV(float x0, float y0, float x1, float y1, Color top, Color bottom);
    void tri(float ax, float ay, float bx, float by, float cx, float cy, Color c);
    void circle(float cx, float cy, float r, int seg, Color c);
    void text(float x, float y, const std::string& s, float scale, Color c);
    float textWidth(const std::string& s, float scale) const {      // UTF-8: kod noktasi sayisi
        const std::string& t = translate(lang, s);
        size_t n = 0; for (unsigned char c : t) n += (c & 0xC0) != 0x80;
        return n * 6.0f * scale - scale;
    }
    Lang lang = Lang::TR;              // arayuz dili: text() yazarken cevirir (Lang.h)
    void textCentered(float cx, float y, const std::string& s, float scale, Color c) { text(cx - textWidth(s, scale) * 0.5f, y, s, scale, c); }
    void flush2D();

    // 3D arac modeli: sanal piksel dikdortgenine (sol ust x,y) cizer
    // wheelSpin: teker donus acisi (rad, ileri +), steer: on teker sapmasi (rad, sola +)
    // Sahne isigi: gunes ve ortam (gece ~0.15 / 0.35); yeni karede (begin) gunduze doner
    void setSceneLight(float sun, float ambient) { sun_ = sun; amb_ = ambient; }
    // Arac gorunumu (yalniz siradaki drawCar icin; sonra fabrikaya doner): boya, cila, serit, jant rengi,
    // govde kiti / kanat (aero), basiklik (suspansiyon). Renkler golgelendiricide degisir: mesh yeniden kurulmaz.
    struct CarLook {
        bool paintOn = false; float paint[3] = {1, 1, 1};
        float paintGloss = 1.0f;                // 0.12 mat .. 1.0 parlak .. 1.3 metalik
        int stripe = 0; float stripeCol[3] = {1, 1, 1};   // 0 yok, 1 orta genis, 2 cift ince, 3 yan
        bool rimOn = false; float rim[3] = {0.6f, 0.6f, 0.6f};
        int aero = 0;                           // aeroTable indeksi (1 on lip, 2 yan etek, 3 spoiler, 4 ducktail, 5 GT, 6 yaris, 7 difuzor, 9 ozel)
        float wingH = 0.3f;                     // ozel kanat yuksekligi (m)
        float drop = 0.0f;                      // govde alcalmasi (m; teker yerinde kalir)
        float alpha = 1.0f;                     // < 1: yari saydam (drag hayaleti); golge cizilmez
    };
    void setCarLook(const CarLook& l) { look_ = l; }
    void drawCar(int carId, float vx, float vy, float vwid, float vhei, const Mat4& proj, const Mat4& view, const Mat4& model,
                 float wheelSpin = 0.0f, float steer = 0.0f);

private:
    float scaleFor(int screenW, int screenH) const;
    void scaleXY(int screenW, int screenH, float& sx, float& sy) const;   // doldurmada eksenler ayri
    struct WheelDraw { float cx, cy, cz; int first, count; };
    struct Mesh { unsigned vao = 0, vbo = 0; int count = 0, bodyCount = 0; float halfL = 2, halfW = 0.9f; std::vector<WheelDraw> wheels;
                  float rearX = -2, frontX = 2, rearTop = 1.0f; unsigned paint = 0xC0C0C0; };
    const Mesh& mesh(int carId);
    const Mesh& kitMesh(int carId, int aero, float wingH);   // govde kiti / kanat (arac olcusune gore)
    std::map<long, Mesh> kits_;
    CarLook look_;
    int uPaint_ = -1, uRim_ = -1, uStripe_ = -1, uPG_ = -1;
    void ensureTarget();                // FBO boyutu = kFbo x olcek (olcek degisince yeniden)
    std::vector<float> batch_;   // x y r g b a
    std::map<int, Mesh> meshes_;
    unsigned p3d_ = 0, p2d_ = 0, pBlit_ = 0, fbo_ = 0, tex_ = 0, depth_ = 0, vao2d_ = 0, vbo2d_ = 0, vaoQ_ = 0, vboQ_ = 0;
    unsigned vaoSh_ = 0, vboSh_ = 0;    // arac golgesi (birim kare, model uzayinda olceklenir)
    int uMvp_ = -1, uModel_ = -1, uSize_ = -1, uUv_ = -1, uEye_ = -1, uAlpha_ = -1, uShadow_ = -1;
    int scale_ = 2, texSize_ = 0;
    float sun_ = 1.0f, amb_ = 1.0f;
    int uLight_ = -1;
    int vw_ = 360, vh_ = 640;
    bool ready_ = false;
};

} // namespace zk
