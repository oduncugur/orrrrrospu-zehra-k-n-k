// ZEHRA KINIK - Arac basina on / arka yuz (far, stop, izgara). LowPolyModel.cpp icinde dahil edilir.
// Ortak "kutu far" yerine her aracin kendi far / stop sekli, boyutu ve yeri; parcalar burnun / kuyrugun egrisine oturur.
//   lamp : far sekli   w, h: tek far (yan basina) genislik / yukseklik (m)   y: far merkezi, yari genislik orani (0 orta .. 1 kenar)
//   tail : stop sekli  tw, th, ty: ayni anlamda      grille: izgara tipi  gw, gh: genislik / yukseklik (m)
// Logo / amblem yok.

enum FaceLamp { FL_RECT, FL_ROUND, FL_TWIN, FL_POP, FL_SWEPT, FL_OVAL, FL_SQUINT, FL_FENDER };
enum FaceTail { FT_RECT, FT_ROUND, FT_TWINR, FT_BAR, FT_VERT, FT_SLIM };
enum FaceGrille { FG_NONE, FG_SLOT, FG_WIDE, FG_FULL, FG_KIDNEY, FG_MOUTH, FG_HEX };

struct FaceSpec { int id; FaceLamp lamp; double w, h, y; FaceTail tail; double tw, th, ty; FaceGrille grille; double gw, gh; };

const FaceSpec kFaces[] = {
    //  id  far        w     h     y      stop       tw    th    ty     izgara      gw    gh
    {  2, FL_SWEPT,  0.40, 0.11, 0.55, FT_RECT,  0.38, 0.11, 0.62, FG_SLOT,   0.46, 0.05},   // Civic EG
    {  5, FL_SWEPT,  0.38, 0.13, 0.55, FT_RECT,  0.26, 0.15, 0.72, FG_SLOT,   0.42, 0.06},   // Civic EK9
    {  8, FL_SQUINT, 0.42, 0.11, 0.58, FT_SLIM,  0.40, 0.08, 0.66, FG_MOUTH,  0.80, 0.16},   // Civic FD2
    { 11, FL_RECT,   0.36, 0.09, 0.55, FT_BAR,   0.00, 0.10, 0.00, FG_SLOT,   0.40, 0.04},   // CR-X
    { 12, FL_TWIN,   0.34, 0.12, 0.55, FT_SLIM,  0.42, 0.09, 0.62, FG_SLOT,   0.40, 0.05},   // Integra DC2 (JDM dortlu far)
    { 17, FL_SWEPT,  0.36, 0.12, 0.58, FT_SLIM,  0.34, 0.09, 0.66, FG_MOUTH,  0.60, 0.12},   // S2000
    { 19, FL_POP,    0.34, 0.08, 0.60, FT_BAR,   0.00, 0.11, 0.00, FG_NONE,   0.00, 0.00},   // NSX NA1
    { 20, FL_SWEPT,  0.36, 0.10, 0.60, FT_BAR,   0.00, 0.11, 0.00, FG_NONE,   0.00, 0.00},   // NSX NA2
    { 24, FL_POP,    0.34, 0.08, 0.58, FT_RECT,  0.44, 0.12, 0.55, FG_SLOT,   0.50, 0.04},   // AE86 Trueno
    { 27, FL_RECT,   0.40, 0.11, 0.55, FT_RECT,  0.40, 0.12, 0.58, FG_SLOT,   0.44, 0.05},   // Corolla AE92
    { 33, FL_POP,    0.36, 0.08, 0.60, FT_BAR,   0.00, 0.10, 0.00, FG_SLOT,   0.40, 0.04},   // Supra A70
    { 34, FL_OVAL,   0.40, 0.14, 0.58, FT_TWINR, 0.17, 0.17, 0.66, FG_MOUTH,  0.70, 0.13},   // Supra A80
    { 46, FL_SQUINT, 0.36, 0.09, 0.62, FT_SLIM,  0.36, 0.07, 0.70, FG_MOUTH,  0.90, 0.22},   // LFA
    { 50, FL_RECT,   0.38, 0.10, 0.56, FT_TWINR, 0.17, 0.17, 0.66, FG_WIDE,   0.50, 0.08},   // R32 GT-R
    { 52, FL_SQUINT, 0.40, 0.12, 0.58, FT_TWINR, 0.18, 0.18, 0.66, FG_MOUTH,  0.80, 0.16},   // R34 GT-R
    { 53, FL_SQUINT, 0.40, 0.12, 0.58, FT_TWINR, 0.18, 0.18, 0.66, FG_MOUTH,  0.70, 0.13},   // R34 GT-T
    { 55, FL_SWEPT,  0.40, 0.13, 0.62, FT_TWINR, 0.17, 0.17, 0.70, FG_HEX,    0.80, 0.22},   // GT-R R35
    { 57, FL_POP,    0.34, 0.08, 0.58, FT_BAR,   0.00, 0.10, 0.00, FG_SLOT,   0.40, 0.04},   // 180SX
    { 59, FL_SQUINT, 0.40, 0.11, 0.56, FT_RECT,  0.36, 0.11, 0.62, FG_MOUTH,  0.70, 0.14},   // Silvia S15
    { 64, FL_ROUND,  0.18, 0.18, 0.66, FT_RECT,  0.34, 0.10, 0.62, FG_WIDE,   0.50, 0.08},   // 240Z
    { 75, FL_SQUINT, 0.34, 0.09, 0.56, FT_SLIM,  0.30, 0.08, 0.64, FG_HEX,    0.56, 0.14},   // MX-5 ND
    { 78, FL_POP,    0.32, 0.07, 0.58, FT_TWINR, 0.15, 0.15, 0.62, FG_MOUTH,  0.56, 0.12},   // RX-7 FD
    { 85, FL_RECT,   0.42, 0.13, 0.56, FT_RECT,  0.38, 0.13, 0.60, FG_SLOT,   0.30, 0.07},   // Impreza GC8
    { 87, FL_RECT,   0.42, 0.13, 0.56, FT_RECT,  0.38, 0.13, 0.60, FG_SLOT,   0.30, 0.07},   // 22B
    { 88, FL_OVAL,   0.40, 0.16, 0.56, FT_TWINR, 0.16, 0.16, 0.64, FG_HEX,    0.50, 0.12},   // Impreza GDB
    {100, FL_RECT,   0.40, 0.14, 0.58, FT_RECT,  0.40, 0.12, 0.60, FG_MOUTH,  0.80, 0.18},   // Evo VI
    {102, FL_SWEPT,  0.42, 0.13, 0.58, FT_RECT,  0.40, 0.12, 0.60, FG_MOUTH,  0.84, 0.20},   // Evo IX
    {118, FL_TWIN,   0.32, 0.13, 0.52, FT_RECT,  0.36, 0.14, 0.62, FG_KIDNEY, 0.30, 0.13},   // M3 E30
    {120, FL_RECT,   0.40, 0.11, 0.56, FT_RECT,  0.38, 0.11, 0.62, FG_KIDNEY, 0.30, 0.10},   // M3 E36
    {133, FL_ROUND,  0.17, 0.17, 0.62, FT_VERT,  0.08, 0.16, 0.80, FG_WIDE,   0.56, 0.16},   // Mini
    {151, FL_ROUND,  0.18, 0.18, 0.62, FT_RECT,  0.30, 0.12, 0.62, FG_WIDE,   0.74, 0.14},   // Golf Mk1
    {152, FL_TWIN,   0.36, 0.15, 0.58, FT_RECT,  0.34, 0.12, 0.62, FG_WIDE,   0.56, 0.14},   // Golf Mk2 GTI
    {164, FL_FENDER, 0.19, 0.19, 0.64, FT_RECT,  0.26, 0.09, 0.66, FG_NONE,   0.00, 0.00},   // 911 RS 2.7
    {166, FL_FENDER, 0.19, 0.19, 0.66, FT_BAR,   0.00, 0.10, 0.00, FG_NONE,   0.00, 0.00},   // 930 Turbo
    {167, FL_FENDER, 0.19, 0.19, 0.64, FT_BAR,   0.00, 0.10, 0.00, FG_NONE,   0.00, 0.00},   // 964 RS
    {171, FL_OVAL,   0.26, 0.16, 0.64, FT_SLIM,  0.30, 0.08, 0.70, FG_MOUTH,  0.70, 0.12},   // 997 GT3 RS
    {177, FL_SWEPT,  0.30, 0.12, 0.64, FT_SLIM,  0.30, 0.07, 0.70, FG_MOUTH,  0.80, 0.14},   // Carrera GT
    {184, FL_RECT,   0.30, 0.08, 0.64, FT_TWINR, 0.15, 0.15, 0.62, FG_MOUTH,  0.90, 0.08},   // F40
    {185, FL_POP,    0.32, 0.07, 0.62, FT_BAR,   0.00, 0.14, 0.00, FG_WIDE,   0.80, 0.08},   // Testarossa
    {195, FL_POP,    0.30, 0.06, 0.60, FT_RECT,  0.40, 0.12, 0.60, FG_NONE,   0.00, 0.00},   // Countach
    {213, FL_TWIN,   0.38, 0.12, 0.58, FT_RECT,  0.32, 0.13, 0.62, FG_WIDE,   0.50, 0.10},   // Delta Integrale
    {214, FL_POP,    0.30, 0.07, 0.60, FT_TWINR, 0.14, 0.14, 0.62, FG_NONE,   0.00, 0.00},   // Stratos
    {216, FL_TWIN,   0.34, 0.13, 0.58, FT_RECT,  0.34, 0.12, 0.60, FG_WIDE,   0.50, 0.12},   // Murat 124
    {217, FL_RECT,   0.30, 0.12, 0.56, FT_RECT,  0.32, 0.16, 0.62, FG_WIDE,   0.56, 0.12},   // Sahin
    {218, FL_RECT,   0.30, 0.12, 0.56, FT_RECT,  0.32, 0.16, 0.62, FG_WIDE,   0.56, 0.12},   // Dogan
    {219, FL_RECT,   0.30, 0.12, 0.56, FT_VERT,  0.10, 0.26, 0.82, FG_WIDE,   0.56, 0.12},   // Kartal
    {220, FL_ROUND,  0.17, 0.17, 0.60, FT_RECT,  0.30, 0.10, 0.62, FG_WIDE,   0.60, 0.12},   // Anadol A1
    {222, FL_ROUND,  0.18, 0.18, 0.66, FT_VERT,  0.20, 0.14, 0.62, FG_FULL,   0.00, 0.16},   // Mustang 69
    {227, FL_SQUINT, 0.36, 0.10, 0.64, FT_VERT,  0.24, 0.14, 0.62, FG_HEX,    0.80, 0.22},   // Mustang S550
    {231, FL_RECT,   0.34, 0.12, 0.58, FT_RECT,  0.32, 0.13, 0.62, FG_WIDE,   0.50, 0.10},   // Escort Cosworth
    {232, FL_RECT,   0.36, 0.12, 0.58, FT_RECT,  0.34, 0.12, 0.62, FG_WIDE,   0.50, 0.08},   // Sierra Cosworth
    {250, FL_ROUND,  0.18, 0.18, 0.66, FT_RECT,  0.30, 0.10, 0.62, FG_FULL,   0.00, 0.16},   // Camaro 69
    {264, FL_POP,    0.00, 0.00, 0.00, FT_BAR,   0.00, 0.10, 0.00, FG_FULL,   0.00, 0.20},   // Charger 69 (gizli far)
    {272, FL_SWEPT,  0.36, 0.11, 0.64, FT_SLIM,  0.30, 0.08, 0.70, FG_MOUTH,  0.80, 0.14},   // Viper ACR
    {282, FL_RECT,   0.30, 0.12, 0.56, FT_VERT,  0.12, 0.22, 0.80, FG_WIDE,   0.56, 0.12},   // Renault 12
    {284, FL_RECT,   0.28, 0.12, 0.52, FT_VERT,  0.10, 0.24, 0.84, FG_WIDE,   0.50, 0.10},   // R5 Turbo
    {290, FL_RECT,   0.30, 0.12, 0.56, FT_VERT,  0.12, 0.22, 0.80, FG_WIDE,   0.56, 0.12},   // Dacia 1300
    {291, FL_RECT,   0.34, 0.11, 0.54, FT_RECT,  0.30, 0.12, 0.62, FG_WIDE,   0.50, 0.06},   // 205 GTI
    {302, FL_ROUND,  0.16, 0.16, 0.58, FT_RECT,  0.32, 0.12, 0.62, FG_WIDE,   0.60, 0.12},   // VAZ-2101
    {308, FL_OVAL,   0.26, 0.12, 0.64, FT_ROUND, 0.10, 0.10, 0.66, FG_MOUTH,  0.60, 0.12},   // Exige
    {313, FL_OVAL,   0.28, 0.10, 0.64, FT_TWINR, 0.14, 0.14, 0.62, FG_MOUTH,  0.70, 0.10},   // McLaren F1
    {315, FL_SQUINT, 0.30, 0.08, 0.64, FT_SLIM,  0.36, 0.06, 0.70, FG_MOUTH,  0.90, 0.14},   // 720S
};

const FaceSpec* faceSpecFor(int id) {
    for (const FaceSpec& f : kFaces) if (f.id == id) return &f;
    return nullptr;
}
