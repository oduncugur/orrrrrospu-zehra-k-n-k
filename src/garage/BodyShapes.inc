// ZEHRA KINIK - Arac basina govde sekli. LowPolyModel.cpp icinde dahil edilir.
// Her arac bir siluet arketipi (gercek aracin yan profili) + far/stop/izgara/jant/kapi + jant yaricapi (m, 0 = otomatik).
// Olculer (uzunluk/genislik/yukseklik/dingil) VehicleTable.inc'teki gercek degerlerden gelir. Logo/amblem yoktur.
//
// Arketip profil alanlari (x: on tampondan oran 0..1, z: toplam yukseklige oran; tavan = 1):
//   noseZ  kaput on kenari        hoodZ  kaput, ön cam dibinde     cowlX  on cam dibi
//   roofFX on cam ustu (tavan basi) roofRX tavan sonu (arka cam ustu)
//   backX  arka cam dibi (bagaj/kuyruk basi)   deckZ  bagaj yuksekligi   tailZ  arka ust kenar
//   roofW  tavan genisligi (W orani)          beltW  bel cizgisi genisligi (tumblehome)
//   open   ustu acik (roadster): tavan yok, on cam paneli + kokpit
//   bed    kasa (pickup): backX'ten sonrasi acik kasa

enum Arch {
    HATCH_BOX, HATCH_ROUND, HATCH_MODERN, KEI_BOX, MINI_CLASSIC,
    SEDAN_BOX, SEDAN_90, SEDAN_MODERN, WAGON,
    COUPE_NOTCH, COUPE_FAST, COUPE_LIFT, MUSCLE_NOTCH, MUSCLE_FAST,
    MID_ENGINE, WEDGE, FRONT_LONG, P911, BEETLE,
    ROADSTER, ROADSTER_LONG, ROADSTER_MID,
    VAN_FLAT, SUV_BOX, SUV_SMALL, PICKUP, ARCH_COUNT
};

struct ArchSpec { double noseZ, hoodZ, cowlX, roofFX, roofRX, backX, deckZ, tailZ, roofW, beltW; bool open, bed; };

const ArchSpec kArch[ARCH_COUNT] = {
    //            nose  hood  cowl  roofF roofR back  deck  tail  roofW beltW open   bed
    /*HATCH_BOX*/   {0.56, 0.63, 0.30, 0.45, 0.86, 0.97, 0.66, 0.60, 0.80, 0.97, false, false},
    /*HATCH_ROUND*/ {0.50, 0.60, 0.28, 0.46, 0.80, 0.96, 0.62, 0.57, 0.78, 0.95, false, false},
    /*HATCH_MOD*/   {0.55, 0.64, 0.27, 0.45, 0.78, 0.95, 0.68, 0.62, 0.78, 0.94, false, false},
    /*KEI_BOX*/     {0.55, 0.60, 0.24, 0.38, 0.88, 0.98, 0.62, 0.58, 0.84, 0.98, false, false},
    /*MINI*/        {0.60, 0.64, 0.24, 0.32, 0.90, 0.98, 0.64, 0.60, 0.86, 0.99, false, false},
    /*SEDAN_BOX*/   {0.56, 0.61, 0.30, 0.42, 0.68, 0.79, 0.65, 0.63, 0.82, 0.98, false, false},
    /*SEDAN_90*/    {0.53, 0.60, 0.30, 0.44, 0.69, 0.81, 0.67, 0.65, 0.80, 0.96, false, false},
    /*SEDAN_MOD*/   {0.56, 0.64, 0.30, 0.46, 0.68, 0.84, 0.71, 0.69, 0.76, 0.94, false, false},
    /*WAGON*/       {0.54, 0.61, 0.29, 0.42, 0.91, 0.98, 0.66, 0.62, 0.82, 0.97, false, false},
    /*COUPE_NOTCH*/ {0.52, 0.59, 0.33, 0.47, 0.66, 0.81, 0.66, 0.65, 0.78, 0.96, false, false},
    /*COUPE_FAST*/  {0.48, 0.56, 0.35, 0.51, 0.62, 0.89, 0.67, 0.66, 0.74, 0.94, false, false},
    /*COUPE_LIFT*/  {0.47, 0.55, 0.38, 0.53, 0.66, 0.93, 0.64, 0.61, 0.76, 0.95, false, false},
    /*MUSCLE_N*/    {0.60, 0.63, 0.41, 0.54, 0.69, 0.81, 0.67, 0.66, 0.80, 0.98, false, false},
    /*MUSCLE_F*/    {0.60, 0.63, 0.41, 0.55, 0.64, 0.91, 0.68, 0.67, 0.78, 0.98, false, false},
    /*MID_ENGINE*/  {0.43, 0.50, 0.30, 0.45, 0.58, 0.84, 0.72, 0.70, 0.70, 0.93, false, false},
    /*WEDGE*/       {0.30, 0.46, 0.27, 0.48, 0.59, 0.82, 0.76, 0.73, 0.66, 0.92, false, false},
    /*FRONT_LONG*/  {0.48, 0.57, 0.46, 0.59, 0.68, 0.90, 0.64, 0.62, 0.72, 0.93, false, false},
    /*P911*/        {0.42, 0.52, 0.33, 0.46, 0.60, 0.95, 0.64, 0.60, 0.74, 0.93, false, false},
    /*BEETLE*/      {0.44, 0.62, 0.34, 0.46, 0.62, 0.95, 0.52, 0.40, 0.70, 0.90, false, false},
    /*ROADSTER*/    {0.56, 0.68, 0.42, 0.49, 0.49, 0.67, 0.71, 0.69, 0.80, 0.96, true,  false},
    /*ROADSTER_L*/  {0.50, 0.64, 0.50, 0.56, 0.56, 0.74, 0.70, 0.68, 0.80, 0.95, true,  false},
    /*ROADSTER_M*/  {0.48, 0.60, 0.33, 0.40, 0.40, 0.62, 0.74, 0.72, 0.80, 0.95, true,  false},
    /*VAN_FLAT*/    {0.48, 0.52, 0.03, 0.08, 0.96, 0.99, 0.64, 0.60, 0.88, 0.98, false, false},
    /*SUV_BOX*/     {0.63, 0.67, 0.27, 0.36, 0.95, 0.99, 0.66, 0.63, 0.86, 0.98, false, false},
    /*SUV_SMALL*/   {0.63, 0.67, 0.30, 0.35, 0.93, 0.99, 0.66, 0.63, 0.88, 0.99, false, false},
    /*PICKUP*/      {0.64, 0.68, 0.28, 0.37, 0.51, 0.53, 0.66, 0.64, 0.86, 0.98, false, true},
};

// Far: R dikdortgen, O yuvarlak, Q cift yuvarlak (yan basina 2), P acilir (kapali), S ince modern, V kapakli oval,
//      F camurlukta yuvarlak (911 / Beetle)
enum Lamp { L_R, L_O, L_Q, L_P, L_S, L_V, L_F };
// Stop: TR dikdortgen, TO cift yuvarlak (yan basina 2), TB tam genislik serit, TV dikey, TS ince, T1 tek yuvarlak
enum Tail { T_R, T_O, T_B, T_V, T_S, T_1 };
// Izgara: G0 yok, G1 ince, G2 genis, G3 buyuk/uzun (kamyonet, kas), G4 cift bobrek, G5 kalkan, G6 buyuk trapez
enum Grille { G_0, G_1, G_2, G_3, G_4, G_5, G_6 };
// Jant: W0 celik/canak, W5 bes kollu, WM cok kollu / ag, WT turbofan / disk
enum Rim { W_0, W_5, W_M, W_T };

struct CarShape { int id; Arch arch; Lamp lamp; Tail tail; Grille grille; Rim rim; int doors; double wheelR; };

#define S(id, a, l, t, g, w, d, r) {id, a, L_##l, T_##t, G_##g, W_##w, d, r}
const CarShape kShapes[] = {
    // Kaze (Honda)
    S(1, HATCH_BOX, R, R, 1, 5, 2, 0.29),   S(2, HATCH_ROUND, R, R, 1, 5, 2, 0.29),  S(3, HATCH_ROUND, R, R, 1, T, 2, 0.29),
    S(4, HATCH_ROUND, R, R, 1, 5, 2, 0.29), S(5, HATCH_ROUND, R, R, 1, 5, 2, 0.30),  S(6, HATCH_ROUND, R, R, 1, T, 2, 0.29),
    S(7, HATCH_MODERN, S, V, 1, 5, 2, 0.31), S(8, SEDAN_MODERN, S, S, 1, 5, 4, 0.33), S(9, COUPE_NOTCH, S, S, 1, 5, 2, 0.32),
    S(10, HATCH_MODERN, S, S, 2, M, 4, 0.34), S(11, COUPE_LIFT, R, B, 1, 5, 2, 0.28), S(12, COUPE_LIFT, Q, R, 1, 5, 2, 0.30),
    S(13, COUPE_LIFT, S, R, 1, 5, 2, 0.31), S(14, SEDAN_MODERN, S, R, 1, 5, 4, 0.32), S(15, SEDAN_MODERN, S, R, 2, 5, 4, 0.32),
    S(16, COUPE_NOTCH, R, R, 1, 5, 2, 0.31), S(17, ROADSTER, S, R, 1, 5, 2, 0.31),   S(18, ROADSTER, S, R, 1, 5, 2, 0.32),
    S(19, MID_ENGINE, P, B, 0, M, 2, 0.31), S(20, MID_ENGINE, S, B, 0, M, 2, 0.32),  S(21, ROADSTER_MID, R, R, 0, 0, 2, 0.27),
    S(22, KEI_BOX, R, R, 1, 0, 2, 0.27),    S(23, HATCH_ROUND, R, R, 1, M, 2, 0.30),
    // Toyota / Lexus
    S(24, COUPE_LIFT, P, R, 1, M, 2, 0.28), S(25, COUPE_NOTCH, R, R, 1, 5, 2, 0.29), S(26, COUPE_NOTCH, R, R, 1, 5, 2, 0.29),
    S(27, HATCH_BOX, R, R, 1, 5, 2, 0.29),  S(28, COUPE_LIFT, Q, B, 1, 5, 2, 0.30),  S(29, COUPE_LIFT, Q, R, 1, 5, 2, 0.31),
    S(30, COUPE_NOTCH, R, R, 2, 0, 2, 0.28), S(31, MID_ENGINE, P, B, 0, 5, 2, 0.31), S(32, MID_ENGINE, P, B, 0, 5, 2, 0.29),
    S(33, COUPE_LIFT, P, B, 1, 5, 2, 0.31), S(34, COUPE_FAST, R, O, 1, 5, 2, 0.32),  S(35, COUPE_FAST, R, O, 1, 5, 2, 0.32),
    S(36, COUPE_FAST, S, S, 2, 5, 2, 0.34), S(37, SEDAN_90, R, R, 2, 5, 4, 0.32),    S(38, SEDAN_90, R, R, 2, 5, 4, 0.31),
    S(39, COUPE_NOTCH, R, R, 1, 5, 2, 0.31), S(40, SEDAN_90, Q, R, 2, 5, 4, 0.32),   S(41, COUPE_FAST, S, S, 2, 5, 2, 0.31),
    S(42, HATCH_MODERN, S, B, 3, 5, 2, 0.32), S(43, HATCH_MODERN, S, S, 3, 5, 4, 0.33), S(44, HATCH_ROUND, R, V, 1, T, 4, 0.29),
    S(45, HATCH_ROUND, R, R, 1, 5, 2, 0.28), S(46, FRONT_LONG, S, S, 2, 5, 2, 0.34), S(47, SUV_BOX, R, R, 2, 0, 4, 0.39),
    S(48, PICKUP, R, R, 2, 0, 2, 0.36),     S(49, FRONT_LONG, P, 1, 2, M, 2, 0.30),
    // Nissan / Datsun
    S(50, COUPE_NOTCH, R, O, 1, M, 2, 0.31), S(51, COUPE_NOTCH, R, O, 1, 5, 2, 0.32), S(52, COUPE_NOTCH, R, O, 1, 5, 2, 0.32),
    S(53, SEDAN_90, R, O, 1, 5, 4, 0.32),   S(54, COUPE_NOTCH, R, O, 1, M, 2, 0.30), S(55, COUPE_FAST, S, O, 6, 5, 2, 0.35),
    S(56, COUPE_NOTCH, R, R, 1, 5, 2, 0.30), S(57, COUPE_LIFT, P, B, 0, 5, 2, 0.30), S(58, COUPE_NOTCH, R, R, 1, 5, 2, 0.31),
    S(59, COUPE_NOTCH, S, R, 1, 5, 2, 0.32), S(60, COUPE_NOTCH, R, R, 1, 5, 2, 0.31), S(61, COUPE_LIFT, S, B, 1, 5, 2, 0.31),
    S(62, COUPE_FAST, S, V, 2, 5, 2, 0.33), S(63, COUPE_FAST, S, V, 2, 5, 2, 0.34),  S(64, COUPE_LIFT, O, R, 1, 0, 2, 0.28),
    S(65, COUPE_LIFT, O, R, 1, 0, 2, 0.29), S(66, SEDAN_BOX, O, R, 2, 0, 2, 0.28),   S(67, HATCH_ROUND, R, R, 1, 5, 2, 0.29),
    S(68, HATCH_ROUND, R, R, 1, 5, 2, 0.29), S(69, SEDAN_BOX, R, R, 2, 0, 2, 0.27),  S(70, SUV_BOX, R, R, 3, 0, 4, 0.40),
    S(71, SEDAN_BOX, R, R, 1, 5, 4, 0.30),
    // Mazda
    S(72, ROADSTER, P, R, 0, M, 2, 0.28),   S(73, ROADSTER, P, R, 0, M, 2, 0.29),    S(74, ROADSTER, V, R, 1, 5, 2, 0.29),
    S(75, ROADSTER, S, O, 2, 5, 2, 0.31),   S(76, COUPE_LIFT, P, R, 0, 5, 2, 0.28),  S(77, COUPE_LIFT, P, B, 0, T, 2, 0.30),
    S(78, COUPE_FAST, P, O, 0, 5, 2, 0.31), S(79, COUPE_FAST, S, O, 1, 5, 2, 0.33),  S(80, COUPE_NOTCH, Q, O, 2, 0, 2, 0.28),
    S(81, COUPE_NOTCH, S, B, 1, 5, 2, 0.31), S(82, HATCH_MODERN, S, R, 2, 5, 4, 0.33), S(83, HATCH_BOX, R, R, 1, 5, 2, 0.29),
    S(84, WEDGE, R, R, 0, T, 2, 0.33),
    // Subaru
    S(85, SEDAN_90, R, R, 1, 5, 4, 0.30),   S(86, SEDAN_90, R, R, 1, 5, 4, 0.31),    S(87, COUPE_NOTCH, R, R, 1, 5, 2, 0.32),
    S(88, SEDAN_90, O, O, 1, 5, 4, 0.32),   S(89, HATCH_MODERN, S, R, 2, 5, 4, 0.33), S(90, SEDAN_MODERN, S, R, 3, 5, 4, 0.34),
    S(91, SEDAN_MODERN, S, R, 3, 5, 4, 0.33), S(92, COUPE_NOTCH, R, R, 1, 5, 2, 0.30), S(93, COUPE_FAST, S, S, 2, 5, 2, 0.31),
    S(94, SEDAN_90, R, R, 1, 5, 4, 0.31),   S(95, SEDAN_MODERN, S, R, 2, 5, 4, 0.32), S(96, COUPE_FAST, S, B, 1, 5, 2, 0.31),
    S(97, WAGON, S, V, 2, 5, 4, 0.35),
    // Mitsubishi
    S(98, SEDAN_90, R, R, 2, 5, 4, 0.30),   S(99, SEDAN_90, R, R, 2, 5, 4, 0.31),    S(100, SEDAN_90, R, R, 2, 5, 4, 0.31),
    S(101, SEDAN_90, R, R, 3, 5, 4, 0.32),  S(102, SEDAN_90, R, R, 3, 5, 4, 0.32),   S(103, SEDAN_MODERN, S, S, 6, 5, 4, 0.33),
    S(104, SEDAN_90, R, R, 2, 5, 4, 0.30),  S(105, COUPE_FAST, S, B, 1, 5, 2, 0.32), S(106, HATCH_ROUND, R, R, 1, 5, 2, 0.29),
    S(107, COUPE_NOTCH, S, B, 1, 5, 2, 0.30), S(108, COUPE_LIFT, P, R, 1, 5, 2, 0.30), S(109, SUV_BOX, R, R, 3, 5, 2, 0.39),
    // Suzuki / Daihatsu
    S(110, ROADSTER, O, R, 0, 5, 2, 0.26),  S(111, HATCH_MODERN, S, V, 2, 5, 4, 0.30), S(112, KEI_BOX, R, R, 1, 5, 2, 0.26),
    S(113, HATCH_BOX, R, R, 1, 0, 2, 0.27), S(114, SUV_SMALL, O, R, 2, 0, 2, 0.35),  S(115, ROADSTER, O, O, 1, 5, 2, 0.27),
    S(116, HATCH_BOX, R, R, 1, 5, 2, 0.28),
    // BMW / Mini
    S(117, SEDAN_BOX, O, 1, 4, 0, 2, 0.29), S(118, COUPE_NOTCH, Q, R, 4, M, 2, 0.30), S(119, SEDAN_BOX, Q, R, 4, M, 4, 0.30),
    S(120, COUPE_NOTCH, Q, R, 4, 5, 2, 0.32), S(121, SEDAN_90, Q, R, 4, 5, 4, 0.31), S(122, COUPE_NOTCH, S, R, 4, 5, 2, 0.33),
    S(123, COUPE_NOTCH, S, R, 4, 5, 2, 0.33), S(124, SEDAN_90, S, R, 4, 5, 4, 0.33), S(125, SEDAN_MODERN, S, S, 4, 5, 4, 0.34),
    S(126, COUPE_NOTCH, S, S, 4, 5, 2, 0.34), S(127, SEDAN_MODERN, S, S, 4, 5, 4, 0.34), S(128, COUPE_NOTCH, S, R, 4, 5, 2, 0.33),
    S(129, COUPE_NOTCH, S, S, 4, 5, 2, 0.34), S(130, MID_ENGINE, P, R, 4, 5, 2, 0.31), S(131, ROADSTER, S, S, 4, 5, 2, 0.33),
    S(132, COUPE_NOTCH, Q, R, 4, 0, 2, 0.30), S(133, MINI_CLASSIC, O, R, 2, 0, 2, 0.24), S(134, MINI_CLASSIC, V, V, 2, 5, 2, 0.30),
    // Mercedes
    S(135, SEDAN_BOX, R, R, 2, T, 4, 0.30), S(136, SEDAN_BOX, R, R, 2, T, 4, 0.31),  S(137, SEDAN_MODERN, Q, R, 2, 5, 4, 0.33),
    S(138, SEDAN_MODERN, S, R, 2, 5, 4, 0.34), S(139, FRONT_LONG, S, S, 2, 5, 2, 0.35), S(140, FRONT_LONG, S, S, 3, 5, 2, 0.35),
    S(141, FRONT_LONG, O, R, 2, 0, 2, 0.31),
    // Audi
    S(142, COUPE_NOTCH, R, R, 2, T, 2, 0.30), S(143, COUPE_NOTCH, R, R, 2, T, 2, 0.30), S(144, WAGON, R, B, 2, T, 4, 0.32),
    S(145, SEDAN_90, R, R, 2, 5, 4, 0.32),  S(146, SEDAN_MODERN, S, R, 6, 5, 4, 0.34), S(147, MID_ENGINE, S, S, 6, 5, 2, 0.34),
    S(148, MID_ENGINE, S, S, 6, 5, 2, 0.34), S(149, COUPE_FAST, S, S, 6, 5, 2, 0.34), S(150, SEDAN_MODERN, S, S, 6, 5, 4, 0.33),
    // Volkswagen
    S(151, HATCH_BOX, O, R, 2, M, 2, 0.28), S(152, HATCH_BOX, O, R, 2, 5, 2, 0.29),  S(153, HATCH_BOX, Q, R, 2, 5, 2, 0.29),
    S(154, COUPE_LIFT, R, R, 1, 5, 2, 0.31), S(155, HATCH_ROUND, R, R, 1, 5, 2, 0.32), S(156, HATCH_ROUND, R, R, 1, 5, 2, 0.31),
    S(157, HATCH_MODERN, S, R, 1, M, 4, 0.33), S(158, HATCH_MODERN, S, R, 1, 5, 4, 0.34), S(159, COUPE_LIFT, Q, R, 1, 0, 2, 0.28),
    S(160, BEETLE, F, R, 0, 0, 2, 0.31),    S(161, VAN_FLAT, O, R, 0, 0, 2, 0.32),   S(162, HATCH_BOX, R, R, 1, 5, 2, 0.28),
    // Porsche
    S(163, P911, F, 1, 0, 0, 2, 0.30),      S(164, P911, F, R, 0, 5, 2, 0.30),       S(165, P911, F, B, 0, 5, 2, 0.31),
    S(166, P911, F, B, 0, 5, 2, 0.32),      S(167, P911, F, B, 0, M, 2, 0.32),       S(168, P911, F, B, 0, 5, 2, 0.32),
    S(169, P911, F, S, 0, 5, 2, 0.33),      S(170, P911, F, S, 0, 5, 2, 0.33),       S(171, P911, F, S, 0, M, 2, 0.34),
    S(172, P911, F, S, 0, 5, 2, 0.34),      S(173, P911, F, B, 0, 5, 2, 0.35),       S(174, COUPE_LIFT, P, B, 0, T, 2, 0.31),
    S(175, COUPE_LIFT, F, B, 0, T, 2, 0.32), S(176, ROADSTER_MID, F, R, 0, 5, 2, 0.32), S(177, MID_ENGINE, S, S, 0, 5, 2, 0.34),
    S(178, P911, F, B, 0, T, 2, 0.32),      S(179, P911, F, S, 0, M, 2, 0.33),
    // Ferrari
    S(180, MID_ENGINE, O, O, 1, 0, 2, 0.29), S(181, MID_ENGINE, P, O, 1, 5, 2, 0.30), S(182, MID_ENGINE, P, R, 1, 5, 2, 0.31),
    S(183, MID_ENGINE, P, O, 1, 5, 2, 0.31), S(184, WEDGE, P, O, 1, 5, 2, 0.33),     S(185, WEDGE, P, B, 1, 5, 2, 0.32),
    S(186, MID_ENGINE, P, O, 1, 5, 2, 0.32), S(187, MID_ENGINE, S, O, 1, 5, 2, 0.33), S(188, MID_ENGINE, S, O, 1, 5, 2, 0.34),
    S(189, MID_ENGINE, S, O, 1, 5, 2, 0.35), S(190, FRONT_LONG, S, O, 2, 5, 2, 0.33), S(191, WEDGE, S, O, 1, 5, 2, 0.35),
    S(192, FRONT_LONG, S, O, 2, 5, 2, 0.36), S(193, WEDGE, S, O, 1, 5, 2, 0.34),
    // Lamborghini
    S(194, WEDGE, O, R, 1, M, 2, 0.31),     S(195, WEDGE, P, R, 0, T, 2, 0.33),      S(196, WEDGE, P, R, 0, 5, 2, 0.34),
    S(197, WEDGE, S, S, 0, 5, 2, 0.35),     S(198, MID_ENGINE, S, S, 0, 5, 2, 0.34), S(199, MID_ENGINE, S, S, 0, 5, 2, 0.35),
    S(200, WEDGE, S, S, 0, 5, 2, 0.36),
    // Alfa Romeo / Fiat / Lancia
    S(201, COUPE_NOTCH, O, R, 5, 0, 2, 0.28), S(202, ROADSTER, V, R, 5, 0, 2, 0.28), S(203, SEDAN_90, S, R, 5, 5, 4, 0.31),
    S(204, HATCH_ROUND, S, R, 5, 5, 2, 0.32), S(205, SEDAN_MODERN, S, S, 5, 5, 4, 0.34), S(206, COUPE_NOTCH, R, R, 5, 5, 2, 0.30),
    S(207, FRONT_LONG, O, O, 5, 5, 2, 0.34), S(208, ROADSTER, O, R, 2, 0, 2, 0.28),  S(209, SEDAN_BOX, Q, R, 2, 5, 2, 0.29),
    S(210, HATCH_BOX, R, R, 1, 5, 2, 0.27), S(211, HATCH_ROUND, R, V, 1, 5, 2, 0.29), S(212, COUPE_NOTCH, V, O, 1, 5, 2, 0.31),
    S(213, HATCH_BOX, Q, R, 1, 5, 4, 0.30), S(214, MID_ENGINE, P, O, 0, 5, 2, 0.30),      S(215, MID_ENGINE, R, O, 0, 5, 2, 0.31),
    // Tofas / Anadol
    S(216, SEDAN_BOX, Q, R, 2, 0, 4, 0.28), S(217, SEDAN_BOX, R, R, 2, 0, 4, 0.28),  S(218, SEDAN_BOX, R, R, 2, 5, 4, 0.29),
    S(219, WAGON, R, R, 2, 0, 4, 0.28),     S(220, SEDAN_BOX, O, R, 2, 0, 2, 0.28),  S(221, COUPE_LIFT, O, R, 2, 5, 2, 0.29),
    // Ford
    S(222, MUSCLE_FAST, Q, V, 2, 5, 2, 0.32), S(223, MUSCLE_FAST, Q, V, 2, 5, 2, 0.32), S(224, MUSCLE_FAST, Q, B, 2, 5, 2, 0.32),
    S(225, COUPE_LIFT, R, R, 1, T, 2, 0.31), S(226, COUPE_NOTCH, R, V, 1, 5, 2, 0.32), S(227, COUPE_FAST, S, V, 3, 5, 2, 0.35),
    S(228, COUPE_FAST, S, V, 3, 5, 2, 0.35), S(229, MID_ENGINE, O, O, 2, 5, 2, 0.34), S(230, MID_ENGINE, V, O, 1, M, 2, 0.31),
    S(231, HATCH_BOX, R, R, 1, 5, 2, 0.31), S(232, COUPE_LIFT, R, R, 1, T, 2, 0.31), S(233, SEDAN_BOX, O, R, 2, 5, 2, 0.28),
    S(234, SEDAN_BOX, O, R, 2, 0, 2, 0.27), S(235, HATCH_MODERN, S, V, 3, 5, 4, 0.34), S(236, HATCH_MODERN, S, V, 3, 5, 2, 0.32),
    S(237, HATCH_ROUND, R, V, 1, 0, 2, 0.28), S(238, PICKUP, R, V, 3, 5, 4, 0.42), S(239, PICKUP, R, V, 3, 5, 2, 0.38),
    S(240, COUPE_LIFT, Q, R, 1, T, 2, 0.29), S(241, SEDAN_BOX, R, R, 2, 0, 4, 0.28), S(242, MID_ENGINE, R, R, 0, 5, 2, 0.31),
    // Chevrolet / Pontiac / Buick
    S(243, FRONT_LONG, P, O, 2, 5, 2, 0.31), S(244, FRONT_LONG, P, O, 1, 5, 2, 0.33), S(245, COUPE_LIFT, P, R, 0, 5, 2, 0.33),
    S(246, COUPE_LIFT, P, O, 1, 5, 2, 0.33), S(247, COUPE_LIFT, S, O, 1, 5, 2, 0.34), S(248, COUPE_LIFT, S, O, 1, 5, 2, 0.35),
    S(249, COUPE_FAST, S, S, 3, 5, 2, 0.35), S(250, MUSCLE_NOTCH, O, R, 2, 5, 2, 0.32), S(251, MUSCLE_NOTCH, O, R, 2, 5, 2, 0.32),
    S(252, COUPE_LIFT, R, R, 1, 5, 2, 0.32), S(253, COUPE_FAST, S, R, 3, 5, 2, 0.35), S(254, COUPE_FAST, S, S, 3, 5, 2, 0.35),
    S(255, MUSCLE_NOTCH, Q, R, 2, 5, 2, 0.32), S(256, SEDAN_BOX, R, O, 2, 5, 4, 0.33), S(257, COUPE_NOTCH, R, R, 2, 5, 2, 0.32),
    S(258, PICKUP, R, V, 3, 5, 4, 0.39),    S(259, PICKUP, O, V, 3, 0, 2, 0.36),     S(260, MUSCLE_NOTCH, Q, R, 4, 5, 2, 0.32),
    S(261, MUSCLE_FAST, Q, B, 4, T, 2, 0.33), S(262, COUPE_LIFT, P, B, 4, 5, 2, 0.33), S(263, SEDAN_BOX, R, R, 3, M, 2, 0.32),
    // Dodge / Plymouth
    S(264, MUSCLE_NOTCH, P, O, 3, 5, 2, 0.32), S(265, MUSCLE_NOTCH, P, O, 3, 5, 2, 0.32), S(266, MUSCLE_NOTCH, O, R, 2, 0, 2, 0.32),
    S(267, MUSCLE_NOTCH, Q, R, 2, 5, 2, 0.32), S(268, MUSCLE_NOTCH, Q, B, 2, 5, 2, 0.32), S(269, MUSCLE_NOTCH, Q, B, 3, 5, 2, 0.36),
    S(270, SEDAN_MODERN, S, B, 3, 5, 4, 0.36), S(271, ROADSTER_LONG, V, R, 1, 5, 2, 0.34), S(272, FRONT_LONG, S, S, 1, 5, 2, 0.35),
    S(273, SEDAN_90, R, R, 2, 5, 4, 0.31),  S(274, PICKUP, R, V, 3, 5, 2, 0.40),     S(275, MUSCLE_NOTCH, P, R, 0, 5, 2, 0.32),
    // Hyundai / Kia
    S(276, COUPE_FAST, S, R, 2, 5, 2, 0.33), S(277, COUPE_FAST, S, R, 2, 5, 2, 0.34), S(278, HATCH_MODERN, S, S, 2, 5, 4, 0.33),
    S(279, HATCH_MODERN, S, R, 2, 5, 4, 0.33), S(280, HATCH_ROUND, R, V, 1, T, 4, 0.28), S(281, SEDAN_MODERN, S, S, 2, 5, 4, 0.35),
    // Renault / Alpine / Dacia
    S(282, SEDAN_BOX, R, R, 2, 0, 4, 0.28), S(283, HATCH_BOX, R, R, 1, T, 2, 0.30),  S(284, HATCH_BOX, R, R, 1, 5, 2, 0.28),
    S(285, HATCH_ROUND, R, R, 1, 5, 2, 0.29), S(286, HATCH_ROUND, S, R, 1, 5, 2, 0.31), S(287, HATCH_ROUND, S, R, 1, 5, 2, 0.32),
    S(288, HATCH_MODERN, S, R, 1, 5, 2, 0.33), S(289, COUPE_LIFT, Q, O, 0, 5, 2, 0.28), S(290, SEDAN_BOX, R, R, 2, 0, 4, 0.28),
    // Peugeot / Citroen
    S(291, HATCH_BOX, R, R, 1, 5, 2, 0.28), S(292, HATCH_BOX, R, R, 1, T, 2, 0.31),  S(293, HATCH_ROUND, R, R, 1, 5, 2, 0.28),
    S(294, HATCH_ROUND, R, R, 1, 5, 2, 0.30), S(295, HATCH_ROUND, R, R, 1, 5, 2, 0.28), S(296, HATCH_BOX, R, R, 1, T, 4, 0.29),
    // Volvo / Saab
    S(297, SEDAN_BOX, R, R, 2, T, 4, 0.30), S(298, WAGON, R, V, 2, 5, 4, 0.32),      S(299, COUPE_NOTCH, O, R, 2, 0, 2, 0.29),
    S(300, COUPE_LIFT, R, R, 1, T, 2, 0.30), S(301, HATCH_ROUND, R, R, 1, 5, 2, 0.32),
    // VAZ / Lada
    S(302, SEDAN_BOX, O, R, 2, 0, 4, 0.28), S(303, SEDAN_BOX, Q, R, 2, 0, 4, 0.28),  S(304, SEDAN_BOX, R, R, 2, 0, 4, 0.28),
    S(305, SUV_SMALL, O, R, 2, 0, 2, 0.35), S(306, HATCH_BOX, R, R, 1, 0, 2, 0.28),
    // Lotus / McLaren / Aston / Jaguar / Jeep
    S(307, ROADSTER_MID, V, O, 1, 5, 2, 0.29), S(308, MID_ENGINE, S, O, 1, 5, 2, 0.31), S(309, WEDGE, P, R, 1, 5, 2, 0.32),
    S(310, ROADSTER, P, R, 1, 0, 2, 0.27),  S(311, ROADSTER, O, O, 1, 0, 2, 0.27),   S(312, MID_ENGINE, S, O, 1, 5, 2, 0.33),
    S(313, MID_ENGINE, O, R, 0, 5, 2, 0.32), S(314, MID_ENGINE, S, S, 0, 5, 2, 0.34), S(315, MID_ENGINE, S, S, 0, 5, 2, 0.35),
    S(316, FRONT_LONG, V, R, 6, M, 2, 0.30), S(317, FRONT_LONG, S, S, 6, 5, 2, 0.34), S(318, FRONT_LONG, S, S, 6, 5, 2, 0.35),
    S(319, FRONT_LONG, V, R, 1, M, 2, 0.30), S(320, WEDGE, S, B, 1, 5, 2, 0.34),     S(321, COUPE_FAST, S, S, 3, 5, 2, 0.34),
    S(322, SUV_SMALL, R, R, 3, 0, 2, 0.38), S(323, SUV_BOX, R, R, 3, 0, 4, 0.36),    S(324, SUV_SMALL, O, R, 3, 0, 2, 0.37),
};
#undef S

// ---- Arac basina ozel profil (en taninir araclar) ----
// Arketipin yerine gecen tam yan profil + camurluk siskinligi + yan cam sonu + fabrika kanadi + ozel detaylar.
// flF / flR < 0: arketipten otomatik.  sideEnd 0: otomatik (yan camin bittigi x orani).
// Kanat: K0 yok, K1 dudak / ducktail (kuyruk ust kenari), K2 orta ayakli (Evo / R34 / STI), K3 yuksek (F40 / 22B),
//        K4 tavan spoyleri (hatchback), K5 balina kuyrugu (930 / Escort Cosworth)
// Ozel: FX_STRAKE yan izgaralar (Testarossa), FX_LOUVER arka cam panjuru, FX_VENT kaput hava cikisi, FX_FENDERLAMP
enum Wing { K0, K1, K2, K3, K4, K5 };
enum { FX_STRAKE = 1, FX_LOUVER = 2, FX_VENT = 4 };
struct CarProfile { int id; ArchSpec a; double flF, flR, sideEnd; int wing, fx; };

#define P(id, n, h, c, rf, rr, b, d, t, rw, bw, flF, flR, se, k, fx) {id, {n, h, c, rf, rr, b, d, t, rw, bw, false, false}, flF, flR, se, k, fx}
#define PX(id, flF, flR, k, fx) {id, {-1, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, false}, flF, flR, 0, k, fx}   // yalniz detay, profil arketipten
const CarProfile kProfiles[] = {
    //  id   nose  hood  cowl  roofF roofR back  deck  tail  roofW beltW  flF    flR   side  kanat fx
    // Honda
    P(  1, 0.50, 0.58, 0.28, 0.45, 0.82, 0.96, 0.60, 0.55, 0.78, 0.95, -1,   -1,   0,    K0, 0),          // Civic EF: uzun alcak tavan
    P(  2, 0.48, 0.57, 0.27, 0.45, 0.80, 0.96, 0.60, 0.54, 0.78, 0.94, -1,   -1,   0,    K0, 0),          // Civic EG: dusuk kesik kuyruk
    P(  3, 0.48, 0.57, 0.27, 0.45, 0.80, 0.96, 0.60, 0.54, 0.78, 0.94, -1,   -1,   0,    K0, 0),
    P(  4, 0.50, 0.59, 0.26, 0.44, 0.80, 0.96, 0.63, 0.58, 0.78, 0.94, -1,   -1,   0,    K0, 0),          // Civic EK: daha yuksek kuyruk
    P(  5, 0.50, 0.59, 0.26, 0.44, 0.80, 0.96, 0.63, 0.58, 0.78, 0.94, -1,   -1,   0,    K4, 0),          // EK9: tavan spoyleri
    P(  6, 0.50, 0.59, 0.26, 0.44, 0.80, 0.96, 0.63, 0.58, 0.78, 0.94, -1,   -1,   0,    K0, 0),
    P( 11, 0.46, 0.55, 0.32, 0.50, 0.70, 0.97, 0.62, 0.60, 0.76, 0.94, -1,   -1,   0.66, K0, 0),          // CR-X: kisa, kamm kuyruk
    P( 12, 0.45, 0.55, 0.36, 0.52, 0.67, 0.92, 0.66, 0.64, 0.76, 0.94, -1,   -1,   0,    K2, 0),          // Integra DC2 Type R
    P( 19, 0.38, 0.48, 0.31, 0.46, 0.58, 0.79, 0.74, 0.73, 0.70, 0.92, 0.02, 0.04, 0.62, K1, 0),          // NSX: kabin onde, uzun duz kuyruk
    P( 20, 0.38, 0.48, 0.31, 0.46, 0.58, 0.79, 0.74, 0.73, 0.70, 0.92, 0.02, 0.04, 0.62, K1, 0),
    // Toyota
    P( 24, 0.50, 0.58, 0.34, 0.49, 0.64, 0.93, 0.62, 0.60, 0.78, 0.96, -1,   -1,   0.70, K0, 0),          // AE86 Trueno: kamali hatchback
    P( 33, 0.44, 0.55, 0.37, 0.52, 0.66, 0.92, 0.64, 0.62, 0.76, 0.95, -1,   -1,   0,    K1, 0),          // Supra A70
    P( 34, 0.42, 0.55, 0.37, 0.52, 0.63, 0.86, 0.66, 0.64, 0.72, 0.93, 0.03, 0.04, 0,    K3, 0),          // Supra A80: balon kabin + kemer kanat
    P( 35, 0.42, 0.55, 0.37, 0.52, 0.63, 0.86, 0.66, 0.64, 0.72, 0.93, 0.03, 0.04, 0,    K1, 0),
    // Nissan
    P( 50, 0.55, 0.62, 0.33, 0.47, 0.66, 0.79, 0.68, 0.66, 0.80, 0.97, 0.03, 0.04, 0,    K2, 0),          // R32 GT-R
    P( 51, 0.51, 0.60, 0.33, 0.48, 0.67, 0.81, 0.68, 0.66, 0.78, 0.96, 0.03, 0.04, 0,    K2, 0),          // R33 GT-R
    P( 52, 0.56, 0.63, 0.32, 0.47, 0.65, 0.79, 0.72, 0.71, 0.79, 0.97, 0.035,0.04, 0,    K2, 0),          // R34 GT-R: kutu, yuksek kuyruk
    P( 53, 0.56, 0.63, 0.32, 0.47, 0.65, 0.79, 0.72, 0.71, 0.79, 0.97, -1,   -1,   0,    K2, 0),
    P( 54, 0.55, 0.62, 0.33, 0.47, 0.66, 0.79, 0.68, 0.66, 0.80, 0.97, -1,   -1,   0,    K1, 0),
    P( 55, 0.50, 0.60, 0.33, 0.50, 0.65, 0.84, 0.73, 0.71, 0.76, 0.95, 0.03, 0.04, 0.70, K1, 0),          // R35: yelkenli C direk
    P( 56, 0.48, 0.57, 0.34, 0.50, 0.66, 0.79, 0.66, 0.65, 0.78, 0.96, -1,   -1,   0,    K0, 0),          // S13 Silvia
    P( 57, 0.44, 0.56, 0.34, 0.50, 0.64, 0.92, 0.64, 0.62, 0.78, 0.96, -1,   -1,   0.70, K1, 0),          // 180SX: acilir far + hatchback
    P( 58, 0.49, 0.58, 0.34, 0.50, 0.66, 0.80, 0.67, 0.66, 0.77, 0.95, -1,   -1,   0,    K1, 0),          // S14
    P( 59, 0.49, 0.58, 0.34, 0.50, 0.66, 0.80, 0.68, 0.67, 0.77, 0.95, -1,   -1,   0,    K2, 0),          // S15 Spec-R
    P( 60, 0.49, 0.58, 0.34, 0.50, 0.66, 0.80, 0.67, 0.66, 0.77, 0.95, -1,   -1,   0,    K0, 0),
    P( 61, 0.42, 0.54, 0.38, 0.54, 0.64, 0.88, 0.66, 0.65, 0.74, 0.93, 0.03, 0.04, 0,    K0, 0),          // 300ZX Z32
    P( 64, 0.48, 0.58, 0.45, 0.58, 0.66, 0.95, 0.62, 0.58, 0.76, 0.95, 0.02, 0.03, 0.80, K0, 0),          // 240Z: uzun kaput, fastback
    P( 65, 0.48, 0.58, 0.45, 0.58, 0.66, 0.95, 0.62, 0.58, 0.76, 0.95, 0.02, 0.03, 0.80, K0, 0),
    // Mazda
    P( 76, 0.40, 0.53, 0.36, 0.52, 0.62, 0.92, 0.63, 0.61, 0.74, 0.94, -1,   -1,   0.70, K0, 0),          // RX-7 FB
    P( 77, 0.40, 0.53, 0.37, 0.52, 0.63, 0.92, 0.64, 0.62, 0.74, 0.94, 0.02, 0.03, 0.70, K0, 0),          // RX-7 FC
    P( 78, 0.34, 0.50, 0.38, 0.53, 0.60, 0.88, 0.67, 0.65, 0.68, 0.92, 0.04, 0.055,0,    K2, 0),          // RX-7 FD: sivri burun, cift kubbe
    // Subaru
    P( 85, 0.55, 0.62, 0.30, 0.45, 0.68, 0.81, 0.68, 0.67, 0.80, 0.96, -1,   -1,   0,    K0, FX_VENT),    // Impreza GC8 WRX
    P( 86, 0.55, 0.62, 0.30, 0.45, 0.68, 0.81, 0.68, 0.67, 0.80, 0.96, -1,   -1,   0,    K2, FX_VENT),    // STi: yuksek ayakli kanat
    P( 87, 0.55, 0.62, 0.31, 0.46, 0.66, 0.81, 0.68, 0.67, 0.80, 0.96, 0.05, 0.06, 0,    K3, FX_VENT),    // 22B: genis govde + ayarli kanat
    P( 88, 0.55, 0.63, 0.30, 0.45, 0.68, 0.82, 0.70, 0.69, 0.78, 0.95, 0.03, 0.03, 0,    K3, FX_VENT),    // GDB STi
    // Mitsubishi Evo
    P( 98, 0.56, 0.62, 0.30, 0.44, 0.68, 0.81, 0.68, 0.67, 0.80, 0.96, 0.02, 0.02, 0,    K3, FX_VENT),
    P( 99, 0.56, 0.62, 0.30, 0.44, 0.68, 0.81, 0.68, 0.67, 0.80, 0.96, 0.02, 0.02, 0,    K3, FX_VENT),
    P(100, 0.56, 0.62, 0.30, 0.44, 0.68, 0.81, 0.68, 0.67, 0.80, 0.96, 0.04, 0.04, 0,    K3, FX_VENT),    // Evo VI: genis camurluk, dev kanat
    P(101, 0.56, 0.63, 0.30, 0.45, 0.68, 0.82, 0.70, 0.69, 0.79, 0.96, 0.03, 0.03, 0,    K2, FX_VENT),
    P(102, 0.56, 0.63, 0.30, 0.45, 0.68, 0.82, 0.70, 0.69, 0.79, 0.96, 0.03, 0.03, 0,    K2, FX_VENT),
    P(103, 0.57, 0.65, 0.30, 0.46, 0.68, 0.83, 0.72, 0.71, 0.77, 0.95, 0.03, 0.03, 0,    K2, FX_VENT),
    // BMW / Mini
    P(118, 0.55, 0.60, 0.33, 0.46, 0.67, 0.79, 0.70, 0.69, 0.84, 0.98, 0.04, 0.045,0,    K2, 0),          // M3 E30: dik cam evi, kutu camurluk
    P(119, 0.55, 0.60, 0.33, 0.46, 0.68, 0.79, 0.67, 0.66, 0.84, 0.98, -1,   -1,   0,    K0, 0),
    P(120, 0.52, 0.60, 0.34, 0.49, 0.67, 0.81, 0.68, 0.67, 0.78, 0.96, -1,   -1,   0,    K1, 0),          // M3 E36
    P(122, 0.53, 0.61, 0.34, 0.49, 0.67, 0.82, 0.70, 0.69, 0.77, 0.95, 0.02, 0.03, 0,    K1, 0),          // M3 E46
    P(123, 0.53, 0.61, 0.34, 0.49, 0.67, 0.82, 0.70, 0.69, 0.77, 0.95, 0.02, 0.03, 0,    K1, 0),
    // VW
    P(151, 0.56, 0.62, 0.30, 0.42, 0.84, 0.97, 0.62, 0.58, 0.82, 0.98, -1,   -1,   0,    K0, 0),          // Golf Mk1: kosegen kutu
    P(152, 0.55, 0.62, 0.30, 0.43, 0.85, 0.97, 0.64, 0.60, 0.82, 0.97, -1,   -1,   0,    K0, 0),          // Golf Mk2
    P(153, 0.55, 0.62, 0.30, 0.43, 0.85, 0.97, 0.64, 0.60, 0.82, 0.97, -1,   -1,   0,    K0, 0),
    // Porsche 911: dik on cam, uzun akan kuyruk, arka yan cam ~%70'te biter
    P(164, 0.40, 0.52, 0.33, 0.50, 0.60, 0.97, 0.56, 0.52, 0.74, 0.93, 0.03, 0.05, 0.70, K1, 0),          // RS 2.7 ducktail
    P(165, 0.40, 0.52, 0.33, 0.50, 0.60, 0.97, 0.56, 0.52, 0.74, 0.93, 0.03, 0.05, 0.70, K0, 0),
    P(166, 0.40, 0.52, 0.33, 0.50, 0.60, 0.97, 0.58, 0.54, 0.74, 0.93, 0.04, 0.08, 0.70, K5, 0),          // 930: balina kuyruk + genis arka
    P(167, 0.40, 0.52, 0.33, 0.50, 0.60, 0.97, 0.56, 0.52, 0.74, 0.93, 0.03, 0.05, 0.70, K1, 0),          // 964 RS
    P(168, 0.38, 0.52, 0.33, 0.50, 0.60, 0.97, 0.57, 0.53, 0.74, 0.93, 0.04, 0.06, 0.70, K1, 0),          // 993
    P(169, 0.40, 0.50, 0.35, 0.48, 0.58, 0.95, 0.62, 0.60, 0.72, 0.93, 0.03, 0.05, 0.70, K3, 0),          // 996 GT3
    P(170, 0.40, 0.50, 0.35, 0.48, 0.58, 0.95, 0.62, 0.60, 0.72, 0.93, 0.03, 0.06, 0.70, K2, 0),          // 996 Turbo
    // Ferrari
    P(184, 0.34, 0.46, 0.30, 0.45, 0.55, 0.86, 0.79, 0.79, 0.68, 0.92, 0.03, 0.05, 0.60, K3, FX_LOUVER),  // F40: panjurlu motor kapagi, entegre kanat
    P(185, 0.38, 0.47, 0.32, 0.46, 0.58, 0.72, 0.74, 0.74, 0.70, 0.93, 0.02, 0.07, 0.60, K0, FX_STRAKE),  // Testarossa: yan izgaralar, genis kic
    P(186, 0.37, 0.47, 0.31, 0.46, 0.57, 0.80, 0.74, 0.73, 0.70, 0.93, 0.02, 0.05, 0.60, K1, 0),          // F355
    P(187, 0.38, 0.48, 0.31, 0.46, 0.58, 0.83, 0.74, 0.73, 0.70, 0.93, 0.02, 0.05, 0.60, K1, 0),          // 360
    P(188, 0.38, 0.48, 0.31, 0.46, 0.58, 0.83, 0.74, 0.73, 0.70, 0.93, 0.02, 0.05, 0.60, K1, 0),          // F430
    P(189, 0.37, 0.48, 0.31, 0.46, 0.58, 0.84, 0.75, 0.74, 0.70, 0.93, 0.02, 0.05, 0.60, K1, 0),          // 458
    PX(191, -1, -1, K1, 0), PX(193, -1, -1, K3, 0),                                                      // Enzo, F50
    // Lamborghini: tek cizgi kama
    P(194, 0.36, 0.46, 0.30, 0.45, 0.56, 0.86, 0.68, 0.66, 0.70, 0.93, 0.02, 0.04, 0.60, K0, FX_LOUVER),  // Miura
    P(195, 0.28, 0.42, 0.22, 0.46, 0.56, 0.76, 0.76, 0.74, 0.64, 0.90, 0.03, 0.06, 0.58, K3, 0),          // Countach QV
    P(196, 0.30, 0.44, 0.25, 0.47, 0.57, 0.78, 0.75, 0.73, 0.64, 0.91, 0.03, 0.06, 0.60, K1, 0),          // Diablo
    P(197, 0.31, 0.45, 0.26, 0.47, 0.57, 0.80, 0.75, 0.73, 0.65, 0.91, 0.03, 0.06, 0.60, K0, 0),          // Murcielago
    PX(198, -1, -1, K0, 0), PX(199, -1, -1, K0, 0), PX(200, -1, -1, K0, 0),
    // Fiat / Lancia / Tofas: dik cam evi, kutu
    P(209, 0.57, 0.62, 0.31, 0.43, 0.69, 0.80, 0.67, 0.66, 0.84, 0.98, 0.04, 0.04, 0,    K1, 0),          // Fiat 131 Abarth
    P(213, 0.56, 0.62, 0.29, 0.42, 0.84, 0.97, 0.64, 0.60, 0.80, 0.97, 0.05, 0.05, 0,    K4, 0),          // Delta Integrale
    P(214, 0.36, 0.48, 0.24, 0.42, 0.60, 0.80, 0.68, 0.66, 0.66, 0.92, 0.03, 0.04, 0,    K1, 0),          // Stratos
    P(215, 0.36, 0.48, 0.30, 0.45, 0.58, 0.84, 0.70, 0.70, 0.70, 0.93, 0.04, 0.05, 0.60, K1, 0),          // 037
    P(216, 0.59, 0.63, 0.31, 0.40, 0.66, 0.77, 0.64, 0.63, 0.86, 0.99, -1,   -1,   0,    K0, 0),          // Murat 124
    P(217, 0.58, 0.63, 0.30, 0.42, 0.69, 0.80, 0.67, 0.66, 0.85, 0.99, -1,   -1,   0,    K0, 0),          // Sahin
    P(218, 0.58, 0.63, 0.30, 0.42, 0.69, 0.80, 0.67, 0.66, 0.85, 0.99, -1,   -1,   0,    K0, 0),          // Dogan
    // Ford
    P(222, 0.58, 0.63, 0.42, 0.56, 0.63, 0.92, 0.67, 0.66, 0.78, 0.98, -1,   0.03, 0.74, K1, FX_LOUVER),  // Mustang Boss 302 sportsroof
    P(223, 0.58, 0.63, 0.42, 0.56, 0.63, 0.92, 0.67, 0.66, 0.78, 0.98, -1,   0.03, 0.74, K1, 0),
    P(224, 0.58, 0.63, 0.42, 0.56, 0.63, 0.92, 0.67, 0.66, 0.78, 0.98, -1,   0.03, 0.74, K1, 0),
    P(225, 0.55, 0.61, 0.37, 0.51, 0.66, 0.91, 0.64, 0.62, 0.80, 0.97, -1,   -1,   0.72, K1, 0),          // Fox body hatchback
    P(226, 0.53, 0.61, 0.40, 0.55, 0.66, 0.82, 0.69, 0.68, 0.76, 0.96, 0.02, 0.03, 0,    K1, 0),          // SN95
    P(227, 0.54, 0.62, 0.41, 0.56, 0.64, 0.86, 0.71, 0.70, 0.74, 0.95, 0.02, 0.04, 0.73, K1, 0),          // S550 fastback
    P(228, 0.54, 0.62, 0.41, 0.56, 0.64, 0.86, 0.71, 0.70, 0.74, 0.95, 0.02, 0.04, 0.73, K1, 0),
    P(229, 0.36, 0.48, 0.32, 0.46, 0.57, 0.84, 0.74, 0.74, 0.68, 0.92, 0.02, 0.05, 0.60, K0, 0),          // Ford GT
    P(231, 0.54, 0.61, 0.30, 0.44, 0.80, 0.95, 0.66, 0.62, 0.80, 0.96, 0.04, 0.04, 0,    K5, 0),          // Escort RS Cosworth
    P(232, 0.52, 0.60, 0.30, 0.45, 0.76, 0.93, 0.66, 0.64, 0.80, 0.96, 0.03, 0.03, 0,    K5, 0),          // Sierra Cosworth
    // Chevrolet / Pontiac
    P(246, 0.40, 0.55, 0.42, 0.57, 0.67, 0.92, 0.72, 0.72, 0.72, 0.94, 0.02, 0.04, 0.72, K0, 0),          // C5
    P(247, 0.42, 0.56, 0.42, 0.57, 0.67, 0.92, 0.72, 0.72, 0.72, 0.94, 0.02, 0.04, 0.72, K1, 0),          // C6
    P(248, 0.42, 0.56, 0.42, 0.57, 0.67, 0.92, 0.72, 0.72, 0.72, 0.94, 0.03, 0.05, 0.72, K1, 0),
    P(250, 0.62, 0.65, 0.42, 0.56, 0.68, 0.81, 0.67, 0.66, 0.80, 0.98, -1,   -1,   0,    K1, 0),          // Camaro 69
    P(251, 0.62, 0.65, 0.42, 0.56, 0.68, 0.81, 0.67, 0.66, 0.80, 0.98, -1,   -1,   0,    K1, 0),
    P(252, 0.40, 0.55, 0.42, 0.57, 0.66, 0.91, 0.66, 0.64, 0.74, 0.95, -1,   -1,   0.72, K1, 0),          // Camaro 98
    // Dodge
    PX(271, 0.03, 0.05, K0, 0),                                                                          // Viper RT/10 (acik)
    PX(272, 0.03, 0.05, K3, 0),                                                                          // Viper ACR
    // Peugeot / Renault
    P(291, 0.55, 0.61, 0.29, 0.43, 0.83, 0.97, 0.63, 0.59, 0.82, 0.97, -1,   -1,   0,    K0, 0),          // 205 GTI
    P(292, 0.55, 0.61, 0.29, 0.43, 0.83, 0.97, 0.66, 0.63, 0.82, 0.97, 0.06, 0.07, 0,    K4, 0),          // 205 T16
    PX(283, 0.03, 0.08, K4, 0),     PX(242, 0.03, 0.04, K3, 0),                                          // RS200                                                                          // R5 Turbo
    // VAZ
    P(302, 0.59, 0.63, 0.31, 0.40, 0.66, 0.77, 0.64, 0.63, 0.86, 0.99, -1,   -1,   0,    K0, 0),          // 2101
    P(303, 0.59, 0.63, 0.31, 0.40, 0.66, 0.77, 0.64, 0.63, 0.86, 0.99, -1,   -1,   0,    K0, 0),
    P(304, 0.60, 0.64, 0.31, 0.41, 0.67, 0.78, 0.66, 0.65, 0.86, 0.99, -1,   -1,   0,    K0, 0),          // 2107
    // McLaren / Jaguar
    P(313, 0.38, 0.48, 0.30, 0.48, 0.58, 0.84, 0.72, 0.70, 0.66, 0.92, 0.02, 0.04, 0.62, K0, 0),          // McLaren F1: orta kabin, balon
    PX(314, -1, -1, K0, 0), PX(315, -1, -1, K0, 0), PX(320, -1, -1, K0, 0),
};
#undef P
#undef PX
