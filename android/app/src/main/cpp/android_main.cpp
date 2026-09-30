// ZEHRA KINIK - Android istemcisi (Faz 2: Garaj + motor ses testi)
//
//  * 360x640 dahili sanal tampon -> en yakin komsu (nearest) ile piksel-keskin tam ekran olcekleme.
//  * PS1 tarzi low-poly arac modeli (duz golgeleme), 324 arac arasinda gezinme.
//  * Sag kenar dikey kizak: analog gaz (0..1). Motor bosta serbest devirlenir (PowertrainCore:
//    VTEC, ITB tepkisi, devir kesici). Ses AAudio geri cagrisinda ProceduralEngineAudio ile uretilir.
#include "audio/ProceduralEngineAudio.h"
#include "garage/LowPolyModel.h"
#include "garage/VehicleCatalog.h"
#include "sim/PowertrainCore.h"

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <aaudio/AAudio.h>
#include <android/log.h>
#include <android_native_app_glue.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "ZehraKinik", __VA_ARGS__)

using namespace zk;

namespace {

constexpr int kVW = 360, kVH = 640;   // sanal cozunurluk

// ------------------------------------------------------------------ 5x7 bitmap font
const char* glyph(char c) {
    switch (std::toupper((unsigned char)c)) {
    case '0': return ".###.#...##..###.#.###..##...#.###.";
    case '1': return "..#...##....#....#....#....#...###.";
    case '2': return ".###.#...#....#...#...#...#...#####";
    case '3': return "#####...#...#.....#.....##...#.###.";
    case '4': return "...#...##..#.#.#..#.#####...#....#.";
    case '5': return "######....####.....#....##...#.###.";
    case '6': return "..##..#...#....####.#...##...#.###.";
    case '7': return "#####....#...#...#...#....#....#...";
    case '8': return ".###.#...##...#.###.#...##...#.###.";
    case '9': return ".###.#...##...#.####....#...#..##..";
    case 'A': return ".###.#...##...#######...##...##...#";
    case 'B': return "####.#...##...#####.#...##...#####.";
    case 'C': return ".###.#...##....#....#....#...#.###.";
    case 'D': return "###..#..#.#...##...##...##..#.###..";
    case 'E': return "######....#....####.#....#....#####";
    case 'F': return "######....#....####.#....#....#....";
    case 'G': return ".###.#...##....#.####...##...#.####";
    case 'H': return "#...##...##...#######...##...##...#";
    case 'I': return ".###...#....#....#....#....#...###.";
    case 'J': return "..###...#....#....#....#.#..#..##..";
    case 'K': return "#...##..#.#.#..##...#.#..#..#.#...#";
    case 'L': return "#....#....#....#....#....#....#####";
    case 'M': return "#...###.###.#.##.#.##...##...##...#";
    case 'N': return "#...##...###..##.#.##..###...##...#";
    case 'O': return ".###.#...##...##...##...##...#.###.";
    case 'P': return "####.#...##...#####.#....#....#....";
    case 'Q': return ".###.#...##...##...##.#.##..#..##.#";
    case 'R': return "####.#...##...#####.#.#..#..#.#...#";
    case 'S': return ".#####....#.....###.....#....#####.";
    case 'T': return "#####..#....#....#....#....#....#..";
    case 'U': return "#...##...##...##...##...##...#.###.";
    case 'V': return "#...##...##...##...##...#.#.#...#..";
    case 'W': return "#...##...##...##.#.##.#.##.#.#.#.#.";
    case 'X': return "#...##...#.#.#...#...#.#.#...##...#";
    case 'Y': return "#...##...#.#.#...#....#....#....#..";
    case 'Z': return "#####....#...#...#...#...#....#####";
    case '-': return "...............#####...............";
    case '.': return "..........................##...##..";
    case '/': return "....#...#....#...#...#....#...#....";
    case ':': return "......##...##........##...##.......";
    case '<': return "...#...#...#...#.....#.....#.....#.";
    case '>': return ".#.....#.....#.....#...#...#...#...";
    case '#': return ".#.#..#.#.#####.#.#.#####.#.#..#.#.";
    case '%': return "##..###..#...#...#...#...#..###..##";
    case '(': return "...#...#...#....#....#.....#.....#.";
    case ')': return ".#.....#.....#....#....#...#...#...";
    case '+': return ".......#....#..#####..#....#.......";
    case '!': return "..#....#....#....#..............#..";
    case '=': return "..........#####.....#####..........";
    default:  return nullptr;
    }
}

// ------------------------------------------------------------------ GL yardimcilari
GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[512]; glGetShaderInfoLog(s, 512, nullptr, log); LOGI("shader: %s", log); }
    return s;
}
GLuint program(const char* vs, const char* fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, compile(GL_VERTEX_SHADER, vs));
    glAttachShader(p, compile(GL_FRAGMENT_SHADER, fs));
    glLinkProgram(p);
    return p;
}

struct Mat4 { float m[16]; };
Mat4 mul(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int c = 0; c < 4; ++c)
        for (int rr = 0; rr < 4; ++rr) {
            float s = 0; for (int k = 0; k < 4; ++k) s += a.m[k * 4 + rr] * b.m[c * 4 + k];
            r.m[c * 4 + rr] = s;
        }
    return r;
}
Mat4 perspective(float fovy, float aspect, float n, float f) {
    const float t = 1.0f / std::tan(fovy * 0.5f);
    Mat4 r{}; r.m[0] = t / aspect; r.m[5] = t; r.m[10] = (f + n) / (n - f); r.m[11] = -1; r.m[14] = 2 * f * n / (n - f);
    return r;
}
Mat4 lookAt(float ex, float ey, float ez, float cx, float cy, float cz) {
    float fx = cx - ex, fy = cy - ey, fz = cz - ez; float l = std::sqrt(fx * fx + fy * fy + fz * fz); fx /= l; fy /= l; fz /= l;
    float sx = fy * 0 - fz * 1, sy = fz * 0 - fx * 0, sz = fx * 1 - fy * 0; l = std::sqrt(sx * sx + sy * sy + sz * sz); sx /= l; sy /= l; sz /= l;
    const float ux = sy * fz - sz * fy, uy = sz * fx - sx * fz, uz = sx * fy - sy * fx;
    Mat4 r{};
    r.m[0] = sx; r.m[4] = sy; r.m[8] = sz;
    r.m[1] = ux; r.m[5] = uy; r.m[9] = uz;
    r.m[2] = -fx; r.m[6] = -fy; r.m[10] = -fz;
    r.m[12] = -(sx * ex + sy * ey + sz * ez); r.m[13] = -(ux * ex + uy * ey + uz * ez); r.m[14] = fx * ex + fy * ey + fz * ez;
    r.m[15] = 1;
    return r;
}
Mat4 rotY(float a) {
    Mat4 r{}; const float c = std::cos(a), s = std::sin(a);
    r.m[0] = c; r.m[2] = -s; r.m[5] = 1; r.m[8] = s; r.m[10] = c; r.m[15] = 1;
    return r;
}

const char* kVs3D = R"(#version 300 es
layout(location=0) in vec3 aPos; layout(location=1) in vec3 aNrm; layout(location=2) in vec3 aCol;
uniform mat4 uMvp; uniform mat4 uModel; flat out vec3 vCol;
void main(){ vec3 n = normalize(mat3(uModel) * aNrm); float l = 0.38 + 0.62 * max(dot(n, normalize(vec3(0.4,0.9,0.5))), 0.0);
  vCol = aCol * l; gl_Position = uMvp * vec4(aPos, 1.0); })";
const char* kFs3D = R"(#version 300 es
precision mediump float; flat in vec3 vCol; out vec4 o; void main(){ o = vec4(vCol, 1.0); })";
const char* kVs2D = R"(#version 300 es
layout(location=0) in vec2 aPos; layout(location=1) in vec3 aCol; out vec3 vCol;
void main(){ vCol = aCol; gl_Position = vec4(aPos.x / 180.0 - 1.0, 1.0 - aPos.y / 320.0, 0.0, 1.0); })";
const char* kFs2D = R"(#version 300 es
precision mediump float; in vec3 vCol; out vec4 o; void main(){ o = vec4(vCol, 1.0); })";
const char* kVsBlit = R"(#version 300 es
layout(location=0) in vec2 aPos; out vec2 vUv; void main(){ vUv = aPos * 0.5 + 0.5; gl_Position = vec4(aPos, 0.0, 1.0); })";
const char* kFsBlit = R"(#version 300 es
precision mediump float; in vec2 vUv; uniform sampler2D uTex; out vec4 o; void main(){ o = texture(uTex, vUv); })";

// ------------------------------------------------------------------ uygulama durumu
struct Audio {
    AAudioStream* stream = nullptr;
    std::mutex lock;
    std::unique_ptr<ProceduralEngineAudio> synth;
    std::atomic<float> rpm{900}, throttle{0};
    std::atomic<bool> cut{false};
};

struct App {
    android_app* app = nullptr;
    EGLDisplay dpy = EGL_NO_DISPLAY; EGLSurface surf = EGL_NO_SURFACE; EGLContext ctx = EGL_NO_CONTEXT;
    int sw = 0, sh = 0;
    GLuint p3d = 0, p2d = 0, pBlit = 0, fbo = 0, fboTex = 0, fboDepth = 0, vbo3d = 0, vao3d = 0, vbo2d = 0, vao2d = 0,
           vboQuad = 0, vaoQuad = 0;
    GLint uMvp = -1, uModel = -1;
    int meshVerts = 0, meshCarId = -1;
    std::vector<float> ui;   // x, y, r, g, b
    // Oyun durumu
    int carIdx = 0;
    std::unique_ptr<PowertrainCore> pt;
    float throttle = 0.0f; int throttlePointer = -1;
    float spin = 0.0f;
    Audio audio;
    bool running = false;
    std::chrono::steady_clock::time_point last;
    double simAcc = 0.0;
};

// ------------------------------------------------------------------ ses
aaudio_data_callback_result_t audioCb(AAudioStream*, void* user, void* data, int32_t frames) {
    Audio* a = static_cast<Audio*>(user);
    float* out = static_cast<float*>(data);
    if (a->lock.try_lock()) {
        if (a->synth) {
            EngineAudioInput in;
            in.rpm = a->rpm.load(); in.throttle = a->throttle.load(); in.fuelCut = a->cut.load(); in.boost = -1.0;
            a->synth->setInput(in);
            a->synth->render(out, frames);
            for (int i = 0; i < frames; ++i) out[i] *= 0.8f;
        } else std::memset(out, 0, sizeof(float) * frames);
        a->lock.unlock();
    } else std::memset(out, 0, sizeof(float) * frames);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void startAudio(App& s) {
    if (s.audio.stream) return;
    AAudioStreamBuilder* b = nullptr;
    if (AAudio_createStreamBuilder(&b) != AAUDIO_OK) return;
    AAudioStreamBuilder_setFormat(b, AAUDIO_FORMAT_PCM_FLOAT);
    AAudioStreamBuilder_setChannelCount(b, 1);
    AAudioStreamBuilder_setSampleRate(b, 48000);
    AAudioStreamBuilder_setPerformanceMode(b, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setDataCallback(b, audioCb, &s.audio);
    if (AAudioStreamBuilder_openStream(b, &s.audio.stream) == AAUDIO_OK) AAudioStream_requestStart(s.audio.stream);
    else s.audio.stream = nullptr;
    AAudioStreamBuilder_delete(b);
}
void stopAudio(App& s) {
    if (!s.audio.stream) return;
    AAudioStream_requestStop(s.audio.stream);
    AAudioStream_close(s.audio.stream);
    s.audio.stream = nullptr;
}

void selectCar(App& s, int idx) {
    const auto& cat = vehicleCatalog();
    s.carIdx = (idx % (int)cat.size() + (int)cat.size()) % (int)cat.size();
    const VehicleDef& v = cat[s.carIdx];
    s.pt = std::make_unique<PowertrainCore>(buildEngineSpec(v), ClutchSpec{}, DiffSpec{}, buildGearbox(v));
    s.pt->setGear(0);
    s.pt->setClutchPedal(1.0);
    auto synth = std::make_unique<ProceduralEngineAudio>(v, 48000);
    std::lock_guard<std::mutex> g(s.audio.lock);
    s.audio.synth = std::move(synth);
}

// ------------------------------------------------------------------ cizim
void uploadMesh(App& s) {
    const VehicleDef& v = vehicleCatalog()[s.carIdx];
    if (s.meshCarId == v.id) return;
    const LowPolyMesh m = buildVehicleMesh(v);
    static const float matCol[MatCount][3] = {{0, 0, 0}, {0.08f, 0.1f, 0.14f}, {0.05f, 0.05f, 0.05f}, {0.7f, 0.7f, 0.72f},
                                              {1.0f, 0.97f, 0.85f}, {0.75f, 0.05f, 0.05f}, {0.12f, 0.12f, 0.12f}};
    const float paint[3] = {((m.paintRGB >> 16) & 255) / 255.f, ((m.paintRGB >> 8) & 255) / 255.f, (m.paintRGB & 255) / 255.f};
    std::vector<float> buf;
    for (const Tri& t : m.tris) {
        const Vertex* p[3] = {&m.verts[t.a], &m.verts[t.b], &m.verts[t.c]};
        // (x ileri, y sol, z yukari) -> GL (x, z, -y)
        float g[3][3];
        for (int k = 0; k < 3; ++k) { g[k][0] = p[k]->x; g[k][1] = p[k]->z; g[k][2] = -p[k]->y; }
        float ux = g[1][0] - g[0][0], uy = g[1][1] - g[0][1], uz = g[1][2] - g[0][2];
        float vx = g[2][0] - g[0][0], vy = g[2][1] - g[0][1], vz = g[2][2] - g[0][2];
        float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
        const float l = std::sqrt(nx * nx + ny * ny + nz * nz) + 1e-9f; nx /= l; ny /= l; nz /= l;
        if (ny < -0.2f) { nx = -nx; ny = -ny; nz = -nz; }     // cift yuzlu: asagi bakan normali cevir
        const float* c = t.material == MatPaint ? paint : matCol[t.material];
        for (int k = 0; k < 3; ++k) buf.insert(buf.end(), {g[k][0], g[k][1], g[k][2], nx, ny, nz, c[0], c[1], c[2]});
    }
    glBindBuffer(GL_ARRAY_BUFFER, s.vbo3d);
    glBufferData(GL_ARRAY_BUFFER, buf.size() * sizeof(float), buf.data(), GL_STATIC_DRAW);
    s.meshVerts = (int)buf.size() / 9;
    s.meshCarId = v.id;
}

void rect(App& s, float x0, float y0, float x1, float y1, float r, float g, float b) {
    const float q[6][2] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y0}, {x1, y1}, {x0, y1}};
    for (auto& p : q) s.ui.insert(s.ui.end(), {p[0], p[1], r, g, b});
}
void text(App& s, float x, float y, const std::string& str, float scale, float r, float g, float b) {
    for (char ch : str) {
        const char* gl = glyph(ch);
        if (gl) {
            const size_t len = std::strlen(gl);
            for (int row = 0; row < 7; ++row)
                for (int col = 0; col < 5; ++col) {
                    const size_t i = row * 5 + col;
                    if (i < len && gl[i] == '#') rect(s, x + col * scale, y + row * scale, x + (col + 1) * scale, y + (row + 1) * scale, r, g, b);
                }
        }
        x += 6 * scale;
    }
}

struct Button { float x0, y0, x1, y1; int delta; const char* label; };
const Button kButtons[] = {{8, 560, 68, 628, -10, "<<"}, {74, 560, 134, 628, -1, "<"},
                           {140, 560, 200, 628, +1, ">"}, {206, 560, 266, 628, +10, ">>"}};
const float kSlider[4] = {292, 360, 352, 628};

bool initGL(App& s) {
    s.dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(s.dpy, nullptr, nullptr);
    const EGLint attr[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RED_SIZE, 8,
                           EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 16, EGL_NONE};
    EGLConfig cfg; EGLint n = 0;
    if (!eglChooseConfig(s.dpy, attr, &cfg, 1, &n) || n < 1) return false;
    s.surf = eglCreateWindowSurface(s.dpy, cfg, s.app->window, nullptr);
    const EGLint ctxAttr[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    s.ctx = eglCreateContext(s.dpy, cfg, EGL_NO_CONTEXT, ctxAttr);
    if (eglMakeCurrent(s.dpy, s.surf, s.surf, s.ctx) == EGL_FALSE) return false;
    eglQuerySurface(s.dpy, s.surf, EGL_WIDTH, &s.sw);
    eglQuerySurface(s.dpy, s.surf, EGL_HEIGHT, &s.sh);

    s.p3d = program(kVs3D, kFs3D); s.uMvp = glGetUniformLocation(s.p3d, "uMvp"); s.uModel = glGetUniformLocation(s.p3d, "uModel");
    s.p2d = program(kVs2D, kFs2D);
    s.pBlit = program(kVsBlit, kFsBlit);

    // 360x640 sanal tampon (piksel-keskin)
    glGenTextures(1, &s.fboTex);
    glBindTexture(GL_TEXTURE_2D, s.fboTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kVW, kVH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenRenderbuffers(1, &s.fboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, s.fboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, kVW, kVH);
    glGenFramebuffers(1, &s.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, s.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s.fboTex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, s.fboDepth);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glGenVertexArrays(1, &s.vao3d); glGenBuffers(1, &s.vbo3d);
    glBindVertexArray(s.vao3d); glBindBuffer(GL_ARRAY_BUFFER, s.vbo3d);
    for (int a = 0; a < 3; ++a) {
        glEnableVertexAttribArray(a);
        glVertexAttribPointer(a, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(a * 3 * sizeof(float)));
    }
    glGenVertexArrays(1, &s.vao2d); glGenBuffers(1, &s.vbo2d);
    glBindVertexArray(s.vao2d); glBindBuffer(GL_ARRAY_BUFFER, s.vbo2d);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));
    const float quad[] = {-1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1};
    glGenVertexArrays(1, &s.vaoQuad); glGenBuffers(1, &s.vboQuad);
    glBindVertexArray(s.vaoQuad); glBindBuffer(GL_ARRAY_BUFFER, s.vboQuad);
    glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glBindVertexArray(0);
    s.meshCarId = -1;
    return true;
}

void termGL(App& s) {
    if (s.dpy != EGL_NO_DISPLAY) {
        eglMakeCurrent(s.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (s.ctx != EGL_NO_CONTEXT) eglDestroyContext(s.dpy, s.ctx);
        if (s.surf != EGL_NO_SURFACE) eglDestroySurface(s.dpy, s.surf);
        eglTerminate(s.dpy);
    }
    s.dpy = EGL_NO_DISPLAY; s.surf = EGL_NO_SURFACE; s.ctx = EGL_NO_CONTEXT;
}

// Ekran -> sanal koordinat (letterbox)
void viewport(const App& s, int& vx, int& vy, int& vw, int& vh) {
    const float sc = std::min((float)s.sw / kVW, (float)s.sh / kVH);
    vw = (int)(kVW * sc); vh = (int)(kVH * sc); vx = (s.sw - vw) / 2; vy = (s.sh - vh) / 2;
}
void toVirtual(const App& s, float px, float py, float& x, float& y) {
    int vx, vy, vw, vh; viewport(s, vx, vy, vw, vh);
    x = (px - vx) / vw * kVW; y = (py - vy) / vh * kVH;
}

std::string upper(const std::string& in) { std::string o = in; for (char& c : o) c = (char)std::toupper((unsigned char)c); return o; }

void drawFrame(App& s) {
    if (s.dpy == EGL_NO_DISPLAY) return;
    const VehicleDef& v = vehicleCatalog()[s.carIdx];
    const EngineDef& e = engineTable()[v.engine];
    uploadMesh(s);

    glBindFramebuffer(GL_FRAMEBUFFER, s.fbo);
    glViewport(0, 0, kVW, kVH);
    glClearColor(0.07f, 0.08f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // ---- 3D arac (ust bolge) ----
    glEnable(GL_DEPTH_TEST);
    glViewport(0, 640 - 380, kVW, 300);
    const float L = (float)v.lengthM;
    const Mat4 model = rotY(s.spin);
    const Mat4 proj = perspective(0.75f, 360.0f / 300.0f, 0.1f, 50.0f);
    const Mat4 view = lookAt(0.0f, 1.1f + 0.2f * L, 1.35f * L + 1.2f, 0.0f, 0.55f, 0.0f);
    const Mat4 mvp = mul(proj, mul(view, model));
    glUseProgram(s.p3d);
    glUniformMatrix4fv(s.uMvp, 1, GL_FALSE, mvp.m);
    glUniformMatrix4fv(s.uModel, 1, GL_FALSE, model.m);
    glBindVertexArray(s.vao3d);
    glDrawArrays(GL_TRIANGLES, 0, s.meshVerts);
    glDisable(GL_DEPTH_TEST);

    // ---- HUD ----
    glViewport(0, 0, kVW, kVH);
    s.ui.clear();
    rect(s, 0, 0, 360, 58, 0.12f, 0.13f, 0.16f);
    char buf[128];
    std::snprintf(buf, sizeof buf, "#%d  %s", v.id, upper(v.brand).c_str());
    text(s, 8, 8, buf, 2, 0.95f, 0.75f, 0.2f);
    text(s, 8, 30, upper(v.model).substr(0, 29), 2, 1, 1, 1);
    std::snprintf(buf, sizeof buf, "%d %s %s %.0fKG", v.year, bodyName(v.body), driveName(v.drive), v.massKg);
    text(s, 8, 300, upper(buf), 2, 0.8f, 0.8f, 0.85f);
    std::snprintf(buf, sizeof buf, "%s %s %.1fL", e.code, layoutName(e.layout), e.displacementL);
    text(s, 8, 320, upper(buf), 2, 0.8f, 0.8f, 0.85f);
    std::snprintf(buf, sizeof buf, "%s %.0fHP %.0fNM", inductionName(e.induction), e.powerHp, e.torqueNm);
    text(s, 8, 340, upper(buf), 2, 0.8f, 0.8f, 0.85f);
    if (!v.streetLegal) text(s, 8, 280, "YARIS ARACI - ROMORK", 2, 1.0f, 0.3f, 0.3f);

    // Devir gostergesi
    const float rpm = (float)s.pt->rpm(), red = (float)e.redline;
    rect(s, 8, 380, 280, 420, 0.15f, 0.15f, 0.18f);
    const float fill = std::clamp(rpm / (red * 1.08f), 0.0f, 1.0f);
    const bool hot = rpm > red * 0.9f;
    rect(s, 8, 380, 8 + 272 * fill, 420, hot ? 0.95f : 0.2f, hot ? 0.2f : 0.85f, 0.3f);
    rect(s, 8 + 272 * (1 / 1.08f), 376, 10 + 272 * (1 / 1.08f), 424, 1, 0.2f, 0.2f);   // redline cizgisi
    std::snprintf(buf, sizeof buf, "%5.0f RPM", rpm);
    text(s, 8, 430, buf, 3, 1, 1, 1);
    if (s.pt->vtecActive()) text(s, 190, 430, "VTEC", 3, 1.0f, 0.2f, 0.2f);
    if (s.pt->limiterHit()) text(s, 8, 460, "KESICI!", 2, 1.0f, 0.5f, 0.1f);
    std::snprintf(buf, sizeof buf, "YAG %.1f BAR", s.pt->oilPressureBar());
    text(s, 8, 480, buf, 2, 0.7f, 0.8f, 0.7f);
    std::snprintf(buf, sizeof buf, "YAKIT %.1f G", s.pt->fuelGrams());
    text(s, 8, 500, buf, 2, 0.7f, 0.8f, 0.7f);
    text(s, 8, 530, "ARAC SEC", 2, 0.6f, 0.6f, 0.65f);

    for (const Button& b : kButtons) {
        rect(s, b.x0, b.y0, b.x1, b.y1, 0.2f, 0.22f, 0.28f);
        text(s, (b.x0 + b.x1) / 2 - std::strlen(b.label) * 9 + 1.5f, b.y0 + 24, b.label, 3, 1, 1, 1);
    }
    // Gaz kizagi
    rect(s, kSlider[0], kSlider[1], kSlider[2], kSlider[3], 0.18f, 0.18f, 0.22f);
    const float ty = kSlider[3] - (kSlider[3] - kSlider[1]) * s.throttle;
    rect(s, kSlider[0], ty, kSlider[2], kSlider[3], 0.9f, 0.55f, 0.1f);
    text(s, kSlider[0] + 6, kSlider[1] - 18, "GAZ", 2, 1, 1, 1);

    glUseProgram(s.p2d);
    glBindVertexArray(s.vao2d);
    glBindBuffer(GL_ARRAY_BUFFER, s.vbo2d);
    glBufferData(GL_ARRAY_BUFFER, s.ui.size() * sizeof(float), s.ui.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(s.ui.size() / 5));

    // ---- en yakin komsu ile ekrana ----
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, s.sw, s.sh);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    int vx, vy, vw, vh; viewport(s, vx, vy, vw, vh);
    glViewport(vx, vy, vw, vh);
    glUseProgram(s.pBlit);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s.fboTex);
    glBindVertexArray(s.vaoQuad);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    eglSwapBuffers(s.dpy, s.surf);
}

// ------------------------------------------------------------------ girdi
int32_t onInput(android_app* app, AInputEvent* ev) {
    App& s = *static_cast<App*>(app->userData);
    if (AInputEvent_getType(ev) != AINPUT_EVENT_TYPE_MOTION) return 0;
    const int32_t action = AMotionEvent_getAction(ev);
    const int32_t act = action & AMOTION_EVENT_ACTION_MASK;
    const size_t idx = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    auto sliderValue = [&](float y) { return std::clamp((kSlider[3] - y) / (kSlider[3] - kSlider[1]), 0.0f, 1.0f); };

    if (act == AMOTION_EVENT_ACTION_DOWN || act == AMOTION_EVENT_ACTION_POINTER_DOWN) {
        float x, y; toVirtual(s, AMotionEvent_getX(ev, idx), AMotionEvent_getY(ev, idx), x, y);
        if (x >= kSlider[0] - 10 && y >= kSlider[1] - 30) {
            s.throttlePointer = AMotionEvent_getPointerId(ev, idx);
            s.throttle = sliderValue(y);
        } else {
            for (const Button& b : kButtons)
                if (x >= b.x0 && x <= b.x1 && y >= b.y0 && y <= b.y1) selectCar(s, s.carIdx + b.delta);
        }
    } else if (act == AMOTION_EVENT_ACTION_MOVE) {
        for (size_t i = 0; i < AMotionEvent_getPointerCount(ev); ++i) {
            if (AMotionEvent_getPointerId(ev, i) != s.throttlePointer) continue;
            float x, y; toVirtual(s, AMotionEvent_getX(ev, i), AMotionEvent_getY(ev, i), x, y);
            s.throttle = sliderValue(y);
        }
    } else if (act == AMOTION_EVENT_ACTION_UP || act == AMOTION_EVENT_ACTION_CANCEL ||
               (act == AMOTION_EVENT_ACTION_POINTER_UP && AMotionEvent_getPointerId(ev, idx) == s.throttlePointer)) {
        if (act != AMOTION_EVENT_ACTION_POINTER_UP || AMotionEvent_getPointerId(ev, idx) == s.throttlePointer) {
            s.throttlePointer = -1;
            s.throttle = 0.0f;   // yay geri getirir
        }
    }
    return 1;
}

void onCmd(android_app* app, int32_t cmd) {
    App& s = *static_cast<App*>(app->userData);
    switch (cmd) {
    case APP_CMD_INIT_WINDOW:
        if (app->window && initGL(s)) { s.running = true; startAudio(s); }
        break;
    case APP_CMD_TERM_WINDOW:
        s.running = false; stopAudio(s); termGL(s);
        break;
    case APP_CMD_PAUSE: stopAudio(s); break;
    case APP_CMD_RESUME: if (s.dpy != EGL_NO_DISPLAY) startAudio(s); break;
    case APP_CMD_CONFIG_CHANGED:
    case APP_CMD_WINDOW_RESIZED:
        if (s.dpy != EGL_NO_DISPLAY) { eglQuerySurface(s.dpy, s.surf, EGL_WIDTH, &s.sw); eglQuerySurface(s.dpy, s.surf, EGL_HEIGHT, &s.sh); }
        break;
    default: break;
    }
}

} // namespace

void android_main(android_app* app) {
    App s;
    s.app = app;
    app->userData = &s;
    app->onAppCmd = onCmd;
    app->onInputEvent = onInput;
    selectCar(s, 4);   // acilista: Civita Mk6 Tip-R (VTEC)
    s.last = std::chrono::steady_clock::now();

    while (true) {
        int events = 0;
        android_poll_source* src = nullptr;
        // Calisirken beklemeden, arka planda uyuyarak olaylari isle
        while (ALooper_pollOnce(s.running ? 0 : -1, nullptr, &events, (void**)&src) >= 0) {
            if (src) src->process(app, src);
            if (app->destroyRequested) { stopAudio(s); termGL(s); return; }
        }
        const auto now = std::chrono::steady_clock::now();
        double frame = std::chrono::duration<double>(now - s.last).count();
        s.last = now;
        frame = std::min(frame, 0.1);
        if (!s.running) continue;

        // Motor: bosta (vites N), 1 ms sabit adim
        s.pt->setThrottle(s.throttle);
        s.simAcc += frame;
        while (s.simAcc >= 0.001) { s.pt->step(0.001, 0.0, 0.0, 1.0); s.simAcc -= 0.001; }
        s.audio.rpm = (float)s.pt->rpm();
        s.audio.throttle = s.throttle;
        s.audio.cut = s.pt->limiterHit();
        s.pt->drainEvents();
        s.spin += (float)frame * 0.6f;
        drawFrame(s);
    }
}
