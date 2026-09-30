// ZEHRA KINIK - Faz 1 konsol simulasyonu: burnout -> start agaci -> 0-400 m drag -> ABS'siz fren
// Derleme: cmake -B build && cmake --build build && ./build/zehra_sim [--stock-axles] [--help]
#include "garage/VehicleCatalog.h"
#include "sim/DrivetrainFailure.h"
#include "sim/PowertrainCore.h"
#include "sim/Suspension.h"
#include "sim/WheelSimulation.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

using namespace zk;

namespace {

struct Options {
    bool stockAxles = false, drySump = false, burnout = true, plenum = false, feather = true;
    double launchRpm = -1.0, dumpTime = 0.12; // launchRpm < 0: otomatik
    int car = 0;
    std::string road = "drag";
    double oilL = 4.5, psi = 16.0, fuelL = 8.0, gasket = 0.70, logInterval = 1.0;
    FuelType fuel = FuelType::Race100;
    Valvetrain vt = Valvetrain::V16;
    std::string csv;
};

bool argVal(const char* a, const char* key, double& out) {
    const size_t n = std::strlen(key);
    if (std::strncmp(a, key, n) == 0 && a[n] == '=') { out = std::atof(a + n + 1); return true; }
    return false;
}

void usage() {
    std::puts("zehra_sim secenekleri:\n"
              "  --stock-axles      stok 1045 aks (clutch dump'ta kirilir)\n"
              "  --dry-sump         kuru karter (yagsiz kalma yok, CoG -10cm motor)\n"
              "  --oil=L            karter yag miktari (islak karter), vars. 4.5\n"
              "  --psi=P            on drag slick basinci, vars. 16\n"
              "  --fuel-liters=L    depodaki yakit, vars. 8 (60L = +45kg)\n"
              "  --gasket=mm        kapak contasi kalinligi, vars. 0.70\n"
              "  --e85 | --pump95   yakit tipi (vars. 100 oktan yaris)\n"
              "  --8v | --20v       subap mimarisi (vars. 16V)\n"
              "  --plenum           ITB yerine plenum manifold\n"
              "  --road=P           yol: drag (A), otoban (B+derz), sehir (C+kasis/rogar/cukur), koy (E), dag\n"
              "  --car=N            garaj katalogundan arac (bkz. zehra_garage --list)\n"
              "  --launch-rpm=R     2-step kalkis devri, vars. 6500 (katalog aracinda otomatik)\n"
              "  --dump=s           debriyaj birakma suresi, vars. 0.12 (clutch dump)\n"
              "  --no-burnout       burnout atlama\n"
              "  --no-feather       surucu gaz modulasyonu yok (tam gaz, patinaj serbest)\n"
              "  --log=s            log araligi saniye, vars. 1.0\n"
              "  --csv=dosya        100 Hz telemetri CSV (MoTeC/VBOX tarzi)");
}

const char* fuelName(FuelType f) {
    return f == FuelType::E85 ? "E85" : f == FuelType::Pump95 ? "95 Oktan" : "100+ Yaris";
}

} // namespace

int main(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (!std::strcmp(a, "--help") || !std::strcmp(a, "-h")) { usage(); return 0; }
        else if (!std::strcmp(a, "--stock-axles")) o.stockAxles = true;
        else if (!std::strcmp(a, "--dry-sump"))    o.drySump = true;
        else if (!std::strcmp(a, "--no-burnout"))  o.burnout = false;
        else if (!std::strcmp(a, "--plenum"))      o.plenum = true;
        else if (!std::strcmp(a, "--no-feather"))  o.feather = false;
        else if (!std::strcmp(a, "--e85"))         o.fuel = FuelType::E85;
        else if (!std::strcmp(a, "--pump95"))      o.fuel = FuelType::Pump95;
        else if (!std::strcmp(a, "--8v"))          o.vt = Valvetrain::V8;
        else if (!std::strcmp(a, "--20v"))         o.vt = Valvetrain::V20;
        else if (!std::strncmp(a, "--csv=", 6))    o.csv = a + 6;
        else if (!std::strncmp(a, "--car=", 6))    o.car = std::atoi(a + 6);
        else if (!std::strncmp(a, "--road=", 7))   o.road = a + 7;
        else if (argVal(a, "--oil", o.oilL) || argVal(a, "--psi", o.psi) || argVal(a, "--fuel-liters", o.fuelL) ||
                 argVal(a, "--gasket", o.gasket) || argVal(a, "--log", o.logInterval) ||
                 argVal(a, "--launch-rpm", o.launchRpm) || argVal(a, "--dump", o.dumpTime)) {}
        else { std::fprintf(stderr, "Bilinmeyen secenek: %s\n", a); usage(); return 1; }
    }

    // ---------------- Arac kurulumu ----------------
    const VehicleDef* car = o.car ? findVehicle(o.car) : nullptr;
    if (o.car && !car) { std::fprintf(stderr, "Katalogda arac yok: %d\n", o.car); return 1; }
    EngineSpec eng = car ? buildEngineSpec(*car) : EngineSpec::K20Default();
    GearboxSpec gbx = car ? buildGearbox(*car) : GearboxSpec{};
    eng.gasketMm = o.gasket; eng.fuel = o.fuel;
    if (!car) { eng.valvetrain = o.vt; if (o.plenum) { eng.intake = IntakeType::Plenum; eng.name = "K20A (Plenum, 16V i-VTEC)"; } }
    double Tmax = 0.0;
    for (auto* c : {&eng.lowCam, &eng.highCam}) for (auto& pr : *c) Tmax = std::max(Tmax, pr.second);
    ClutchSpec clutch;
    if (car) clutch.maxTorque = Tmax * 1.7;          // sokak baskisi: motor torkunun ~1.7 kati
    PowertrainCore pt(eng, clutch, DiffSpec{}, gbx);
    const Drive drive = car ? car->drive : Drive::FWD;
    const Gearbox boxType = car ? gearboxTable()[car->gearbox].type : Gearbox::HPattern;
    // Cekis dagitimi: AWD'de merkez dagitim %40 on / %60 arka (sabit oranli)
    const double frontShare = drive == Drive::FWD ? 1.0 : drive == Drive::RWD ? 0.0 : 0.40;
    const int dL = drive == Drive::RWD ? 2 : 0, dR = dL + 1;   // birincil tahrikli aks
    // Aks capi: fabrika muhendisligi, 1. viteste tepe motor torkunda stok kopma gerilmesinin ~%55'i
    AxleSpec axle = o.stockAxles ? AxleSpec::Stock() : AxleSpec::Chromoly();
    if (car) {
        const double G1 = gbx.ratios.front() * gbx.finalDrive;
        const double perSide = Tmax * G1 * gbx.efficiency * 0.5 * std::max(frontShare, 1.0 - frontShare);
        axle.diameterMm = std::cbrt(16.0 * perSide / (3.14159265 * 0.55 * AxleSpec::Stock().tauUltMPa * 1e6)) * 1000.0;
    }
    DrivetrainFailure fail(axle, LubeSpec{o.drySump, o.oilL, 3.0});
    const double launchRpm = o.launchRpm > 0 ? o.launchRpm
                           : car ? std::clamp(0.55 * eng.redlineRpm, 3000.0, 6500.0) : 6500.0;
    const double shiftRpm = eng.redlineRpm - 250.0;

    TireParams slick;                           // tahrikli aks: yapiskan drag slick, dovme jant
    TireParams skinny; skinny.muPeak = 1.0; skinny.wheelMass = 9.0; skinny.B = 10.0; // serbest aks: ince "skinny"
    const double ambient = 25.0;
    const bool fDriven = frontShare > 0.0, rDriven = frontShare < 1.0;
    WheelSimulation w[4] = {
        WheelSimulation(fDriven ? slick : skinny, fDriven ? o.psi : 32.0, fDriven ? 55.0 : 30.0, ambient),
        WheelSimulation(fDriven ? slick : skinny, fDriven ? o.psi : 32.0, fDriven ? 55.0 : 30.0, ambient),
        WheelSimulation(rDriven ? slick : skinny, rDriven ? o.psi : 32.0, rDriven ? 55.0 : 30.0, ambient),
        WheelSimulation(rDriven ? slick : skinny, rDriven ? o.psi : 32.0, rDriven ? 55.0 : 30.0, ambient)};
    w[dL].setSurfaceMu(0.96); // sol serit daha az lastik kaplamali -> LSD asimetri yonetir

    const double fuelDensity = (o.fuel == FuelType::E85) ? 0.785 : 0.745;
    const double baseMass = (car ? car->massKg : 1080.0) + 75.0;      // kuru arac + surucu
    double fuelKg = o.fuelL * fuelDensity;
    const double hCoG = (car ? 0.36 * car->heightM : 0.50) - (o.drySump ? 0.012 : 0.0);
    VehicleLoad vl{baseMass + fuelKg, car ? car->wheelbaseM : 2.62, car ? car->widthM * 0.85 : 1.50, hCoG,
                   car ? car->frontWeight : 0.62};
    // Suspansiyon: kasa tipine gore dogal frekans (Hz); yaris araclari sert
    double fRide = 1.6;
    if (car) {
        switch (car->body) {
        case Body::Super: fRide = 2.2; break;   case Body::Roadster: fRide = 1.9; break;
        case Body::Muscle: fRide = 1.4; break;  case Body::SUV: case Body::Pickup: case Body::Van: fRide = 1.2; break;
        case Body::Sedan: case Body::Wagon: fRide = 1.45; break; default: fRide = 1.7; break;
        }
        if (!car->streetLegal) fRide = 2.8;
    }
    Suspension susp(SuspensionSetup::fromVehicle(vl.mass, vl.wheelbase, vl.track, vl.hCoG, vl.frontStatic,
                                                 fRide, fRide * 1.1, 0.30, 0.60));
    for (int i = 0; i < 4; ++i) susp.setTirePressure(i, w[i].psi());
    const RoadProfile roadProfile = RoadProfile::preset(o.road);
    const double rho = 1.20;
    double CdA = 0.68;
    if (car) {
        const double cd = car->body == Body::Super ? 0.34 : car->body == Body::SUV || car->body == Body::Pickup ? 0.45
                        : car->body == Body::Van ? 0.48 : car->body == Body::Muscle ? 0.40 : 0.34;
        CdA = cd * car->widthM * car->heightM * 0.85;
    }
    const double brakeTotal = 7000.0 * (baseMass / 1155.0), bias = 0.65; // %65 on / %35 arka, ABS yok

    std::printf("=== ZEHRA KINIK :: Guc Aktarma Simulasyonu ===\n");
    if (car) std::printf("Arac #%d: %s (%d) | %s %s | %.0f kg | %.0f HP\n", car->id, car->fullName().c_str(), car->year,
                         bodyName(car->body), driveName(drive), baseMass, peakPowerHp(*car));
    std::printf("Motor: %s | CR %.2f:1 (conta %.2f mm) | Yakit: %s\n", eng.name.c_str(),
                pt.compressionRatio(), o.gasket, fuelName(o.fuel));
    std::printf("Sanziman: %s, %zu vites, son disli %.2f | Aks: %s %.1f mm\n", gearboxName(boxType), gbx.ratios.size(),
                gbx.finalDrive, fail.axle().material.c_str(), fail.axle().diameterMm);
    std::printf("Karter: %s (%.1f L, kritik %.2f g) | Slick %.1f PSI | Depo %.0f L (%.1f kg)\n",
                o.drySump ? "KURU" : "ISLAK", o.oilL, o.drySump ? 99.0 : fail.criticalG(), o.psi, o.fuelL, fuelKg);
    std::printf("Diferansiyel: 1.5-Way plaka LSD (45/60 rampa) | I_w=%.3f kg*m^2\n", w[dL].inertia());
    std::printf("Suspansiyon: %.2f Hz on / %.2f Hz arka, sonum 0.30 bump / 0.60 rebound | Yol: %s (%s)\n\n", fRide,
                fRide * 1.1, o.road.c_str(), roadProfile.describe().c_str());

    FILE* csv = o.csv.empty() ? nullptr : std::fopen(o.csv.c_str(), "w");
    if (csv) std::fprintf(csv, "t,dist,v_kmh,ax_g,gear,rpm,wL,wR,kappaL,kappaR,vtec,fuel_g,tireL_C,tireR_C,FzF,TL,TR,tauL,oil_bar\n");

    // ---------------- Faz makinesi ----------------
    enum Phase { BURNOUT, SETTLE, STAGE, RUN, BRAKE, DONE } phase = o.burnout ? BURNOUT : STAGE;
    const double dt = 1e-5;                     // 100 kHz alt adim (debriyaj/LSD sertligi icin)
    double t = 0.0, phaseT = 0.0, V = 0.0, dist = 0.0, axRaw = 0.0, axF = 0.0;
    double tLaunch = -1.0, nextLog = 0.0, nextCsv = 0.0;
    double shiftT = -1.0; bool brakeHard = true;
    double t60 = -1, t100m = -1, t201 = -1, t305 = -1, t0100 = -1, peakG = 0, maxV = 0;
    const double reaction = 0.050;
    int treeStage = 0;
    double fuelAtLaunch = 0.0, hapticIntensity = 0.0;
    double footThr = 1.0;
    int suspCounter = 0, airLogs = 0;
    bool wasAir[4] = {false, false, false, false}, wasStop[4] = {false, false, false, false};
    const char* cornerName[4] = {"on sol", "on sag", "arka sol", "arka sag"}; // surucunun gaz ayagi: patinaji hedef kaymada tutar

    auto logEvent = [&](const std::string& s) {
        std::printf("  [%7.3f s] %s\n", tLaunch >= 0 ? t - tLaunch : t, s.c_str());
    };
    auto printHeader = [] {
        std::printf("%6s %7s %6s %2s %5s %6s %6s %6s %5s %7s %6s %6s %5s %5s\n", "t[s]", "x[m]", "km/h", "G",
                    "RPM", "wL", "wR", "kapL", "VTEC", "yakit_g", "LastL", "LastR", "tauL", "yag");
    };

    while (phase != DONE && t < 60.0) {
        // ---- surucu girdileri ----
        double brakePedal = 0.0, handbrake = 0.0;
        switch (phase) {
        case BURNOUT: {
            pt.setGear(1);
            // Once debriyaj basili devir yukselt, sonra debriyaji isirma noktasinda kaydirarak tut
            // (burnout'ta debriyaj tokatlanmaz; balata kaydirilir, aks korunur)
            pt.setTwoStep(true, 6000.0);
            pt.setThrottle(phaseT < 1.0 ? 1.0 : 0.8);
            // devir 3500 altina duserse debriyaja biraz daha basarak motoru stop ettirmez
            const double hold = pt.rpm() < 3500.0 ? 0.47 + (3500.0 - pt.rpm()) / 3500.0 * 0.2 : 0.47;
            pt.setClutchPedal(phaseT < 1.0 ? 1.0 : std::max(hold, 1.0 - (phaseT - 1.0) / 0.6));
            handbrake = 1.0;
            if (phaseT >= 5.0) { pt.setTwoStep(false, 0.0); phase = SETTLE; phaseT = 0.0; logEvent("Burnout bitti, on lastikler " +
                std::to_string((int)w[dL].tempC()) + "/" + std::to_string((int)w[dR].tempC()) + " C"); }
            break;
        }
        case SETTLE:
            pt.setThrottle(0.0); pt.setClutchPedal(1.0); brakePedal = 0.6;
            if (phaseT > 0.5 && std::fabs(w[dL].omega()) < 0.01 && std::fabs(w[dR].omega()) < 0.01) {
                if (pt.stalled()) { pt.restart(); logEvent("Mars basildi, motor yeniden calisti"); }
                phase = STAGE; phaseT = 0.0; logEvent("Stage isiklari yandi. 2-Step " + std::to_string((int)launchRpm) + " RPM aktif.");
            }
            break;
        case STAGE: {
            pt.setGear(1); pt.setClutchPedal(1.0); pt.setThrottle(1.0); pt.setTwoStep(true, launchRpm);
            brakePedal = 0.3;
            // Pro-tree degil, sportsman: 3 amber 0.5 s arayla, yesil
            const double tree0 = 2.0;
            if (treeStage < 3 && phaseT >= tree0 + 0.5 * treeStage) {
                logEvent(std::string("AGAC: amber ") + std::to_string(treeStage + 1)); ++treeStage;
            }
            if (treeStage == 3 && phaseT >= tree0 + 1.5) { logEvent("AGAC: *** YESIL ***"); ++treeStage; }
            if (treeStage == 4 && phaseT >= tree0 + 1.5 + reaction) {
                phase = RUN; phaseT = 0.0; tLaunch = t; fuelAtLaunch = pt.fuelGrams();
                pt.setTwoStep(false, 0.0);
                std::printf("\n>>> KALKIS! Tepki suresi %.3f s, debriyaj %d RPM'de tokatlandi\n\n",
                            reaction, (int)pt.rpm());
                printHeader();
                nextLog = t;
            }
            break;
        }
        case RUN: {
            double clutch = std::max(0.0, 1.0 - phaseT / o.dumpTime); // clutch dump
            // Ayak modulasyonu (pedal feathering): hedef kayma ~%12, drag slickin Pacejka tepesi
            const double kap = 0.5 * (w[dL].kappa() + w[dR].kappa());
            if (o.feather && phaseT > 0.12)
                footThr = std::clamp(footThr + dt * 8.0 * (0.12 - kap), pt.rpm() < 5500.0 ? 1.0 : 0.35, 1.0);
            double thr = footThr;
            if (shiftT < 0.0 && pt.rpm() > shiftRpm && pt.gear() < pt.gearCount()) shiftT = 0.0;
            if (shiftT >= 0.0 && (boxType == Gearbox::Dogbox || boxType == Gearbox::DCT)) {
                // Dogbox: 35 ms ateslemesi kesik debriyajsiz; DCT: kesintisiz (cift kavrama ortusmesi)
                if (shiftT == 0.0) {
                    pt.setGear(pt.gear() + 1);
                    logEvent("Vites " + std::to_string(pt.gear()) + (boxType == Gearbox::Dogbox ? " (dogbox, atesleme kesme)" : " (DCT)"));
                }
                const double dur = boxType == Gearbox::Dogbox ? 0.035 : 0.060;
                if (boxType == Gearbox::Dogbox) thr = 0.0;
                shiftT += dt;
                if (shiftT > dur) shiftT = -1.0;
            } else if (shiftT >= 0.0) {                           // H-desen / tork konvertorlu: ~270-350 ms
                const double k = boxType == Gearbox::TorqueConverter ? 1.3 : 1.0;
                if (shiftT < 0.10 * k)      { clutch = 1.0; thr = 0.0; }
                else if (shiftT < 0.17 * k) { clutch = 1.0; thr = 0.3;
                                          if (shiftT - dt < 0.10 * k) {
                                              pt.setGear(pt.gear() + 1);
                                              logEvent("Vites " + std::to_string(pt.gear()) + " (" +
                                                       std::to_string((int)pt.rpm()) + " rpm)"); } }
                else if (shiftT < 0.27 * k) { clutch = 1.0 - (shiftT - 0.17 * k) / (0.10 * k); thr = footThr; }
                else shiftT = -1.0;
                if (shiftT >= 0.0) shiftT += dt;
            }
            pt.setClutchPedal(clutch); pt.setThrottle(thr);
            if (dist >= 400.0) {
                const double et = t - tLaunch;
                std::printf("\n=== 400 m (1/4 mil) BITIS ===\n");
                std::printf("  60 ft : %6.3f s | 100 m : %6.3f s | 201 m (1/8): %6.3f s | 1000 ft: %6.3f s\n",
                            t60, t100m, t201, t305);
                std::printf("  0-100 km/h: %6.3f s | 400 m ET: %6.3f s | Trap: %.1f km/h | Tepe ivme: %.2f g\n",
                            t0100, et, V * 3.6, peakG);
                std::printf("  Kosuda yakilan yakit: %.1f g (toplam %.1f g) | Balata %.0f C | En yuksek aks gerilmesi %.0f MPa\n\n",
                            pt.fuelGrams() - fuelAtLaunch, pt.fuelGrams(), pt.clutchTempC(), fail.peakShearMPa());
                phase = BRAKE; phaseT = 0.0;
                logEvent("Parasut yok, ABS yok: frene asildi (%65 on / %35 arka)");
            }
            if (fail.snapped(0) || fail.snapped(1)) {
                logEvent("Aks kirik: arac yuruyemez. Cekici/romork cagrildi, krom-moly aks sart!");
                phase = BRAKE; phaseT = 0.0;
            }
            if (t - tLaunch > 40.0) { logEvent("Zaman asimi: 400 m tamamlanamadi."); phase = BRAKE; phaseT = 0; }
            break;
        }
        case BRAKE:
            pt.setThrottle(0.0); pt.setClutchPedal(1.0);
            if (brakeHard && V < 50.0 / 3.6) {
                brakeHard = false;
                if (maxV > 60.0 / 3.6) logEvent("Kilitlenmeyi fark etti: fren gevsetildi");
            }
            brakePedal = brakeHard ? 1.0 : 0.22;
            if (V < 0.05) phase = DONE;
            break;
        default: break;
        }

        // ---- guc aktarma ----
        pt.setAxleSnapped(fail.snapped(0), fail.snapped(1));
        pt.setIgnitionKilled(fail.bearingSpun());
        const double wLin = drive == Drive::AWD ? frontShare * w[0].omega() + (1 - frontShare) * w[2].omega() : w[dL].omega();
        const double wRin = drive == Drive::AWD ? frontShare * w[1].omega() + (1 - frontShare) * w[3].omega() : w[dR].omega();
        pt.step(dt, wLin, wRin, fail.oilPressureFactor());
        {
            const double sh = std::max(frontShare, 1.0 - frontShare);
            fail.updateAxles(dt, sh * pt.axleTorqueL(), sh * pt.axleTorqueR());
        }
        fail.updateOil(dt, axF, 0.0, pt.rpm());

        // ---- dinamik yuk + tekerlekler ----
        // ---- suspansiyon (2 kHz) -> dinamik tekerlek yukleri ----
        double Fz[4];
        vl.mass = baseMass + fuelKg;
        if (++suspCounter >= 50) {
            suspCounter = 0;
            susp.step(50 * dt, roadProfile, dist, axRaw, 0.0);
            for (int c = 0; c < 4; ++c) {
                if (susp.airborne(c) && !wasAir[c] && (phase == RUN || phase == BRAKE) && airLogs < 12) {
                    ++airLogs;
                    logEvent(std::string("Tekerlek havalandi: ") + cornerName[c] + " @ " + std::to_string((int)dist) + " m");
                }
                if (susp.onBumpStop(c) && !wasStop[c] && (phase == RUN || phase == BRAKE) && airLogs < 12) {
                    ++airLogs;
                    logEvent(std::string("Takoza vurdu (bottoming): ") + cornerName[c]);
                }
                wasAir[c] = susp.airborne(c); wasStop[c] = susp.onBumpStop(c);
            }
        }
        for (int c = 0; c < 4; ++c) Fz[c] = susp.tireLoad(c);
        const double bF = brakePedal * brakeTotal * bias * 0.5;
        const double bR = brakePedal * brakeTotal * (1 - bias) * 0.5 + handbrake * 1500.0;
        double sumFx = 0.0;
        for (int i = 0; i < 4; ++i) {
            w[i].setNormalLoad(Fz[i]);
            const double share = i < 2 ? frontShare : 1.0 - frontShare;
            const double Ta = share * ((i % 2 == 0) ? pt.axleTorqueL() : pt.axleTorqueR());
            w[i].step(dt, V, Ta, i < 2 ? bF : bR);
            sumFx += w[i].Fx();
            const double h = w[i].consumeHapticPulse();
            if (h > 0.0) hapticIntensity = std::max(hapticIntensity, h);
        }

        // ---- govde ----
        const double drag = 0.5 * rho * CdA * V * V;
        if (phase == BURNOUT || phase == STAGE || phase == SETTLE) { axRaw = 0.0; V = 0.0; } // line-lock / el freni tutar
        else {
            axRaw = (sumFx - drag) / vl.mass;
            V += axRaw * dt;
            if (V < 0.0) V = 0.0;
        }
        axF += (axRaw - axF) * std::min(1.0, dt / 0.08);   // govde pitch gecikmesi (suspansiyon)
        dist += V * dt;
        fuelKg = std::max(0.0, o.fuelL * fuelDensity - pt.fuelGrams() * 1e-3);
        t += dt; phaseT += dt;

        if (phase == RUN || phase == BRAKE) {
            const double tr = t - tLaunch;
            peakG = std::max(peakG, axF / 9.81); maxV = std::max(maxV, V);
            if (t60   < 0 && dist >= 18.288)  t60 = tr;
            if (t100m < 0 && dist >= 100.584) t100m = tr;
            if (t201  < 0 && dist >= 201.168) t201 = tr;
            if (t305  < 0 && dist >= 304.8)   t305 = tr;
            if (t0100 < 0 && V >= 100.0 / 3.6) { t0100 = tr; logEvent("0-100 km/h: " + std::to_string(tr).substr(0, 5) + " s"); }
        }

        for (auto& e : pt.drainEvents()) logEvent(e);
        for (auto& e : fail.drainEvents()) logEvent(e);

        if ((phase == RUN || phase == BRAKE || phase == DONE) && t >= nextLog) {
            nextLog += o.logInterval;
            std::printf("%6.2f %7.1f %6.1f %2d %5.0f %6.1f %6.1f %6.3f %5s %7.1f %6.1f %6.1f %5.0f %5.2f\n",
                        t - tLaunch, dist, V * 3.6, pt.gear(), pt.rpm(), w[dL].omega(), w[dR].omega(),
                        w[dL].kappa(), pt.vtecActive() ? "ON" : "off", pt.fuelGrams(), w[dL].tempC(),
                        w[dR].tempC(), fail.shearMPa(0), pt.oilPressureBar());
        }
        if (csv && t >= nextCsv) {
            nextCsv += 0.01;
            std::fprintf(csv, "%.3f,%.3f,%.2f,%.3f,%d,%.0f,%.2f,%.2f,%.4f,%.4f,%d,%.2f,%.1f,%.1f,%.0f,%.1f,%.1f,%.1f,%.2f\n",
                         tLaunch >= 0 ? t - tLaunch : t - 1000.0, dist, V * 3.6, axF / 9.81, pt.gear(), pt.rpm(),
                         w[dL].omega(), w[dR].omega(), w[dL].kappa(), w[dR].kappa(), pt.vtecActive() ? 1 : 0,
                         pt.fuelGrams(), w[dL].tempC(), w[dR].tempC(), Fz[dL] + Fz[dR], pt.axleTorqueL(),
                         pt.axleTorqueR(), fail.shearMPa(0), pt.oilPressureBar());
        }
    }

    std::printf("\n=== DURUS / HASAR RAPORU ===\n");
    std::printf("  Toplam mesafe %.1f m | Toplam yakit %.1f g | Kalan depo %.2f kg\n", dist, pt.fuelGrams(), fuelKg);
    const char* wn[4] = {"On Sol", "On Sag", "Arka Sol", "Arka Sag"};
    for (int i = 0; i < 4; ++i)
        std::printf("  %-8s: %5.1f C, %4.1f PSI, flat-spot %.2f mm, haptik vuruntu %d kez\n", wn[i], w[i].tempC(),
                    w[i].psi(), w[i].flatSpotMm(), w[i].hapticPulseCount());
    std::printf("  Suspansiyon: havalanma sayisi FL/FR/RL/RR = %d/%d/%d/%d | takoz = %d/%d/%d/%d\n",
                susp.airborneEvents(0), susp.airborneEvents(1), susp.airborneEvents(2), susp.airborneEvents(3),
                susp.bumpStopEvents(0), susp.bumpStopEvents(1), susp.bumpStopEvents(2), susp.bumpStopEvents(3));
    std::printf("  Tepe lastik yuku FL/FR/RL/RR = %.0f/%.0f/%.0f/%.0f N\n", susp.peakLoad(0), susp.peakLoad(1),
                susp.peakLoad(2), susp.peakLoad(3));
    if (hapticIntensity > 0.0)
        std::printf("  >> Telefon titresimi: flat-spot her turda %.0f%% siddetle vuruyor\n", hapticIntensity * 100.0);
    std::printf("  Akslar: sol %s (burulma %.1f deg), sag %s (burulma %.1f deg), tepe tau %.0f MPa\n",
                fail.snapped(0) ? "KIRIK" : "saglam", fail.twistDeg(0), fail.snapped(1) ? "KIRIK" : "saglam",
                fail.twistDeg(1), fail.peakShearMPa());
    std::printf("  Motor yataklari: hasar %%%.0f %s | Motor: %s\n", fail.bearingDamage() * 100.0,
                fail.bearingSpun() ? "(SARDI)" : "", pt.stalled() ? "stop etti" : "calisiyor");
    if (csv) { std::fclose(csv); std::printf("  Telemetri: %s\n", o.csv.c_str()); }
    return 0;
}
