// ZEHRA KINIK - Elle modellenmis araclar (genel ureteci kullanmaz). LowPolyModel.cpp icinde, Builder'dan sonra dahil edilir.
// Her arac: istasyon tablosu (on tampondan oran s) -> 16 noktali kesit halkalari (sol 9 nokta + ayna), kendine has
// on / arka yuz detaylari. Olculer VehicleTable.inc'teki gercek degerlerden. Logo / amblem yok.

// Parcali dogrusal tablo: (s, deger) ciftleri
static double lerpTab(const double (*t)[2], int n, double s) {
    if (s <= t[0][0]) return t[0][1];
    for (int i = 1; i < n; ++i)
        if (s <= t[i][0]) { const double k = (s - t[i - 1][0]) / (t[i][0] - t[i - 1][0]); return t[i - 1][1] + (t[i][1] - t[i - 1][1]) * k; }
    return t[n - 1][1];
}

// ---------------------------------------------------------------- BMW M3 E46 (kupe, 2000)
static LowPolyMesh buildE46(const VehicleDef& v) {
    LowPolyMesh m; m.paintRGB = v.paintRGB;
    Builder B{m};
    const double L = v.lengthM, W = v.widthM, HW = W * 0.5;
    // Yan profil (z, m): ust yuzey (kaput / cam / tavan / bagaj), bel cizgisi, alt
    static const double kTop[][2] = {{0.0, 0.68}, {0.012, 0.715}, {0.035, 0.74}, {0.08, 0.775}, {0.16, 0.815}, {0.26, 0.845}, {0.335, 0.865},
                                     {0.465, 1.36}, {0.56, 1.38}, {0.655, 1.345}, {0.72, 1.17}, {0.795, 0.985}, {0.90, 0.995},
                                     {0.96, 0.985}, {0.985, 0.94}, {1.0, 0.84}};
    static const double kBelt[][2] = {{0.0, 0.64}, {0.04, 0.70}, {0.20, 0.80}, {0.335, 0.85}, {0.55, 0.89}, {0.72, 0.93}, {0.80, 0.95},
                                      {0.965, 0.93}, {1.0, 0.82}};
    static const double kBot[][2] = {{0.0, 0.30}, {0.03, 0.21}, {0.08, 0.17}, {0.92, 0.17}, {0.975, 0.22}, {1.0, 0.31}};
    static const double kHw[][2] = {{0.0, 0.87}, {0.015, 0.935}, {0.05, 0.97}, {0.13, 0.985}, {0.5, 1.0}, {0.86, 0.99}, {0.955, 0.955},
                                    {0.99, 0.90}, {1.0, 0.86}};
    constexpr int nT = sizeof kTop / sizeof *kTop, nB = sizeof kBelt / sizeof *kBelt, nBo = sizeof kBot / sizeof *kBot, nH = sizeof kHw / sizeof *kHw;
    const double cowl = 0.335, roofF = 0.465, roofR = 0.655, deck = 0.795, sideGlass0 = 0.36, sideGlass1 = 0.665;
    // Teker: E46 M3 on cikinti ~0.80 m, arka ~0.96 m; 18" (r 0.33)
    const double r = 0.33, ovF = 0.80, xFront = L * 0.5 - ovF, xRear = xFront - v.wheelbaseM;
    const double sF = ovF / L, sR = (L * 0.5 - xRear) / L;
    auto X = [&](double s) { return L * (0.5 - s); };
    auto top = [&](double s) { return lerpTab(kTop, nT, s); };
    auto belt = [&](double s) { return std::min(lerpTab(kBelt, nB, s), top(s) - 0.012); };
    auto bot = [&](double s) { return lerpTab(kBot, nBo, s); };
    auto hw = [&](double s) { return HW * lerpTab(kHw, nH, s); };
    auto flare = [&](double s) {                                          // genis camurluklar (M3)
        const double a = (s - sF) / 0.07, b = (s - sR) / 0.07;
        return 0.022 * std::exp(-a * a) + 0.030 * std::exp(-b * b);
    };
    auto cabin = [&](double s) { return s > cowl && s < deck; };
    // Sol yarim kesit: 9 nokta (alt orta .. ust orta)
    auto ring = [&](double s, double (*p)[2]) {
        const double h = hw(s), zb = bot(s), zl = belt(s), zt = top(s), fl = flare(s);
        p[0][0] = 0;            p[0][1] = zb;
        p[1][0] = h * 0.90;     p[1][1] = zb;
        p[2][0] = h * 0.985;    p[2][1] = zb + 0.09;
        p[3][0] = h + fl;       p[3][1] = std::min(zb + 0.30, zl - 0.10);
        p[4][0] = h + fl * 0.6; p[4][1] = zl - 0.045;
        p[5][0] = h * 0.985;    p[5][1] = zl;
        if (cabin(s)) {                                                   // cerceve: cam alti, tavan kenari (tumblehome)
            const double z6 = zl + 0.045, z7 = std::max(zt - 0.035, z6 + 0.006);
            const double k = std::clamp((zt - zl) / (1.37 - 0.86), 0.0, 1.0);   // camin dikligi: tavanda tam iceri
            p[6][0] = h * 0.935;               p[6][1] = z6;
            p[7][0] = h * (0.935 - 0.17 * k);  p[7][1] = z7;
        } else {                                                          // kaput / bagaj: omuz + power-dome
            const double dome = (s > 0.06 && s < 0.31) ? 0.022 * std::sin(kPi * (s - 0.06) / 0.25) : 0.0;
            p[6][0] = h * 0.955; p[6][1] = std::max(zl + 0.012, zt - 0.025);
            p[7][0] = h * 0.40;  p[7][1] = zt + dome;
        }
        p[8][0] = 0; p[8][1] = cabin(s) ? zt + 0.006 : zt + ((s > 0.06 && s < 0.31) ? 0.022 * std::sin(kPi * (s - 0.06) / 0.25) : 0.0);
    };
    // Istasyonlar: uclarda ve cam gecislerinde sik, aks merkezleri dahil
    std::vector<double> ss = {0.0, 0.006, 0.015, 0.03, 0.05, 0.08, 0.11, 0.14, 0.20, 0.25, 0.30, cowl, 0.36, 0.40, 0.435, roofF, 0.51, 0.56,
                              0.61, roofR, 0.69, sideGlass1, 0.74, 0.77, deck, 0.83, 0.87, 0.91, 0.94, 0.965, 0.985, 0.995, 1.0,
                              sF - 0.06, sF, sF + 0.06, sR - 0.06, sR, sR + 0.06, sideGlass0};
    std::sort(ss.begin(), ss.end());
    ss.erase(std::unique(ss.begin(), ss.end(), [](double a, double b) { return std::fabs(a - b) < 0.004; }), ss.end());
    // Halkalar: dongu sirasi L0..L8, R7..R1 (16 nokta)
    std::vector<std::array<int, 16>> rings;
    for (double s : ss) {
        double p[9][2]; ring(s, p);
        std::array<int, 16> R{};
        for (int k = 0; k <= 8; ++k) R[k] = B.v(X(s), p[k][0], p[k][1]);
        for (int k = 7; k >= 1; --k) R[16 - k] = B.v(X(s), -p[k][0], p[k][1]);
        rings.push_back(R);
    }
    auto segMat = [&](int j, double s) {                                  // j: sol yari segment (0 alt .. 7 ust)
        if (j == 0) return (int)MatDark;
        if (j == 6 && s > sideGlass0 && s < sideGlass1) return (int)MatGlass;         // yan camlar (cercevesiz kupe)
        if (j == 7 && ((s > cowl && s < roofF) || (s > roofR && s < deck))) return (int)MatGlass;   // on / arka cam
        if (j == 6 && cabin(s)) return (int)MatPaint;                    // A / C direkleri govde renginde (kalin C diregi)
        return (int)MatPaint;
    };
    for (size_t i = 0; i + 1 < rings.size(); ++i) {
        const double sm = 0.5 * (ss[i] + ss[i + 1]);
        for (int k = 0; k < 16; ++k) {
            const int k2 = (k + 1) % 16, j = k < 8 ? k : 15 - k;
            B.quad(rings[i][k], rings[i + 1][k], rings[i + 1][k2], rings[i][k2], segMat(j, sm));
        }
    }
    for (int end = 0; end < 2; ++end) {                                   // on / arka kapak (yelpaze)
        const auto& R = end ? rings.back() : rings.front();
        const double s = end ? 1.0 : 0.0;
        const int c = B.v(X(s) + (end ? -0.004 : 0.004), 0, 0.5 * (bot(s) + top(s)));
        for (int k = 0; k < 16; ++k) B.tri(c, R[k], R[(k + 1) % 16], MatPaint);
    }
    const double xF = X(0.0), xB = X(1.0);
    // ---- on yuz: bobrek izgara, angel-eye farlar, alt hava girisi, sis farlari, plaka
    for (double sg : {1.0, -1.0}) {
        B.diskX(xF + 0.010, sg * 0.105, 0.585, 0.088, 0.070, 10, MatChrome);       // bobrek cercevesi
        B.diskX(xF + 0.014, sg * 0.105, 0.585, 0.072, 0.056, 10, MatDark);
        B.panel(xF + 0.006, sg * 0.21, 0.545, xF + 0.006, sg * 0.21, 0.655, xF + 0.006, sg * HW * 0.86, 0.665, xF + 0.006, sg * HW * 0.86, 0.565, MatDark);   // far muhafazasi
        for (double yc : {0.33, 0.55}) {                                  // cift yuvarlak angel-eye
            B.diskX(xF + 0.012, sg * yc, 0.605, 0.050, 0.046, 12, MatLight);
            B.diskX(xF + 0.016, sg * yc, 0.605, 0.030, 0.027, 10, MatChrome);
        }
        B.diskX(xF + 0.010, sg * 0.60, 0.385, 0.038, 0.030, 10, MatLight);   // sis fari
    }
    B.panel(xF + 0.012, 0.50, 0.46, xF + 0.012, 0.50, 0.32, xF + 0.012, -0.50, 0.32, xF + 0.012, -0.50, 0.46, MatDark);   // alt hava girisi
    B.panel(xF + 0.016, 0.24, 0.47, xF + 0.016, 0.24, 0.39, xF + 0.016, -0.24, 0.39, xF + 0.016, -0.24, 0.47, MatPlate);
    // ---- arka: L stoplar (yana sarar), bagaj cizgisi, plaka, difuzor, dortlu egzoz
    for (double sg : {1.0, -1.0}) {
        B.panel(xB - 0.008, sg * 0.40, 0.905, xB - 0.008, sg * 0.40, 0.835, xB - 0.008, sg * HW * 0.85, 0.835, xB - 0.008, sg * HW * 0.85, 0.905, MatTail);
        B.panel(xB - 0.008, sg * 0.58, 0.835, xB - 0.008, sg * 0.58, 0.775, xB - 0.008, sg * HW * 0.85, 0.775, xB - 0.008, sg * HW * 0.85, 0.835, MatTail);
        const double ys = sg * (hw(0.975) + 0.004);
        B.panel(X(0.955), ys, 0.900, X(0.955), ys, 0.800, X(0.993), ys * 0.97, 0.790, X(0.993), ys * 0.97, 0.900, MatTail);
        B.panel(xB - 0.009, sg * 0.43, 0.82, xB - 0.009, sg * 0.43, 0.81, xB - 0.009, sg * 0.58, 0.81, xB - 0.009, sg * 0.58, 0.82, MatIndicator);
        for (double ye : {0.40, 0.52}) {                                  // dortlu egzoz
            B.diskX(xB - 0.012, sg * ye, 0.245, 0.042, 0.042, 10, MatChrome);
            B.diskX(xB - 0.016, sg * ye, 0.245, 0.030, 0.030, 10, MatDark);
        }
    }
    B.panel(xB - 0.010, 0.26, 0.66, xB - 0.010, 0.26, 0.56, xB - 0.010, -0.26, 0.56, xB - 0.010, -0.26, 0.66, MatPlate);
    B.panel(xB - 0.007, 0.62, 0.33, xB - 0.007, 0.62, 0.20, xB - 0.007, -0.62, 0.20, xB - 0.007, -0.62, 0.33, MatDark);       // difuzor
    B.panel(X(0.975), 0.62, top(0.975) + 0.006, X(0.975), -0.62, top(0.975) + 0.006, X(0.98), -0.62, top(0.98) + 0.004, X(0.98), 0.62, top(0.98) + 0.004, MatDark);   // bagaj kapagi cizgisi
    // ---- yan: kapi cizgisi, M3 yan havalandirmasi, aynalar, teker yuvalari
    for (double sg : {1.0, -1.0}) {
        for (double sd : {0.345, 0.615}) {                                // kapi araliklari
            const double y = sg * (hw(sd) + 0.003);
            B.panel(X(sd), y, 0.30, X(sd), y, belt(sd), X(sd) - 0.008, y, belt(sd), X(sd) - 0.008, y, 0.30, MatDark);
        }
        {   // camurluk arkasinda yan havalandirma (izgara)
            const double y = sg * (hw(0.25) + flare(0.25) * 0.6 + 0.004);
            B.panel(X(0.235), y, 0.70, X(0.235), y, 0.64, X(0.275), y, 0.64, X(0.275), y, 0.70, MatDark);
            B.panel(X(0.236), y * 1.001, 0.676, X(0.236), y * 1.001, 0.664, X(0.274), y * 1.001, 0.664, X(0.274), y * 1.001, 0.676, MatChrome);
        }
        {   // ayna (A direginin dibi)
            const double s0 = 0.365, s1 = 0.395, y0 = hw(0.38) * 0.95, y1 = y0 + 0.13, z0 = belt(0.38) + 0.04;
            B.box(X(s1), X(s0), sg > 0 ? y0 : -y1, sg > 0 ? y1 : -y0, z0, z0 + 0.085, MatPaint);
            B.panel(X(s1) - 0.003, sg * (y0 + 0.02), z0 + 0.01, X(s1) - 0.003, sg * (y1 - 0.01), z0 + 0.01,
                    X(s1) - 0.003, sg * (y1 - 0.01), z0 + 0.075, X(s1) - 0.003, sg * (y0 + 0.02), z0 + 0.075, MatGlass);
        }
        for (double xa : {xFront, xRear}) {                               // teker yuvasi (koyu yarim disk, camurluk dudagi)
            const double sa = (L * 0.5 - xa) / L, y = sg * (hw(sa) + flare(sa) + 0.003);
            B.archY(xa, y, r - 0.01, 0.0, r * 1.13, 10, MatDark);
        }
    }
    // ---- tekerler (sira: on sol, on sag, arka sol, arka sag); dis yuz camurlukla hizali
    const double tw = 0.255;
    for (double xa : {xFront, xRear})
        for (double sg : {1.0, -1.0}) {
            const double sa = (L * 0.5 - xa) / L, cy = sg * (hw(sa) + flare(sa) + 0.012 - tw * 0.5);
            const size_t t0 = m.tris.size();
            B.wheel(xa, cy, r, r, tw, W_M);
            m.wheels.push_back({(float)xa, (float)cy, (float)r, (float)r, t0, m.tris.size()});
        }
    return m;
}

// Elle modellenmis arac varsa onu dondurur
static bool buildHandModel(const VehicleDef& v, LowPolyMesh& out) {
    if (v.ref == "BMW M3 E46") { out = buildE46(v); return true; }
    return false;
}
