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
    // Referans: yandan fotograf (Wikimedia Commons, "BMW M3 Coupe E46", CC BY 2.0) uzerinde olculen oranlar; olcek teker
    // capindan (0.64 m), tavan / kaput orta hatti kamera yuksekligi icin duzeltildi. Kopya degil, oran referansi.
    static const double kTop[][2] = {{0.0, 0.70}, {0.012, 0.735}, {0.035, 0.765}, {0.08, 0.80}, {0.16, 0.845}, {0.24, 0.885}, {0.283, 0.905},
                                     {0.443, 1.30}, {0.54, 1.37}, {0.64, 1.345}, {0.71, 1.30}, {0.75, 1.24}, {0.893, 0.94},
                                     {0.945, 0.965}, {0.985, 0.99}, {0.995, 0.95}, {1.0, 0.82}};
    static const double kBelt[][2] = {{0.0, 0.62}, {0.04, 0.70}, {0.20, 0.80}, {0.283, 0.85}, {0.55, 0.875}, {0.75, 0.905}, {0.85, 0.915},
                                      {0.965, 0.92}, {1.0, 0.80}};
    static const double kBot[][2] = {{0.0, 0.30}, {0.03, 0.22}, {0.08, 0.16}, {0.92, 0.16}, {0.975, 0.22}, {1.0, 0.30}};
    static const double kHw[][2] = {{0.0, 0.87}, {0.015, 0.935}, {0.05, 0.97}, {0.13, 0.985}, {0.5, 1.0}, {0.86, 0.99}, {0.955, 0.955},
                                    {0.99, 0.90}, {1.0, 0.86}};
    constexpr int nT = sizeof kTop / sizeof *kTop, nB = sizeof kBelt / sizeof *kBelt, nBo = sizeof kBot / sizeof *kBot, nH = sizeof kHw / sizeof *kHw;
    const double cowl = 0.283, roofF = 0.443, roofR = 0.71, deck = 0.893, sideGlass0 = 0.31, sideGlass1 = 0.775;   // uzun yatik arka cam, kisa bagaj
    // Teker: fotografta on cikinti ~0.72 m, arka ~1.04 m; 18" (r 0.32)
    const double r = 0.32, ovF = 0.72, xFront = L * 0.5 - ovF, xRear = xFront - v.wheelbaseM;
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
    // Sol yarim kesit: 12 nokta (alt orta .. ust orta). E46: belirgin esik (marspiyel) kivrimi, kapi kolu hizasinda
    // karakter cizgisi, M3 camurluk sisligi, omuz, cam alti, cam ustu, yuvarlak tavan kenari.
    constexpr int NP = 12, NR = 2 * NP - 2;
    auto ring = [&](double s, double (*p)[2]) {
        const double h = hw(s), zb = bot(s), zl = belt(s), zt = top(s), fl = flare(s);
        const double dome = (s > 0.06 && s < 0.31) ? 0.022 * std::sin(kPi * (s - 0.06) / 0.25) : 0.0;   // M3 kaput kubbesi
        p[0][0] = 0;                  p[0][1] = zb;
        p[1][0] = h * 0.86;           p[1][1] = zb;
        p[2][0] = h * 0.965;          p[2][1] = zb + 0.05;                  // esik alti
        p[3][0] = h * 0.985 + fl * 0.3; p[3][1] = zb + 0.15;                // esik kivrimi (marspiyel)
        p[4][0] = h * 0.975 + fl * 0.4; p[4][1] = zb + 0.20;                // kivrimin ici (golge cizgisi)
        p[5][0] = h + fl;             p[5][1] = std::min(zb + 0.38, zl - 0.20);   // en genis: camurluk / kapi gobegi
        p[6][0] = h * 0.998 + fl * 0.8; p[6][1] = zl - 0.13;                // karakter cizgisi (kapi kolu hizasi)
        p[7][0] = h * 0.990 + fl * 0.4; p[7][1] = zl - 0.04;                // omuz
        if (cabin(s)) {                                                     // cam alti, cam ustu (tumblehome), tavan kenari
            const double z8 = zl + 0.045, z9 = std::max(zt - 0.06, z8 + 0.006);
            const double k = std::clamp((zt - zl) / (1.37 - 0.86), 0.0, 1.0);
            p[8][0] = h * 0.965;               p[8][1] = zl;
            p[9][0] = h * 0.935;               p[9][1] = z8;
            p[10][0] = h * (0.935 - 0.17 * k); p[10][1] = z9;
            p[11][0] = 0;                      p[11][1] = zt + 0.006;
            // tavan kenari yuvarlatma: ust ortaya giden son noktayi biraz iceri ve yukari (yuvarlak tavan)
            p[10][0] = std::max(p[10][0] - 0.0, 0.0);
        } else {                                                            // kaput / bagaj: omuz, kenar, kubbe
            p[8][0] = h * 0.975;  p[8][1] = std::max(zl, zt - 0.06);
            p[9][0] = h * 0.950;  p[9][1] = std::max(zl + 0.012, zt - 0.025);
            p[10][0] = h * 0.55;  p[10][1] = zt + dome * 0.6;
            p[11][0] = 0;         p[11][1] = zt + dome;
        }
    };
    // Istasyonlar: uclarda ve cam gecislerinde sik, aks merkezleri dahil
    std::vector<double> ss = {0.0, 0.006, 0.015, 0.03, 0.05, 0.08, 0.11, 0.14, 0.17, 0.20, 0.24, cowl, 0.32, 0.36, 0.40, roofF, 0.49, 0.54, 0.59,
                              0.64, 0.68, roofR, 0.75, sideGlass1, 0.81, 0.85, deck, 0.92, 0.945, 0.965, 0.985, 0.995, 1.0,
                              sF - 0.08, sF - 0.04, sF, sF + 0.04, sF + 0.08, sR - 0.08, sR - 0.04, sR, sR + 0.04, sR + 0.08, sideGlass0};
    std::sort(ss.begin(), ss.end());
    ss.erase(std::unique(ss.begin(), ss.end(), [](double a, double b) { return std::fabs(a - b) < 0.004; }), ss.end());
    // Halkalar: dongu sirasi L0..L11, R10..R1 (22 nokta)
    std::vector<std::array<int, NR>> rings;
    for (double s : ss) {
        double p[NP][2]; ring(s, p);
        std::array<int, NR> R{};
        for (int k = 0; k < NP; ++k) R[k] = B.v(X(s), p[k][0], p[k][1]);
        for (int k = NP - 2; k >= 1; --k) R[NR - k] = B.v(X(s), -p[k][0], p[k][1]);
        rings.push_back(R);
    }
    auto segMat = [&](int j, double s) {                                  // j: sol yari segment (0 alt .. 10 ust)
        if (j == 0) return (int)MatDark;
        if (j == 3 && s > sF + 0.07 && s < sR - 0.07) return (int)MatDark;   // esik kivriminin golgesi (tekerler arasi)
        if (j == 9 && s > sideGlass0 && s < sideGlass1) return (int)MatGlass;          // yan camlar (cercevesiz kupe)
        if ((j == 10) && ((s > cowl && s < roofF) || (s > roofR && s < deck))) return (int)MatGlass;   // on / arka cam
        return (int)MatPaint;
    };
    for (size_t i = 0; i + 1 < rings.size(); ++i) {
        const double sm = 0.5 * (ss[i] + ss[i + 1]);
        for (int k = 0; k < NR; ++k) {
            const int k2 = (k + 1) % NR, j = k < NP - 1 ? k : NR - 1 - k;
            B.quad(rings[i][k], rings[i + 1][k], rings[i + 1][k2], rings[i][k2], segMat(j, sm));
        }
    }
    for (int end = 0; end < 2; ++end) {                                   // on / arka kapak (yelpaze)
        const auto& R = end ? rings.back() : rings.front();
        const double s = end ? 1.0 : 0.0;
        const int c = B.v(X(s) + (end ? -0.004 : 0.004), 0, 0.5 * (bot(s) + top(s)));
        for (int k = 0; k < NR; ++k) B.tri(c, R[k], R[(k + 1) % NR], MatPaint);
    }
    const double xF = X(0.0), xB = X(1.0);
    // ---- on yuz (referans: Wikimedia Commons "BMW M3 CSL (E46) front", CC0; oranlar olculdu)
    // Yuzey uzerinde x: on kapak duz (|y| <= hw(0)), kose disinda govde kavsine (s) cekilir -> far / sinyal koseye sarilir
    auto frontX = [&](double y) {
        const double ay = std::fabs(y);
        if (ay <= hw(0.0) * 0.985) return xF + 0.006;
        double sl = 0.0;
        for (double t = 0.0; t < 0.12; t += 0.002) { sl = t; if (hw(t) >= ay) break; }
        return X(sl) + 0.004;
    };
    auto quadF = [&](double ya, double za0, double za1, double yb, double zb0, double zb1, double dx, int mat) {   // on yuzde y-z dortgeni
        const double xa = frontX(ya) + dx, xb = frontX(yb) + dx;
        B.panel(xa, ya, za0, xb, yb, zb0, xb, yb, zb1, xa, ya, za1, mat);
    };
    // Far lensi dis hatti (|y| / HW, alt z, ust z): ic uc uzun (bobrek yaninda), disa dogru daralan damla, koseye sarilir
    static const double kHL[][3] = {{0.205, 0.545, 0.690}, {0.30, 0.543, 0.688}, {0.40, 0.546, 0.682}, {0.50, 0.553, 0.672},
                                    {0.62, 0.566, 0.660}, {0.74, 0.585, 0.648}, {0.85, 0.606, 0.637}, {0.94, 0.622, 0.630}, {0.985, 0.628, 0.629}};
    constexpr int nHL = sizeof kHL / sizeof *kHL;
    for (double sg : {1.0, -1.0}) {
        // kaput kapanma cizgisi: farlarin ve bobregin ustunden (ince koyu)
        for (int k = 0; k + 1 < nHL; ++k)
            quadF(sg * kHL[k][0] * HW, kHL[k][2] + 0.004, kHL[k][2] + 0.012, sg * kHL[k + 1][0] * HW, kHL[k + 1][2] + 0.004, kHL[k + 1][2] + 0.012, 0.002, MatDark);
        // lens: koyu cerceve (cizgi) + gumus yansitici ic
        for (int k = 0; k + 1 < nHL; ++k) {
            quadF(sg * kHL[k][0] * HW, kHL[k][1] - 0.006, kHL[k][2] + 0.003, sg * kHL[k + 1][0] * HW, kHL[k + 1][1] - 0.006, kHL[k + 1][2] + 0.003, 0.001, MatDark);
            quadF(sg * kHL[k][0] * HW, kHL[k][1], kHL[k][2], sg * kHL[k + 1][0] * HW, kHL[k + 1][1], kHL[k + 1][2], 0.003, MatChrome);
        }
        // iki yuvarlak far: ic kucuk (kisa huzme), dis buyuk (uzun) - krom halka + lamba
        const double yi = sg * 0.30 * HW, yo = sg * 0.50 * HW;
        B.diskX(frontX(yi) + 0.006, yi, 0.618, 0.050, 0.050, 16, MatTrim);
        B.diskX(frontX(yi) + 0.008, yi, 0.618, 0.044, 0.044, 16, MatLight);
        B.diskX(frontX(yi) + 0.010, yi, 0.618, 0.019, 0.019, 10, MatChrome);
        B.diskX(frontX(yo) + 0.006, yo, 0.613, 0.058, 0.054, 16, MatTrim);
        B.diskX(frontX(yo) + 0.008, yo, 0.613, 0.051, 0.047, 16, MatLight);
        B.diskX(frontX(yo) + 0.010, yo, 0.610, 0.024, 0.022, 10, MatChrome);
        quadF(sg * 0.86 * HW, 0.592, 0.620, sg * 0.95 * HW, 0.605, 0.620, 0.005, MatIndicator);   // lens icinde sinyal (dis uc)
        // Bobrek: kaputun on kenarina gomulu, yaklasik kare, kromlu cerceve + 9 dikey cita
        const double kc = sg * 0.100, kw = 0.084, kz0 = 0.548, kz1 = 0.688;
        quadF(kc - kw, kz0, kz1, kc + kw, kz0 + 0.004, kz1 - 0.004, 0.002, MatChrome);
        quadF(kc - kw + 0.010, kz0 + 0.010, kz1 - 0.010, kc + kw - 0.010, kz0 + 0.013, kz1 - 0.013, 0.005, MatDark);
        for (int k = -4; k <= 4; ++k) {
            const double yc = kc + k * 0.0145;
            quadF(yc - 0.0025, kz0 + 0.012, kz1 - 0.012, yc + 0.0025, kz0 + 0.012, kz1 - 0.012, 0.008, MatTrim);
        }
        // Tampon: yuvarlak sis fari yuvasi (koyu cep + lamba), yan kucuk giris
        const double yf = sg * 0.66 * HW;
        B.diskX(frontX(yf) + 0.004, yf, 0.300, 0.058, 0.050, 16, MatDark);
        B.diskX(frontX(yf) + 0.007, yf, 0.300, 0.038, 0.034, 14, MatLight);
        quadF(sg * 0.40 * HW, 0.230, 0.290, sg * 0.54 * HW, 0.235, 0.285, 0.004, MatDark);
    }
    // Tampon ortasi: plaka (ust) + alt genis hava girisi + ince citalar; alt dudak
    B.panel(xF + 0.010, 0.25, 0.475, xF + 0.010, 0.25, 0.385, xF + 0.010, -0.25, 0.385, xF + 0.010, -0.25, 0.475, MatPlate);
    B.panel(xF + 0.010, 0.36, 0.345, xF + 0.010, 0.34, 0.225, xF + 0.010, -0.34, 0.225, xF + 0.010, -0.36, 0.345, MatDark);
    for (int k = -1; k <= 1; ++k) B.panel(xF + 0.013, k * 0.12 - 0.004, 0.23, xF + 0.013, k * 0.12 + 0.004, 0.23, xF + 0.013, k * 0.12 + 0.004, 0.34, xF + 0.013, k * 0.12 - 0.004, 0.34, MatTrim);
    B.panel(xF + 0.02, 0.80, 0.21, xF - 0.05, 0.80, 0.21, xF - 0.05, -0.80, 0.21, xF + 0.02, -0.80, 0.21, MatDark);   // alt dudak
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
        for (double sd : {0.31, 0.595}) {                                // kapi araliklari
            const double y = sg * (hw(sd) + 0.003);
            B.panel(X(sd), y, 0.30, X(sd), y, belt(sd), X(sd) - 0.008, y, belt(sd), X(sd) - 0.008, y, 0.30, MatDark);
        }
        {   // yan cam alti krom bel seridi + cam ust kenari siyah conta (cercevesiz kupe)
            const int nS = 14;
            for (int k = 0; k < nS; ++k) {
                const double sa = sideGlass0 + (sideGlass1 - sideGlass0) * k / nS, sb = sideGlass0 + (sideGlass1 - sideGlass0) * (k + 1) / nS;
                const double za = belt(sa) + 0.045, zb = belt(sb) + 0.045;
                const double ya = sg * (hw(sa) * 0.935 + 0.004), yb = sg * (hw(sb) * 0.935 + 0.004);
                B.panel(X(sa), ya, za - 0.004, X(sb), yb, zb - 0.004, X(sb), yb, zb + 0.018, X(sa), ya, za + 0.018, MatChrome);
                const double ta = std::max(top(sa) - 0.035, za + 0.006), tb = std::max(top(sb) - 0.035, zb + 0.006);
                const double kA = std::clamp((top(sa) - belt(sa)) / (1.37 - 0.86), 0.0, 1.0), kB = std::clamp((top(sb) - belt(sb)) / (1.37 - 0.86), 0.0, 1.0);
                const double ua = sg * (hw(sa) * (0.935 - 0.17 * kA) + 0.004), ub = sg * (hw(sb) * (0.935 - 0.17 * kB) + 0.004);
                B.panel(X(sa), ua, ta - 0.022, X(sb), ub, tb - 0.022, X(sb), ub, tb, X(sa), ua, ta, MatDark);
            }
        }
        {   // camurluk arkasinda yan havalandirma (izgara)
            const double y = sg * (hw(0.25) + flare(0.25) * 0.6 + 0.004);
            B.panel(X(0.235), y, 0.70, X(0.235), y, 0.64, X(0.275), y, 0.64, X(0.275), y, 0.70, MatDark);
            B.panel(X(0.236), y * 1.001, 0.676, X(0.236), y * 1.001, 0.664, X(0.274), y * 1.001, 0.664, X(0.274), y * 1.001, 0.676, MatChrome);
        }
        {   // ayna (A direginin dibi)
            const double s0 = 0.315, s1 = 0.345, y0 = hw(0.33) * 0.95, y1 = y0 + 0.13, z0 = belt(0.33) + 0.04;
            B.box(X(s1), X(s0), sg > 0 ? y0 : -y1, sg > 0 ? y1 : -y0, z0, z0 + 0.085, MatPaint);
            B.panel(X(s1) - 0.003, sg * (y0 + 0.02), z0 + 0.01, X(s1) - 0.003, sg * (y1 - 0.01), z0 + 0.01,
                    X(s1) - 0.003, sg * (y1 - 0.01), z0 + 0.075, X(s1) - 0.003, sg * (y0 + 0.02), z0 + 0.075, MatGlass);
        }
        for (double xa : {xFront, xRear}) {                               // teker yuvasi (koyu yarim disk, camurluk dudagi)
            const double sa = (L * 0.5 - xa) / L;
            B.archY(xa, sg * (hw(sa) + flare(sa) - 0.30), r - 0.01, 0.0, r * 1.13, 10, MatDark);   // yuva tekerin arkasinda
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

// ---------------------------------------------------------------- trafik: tir (cekici + dorse) / kamyon (kasali)
static LowPolyMesh buildTruck(const VehicleDef& v) {
    LowPolyMesh m; m.paintRGB = v.paintRGB;
    Builder B{m};
    const bool semi = v.ref == "TRUCK SEMI";
    const double L = v.lengthM, HW = v.widthM * 0.5, xF = L * 0.5, r = 0.50, tw = 0.32;
    const double cabL = 2.35, cabZ0 = 0.75, cabZ1 = 3.25;
    // kabin: govde + on cam + izgara + farlar + tampon + gunes siperligi + yan aynalar
    B.box(xF - cabL, xF, -HW + 0.05, HW - 0.05, cabZ0, cabZ1, MatPaint);
    B.box(xF - cabL + 0.25, xF - 0.15, -HW + 0.12, HW - 0.12, cabZ1, cabZ1 + 0.35, MatPaint);   // yuksek tavan
    B.panel(xF + 0.01, -HW + 0.18, 1.85, xF + 0.01, HW - 0.18, 1.85, xF + 0.01, HW - 0.18, 2.95, xF + 0.01, -HW + 0.18, 2.95, MatGlass);
    B.panel(xF + 0.01, -HW + 0.35, 0.95, xF + 0.01, HW - 0.35, 0.95, xF + 0.01, HW - 0.35, 1.70, xF + 0.01, -HW + 0.35, 1.70, MatDark);
    for (double sg : {1.0, -1.0}) {
        B.box(xF - 0.02, xF + 0.03, sg > 0 ? HW - 0.40 : -HW + 0.08, sg > 0 ? HW - 0.08 : -HW + 0.40, 0.80, 1.00, MatLight);
        B.box(xF - cabL + 0.6, xF - 0.4, sg > 0 ? HW - 0.04 : -HW - 0.01, sg > 0 ? HW + 0.01 : -HW + 0.04, 1.95, 2.95, MatGlass);   // yan cam
        B.box(xF - 0.25, xF - 0.15, sg * (HW + 0.05) - 0.05, sg * (HW + 0.05) + 0.05, 1.9, 2.6, MatDark);   // ayna
    }
    B.box(xF - 0.15, xF + 0.08, -HW, HW, 0.45, 0.78, MatDark);                     // tampon
    B.box(xF - cabL - 0.1, xF - 0.3, -HW + 0.25, HW - 0.25, 0.55, 0.80, MatDark);  // sasi
    std::vector<double> axles;
    const double xA0 = xF - 1.3;                                                 // on dingil
    if (semi) {
        const double tx0 = xF - cabL - 0.35, tx1 = -L * 0.5;                    // dorse (beyaz tente kasa)
        B.box(tx1, tx0, -HW, HW, 1.25, 4.0, MatPaint);
        B.box(tx1, tx0 + 0.4, -HW + 0.3, HW - 0.3, 0.95, 1.25, MatDark);         // dorse sasisi
        B.box(tx1 - 0.02, tx1, -HW + 0.1, -HW + 0.4, 1.0, 1.25, MatTail);
        B.box(tx1 - 0.02, tx1, HW - 0.4, HW - 0.1, 1.0, 1.25, MatTail);
        B.box(tx0, xF - cabL, -HW + 0.4, HW - 0.4, 0.8, 1.0, MatDark);           // besinci teker
        axles = {xA0, xA0 - v.wheelbaseM, tx1 + 1.3, tx1 + 2.65, tx1 + 4.0};
    } else {
        const double bx0 = xF - cabL - 0.15, bx1 = -L * 0.5;                    // kasa
        B.box(bx1, bx0, -HW, HW, 1.10, 3.6, MatPlate);
        B.box(bx1 - 0.02, bx1, -HW + 0.1, -HW + 0.4, 0.95, 1.15, MatTail);
        B.box(bx1 - 0.02, bx1, HW - 0.4, HW - 0.1, 0.95, 1.15, MatTail);
        B.box(bx1 + 0.3, bx0, -HW + 0.3, HW - 0.3, 0.75, 1.10, MatDark);
        axles = {xA0, xA0 - v.wheelbaseM, xA0 - v.wheelbaseM - 1.35};
    }
    // tekerler: ilk iki dingil donen tekerler (sira: on sol, on sag, arka sol, arka sag); digerleri sabit
    for (size_t i = 0; i < axles.size(); ++i)
        for (double sg : {1.0, -1.0}) {
            const double cy = sg * (HW - tw * 0.5 - 0.02);
            const size_t t0 = m.tris.size();
            B.wheel(axles[i], cy, r, r, tw, W_M);
            if (i < 2) m.wheels.push_back({(float)axles[i], (float)cy, (float)r, (float)r, t0, m.tris.size()});
        }
    return m;
}

// Elle modellenmis arac varsa onu dondurur
static bool buildHandModel(const VehicleDef& v, LowPolyMesh& out) {
    if (v.ref == "BMW M3 E46") { out = buildE46(v); return true; }
    if (v.ref == "TRUCK SEMI" || v.ref == "TRUCK RIGID") { out = buildTruck(v); return true; }
    return false;
}
