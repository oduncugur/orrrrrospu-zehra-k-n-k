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
