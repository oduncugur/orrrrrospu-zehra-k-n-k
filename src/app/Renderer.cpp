#include "Renderer.h"
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
    case '$': return "..#...#####.#...###...#.#####...#..";
    case ',': return ".....................##....#...#...";
    default:  return nullptr;
    }
}


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

const char* kVs3D = R"(layout(location=0) in vec3 aPos; layout(location=1) in vec3 aNrm; layout(location=2) in vec3 aCol;
uniform mat4 uMvp; uniform mat4 uModel; flat out vec3 vCol;
void main(){ vec3 n = normalize(mat3(uModel) * aNrm); float l = 0.40 + 0.60 * max(dot(n, normalize(vec3(0.35,0.9,0.55))), 0.0);
  vCol = aCol * l; gl_Position = uMvp * vec4(aPos, 1.0); })";
const char* kFs3D = R"(precision mediump float; flat in vec3 vCol; out vec4 o; void main(){ o = vec4(vCol, 1.0); })";
const char* kVs2D = R"(layout(location=0) in vec2 aPos; layout(location=1) in vec4 aCol; uniform vec2 uSize; out vec4 vCol;
void main(){ vCol = aCol; gl_Position = vec4(aPos.x / uSize.x * 2.0 - 1.0, 1.0 - aPos.y / uSize.y * 2.0, 0.0, 1.0); })";
const char* kFs2D = R"(precision mediump float; in vec4 vCol; out vec4 o; void main(){ o = vCol; })";
const char* kVsBlit = R"(layout(location=0) in vec2 aPos; uniform vec2 uUv; out vec2 vUv;
void main(){ vUv = (aPos * 0.5 + 0.5) * uUv; gl_Position = vec4(aPos, 0.0, 1.0); })";
const char* kFsBlit = R"(precision mediump float; in vec2 vUv; uniform sampler2D uTex; out vec4 o; void main(){ o = texture(uTex, vUv); })";

} // namespace

// ------------------------------------------------------------------ matris yardimcilari
Mat4 matMul(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int c = 0; c < 4; ++c)
        for (int rr = 0; rr < 4; ++rr) {
            float s = 0; for (int k = 0; k < 4; ++k) s += a.m[k * 4 + rr] * b.m[c * 4 + k];
            r.m[c * 4 + rr] = s;
        }
    return r;
}
Mat4 matPerspective(float fovy, float aspect, float n, float f) {
    const float t = 1.0f / std::tan(fovy * 0.5f);
    Mat4 r{}; r.m[0] = t / aspect; r.m[5] = t; r.m[10] = (f + n) / (n - f); r.m[11] = -1; r.m[14] = 2 * f * n / (n - f);
    return r;
}
Mat4 matOrtho(float l, float r, float b, float t, float n, float f) {
    Mat4 m{}; m.m[0] = 2 / (r - l); m.m[5] = 2 / (t - b); m.m[10] = -2 / (f - n);
    m.m[12] = -(r + l) / (r - l); m.m[13] = -(t + b) / (t - b); m.m[14] = -(f + n) / (f - n); m.m[15] = 1;
    return m;
}
Mat4 matLookAt(float ex, float ey, float ez, float cx, float cy, float cz) {
    float fx = cx - ex, fy = cy - ey, fz = cz - ez; float l = std::sqrt(fx * fx + fy * fy + fz * fz); fx /= l; fy /= l; fz /= l;
    float sx = -fz, sy = 0.0f, sz = fx; l = std::sqrt(sx * sx + sy * sy + sz * sz); sx /= l; sy /= l; sz /= l;
    const float ux = sy * fz - sz * fy, uy = sz * fx - sx * fz, uz = sx * fy - sy * fx;
    Mat4 r{};
    r.m[0] = sx; r.m[4] = sy; r.m[8] = sz;
    r.m[1] = ux; r.m[5] = uy; r.m[9] = uz;
    r.m[2] = -fx; r.m[6] = -fy; r.m[10] = -fz;
    r.m[12] = -(sx * ex + sy * ey + sz * ez); r.m[13] = -(ux * ex + uy * ey + uz * ez); r.m[14] = fx * ex + fy * ey + fz * ez;
    r.m[15] = 1;
    return r;
}
Mat4 matRotY(float a) { Mat4 r{}; const float c = std::cos(a), s = std::sin(a); r.m[0] = c; r.m[2] = -s; r.m[5] = 1; r.m[8] = s; r.m[10] = c; r.m[15] = 1; return r; }
Mat4 matRotZ(float a) { Mat4 r{}; const float c = std::cos(a), s = std::sin(a); r.m[0] = c; r.m[1] = s; r.m[4] = -s; r.m[5] = c; r.m[10] = 1; r.m[15] = 1; return r; }
Mat4 matTranslate(float x, float y, float z) { Mat4 r{}; r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1; r.m[12] = x; r.m[13] = y; r.m[14] = z; return r; }
Mat4 matScale(float s) { Mat4 r{}; r.m[0] = r.m[5] = r.m[10] = s; r.m[15] = 1; return r; }

// ------------------------------------------------------------------ Renderer
bool Renderer::init() {
    p3d_ = program(kVs3D, kFs3D); uMvp_ = glGetUniformLocation(p3d_, "uMvp"); uModel_ = glGetUniformLocation(p3d_, "uModel");
    p2d_ = program(kVs2D, kFs2D); uSize_ = glGetUniformLocation(p2d_, "uSize");
    pBlit_ = program(kVsBlit, kFsBlit); uUv_ = glGetUniformLocation(pBlit_, "uUv");

    GLuint t, r, f;
    glGenTextures(1, &t); tex_ = t;
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kFbo, kFbo, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenRenderbuffers(1, &r); depth_ = r;
    glBindRenderbuffer(GL_RENDERBUFFER, r);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, kFbo, kFbo);
    glGenFramebuffers(1, &f); fbo_ = f;
    glBindFramebuffer(GL_FRAMEBUFFER, f);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, r);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    GLuint a, b;
    glGenVertexArrays(1, &a); glGenBuffers(1, &b); vao2d_ = a; vbo2d_ = b;
    glBindVertexArray(a); glBindBuffer(GL_ARRAY_BUFFER, b);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
    const float quad[] = {-1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1};
    glGenVertexArrays(1, &a); glGenBuffers(1, &b); vaoQ_ = a; vboQ_ = b;
    glBindVertexArray(a); glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glBindVertexArray(0);
    meshes_.clear();
    ready_ = true;
    return true;
}

void Renderer::shutdown() { ready_ = false; meshes_.clear(); batch_.clear(); }

void Renderer::begin(int vw, int vh, Color c) {
    vw_ = std::min(vw, kFbo); vh_ = std::min(vh, kFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, vw_, vh_);
    glClearColor(c.r, c.g, c.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    batch_.clear();
}

void Renderer::present(int sw, int sh) {
    flush2D();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, sw, sh);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    const float sc = std::min((float)sw / vw_, (float)sh / vh_);
    const int w = (int)(vw_ * sc), h = (int)(vh_ * sc);
    glViewport((sw - w) / 2, (sh - h) / 2, w, h);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glUseProgram(pBlit_);
    glUniform2f(uUv_, (float)vw_ / kFbo, (float)vh_ / kFbo);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_);
    glBindVertexArray(vaoQ_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void Renderer::toVirtual(int sw, int sh, float px, float py, float& x, float& y) const {
    const float sc = std::min((float)sw / vw_, (float)sh / vh_);
    const float w = vw_ * sc, h = vh_ * sc;
    x = (px - (sw - w) * 0.5f) / sc; y = (py - (sh - h) * 0.5f) / sc;
}

void Renderer::tri(float ax, float ay, float bx, float by, float cx, float cy, Color c) {
    batch_.insert(batch_.end(), {ax, ay, c.r, c.g, c.b, c.a, bx, by, c.r, c.g, c.b, c.a, cx, cy, c.r, c.g, c.b, c.a});
}
void Renderer::rect(float x0, float y0, float x1, float y1, Color c) {
    tri(x0, y0, x1, y0, x1, y1, c); tri(x0, y0, x1, y1, x0, y1, c);
}
void Renderer::gradientV(float x0, float y0, float x1, float y1, Color t, Color b) {
    batch_.insert(batch_.end(), {x0, y0, t.r, t.g, t.b, t.a, x1, y0, t.r, t.g, t.b, t.a, x1, y1, b.r, b.g, b.b, b.a,
                                 x0, y0, t.r, t.g, t.b, t.a, x1, y1, b.r, b.g, b.b, b.a, x0, y1, b.r, b.g, b.b, b.a});
}
void Renderer::circle(float cx, float cy, float r, int seg, Color c) {
    for (int i = 0; i < seg; ++i) {
        const float a0 = 6.2831853f * i / seg, a1 = 6.2831853f * (i + 1) / seg;
        tri(cx, cy, cx + r * std::cos(a0), cy + r * std::sin(a0), cx + r * std::cos(a1), cy + r * std::sin(a1), c);
    }
}
void Renderer::text(float x, float y, const std::string& s, float sc, Color c) {
    for (char ch : s) {
        if (const char* g = glyph(ch)) {
            for (int row = 0; row < 7; ++row)
                for (int col = 0; col < 5; ++col)
                    if (g[row * 5 + col] == '#') rect(x + col * sc, y + row * sc, x + (col + 1) * sc, y + (row + 1) * sc, c);
        }
        x += 6 * sc;
    }
}

void Renderer::flush2D() {
    if (batch_.empty()) return;
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glViewport(0, 0, vw_, vh_);
    glUseProgram(p2d_);
    glUniform2f(uSize_, (float)vw_, (float)vh_);
    glBindVertexArray(vao2d_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo2d_);
    glBufferData(GL_ARRAY_BUFFER, batch_.size() * sizeof(float), batch_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(batch_.size() / 6));
    glDisable(GL_BLEND);
    batch_.clear();
}

const Renderer::Mesh& Renderer::mesh(int carId) {
    auto it = meshes_.find(carId);
    if (it != meshes_.end()) return it->second;
    Mesh& M = meshes_[carId];
    const VehicleDef* v = findVehicle(carId);
    if (!v) return M;
    const LowPolyMesh m = buildVehicleMesh(*v);
    std::vector<float> buf;
    for (const Tri& t : m.tris) {
        const Vertex* p[3] = {&m.verts[t.a], &m.verts[t.b], &m.verts[t.c]};
        float g[3][3];   // (x ileri, y sol, z yukari) -> GL (x, z, -y)
        for (int k = 0; k < 3; ++k) { g[k][0] = p[k]->x; g[k][1] = p[k]->z; g[k][2] = -p[k]->y; }
        const float ux = g[1][0] - g[0][0], uy = g[1][1] - g[0][1], uz = g[1][2] - g[0][2];
        const float vx = g[2][0] - g[0][0], vy = g[2][1] - g[0][1], vz = g[2][2] - g[0][2];
        float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
        const float l = std::sqrt(nx * nx + ny * ny + nz * nz) + 1e-9f; nx /= l; ny /= l; nz /= l;
        if (ny < -0.2f) { nx = -nx; ny = -ny; nz = -nz; }     // cift yuzlu: asagi bakan normali cevir
        float c[3]; materialColor(t.material, m.paintRGB, c);
        for (int k = 0; k < 3; ++k) buf.insert(buf.end(), {g[k][0], g[k][1], g[k][2], nx, ny, nz, c[0], c[1], c[2]});
    }
    GLuint a, b;
    glGenVertexArrays(1, &a); glGenBuffers(1, &b); M.vao = a; M.vbo = b;
    glBindVertexArray(a); glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferData(GL_ARRAY_BUFFER, buf.size() * sizeof(float), buf.data(), GL_STATIC_DRAW);
    for (int i = 0; i < 3; ++i) {
        glEnableVertexAttribArray(i);
        glVertexAttribPointer(i, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(i * 3 * sizeof(float)));
    }
    glBindVertexArray(0);
    M.count = (int)buf.size() / 9;
    return M;
}

void Renderer::drawCar(int carId, float x, float y, float w, float h, const Mat4& proj, const Mat4& view, const Mat4& model) {
    flush2D();
    const Mesh& M = mesh(carId);
    if (!M.count) return;
    glEnable(GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);
    glViewport((int)x, vh_ - (int)(y + h), (int)w, (int)h);
    const Mat4 mvp = matMul(proj, matMul(view, model));
    glUseProgram(p3d_);
    glUniformMatrix4fv(uMvp_, 1, GL_FALSE, mvp.m);
    glUniformMatrix4fv(uModel_, 1, GL_FALSE, model.m);
    glBindVertexArray(M.vao);
    glDrawArrays(GL_TRIANGLES, 0, M.count);
    glDisable(GL_DEPTH_TEST);
    glViewport(0, 0, vw_, vh_);
}

} // namespace zk
