#include "Renderer.h"
#include "GLApi.h"
#include "garage/LowPolyModel.h"
#include "garage/VehicleCatalog.h"

#include <algorithm>
#include <array>
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
    case '?': return ".###.#...#....#...#...#.......#....";
    case '\'': return "..#....#...#.......................";
    default:  return nullptr;
    }
}


// Kiril / Yunan buyuk harfler (5x7). Latin benzerleri ASCII glife esler; kucuk harf buyuge cevrilir.
const char* glyphU(uint32_t cp) {
    if (cp < 128) return glyph((char)cp);
    if (cp >= 0x0430 && cp <= 0x044F) cp -= 0x20;                       // kiril kucuk -> buyuk
    else if (cp >= 0x0450 && cp <= 0x045F) cp -= 0x50;
    else if (cp == 0x0491) cp = 0x0490;
    else if (cp == 0x03C2) cp = 0x03A3;                                 // son sigma
    else if (cp >= 0x03B1 && cp <= 0x03C9) cp -= 0x20;                  // yunan kucuk -> buyuk
    switch (cp) {
    case 0x0391: return glyph('A');
    case 0x0392: return glyph('B');
    case 0x0395: return glyph('E');
    case 0x0396: return glyph('Z');
    case 0x0397: return glyph('H');
    case 0x0399: return glyph('I');
    case 0x039A: return glyph('K');
    case 0x039C: return glyph('M');
    case 0x039D: return glyph('N');
    case 0x039F: return glyph('O');
    case 0x03A1: return glyph('P');
    case 0x03A4: return glyph('T');
    case 0x03A5: return glyph('Y');
    case 0x03A7: return glyph('X');
    case 0x0405: return glyph('S');
    case 0x0406: return glyph('I');
    case 0x0408: return glyph('J');
    case 0x0410: return glyph('A');
    case 0x0412: return glyph('B');
    case 0x0415: return glyph('E');
    case 0x041A: return glyph('K');
    case 0x041C: return glyph('M');
    case 0x041D: return glyph('H');
    case 0x041E: return glyph('O');
    case 0x0420: return glyph('P');
    case 0x0421: return glyph('C');
    case 0x0422: return glyph('T');
    case 0x0425: return glyph('X');
    case 0x0393: cp = 0x0413; break;
    case 0x03A0: cp = 0x041F; break;
    case 0x03A6: cp = 0x0424; break;
    default: break;
    }
    switch (cp) {
    case 0x0394: return "..#....#...#.#..#.#.#...##...######";
    case 0x0398: return ".###.#...##...#######...##...#.###.";
    case 0x039B: return "..#....#...#.#..#.#.#...##...##...#";
    case 0x039E: return "#####...........###...........#####";
    case 0x03A3: return "######.....#.....#...#...#....#####";
    case 0x03A8: return "#.#.##.#.##.#.#.###...#....#....#..";
    case 0x03A9: return ".###.#...##...##...#.#.#..#.#.##.##";
    case 0x0401: return ".#.#......######....####.#....#####";
    case 0x0404: return ".###.#...##....###..#....#...#.###.";
    case 0x0407: return ".#.#.......###...#....#....#...###.";
    case 0x0411: return "######....#....####.#...##...#####.";
    case 0x0413: return "######....#....#....#....#....#....";
    case 0x0414: return "..##..#.#..#.#..#.#..#.#.######...#";
    case 0x0416: return "#.#.##.#.#.###...#...###.#.#.##.#.#";
    case 0x0417: return ".###.#...#....#..##.....##...#.###.";
    case 0x0418: return "#...##...##..###.#.###..##...##...#";
    case 0x0419: return "#.#.##...##..###.#.###..##...##...#";
    case 0x041B: return "..###.#..#.#..#.#..#.#..#.#..##...#";
    case 0x041F: return "######...##...##...##...##...##...#";
    case 0x0423: return "#...##...##...#.####....##...#.###.";
    case 0x0424: return "..#...###.#.#.##.#.#.###...#....#..";
    case 0x0426: return "#..#.#..#.#..#.#..#.#..#.#####....#";
    case 0x0427: return "#...##...##...#.####....#....#....#";
    case 0x0428: return "#...##...##.#.##.#.##.#.##.#.######";
    case 0x0429: return "#.#.##.#.##.#.##.#.##.#.######....#";
    case 0x042A: return "##....#....#....###..#..#.#..#.###.";
    case 0x042B: return "#...##...##...####.##.#.##.#.####.#";
    case 0x042C: return "#....#....#....####.#...##...#####.";
    case 0x042D: return ".###.#...#....#..###....##...#.###.";
    case 0x042E: return "#..#.#.#.##.#.####.##.#.##.#.##..#.";
    case 0x042F: return ".#####...##...#.####..#.#.#..##...#";
    case 0x0490: return "....#######....#....#....#....#....";
    default: return nullptr;
    }
}

// UTF-8 cozumleme (gecersiz bayt: tek karakter sayilir)
uint32_t nextCp(const std::string& s, size_t& i) {
    const unsigned char c = (unsigned char)s[i++];
    if (c < 0x80) return c;
    int n = (c >> 5) == 6 ? 1 : (c >> 4) == 14 ? 2 : (c >> 3) == 30 ? 3 : 0;
    uint32_t cp = n == 1 ? (c & 0x1F) : n == 2 ? (c & 0x0F) : (c & 0x07);
    while (n-- > 0 && i < s.size() && ((unsigned char)s[i] & 0xC0) == 0x80) cp = (cp << 6) | ((unsigned char)s[i++] & 0x3F);
    return cp;
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

// 3B arac: piksel basina isik. Gunes (yonlu) + gok/zemin yarikure ortami + Blinn-Phong parlama + gokyuzu yansimasi
// (Fresnel). aGloss: 0 mat (lastik) .. 1 boya/cam/krom; < 0 isik yayan (far, stop). uShadow: zemin golgesi (siyah, alfa).
const char* kVs3D = R"(layout(location=0) in vec3 aPos; layout(location=1) in vec3 aNrm; layout(location=2) in vec3 aCol;
layout(location=3) in float aGloss;
uniform mat4 uMvp; uniform mat4 uModel; out vec3 vN; out vec3 vP; out vec3 vCol; out float vGloss; out vec2 vL; out vec3 vLp;
void main(){ vL = aPos.xz; vLp = aPos; vN = mat3(uModel) * aNrm; vP = (uModel * vec4(aPos, 1.0)).xyz; vCol = aCol; vGloss = aGloss;
  gl_Position = uMvp * vec4(aPos, 1.0); })";
const char* kFs3D = R"(precision mediump float; in vec3 vN; in vec3 vP; in vec3 vCol; in float vGloss; in vec2 vL; in vec3 vLp;
uniform vec3 uEye; uniform float uAlpha; uniform float uShadow; uniform vec2 uLight; out vec4 o;
uniform vec4 uPaint; uniform vec4 uRim; uniform vec4 uStripe; uniform vec2 uPG;
vec3 sky(vec3 r){ float y = r.y;
  vec3 hor = vec3(0.86, 0.80, 0.72), top = vec3(0.32, 0.48, 0.86), gnd = vec3(0.22, 0.24, 0.20);
  return y > 0.0 ? mix(hor, top, pow(y, 0.6)) : mix(hor * 0.7, gnd, pow(-y, 0.4)); }
void main(){
  if (uShadow > 0.5) { float d = length(max(abs(vL) - vec2(0.55), 0.0)) / 0.45;   // yuvarlatilmis dikdortgen, yumusak kenar
    o = vec4(0.0, 0.0, 0.0, uAlpha * (1.0 - smoothstep(0.0, 1.0, d))); return; }
  if (vGloss < 0.0) { o = vec4(vCol, uAlpha); return; }
  vec3 n = normalize(vN); vec3 v = normalize(uEye - vP);
  if (dot(n, v) < 0.0) n = -n;
  vec3 col = vCol; float g = vGloss;
  if (g > 0.995) {                                       // boya (gloss 1.0): renk / serit / cila
    if (uPaint.a > 0.5) col = uPaint.rgb;
    float z = abs(vLp.z);
    bool st = uStripe.a > 2.5 ? (abs(n.y) < 0.6 && abs(vLp.y - uPG.y) < 0.045)
            : uStripe.a > 1.5 ? (n.y > 0.5 && z > 0.07 && z < 0.17)
            : uStripe.a > 0.5 ? (n.y > 0.5 && z < 0.16) : false;
    if (st) col = uStripe.rgb;
    g = uPG.x;
  } else if (abs(g - 0.7) < 0.005 && uRim.a > 0.5) col = uRim.rgb;   // jant
  vec3 l = normalize(vec3(0.35, 0.9, 0.55));
  float dif = max(dot(n, l), 0.0);
  vec3 amb = mix(vec3(0.30, 0.28, 0.25), vec3(0.62, 0.70, 0.85), n.y * 0.5 + 0.5);
  vec3 c = col * (0.42 * amb * uLight.y + 0.72 * dif * uLight.x);
  float fr = 0.04 + 0.96 * pow(1.0 - max(dot(n, v), 0.0), 5.0);
  c = mix(c, sky(reflect(-v, n)) * uLight.y, clamp(g * (0.06 + 0.55 * fr), 0.0, 0.6));
  vec3 h = normalize(l + v);
  c += vec3(1.0, 0.97, 0.9) * pow(max(dot(n, h), 0.0), g > 0.5 ? 70.0 : 12.0) * g * 0.9 * uLight.x;
  o = vec4(min(c, vec3(1.0)), uAlpha); })";
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
    uEye_ = glGetUniformLocation(p3d_, "uEye"); uLight_ = glGetUniformLocation(p3d_, "uLight");
    uPaint_ = glGetUniformLocation(p3d_, "uPaint"); uRim_ = glGetUniformLocation(p3d_, "uRim");
    uStripe_ = glGetUniformLocation(p3d_, "uStripe"); uPG_ = glGetUniformLocation(p3d_, "uPG"); uAlpha_ = glGetUniformLocation(p3d_, "uAlpha"); uShadow_ = glGetUniformLocation(p3d_, "uShadow");
    p2d_ = program(kVs2D, kFs2D); uSize_ = glGetUniformLocation(p2d_, "uSize");
    pBlit_ = program(kVsBlit, kFsBlit); uUv_ = glGetUniformLocation(pBlit_, "uUv");

    texSize_ = 0;
    ensureTarget();

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
    {   // Golge: model uzayinda birim kare (x,z -1..1), zemin hizasi; 3B shader ile (uShadow) cizilir
        const float sh[] = {-1, 0, -1, 0, 1, 0, 0, 0, 0, 0,   1, 0, -1, 0, 1, 0, 0, 0, 0, 0,   1, 0, 1, 0, 1, 0, 0, 0, 0, 0,
                            -1, 0, -1, 0, 1, 0, 0, 0, 0, 0,   1, 0, 1, 0, 1, 0, 0, 0, 0, 0,   -1, 0, 1, 0, 1, 0, 0, 0, 0, 0};
        glGenVertexArrays(1, &a); glGenBuffers(1, &b); vaoSh_ = a; vboSh_ = b;
        glBindVertexArray(a); glBindBuffer(GL_ARRAY_BUFFER, b);
        glBufferData(GL_ARRAY_BUFFER, sizeof sh, sh, GL_STATIC_DRAW);
        for (int i = 0; i < 4; ++i) {
            glEnableVertexAttribArray(i);
            glVertexAttribPointer(i, i == 3 ? 1 : 3, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(i * 3 * sizeof(float)));
        }
    }
    glBindVertexArray(0);
    meshes_.clear(); kits_.clear();
    ready_ = true;
    return true;
}

void Renderer::shutdown() { ready_ = false; meshes_.clear(); kits_.clear(); batch_.clear(); texSize_ = 0; fbo_ = tex_ = depth_ = 0; }

void Renderer::ensureTarget() {
    const int want = kFbo * scale_;
    if (texSize_ == want && fbo_) return;
    if (fbo_) { GLuint f = fbo_, t = tex_, r = depth_; glDeleteFramebuffers(1, &f); glDeleteTextures(1, &t); glDeleteRenderbuffers(1, &r); }
    GLuint t, r, f;
    glGenTextures(1, &t); tex_ = t;
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, want, want, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    // Retro (1x): piksel-keskin; 2-3x: yumusak olcekleme
    const GLint filt = scale_ == 1 ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filt);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filt);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenRenderbuffers(1, &r); depth_ = r;
    glBindRenderbuffer(GL_RENDERBUFFER, r);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, want, want);
    glGenFramebuffers(1, &f); fbo_ = f;
    glBindFramebuffer(GL_FRAMEBUFFER, f);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, r);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    texSize_ = want;
}

void Renderer::bindTarget() { glBindFramebuffer(GL_FRAMEBUFFER, fbo_); }

void Renderer::begin(int vw, int vh, Color c) {
    vw_ = std::min(vw, kFbo); vh_ = std::min(vh, kFbo);
    sun_ = amb_ = 1.0f;
    ensureTarget();
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, vw_ * scale_, vh_ * scale_);
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
    const float sc = scaleFor(sw, sh);
    const int w = (int)(vw_ * sc), h = (int)(vh_ * sc);
    glViewport((sw - w) / 2, (sh - h) / 2, w, h);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glUseProgram(pBlit_);
    glUniform2f(uUv_, (float)(vw_ * scale_) / texSize_, (float)(vh_ * scale_) / texSize_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_);
    glBindVertexArray(vaoQ_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

float Renderer::scaleFor(int sw, int sh) const {
    const float sc = std::min((float)sw / vw_, (float)sh / vh_);
    return integerScale && sc >= 1.0f ? std::floor(sc) : sc;   // pencere sanal tampondan kucukse kesirli kalir
}

void Renderer::toVirtual(int sw, int sh, float px, float py, float& x, float& y) const {
    const float sc = scaleFor(sw, sh);
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
void Renderer::text(float x, float y, const std::string& src, float sc, Color c) {
    const std::string& s = translate(lang, src);
    for (size_t i = 0; i < s.size();) {
        if (const char* g = glyphU(nextCp(s, i))) {
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
    glViewport(0, 0, vw_ * scale_, vh_ * scale_);
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
    // Yuz normalleri disa yonlendirilir (yuz merkezi - yerel merkez; tekerde tekerin merkezi), sonra ayni malzemede
    // 40 dereceden yumusak komsu yuzlerle ortalanir (kasa puruzsuz, keskin kenarlar korunur)
    const size_t nt = m.tris.size();
    std::vector<std::array<float, 3>> fn(nt);
    for (size_t i = 0; i < nt; ++i) {
        const Tri& t = m.tris[i];
        const Vertex &a = m.verts[t.a], &b = m.verts[t.b], &c = m.verts[t.c];
        const float ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z, vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
        float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
        const float l = std::sqrt(nx * nx + ny * ny + nz * nz) + 1e-12f; nx /= l; ny /= l; nz /= l;
        const float cx = (a.x + b.x + c.x) / 3, cy = (a.y + b.y + c.y) / 3, cz = (a.z + b.z + c.z) / 3;
        const bool wheelPart = t.material == MatTire || t.material == MatRim;
        const float ox = wheelPart ? 0.0f : cx * 0.5f, oz = (float)(wheelPart ? cz : v->heightM * 0.45);
        float dx = cx - ox, dy = wheelPart ? cy - (cy > 0 ? 1.0f : -1.0f) * (float)v->widthM * 0.3f : cy, dz = cz - oz;
        if (wheelPart) dx = 0.0f, dz = 0.0f;                                      // teker: yanak disa
        if (nx * dx + ny * dy + nz * dz < 0) { nx = -nx; ny = -ny; nz = -nz; }
        fn[i] = {nx, ny, nz};
    }
    std::vector<std::vector<int>> adj(m.verts.size());
    for (size_t i = 0; i < nt; ++i) for (int k : {m.tris[i].a, m.tris[i].b, m.tris[i].c}) adj[k].push_back((int)i);
    auto gloss = [](int mat) {
        switch (mat) {
        case MatPaint: return 1.0f; case MatGlass: return 0.45f; case MatChrome: return 0.99f; case MatRim: return 0.7f;
        case MatTrim: return 0.25f; case MatPlate: return 0.15f; case MatDark: return 0.05f; case MatTire: return 0.03f;
        default: return -1.0f;                                                   // far / stop / sinyal: isik yayar
        }
    };
    std::vector<float> buf;
    buf.reserve(nt * 30);
    float maxX = 0, maxY = 0;
    for (const Vertex& p : m.verts) { maxX = std::max(maxX, std::fabs(p.x)); maxY = std::max(maxY, std::fabs(p.y)); }
    M.halfL = maxX; M.halfW = maxY; M.paint = m.paintRGB;
    {   // arka uc / bagaj ustu yuksekligi (kit ve kanat yerlesimi icin; GL: x ileri, y yukari)
        float rx = 0, fx = 0;
        for (const Vertex& p : m.verts) { rx = std::min(rx, p.x); fx = std::max(fx, p.x); }
        float top = 0.3f;
        for (const Tri& t : m.tris) if (t.material == MatPaint)
            for (int k : {t.a, t.b, t.c}) if (m.verts[k].x < rx + 0.45f) top = std::max(top, m.verts[k].z);
        M.rearX = rx; M.frontX = fx; M.rearTop = top;
    }
    // Govde ucgenleri once, sonra her teker ayri aralikta (kendi donusumuyle cizilir)
    std::vector<size_t> order;
    order.reserve(nt);
    const size_t bodyEnd = m.wheels.empty() ? nt : m.wheels.front().triBegin;
    for (size_t i = 0; i < bodyEnd; ++i) order.push_back(i);
    for (const WheelPart& w : m.wheels) for (size_t i = w.triBegin; i < w.triEnd; ++i) order.push_back(i);
    for (size_t i = m.wheels.empty() ? nt : m.wheels.back().triEnd; i < nt; ++i) order.push_back(i);
    M.bodyCount = (int)bodyEnd * 3;
    {
        int first = (int)bodyEnd * 3;
        for (const WheelPart& w : m.wheels) {
            const int cnt = (int)(w.triEnd - w.triBegin) * 3;
            M.wheels.push_back({w.cx, w.cz, -w.cy, first, cnt});               // GL (x, z, -y)
            first += cnt;
        }
    }
    for (size_t i : order) {
        const Tri& t = m.tris[i];
        float c[3]; materialColor(t.material, m.paintRGB, c);
        const float gl = gloss(t.material);
        const bool smooth = t.material == MatPaint || t.material == MatGlass || t.material == MatTire;
        for (int k : {t.a, t.b, t.c}) {
            float n[3] = {fn[i][0], fn[i][1], fn[i][2]};
            if (smooth) {
                float sx = 0, sy = 0, sz = 0;
                for (int j : adj[k]) {
                    if (m.tris[j].material != t.material) continue;
                    const float d = fn[j][0] * fn[i][0] + fn[j][1] * fn[i][1] + fn[j][2] * fn[i][2];
                    if (d > 0.766f) { sx += fn[j][0]; sy += fn[j][1]; sz += fn[j][2]; }
                }
                const float l = std::sqrt(sx * sx + sy * sy + sz * sz);
                if (l > 1e-6f) { n[0] = sx / l; n[1] = sy / l; n[2] = sz / l; }
            }
            const Vertex& p = m.verts[k];
            // (x ileri, y sol, z yukari) -> GL (x, z, -y)
            buf.insert(buf.end(), {p.x, p.z, -p.y, n[0], n[2], -n[1], c[0], c[1], c[2], gl});
        }
    }
    GLuint a, b;
    glGenVertexArrays(1, &a); glGenBuffers(1, &b); M.vao = a; M.vbo = b;
    glBindVertexArray(a); glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferData(GL_ARRAY_BUFFER, buf.size() * sizeof(float), buf.data(), GL_STATIC_DRAW);
    for (int i = 0; i < 4; ++i) {
        glEnableVertexAttribArray(i);
        glVertexAttribPointer(i, i == 3 ? 1 : 3, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(i * 3 * sizeof(float)));
    }
    glBindVertexArray(0);
    M.count = (int)buf.size() / 10;
    return M;
}

// Govde kiti / kanat: kutulardan (arac olcusune gore yerlesim). Boya parcalari gloss 1.0 (boya rengini alir), karbon 0.3.
const Renderer::Mesh& Renderer::kitMesh(int carId, int aero, float wingH) {
    const long key = (long)carId * 64 + aero * 4 + (aero == 9 ? std::clamp((int)(wingH * 10), 0, 3) : 0);
    auto it = kits_.find(key);
    if (it != kits_.end()) return it->second;
    Mesh& K = kits_[key];
    const Mesh& M = mesh(carId);
    std::vector<float> buf;
    const float pr = ((M.paint >> 16) & 255) / 255.f, pg = ((M.paint >> 8) & 255) / 255.f, pb = (M.paint & 255) / 255.f;
    auto box = [&](float x0, float x1, float y0, float y1, float z0, float z1, bool paint) {
        const float c[3] = {paint ? pr : 0.06f, paint ? pg : 0.06f, paint ? pb : 0.07f};
        const float gl = paint ? 1.0f : 0.3f;
        const float P[8][3] = {{x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0}, {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}};
        const int F[6][4] = {{0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7}, {1, 5, 6, 2}, {3, 2, 6, 7}, {4, 5, 1, 0}};
        const float N[6][3] = {{0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
        for (int f = 0; f < 6; ++f)
            for (int k : {0, 1, 2, 0, 2, 3}) {
                const float* q = P[F[f][k]];
                buf.insert(buf.end(), {q[0], q[1], q[2], N[f][0], N[f][1], N[f][2], c[0], c[1], c[2], gl});
            }
    };
    const float rx = M.rearX, top = M.rearTop, hw = M.halfW * 0.93f, L = M.halfL;
    switch (aero) {
    case 1: box(M.frontX - 0.28f, M.frontX + 0.03f, 0.10f, 0.14f, -hw * 0.95f, hw * 0.95f, false); break;          // on lip
    case 2: for (float sg : {-1.0f, 1.0f}) box(-L * 0.5f, L * 0.45f, 0.13f, 0.24f, sg > 0 ? hw - 0.02f : -hw - 0.04f, sg > 0 ? hw + 0.04f : -hw + 0.02f, true); break;
    case 3: box(rx + 0.04f, rx + 0.30f, top - 0.01f, top + 0.07f, -hw * 0.82f, hw * 0.82f, true); break;          // spoiler
    case 4: box(rx + 0.02f, rx + 0.24f, top - 0.02f, top + 0.12f, -hw * 0.88f, hw * 0.88f, true); break;          // ducktail
    case 7: box(rx - 0.06f, rx + 0.35f, 0.10f, 0.22f, -hw * 0.8f, hw * 0.8f, false);                                 // difuzor
            for (int k = -2; k <= 2; ++k) box(rx - 0.06f, rx + 0.2f, 0.2f, 0.32f, k * hw * 0.3f - 0.012f, k * hw * 0.3f + 0.012f, false);
            break;
    case 5: case 6: case 9: {                                                                                         // kanat
        const float H = aero == 5 ? 0.24f : aero == 6 ? 0.38f : std::clamp(wingH, 0.15f, 0.45f);
        const float chord = aero == 6 ? 0.42f : 0.32f, span = aero == 6 ? hw * 1.0f : hw * 0.9f;
        for (float sg : {-1.0f, 1.0f}) box(rx + 0.16f, rx + 0.24f, top - 0.02f, top + H, sg * span * 0.5f - 0.02f, sg * span * 0.5f + 0.02f, false);
        box(rx - 0.04f, rx - 0.04f + chord, top + H, top + H + 0.035f, -span, span, false);
        for (float sg : {-1.0f, 1.0f}) box(rx - 0.08f, rx + chord, top + H - 0.10f, top + H + 0.08f, sg * span - 0.012f, sg * span + 0.012f, false);
        if (aero == 6) box(rx - 0.02f, rx + chord * 0.5f, top + H + 0.06f, top + H + 0.085f, -span, span, false);   // ikinci kat
        break;
    }
    default: break;
    }
    if (buf.empty()) return K;
    GLuint a, b;
    glGenVertexArrays(1, &a); glGenBuffers(1, &b); K.vao = a; K.vbo = b;
    glBindVertexArray(a); glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferData(GL_ARRAY_BUFFER, buf.size() * sizeof(float), buf.data(), GL_STATIC_DRAW);
    for (int i = 0; i < 4; ++i) {
        glEnableVertexAttribArray(i);
        glVertexAttribPointer(i, i == 3 ? 1 : 3, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(i * 3 * sizeof(float)));
    }
    glBindVertexArray(0);
    K.count = (int)buf.size() / 10;
    return K;
}

void Renderer::drawCar(int carId, float x, float y, float w, float h, const Mat4& proj, const Mat4& view, const Mat4& model,
                       float wheelSpin, float steer) {
    flush2D();
    const CarLook look = look_;
    look_ = CarLook{};                                                        // gorunum yalniz bu cizim icin
    const Mesh& M = mesh(carId);
    if (!M.count) return;
    glEnable(GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);
    const int S = scale_;
    glViewport((int)(x * S), (vh_ - (int)(y + h)) * S, (int)(w * S), (int)(h * S));
    glUseProgram(p3d_);
    // Kamera konumu (gorunum matrisinin tersinden): yansima ve parlama icin
    const float* V = view.m;
    const float ex = -(V[0] * V[12] + V[1] * V[13] + V[2] * V[14]);
    const float ey = -(V[4] * V[12] + V[5] * V[13] + V[6] * V[14]);
    const float ez = -(V[8] * V[12] + V[9] * V[13] + V[10] * V[14]);
    glUniform3f(uEye_, ex, ey, ez);
    glUniform2f(uLight_, sun_, amb_);
    glUniform4f(uPaint_, look.paint[0], look.paint[1], look.paint[2], look.paintOn ? 1.0f : 0.0f);
    glUniform4f(uRim_, look.rim[0], look.rim[1], look.rim[2], look.rimOn ? 1.0f : 0.0f);
    glUniform4f(uStripe_, look.stripeCol[0], look.stripeCol[1], look.stripeCol[2], (float)look.stripe);
    glUniform2f(uPG_, look.paintGloss, M.rearTop * 0.62f);
    if (look.alpha >= 1.0f) {   // Zemin golgesi: iki kat (yumusak kenar), derinlik yazmadan, alfa karisimi
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE);
        glUniform1f(uShadow_, 1.0f);
        glBindVertexArray(vaoSh_);
        for (int k = 0; k < 1; ++k) {
            const float gx = M.halfL * 1.12f, gz = M.halfW * 1.25f;
            Mat4 sc{}; sc.m[0] = gx; sc.m[5] = 1; sc.m[10] = gz; sc.m[15] = 1; sc.m[13] = 0.015f;
            const Mat4 mm = matMul(model, sc), mvpS = matMul(proj, matMul(view, mm));
            glUniformMatrix4fv(uMvp_, 1, GL_FALSE, mvpS.m);
            glUniformMatrix4fv(uModel_, 1, GL_FALSE, mm.m);
            glUniform1f(uAlpha_, 0.42f);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
        glDepthMask(GL_TRUE); glDisable(GL_BLEND);
        glUniform1f(uShadow_, 0.0f);
    }
    const Mat4 body = look.drop != 0.0f ? matMul(model, matTranslate(0, -look.drop, 0)) : model;   // basiklik: teker yerinde
    const Mat4 mvp = matMul(proj, matMul(view, body));
    glUniformMatrix4fv(uMvp_, 1, GL_FALSE, mvp.m);
    glUniformMatrix4fv(uModel_, 1, GL_FALSE, body.m);
    glUniform1f(uAlpha_, look.alpha);
    if (look.alpha < 1.0f) { glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); }
    if (look.aero > 0) {
        const Mesh& K = kitMesh(carId, look.aero, look.wingH);
        if (K.count) { glBindVertexArray(K.vao); glDrawArrays(GL_TRIANGLES, 0, K.count); }
    }
    glBindVertexArray(M.vao);
    if (M.wheels.size() == 4 && (wheelSpin != 0.0f || steer != 0.0f || look.drop != 0.0f)) {
        glDrawArrays(GL_TRIANGLES, 0, M.bodyCount);
        for (size_t k = 0; k < 4; ++k) {
            const WheelDraw& wd = M.wheels[k];
            // model * T(merkez) * sapma (dikey eksen, yalniz on) * donus (aks = GL z) * T(-merkez)
            Mat4 wm = matMul(model, matTranslate(wd.cx, wd.cy, wd.cz));
            if (k < 2 && steer != 0.0f) wm = matMul(wm, matRotY(steer));
            wm = matMul(matMul(wm, matRotZ(-wheelSpin)), matTranslate(-wd.cx, -wd.cy, -wd.cz));
            const Mat4 wmvp = matMul(proj, matMul(view, wm));
            glUniformMatrix4fv(uMvp_, 1, GL_FALSE, wmvp.m);
            glUniformMatrix4fv(uModel_, 1, GL_FALSE, wm.m);
            glDrawArrays(GL_TRIANGLES, wd.first, wd.count);
        }
        const int rest = M.count - (M.wheels.back().first + M.wheels.back().count);
        if (rest > 0) {
            glUniformMatrix4fv(uMvp_, 1, GL_FALSE, mvp.m);
            glUniformMatrix4fv(uModel_, 1, GL_FALSE, body.m);
            glDrawArrays(GL_TRIANGLES, M.wheels.back().first + M.wheels.back().count, rest);
        }
    } else glDrawArrays(GL_TRIANGLES, 0, M.count);
    if (look.alpha < 1.0f) { glDisable(GL_BLEND); glUniform1f(uAlpha_, 1.0f); }
    glDisable(GL_DEPTH_TEST);
    glViewport(0, 0, vw_ * S, vh_ * S);
}

} // namespace zk
