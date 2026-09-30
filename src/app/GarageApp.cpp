#include "GarageApp.h"
#include "GLApi.h"
#include "garage/LowPolyModel.h"
#include "garage/VehicleCatalog.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace zk {

namespace {

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
GLuint compile(GLenum type, const char* body) {
    GLuint s = glCreateShader(type);
    const char* parts[2] = {ZK_GLSL_VERSION, body};
    glShaderSource(s, 2, parts, nullptr);
    glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[512]; glGetShaderInfoLog(s, 512, nullptr, log); std::fprintf(stderr, "shader: %s\n", log); }
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

const char* kVs3D = R"(layout(location=0) in vec3 aPos; layout(location=1) in vec3 aNrm; layout(location=2) in vec3 aCol;
uniform mat4 uMvp; uniform mat4 uModel; flat out vec3 vCol;
void main(){ vec3 n = normalize(mat3(uModel) * aNrm); float l = 0.38 + 0.62 * max(dot(n, normalize(vec3(0.4,0.9,0.5))), 0.0);
  vCol = aCol * l; gl_Position = uMvp * vec4(aPos, 1.0); })";
const char* kFs3D = R"(precision mediump float; flat in vec3 vCol; out vec4 o; void main(){ o = vec4(vCol, 1.0); })";
const char* kVs2D = R"(layout(location=0) in vec2 aPos; layout(location=1) in vec3 aCol; out vec3 vCol;
void main(){ vCol = aCol; gl_Position = vec4(aPos.x / 180.0 - 1.0, 1.0 - aPos.y / 320.0, 0.0, 1.0); })";
const char* kFs2D = R"(precision mediump float; in vec3 vCol; out vec4 o; void main(){ o = vec4(vCol, 1.0); })";
const char* kVsBlit = R"(layout(location=0) in vec2 aPos; out vec2 vUv; void main(){ vUv = aPos * 0.5 + 0.5; gl_Position = vec4(aPos, 0.0, 1.0); })";
const char* kFsBlit = R"(precision mediump float; in vec2 vUv; uniform sampler2D uTex; out vec4 o; void main(){ o = texture(uTex, vUv); })";


struct Button { float x0, y0, x1, y1; int delta; const char* label; };
const Button kButtons[] = {{8, 560, 68, 628, -10, "<<"}, {74, 560, 134, 628, -1, "<"},
                           {140, 560, 200, 628, +1, ">"}, {206, 560, 266, 628, +10, ">>"}};
const float kSlider[4] = {292, 360, 352, 628};

std::string upper(const std::string& in) { std::string o = in; for (char& c : o) c = (char)std::toupper((unsigned char)c); return o; }

} // namespace

GarageApp::GarageApp() { selectCar(4); }   // acilista: Civita Mk6 Tip-R (VTEC)

void GarageApp::selectCar(int idx) {
    const auto& cat = vehicleCatalog();
    carIdx_ = (idx % (int)cat.size() + (int)cat.size()) % (int)cat.size();
    const VehicleDef& v = cat[carIdx_];
    pt_ = std::make_unique<PowertrainCore>(buildEngineSpec(v), ClutchSpec{}, DiffSpec{}, buildGearbox(v));
    pt_->setGear(0);
    pt_->setClutchPedal(1.0);
    auto synth = std::make_unique<ProceduralEngineAudio>(v, kSampleRate);
    std::lock_guard<std::mutex> g(audioLock_);
    synth_ = std::move(synth);
}

void GarageApp::renderAudio(float* out, int frames) {
    if (audioLock_.try_lock()) {
        if (synth_) {
            EngineAudioInput in;
            in.rpm = aRpm_.load(); in.throttle = aThr_.load(); in.fuelCut = aCut_.load(); in.boost = -1.0;
            synth_->setInput(in);
            synth_->render(out, frames);
            for (int i = 0; i < frames; ++i) out[i] *= 0.8f;
        } else std::memset(out, 0, sizeof(float) * frames);
        audioLock_.unlock();
    } else std::memset(out, 0, sizeof(float) * frames);
}

void GarageApp::update(double dt) {
    dt = std::min(dt, 0.1);
    if (throttleKey_) throttle_ = std::min(1.0f, throttle_ + (float)dt / 0.12f);        // klavye: analog rampa
    else if (throttlePointer_ < 0) throttle_ = std::max(0.0f, throttle_ - (float)dt / 0.08f);
    pt_->setThrottle(throttle_);
    simAcc_ += dt;
    while (simAcc_ >= 0.001) { pt_->step(0.001, 0.0, 0.0, 1.0); simAcc_ -= 0.001; }   // bosta, 1 ms adim
    pt_->drainEvents();
    aRpm_ = (float)pt_->rpm();
    aThr_ = throttle_;
    aCut_ = pt_->limiterHit();
    spin_ += (float)dt * 0.6f;
}

bool GarageApp::initGraphics() {
    p3d_ = program(kVs3D, kFs3D); uMvp_ = glGetUniformLocation(p3d_, "uMvp"); uModel_ = glGetUniformLocation(p3d_, "uModel");
    p2d_ = program(kVs2D, kFs2D);
    pBlit_ = program(kVsBlit, kFsBlit);

    GLuint t, r, f;
    glGenTextures(1, &t); fboTex_ = t;
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kVW, kVH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenRenderbuffers(1, &r); fboDepth_ = r;
    glBindRenderbuffer(GL_RENDERBUFFER, r);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, kVW, kVH);
    glGenFramebuffers(1, &f); fbo_ = f;
    glBindFramebuffer(GL_FRAMEBUFFER, f);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, r);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    GLuint a, b;
    glGenVertexArrays(1, &a); glGenBuffers(1, &b); vao3d_ = a; vbo3d_ = b;
    glBindVertexArray(a); glBindBuffer(GL_ARRAY_BUFFER, b);
    for (int i = 0; i < 3; ++i) {
        glEnableVertexAttribArray(i);
        glVertexAttribPointer(i, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(i * 3 * sizeof(float)));
    }
    glGenVertexArrays(1, &a); glGenBuffers(1, &b); vao2d_ = a; vbo2d_ = b;
    glBindVertexArray(a); glBindBuffer(GL_ARRAY_BUFFER, b);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));
    const float quad[] = {-1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1};
    glGenVertexArrays(1, &a); glGenBuffers(1, &b); vaoQuad_ = a; vboQuad_ = b;
    glBindVertexArray(a); glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glBindVertexArray(0);
    meshCarId_ = -1;
    gl_ = true;
    return true;
}

void GarageApp::shutdownGraphics() { gl_ = false; meshCarId_ = -1; }
void GarageApp::resize(int w, int h) { sw_ = std::max(1, w); sh_ = std::max(1, h); }

void GarageApp::viewport(int& vx, int& vy, int& vw, int& vh) const {
    const float sc = std::min((float)sw_ / kVW, (float)sh_ / kVH);
    vw = (int)(kVW * sc); vh = (int)(kVH * sc); vx = (sw_ - vw) / 2; vy = (sh_ - vh) / 2;
}
void GarageApp::toVirtual(float px, float py, float& x, float& y) const {
    int vx, vy, vw, vh; viewport(vx, vy, vw, vh);
    x = (px - vx) / vw * kVW; y = (py - vy) / vh * kVH;
}

void GarageApp::pointerDown(int id, float px, float py) {
    float x, y; toVirtual(px, py, x, y);
    if (x >= kSlider[0] - 10 && y >= kSlider[1] - 30) {
        throttlePointer_ = id;
        throttle_ = std::clamp((kSlider[3] - y) / (kSlider[3] - kSlider[1]), 0.0f, 1.0f);
        return;
    }
    for (const Button& b : kButtons)
        if (x >= b.x0 && x <= b.x1 && y >= b.y0 && y <= b.y1) selectCar(carIdx_ + b.delta);
}
void GarageApp::pointerMove(int id, float px, float py) {
    if (id != throttlePointer_) return;
    float x, y; toVirtual(px, py, x, y);
    throttle_ = std::clamp((kSlider[3] - y) / (kSlider[3] - kSlider[1]), 0.0f, 1.0f);
}
void GarageApp::pointerUp(int id) {
    if (id == throttlePointer_) { throttlePointer_ = -1; }   // yay geri getirir (update'te sonumlenir)
}

bool GarageApp::readPixelsRGB(std::vector<unsigned char>& rgb, int& w, int& h) {
    w = sw_; h = sh_;
    std::vector<unsigned char> rgba((size_t)w * h * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    rgb.resize((size_t)w * h * 3);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            for (int c = 0; c < 3; ++c) rgb[((size_t)y * w + x) * 3 + c] = rgba[((size_t)(h - 1 - y) * w + x) * 4 + c];
    return true;
}

void GarageApp::uploadMesh() {
    const VehicleDef& v = vehicleCatalog()[carIdx_];
    if (meshCarId_ == v.id) return;
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
    glBindBuffer(GL_ARRAY_BUFFER, vbo3d_);
    glBufferData(GL_ARRAY_BUFFER, buf.size() * sizeof(float), buf.data(), GL_STATIC_DRAW);
    meshVerts_ = (int)buf.size() / 9;
    meshCarId_ = v.id;
}

void GarageApp::rect(float x0, float y0, float x1, float y1, float r, float g, float b) {
    const float q[6][2] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y0}, {x1, y1}, {x0, y1}};
    for (auto& p : q) ui_.insert(ui_.end(), {p[0], p[1], r, g, b});
}
void GarageApp::text(float x, float y, const std::string& str, float scale, float r, float g, float b) {
    for (char ch : str) {
        const char* gl = glyph(ch);
        if (gl) {
            const size_t len = std::strlen(gl);
            for (int row = 0; row < 7; ++row)
                for (int col = 0; col < 5; ++col) {
                    const size_t i = row * 5 + col;
                    if (i < len && gl[i] == '#') rect(x + col * scale, y + row * scale, x + (col + 1) * scale, y + (row + 1) * scale, r, g, b);
                }
        }
        x += 6 * scale;
    }
}

void GarageApp::render() {
    if (!gl_) return;
    const VehicleDef& v = vehicleCatalog()[carIdx_];
    const EngineDef& e = engineTable()[v.engine];
    uploadMesh();

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, kVW, kVH);
    glClearColor(0.07f, 0.08f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // ---- 3D arac (ust bolge) ----
    glEnable(GL_DEPTH_TEST);
    glViewport(0, 640 - 380, kVW, 300);
    const float L = (float)v.lengthM;
    const Mat4 model = rotY(spin_);
    const Mat4 proj = perspective(0.75f, 360.0f / 300.0f, 0.1f, 50.0f);
    const Mat4 view = lookAt(0.0f, 1.1f + 0.2f * L, 1.35f * L + 1.2f, 0.0f, 0.55f, 0.0f);
    const Mat4 mvp = mul(proj, mul(view, model));
    glUseProgram(p3d_);
    glUniformMatrix4fv(uMvp_, 1, GL_FALSE, mvp.m);
    glUniformMatrix4fv(uModel_, 1, GL_FALSE, model.m);
    glBindVertexArray(vao3d_);
    glDrawArrays(GL_TRIANGLES, 0, meshVerts_);
    glDisable(GL_DEPTH_TEST);

    // ---- HUD ----
    glViewport(0, 0, kVW, kVH);
    ui_.clear();
    rect(0, 0, 360, 58, 0.12f, 0.13f, 0.16f);
    char buf[128];
    std::snprintf(buf, sizeof buf, "#%d  %s", v.id, upper(v.brand).c_str());
    text(8, 8, buf, 2, 0.95f, 0.75f, 0.2f);
    text(8, 30, upper(v.model).substr(0, 29), 2, 1, 1, 1);
    std::snprintf(buf, sizeof buf, "%d %s %s %.0fKG", v.year, bodyName(v.body), driveName(v.drive), v.massKg);
    text(8, 300, upper(buf), 2, 0.8f, 0.8f, 0.85f);
    std::snprintf(buf, sizeof buf, "%s %s %.1fL", e.code, layoutName(e.layout), e.displacementL);
    text(8, 320, upper(buf), 2, 0.8f, 0.8f, 0.85f);
    std::snprintf(buf, sizeof buf, "%s %.0fHP %.0fNM", inductionName(e.induction), e.powerHp, e.torqueNm);
    text(8, 340, upper(buf), 2, 0.8f, 0.8f, 0.85f);
    if (!v.streetLegal) text(8, 280, "YARIS ARACI - ROMORK", 2, 1.0f, 0.3f, 0.3f);

    // Devir gostergesi
    const float rpm = (float)pt_->rpm(), red = (float)e.redline;
    rect(8, 380, 280, 420, 0.15f, 0.15f, 0.18f);
    const float fill = std::clamp(rpm / (red * 1.08f), 0.0f, 1.0f);
    const bool hot = rpm > red * 0.9f;
    rect(8, 380, 8 + 272 * fill, 420, hot ? 0.95f : 0.2f, hot ? 0.2f : 0.85f, 0.3f);
    rect(8 + 272 * (1 / 1.08f), 376, 10 + 272 * (1 / 1.08f), 424, 1, 0.2f, 0.2f);   // redline cizgisi
    std::snprintf(buf, sizeof buf, "%5.0f RPM", rpm);
    text(8, 430, buf, 3, 1, 1, 1);
    if (pt_->vtecActive()) text(190, 430, "VTEC", 3, 1.0f, 0.2f, 0.2f);
    if (pt_->limiterHit()) text(8, 460, "KESICI!", 2, 1.0f, 0.5f, 0.1f);
    std::snprintf(buf, sizeof buf, "YAG %.1f BAR", pt_->oilPressureBar());
    text(8, 480, buf, 2, 0.7f, 0.8f, 0.7f);
    std::snprintf(buf, sizeof buf, "YAKIT %.1f G", pt_->fuelGrams());
    text(8, 500, buf, 2, 0.7f, 0.8f, 0.7f);
    text(8, 530, "ARAC SEC", 2, 0.6f, 0.6f, 0.65f);

    for (const Button& b : kButtons) {
        rect(b.x0, b.y0, b.x1, b.y1, 0.2f, 0.22f, 0.28f);
        text((b.x0 + b.x1) / 2 - std::strlen(b.label) * 9 + 1.5f, b.y0 + 24, b.label, 3, 1, 1, 1);
    }
    // Gaz kizagi
    rect(kSlider[0], kSlider[1], kSlider[2], kSlider[3], 0.18f, 0.18f, 0.22f);
    const float ty = kSlider[3] - (kSlider[3] - kSlider[1]) * throttle_;
    rect(kSlider[0], ty, kSlider[2], kSlider[3], 0.9f, 0.55f, 0.1f);
    text(kSlider[0] + 6, kSlider[1] - 18, "GAZ", 2, 1, 1, 1);

    glUseProgram(p2d_);
    glBindVertexArray(vao2d_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo2d_);
    glBufferData(GL_ARRAY_BUFFER, ui_.size() * sizeof(float), ui_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(ui_.size() / 5));

    // ---- en yakin komsu ile ekrana ----
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, sw_, sh_);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    int vx, vy, vw, vh; viewport(vx, vy, vw, vh);
    glViewport(vx, vy, vw, vh);
    glUseProgram(pBlit_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fboTex_);
    glBindVertexArray(vaoQuad_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    }

} // namespace zk
