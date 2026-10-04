#include "VehicleSim.h"
#include "sim/PartTables.h"
#include <algorithm>
#include <cmath>

namespace zk {

namespace {
// Parca etkileri icin tork egrisi carpani (rpm'e bagli olabilir: turbo kiti spool)
void scaleCurves(EngineSpec& e, double (*f)(double rpm, const void*), const void* ctx) {
    for (auto* c : {&e.lowCam, &e.highCam}) for (auto& pr : *c) pr.second *= f(pr.first, ctx);
}
struct TurboCtx { double full, spoolStart, spoolFull; };
double turboMul(double rpm, const void* c) {
    const TurboCtx& t = *static_cast<const TurboCtx*>(c);
    const double x = std::clamp((rpm - t.spoolStart) / (t.spoolFull - t.spoolStart), 0.0, 1.0);
    return 1.0 + (t.full - 1.0) * x * x * (3 - 2 * x);
}
double constMul(double, const void* c) { return *static_cast<const double*>(c); }
// Egri bicimi: dusuk devir (<= %40 redline) carpani -> yuksek devir (>= %75) carpani
struct ShapeCtx { double red, low, high; };
double shapeMul(double rpm, const void* c) {
    const ShapeCtx& s = *static_cast<const ShapeCtx*>(c);
    // 0.40 red'de alt etki, 0.85 red'de tam ust etki; ustunde (ECU ile acilan bolge) %40'a kadar artmaya devam eder
    const double x = std::clamp((rpm / s.red - 0.40) / 0.45, 0.0, 1.4);
    return s.low + (s.high - s.low) * x;
}
// Kompresor: roots / twin-screw dusuk devirden tam boost (parazitik kayip artar), santrifuj devirle karesel
struct SuperCtx { double red, base, bar; int type; };
double superMul(double rpm, const void* c) {
    const SuperCtx& s = *static_cast<const SuperCtx*>(c);
    const double f = rpm / s.red;
    double g, loss;
    if (s.type == 2) { g = std::min(1.0, f * f); loss = 0.02 * f; }
    else { const double x = std::clamp((f - 0.10) / 0.22, 0.0, 1.0); g = x * x * (3 - 2 * x); loss = (s.type == 0 ? 0.045 : 0.03) * f; }
    return (1.0 + s.base + s.bar * g) / (1.0 + s.base) - loss;
}
// Devir siniri yukseltildiyse egriyi yeni sinira uzat (son egimle, nefessizlik artarak)
void extendCurve(TorqueCurve& c, double newMax) {
    if (c.size() < 2) return;
    while (c.back().first < newMax) {
        const auto a = c[c.size() - 2], b = c.back();
        const double slope = std::min((b.second - a.second) / (b.first - a.first), -b.second * 0.00004);
        c.push_back({b.first + 250.0, std::max(0.0, b.second + slope * 250.0 * 0.85)});
    }
}
double peakHp(const EngineSpec& e) {
    double m = 0.0;
    for (auto* c : {&e.lowCam, &e.highCam}) for (auto& pr : *c) if (pr.first <= e.redlineRpm) m = std::max(m, pr.second * pr.first / 7120.9);
    return m;
}
template <class T> const T& row(const std::vector<T>& t, int i) { return t[std::clamp(i, 0, (int)t.size() - 1)]; }
double curveMax(const EngineSpec& e) {
    double m = 0.0;
    for (auto* c : {&e.lowCam, &e.highCam}) for (auto& pr : *c) m = std::max(m, pr.second);
    return m;
}
} // namespace

VehicleSim::VehicleSim(const VehicleSimConfig& cfg) : cfg_(cfg) {
    const VehicleDef* car = cfg.car;
    const Tune* tune = cfg.tune;
    if (tune) { cfg_.drySump = tune->drySump; cfg_.fuel = tune->fuel; }
    const int engIdx = car ? effectiveEngine(*car, tune) : -1, gbIdx = car ? effectiveGearbox(*car, tune) : -1;
    eng_ = car ? buildEngineSpecFor(engIdx) : EngineSpec::K20Default();
    gbx_ = car ? buildGearboxFor(gbIdx) : GearboxSpec{};
    eng_.gasketMm = cfg_.gasketMm; eng_.fuel = cfg_.fuel;
    if (!car) {
        eng_.valvetrain = cfg.valvetrain;
        if (cfg.plenum) { eng_.intake = IntakeType::Plenum; eng_.name = "K20A (Plenum, 16V i-VTEC)"; }
    }
    const double TmaxFactory = car ? curveMax(buildEngineSpec(*car)) : curveMax(eng_);   // aks capi fabrika torkuna gore
    DiffSpec diff;
    if (tune) {
        // ---- parcalar (PartTables; ilk satirlar v1 seviyeleri, ayni islem sirasi) ----
        const EngineDef* ed = car ? &engineTable()[engIdx] : nullptr;
        const bool forced = ed && (ed->induction == Induction::Turbo || ed->induction == Induction::TwinTurbo ||
                                   ed->induction == Induction::Supercharger);
        const double hp0 = peakHp(eng_), red0 = eng_.redlineRpm;
        const IntakeOpt& IT = row(intakeTable(), tune->intake);
        const ShapeOpt& EX = row(exhaustTable(), tune->exhaust);
        bool knockSensor = true;
        const EcuOpt EC = effectiveEcu(*tune, &knockSensor);
        const double ecuMul = (forced || tune->turbo || tune->superch) ? EC.forced : EC.na;
        if (IT.low == IT.high && EX.low == EX.high) {
            double mul = IT.low * EX.low * ecuMul;
            scaleCurves(eng_, constMul, &mul);
        } else {
            ShapeCtx a{red0, IT.low, IT.high}, b{red0, EX.low, EX.high};
            scaleCurves(eng_, shapeMul, &a); scaleCurves(eng_, shapeMul, &b);
            double m = ecuMul; scaleCurves(eng_, constMul, &m);
        }
        {   // Bilesenler: gaz kelebegi, emme manifoldu, egzoz manifoldu, katalizor (0 = stok: etkisiz)
            const IntakeOpt& TH = row(throttleTable(), tune->throttleBody);
            const IntakeOpt& MA = row(intakeManiTable(), tune->intakeMani);
            const ShapeOpt& HE = row(headerTable(), tune->header);
            const ShapeOpt& CT = row(catalystTable(), tune->catalyst);
            auto apply = [&](double lo, double hi) { ShapeCtx c{red0, lo, hi}; scaleCurves(eng_, shapeMul, &c); };
            if (tune->throttleBody > 0) apply(TH.low, TH.high);
            if (tune->intakeMani > 0) apply(MA.low, MA.high);
            if (tune->header > 0) apply(HE.low, HE.high);
            if (tune->catalyst > 0) apply(CT.low, CT.high);
        }
        const double ic = row(intercoolerTable(), tune->intercooler).eff * row(methTable(), tune->meth).eff;   // + su-metanol
        const TurbineOpt& TB = row(turbineTable(), tune->turbine);
        boostFac_ = boostTot_ = ed ? ed->boostBar : 0.0;
        superOnly_ = ed && ed->induction == Induction::Supercharger;
        icCredit_ = (ic - 1.0) * 60.0;                                         // soguk sarj: 1.12 -> ~7 oktan
        ecuAgg_ = boostFac_ > 0 || tune->turbo > 0 || tune->superch > 0 ? (EC.forced - 1.0) * 20.0 : (EC.na - 1.0) * 30.0;
        knockSensor_ = knockSensor;                                            // standalone (vuruntu modulu yok) / eski yaris haritasi
        const double gate = row(wastegateTable(), tune->wastegate).mul * (1.0 + row(boostCtlTable(), tune->boostCtl).extra);
        if (tune->turbo > 0) {
            const TurboOpt T = tune->turbo == kCustomTurbo ? customTurbo(*tune) : row(turboTable(), tune->turbo);
            const double baseBoost = ed ? ed->boostBar : 0.0;
            const double add = (tune->turbine || tune->wastegate || tune->boostCtl) ? T.bar * ic * TB.top * gate : T.bar * ic;
            boostTot_ = baseBoost + add / ic;
            TurboCtx t{(1.0 + baseBoost + add) / (1.0 + baseBoost), eng_.redlineRpm * (T.spoolLo - EC.spool + TB.spool),
                       eng_.redlineRpm * (T.spoolHi - EC.spool + TB.spool)};
            scaleCurves(eng_, turboMul, &t);
            spoolLo_ = t.spoolStart; spoolHi_ = t.spoolFull;
        } else if (ed && ed->boostBar > 0.0 && (tune->turbine || tune->wastegate || tune->boostCtl)) {
            // Fabrika turbosu: wastegate / boost kontrol / turbin fabrika boostunu yukseltir (spool fabrika egrisinde)
            const double b0 = ed->boostBar, b1 = b0 * gate * TB.top * ic;
            boostTot_ = b1 / ic;
            TurboCtx t{(1.0 + b1) / (1.0 + b0), eng_.redlineRpm * (0.25 + TB.spool), eng_.redlineRpm * (0.45 + TB.spool)};
            scaleCurves(eng_, turboMul, &t);
            spoolLo_ = t.spoolStart; spoolHi_ = t.spoolFull;
        }
        if (tune->superch > 0) {
            const SuperOpt& S = row(superTable(), tune->superch);
            SuperCtx sc{red0, ed ? ed->boostBar : 0.0, S.bar * ic, S.type};
            if (tune->turbo == 0 && !(ed && ed->induction != Induction::Supercharger && ed->boostBar > 0)) superOnly_ = true;
            boostTot_ += S.bar;
            scaleCurves(eng_, superMul, &sc);
        }
        if (ecuFineTuneAvailable(*tune) && (tune->ecuTiming || tune->ecuAfr || tune->ecuBoost)) {
            // Dyno ince ayari: avans +%1/derece; AFR 12.8'de en iyi (zengin: guvenli ama guc kaybi, fakir: sicak + vuruntu);
            // boost hedefi yalniz asiri beslemeli motorda (toplam boostla orantili tork)
            const double afr = tune->ecuAfr > 0 ? tune->ecuAfr / 10.0 : 12.5;
            double m = (1.0 + 0.010 * std::clamp(tune->ecuTiming, -4, 6)) * (1.0 - 0.02 * (afr - 12.8) * (afr - 12.8));
            if (boostTot_ > 0.0) m *= std::max(0.5, (1.0 + boostTot_ + tune->ecuBoost / 10.0) / (1.0 + boostTot_));
            scaleCurves(eng_, constMul, &m);
            fineKnock_ = 0.9 * tune->ecuTiming + 3.0 * std::max(0.0, afr - 12.5) + (boostTot_ > 0.0 ? 8.0 * tune->ecuBoost / 10.0 : 0.0);
            heatMul_ = 1.0 + 0.06 * std::max(0.0, afr - 12.5) + 0.01 * std::max(0, tune->ecuTiming);
        }
        const CamOpt C = tune->cam == kCustomCam ? customCam(*tune) : row(camTable(), tune->cam);
        if (tune->cam > 0) { ShapeCtx c{red0, C.low, C.high}; scaleCurves(eng_, shapeMul, &c); }
        const HeadOpt& HD = row(headTable(), tune->head);
        if (tune->head > 0) { ShapeCtx h{red0, HD.low, HD.high}; scaleCurves(eng_, shapeMul, &h); }
        const PistonOpt& PI = row(pistonTable(), tune->piston);
        if (PI.mul != 1.0) { double m = PI.mul; scaleCurves(eng_, constMul, &m); }
        const CrankOpt& CR = row(crankTable(), tune->crank);
        const double disp = tune->crank == kCustomCrank ? 1.0 + std::clamp(tune->custDisp, 0.0, 0.40) : CR.disp;
        const double inRed = (tune->crank == kCustomCrank ? -1500.0 * std::clamp(tune->custDisp, 0.0, 0.40) : CR.redline) + HD.redline;
        if (disp != 1.0) { double d = disp; scaleCurves(eng_, constMul, &d); eng_.inertia *= std::sqrt(disp); }
        eng_.inertia *= CR.inertia * row(rodTable(), tune->rod).inertia;
        const GasketOpt& GK = row(gasketTable(), tune->gasket);
        if (GK.mm > 0.0) { eng_.gasketMm = GK.mm; cfg_.gasketMm = GK.mm; }
        const FuelOpt& FU = row(fuelTable(), tune->fuelSel);
        if (FU.mul != 1.0) { double f = FU.mul; scaleCurves(eng_, constMul, &f); }
        // Devir siniri (kesici): YALNIZ ECU acar (stroker krank dusurur). Supap / kam / kafa kesiciyi degistirmez:
        // motorun guvenle donebilecegi devri (potansiyel) yukseltir; ECU ile bunun ustune cikilirsa supap zorlanir.
        const double redAdd = EC.redline + (tune->crank == kCustomCrank ? -1500.0 * std::clamp(tune->custDisp, 0.0, 0.40) : CR.redline);
        const double potential = row(valveTable(), tune->valve).redline + (tune->cam > 0 ? C.redline : 0.0) + HD.redline;
        valveSafeRpm_ = std::max(eng_.idleRpm + 2500.0, red0 + redAdd - EC.redline + 250.0 + potential);
        // Kam / kafa nefesi guc tepesini yukari tasir (egri tepe ustu gerilir); ECU devri acinca bu bolge kullanilir
        // ECU devri de tepeyi tasir (harita / avans ust devire gore): acilan devrin %60'i; kam / kafa ustune ekler
        const double breathe = 1.2 * ((tune->cam > 0 ? C.redline : 0.0) + HD.redline) + 0.6 * std::max(0.0, EC.redline);
        if (breathe > 0.0) {
            const double piv = 0.45 * red0, k = (red0 + breathe - piv) / (red0 - piv);
            for (auto* c : {&eng_.lowCam, &eng_.highCam}) for (auto& pr : *c) if (pr.first > piv) pr.first = piv + (pr.first - piv) * k;
        }
        if (redAdd > 0.0 || breathe > 0.0) {
            // Katalog egrisi stok kesicinin hemen ustunde ucurumdan duser: (gerilmis) stok kesici ustu atilir, yerine
            // yavas dusen kuyruk (ECU ile acilan devir ise yarar; kam / kafa yoksa guc tepeden sonra yavasca azalir)
            const double keep = red0 + breathe + 1.0;
            for (auto* c : {&eng_.lowCam, &eng_.highCam})
                while (c->size() > 2 && c->back().first > keep) c->pop_back();
        }
        if (redAdd != 0.0) {
            eng_.redlineRpm = std::max(eng_.idleRpm + 2500.0, red0 + redAdd);
            extendCurve(eng_.lowCam, eng_.redlineRpm + 500.0); extendCurve(eng_.highCam, eng_.redlineRpm + 500.0);
        }
        extendCurve(eng_.lowCam, eng_.redlineRpm + 500.0); extendCurve(eng_.highCam, eng_.redlineRpm + 500.0);   // kesici ustu (sekme)
        eng_.inertia *= row(flywheelTable(), tune->flywheel).inertia;
        // Yipranma: dusuk kompresyon / elektrik arizasi guc kaybi
        if (tune->wearEngine > 0 || tune->wearElec > 0) {
            double w = (1.0 - 0.40 * std::clamp(tune->wearEngine, 0.0, 1.0)) * (1.0 - 0.10 * std::clamp(tune->wearElec, 0.0, 1.0));
            scaleCurves(eng_, constMul, &w);
        }
        // Yakit sistemi siniri: fabrika gucunun cap katini asan tork kirpilir (buyuk turbo / kompresor yakit ister).
        // v1 turbo kitleri yakit yukseltmesini icerir.
        double cap = fuelCap(*tune);
        if (tune->turbo == 1 || tune->turbo == 2) cap = std::max(cap, 3.5);
        const double capHp = hp0 * cap;
        for (auto* c : {&eng_.lowCam, &eng_.highCam})
            for (auto& pr : *c)
                if (pr.first > 800.0 && pr.first <= eng_.redlineRpm && pr.second > capHp * 7120.9 / pr.first * 1.02) fuelCapped_ = true;
        for (auto* c : {&eng_.lowCam, &eng_.highCam})
            for (auto& pr : *c) if (pr.first > 800.0) pr.second = std::min(pr.second, capHp * 7120.9 / pr.first);
        // Sanziman: ozel vites oranlari (fabrika oranina carpan)
        if (tune->gearSwap == kCustomGear)
            for (size_t i = 0; i < gbx_.ratios.size() && i < 8; ++i) if (tune->custGear[i] > 0) gbx_.ratios[i] *= std::clamp(tune->custGear[i], 0.6, 1.5);
        if (tune->finalSel > 0) {
            gbx_.finalDrive = tune->finalSel == kCustomFinal ? std::clamp(tune->custFinal > 0 ? tune->custFinal : gbx_.finalDrive, 2.0, 7.5)
                                                              : gbx_.finalDrive * row(finalTable(), tune->finalSel).mul;
        } else if (tune->finalDrive > 0.0) gbx_.finalDrive = tune->finalDrive;
        const DiffOpt& D = row(diffTable(), (int)tune->diff);
        diff.preload = D.preload * (lsdAdjustable(*tune) ? 1.0 + 0.15 * std::clamp(tune->setPreload, -5, 5) : 1.0);   // kurulum: on yuk
        diff.plateFactor = D.plate; diff.rampAccelDeg = D.rampA; diff.rampDecelDeg = D.rampD;
        const NosOpt& N = row(nosTable(), tune->nitrous);
        const double jet = tune->setNos >= 50 ? std::min(tune->setNos, 95) / 100.0 : 1.0;     // kucuk meme: az guc, uzun tup
        nosHp_ = N.hp * jet; nosLeft_ = nosBottle_ = N.bottleS / jet;
        const ElecOpt& E = row(elecTable(), tune->elec);
        if (tune->elec > 0) { tcSlip_ = E.tcSlip; absSlip_ = E.absSlip; }
    }
    double Tmax = curveMax(eng_);
    ClutchSpec clutch;
    if (car) clutch.maxTorque = Tmax * 1.7;          // sokak baskisi: motor torkunun ~1.7 kati
    if (tune) clutch.maxTorque *= row(clutchTable(), tune->clutch).mul;
    // Dayanim ve sogutma: sinir ustu tork hasar biriktirir; isi modeli fabrika gucune gore boyutlu radyator
    if (car && tune) {                                                         // yalniz oyun kurulumu (zehra_sim referansi degismez)
        const Durability du = durability(*car, tune);
        engRating_ = du.engineNm; gbRating_ = du.gearboxNm;
        const double factoryW = peakHp(buildEngineSpec(*car)) * 745.7;
        coolCap_ = factoryW * 0.33 / 105.0 * coolingCapMul(*tune);           // W/K (tam yuk, 40 m/s'de 105 C dengesi)
        coolLow_ = coolingLowSpeed(*tune);                                    // radyator x fan x termostat x yag sogutucu
    }
    pt_ = std::make_unique<PowertrainCore>(eng_, clutch, diff, gbx_);
    tmax_ = std::max(1.0, Tmax);
    if (car && tune) {   // oktan: secilen yakit; istenen: fabrika ~90 + sikistirma + fabrika ustu boost + ECU - ara sogutucu
        static const double kOct[10] = {100, 95, 105, 98, 102, 104, 110, 116, 101, 112};
        octane_ = tune->fuelSel > 0 && tune->fuelSel < 10 ? kOct[tune->fuelSel]
                : tune->fuel == FuelType::Pump95 ? 95.0 : tune->fuel == FuelType::E85 ? 105.0 : 100.0;
        knockReq_ = 90.0 + 2.2 * (pt_->compressionRatio() - 10.5) + 8.0 * std::max(0.0, boostTot_ - boostFac_) + ecuAgg_ - icCredit_ + fineKnock_;
    }
    drive_ = car ? car->drive : Drive::FWD;
    boxType_ = car ? gearboxTable()[gbIdx].type : Gearbox::HPattern;
    // ABS / TC: fabrika donanimi ya da ECU kiti; yalniz oyun kurulumunda (tune var)
    absAvail_ = car && tune && (car->abs || tune->absKit) && tune->wearElec < 0.5;     // bozuk elektrik: ABS/TC calismaz
    tcAvail_ = car && tune && (car->tc || tune->tcKit) && tune->wearElec < 0.5;
    tcOn_ = tcAvail_;
    // Otomatik: debriyaj yerine tork konvertoru (stall devri %42 redline)
    if (boxType_ == Gearbox::TorqueConverter) pt_->setConverter(true, 0.42 * eng_.redlineRpm, PowertrainCore::kConverterTr0);
    // Cekis dagitimi: AWD'de merkez dagitim %40 on / %60 arka (sabit oranli)
    frontShare_ = drive_ == Drive::FWD ? 1.0 : drive_ == Drive::RWD ? 0.0 : 0.40;
    dL_ = drive_ == Drive::RWD ? 2 : 0;
    // Aks capi (fabrika muhendisligi): stok debriyajin aktarabilecegi en yuksek torkta (1.7 x FABRIKA tepe
    // torku, 1. vites) stok celigin kopma dayanimimin %75'i. Stok arac stok kalkista saglam kalir; guc ve
    // debriyaj yukseltildikce stok aksin kirilma riski gercekci bicimde dogar. Otomatikte tasarim torku
    // konvertorun stall tork carpimi (1.9 x).
    const AxleOpt& AX = row(axleTable(), tune ? tune->axles : (cfg.stockAxles ? 0 : 1));
    const int axleLevel = AX.spec;
    AxleSpec axle = axleLevel == 2 ? AxleSpec::Race() : axleLevel == 1 ? AxleSpec::Chromoly() : AxleSpec::Stock();
    if (car) {
        const GearboxSpec factory = buildGearbox(*car);
        const double G1 = factory.ratios.front() * factory.finalDrive;
        const double design = boxType_ == Gearbox::TorqueConverter ? PowertrainCore::kConverterTr0 : 1.7;
        const double perSide = design * TmaxFactory * G1 * factory.efficiency * 0.5 * std::max(frontShare_, 1.0 - frontShare_);
        const double dStock = std::cbrt(16.0 * perSide / (3.14159265 * 0.75 * AxleSpec::Stock().tauUltMPa * 1e6)) * 1000.0;
        axle.diameterMm = dStock * AX.dia;                                    // yukseltme akslari kalindir
    }
    double oilL = cfg.oilLiters;
    if (tune && tune->oil > 0) { const OilOpt& O = row(oilTable(), tune->oil); cfg_.drySump = cfg_.drySump || O.dry; oilL = O.liters; }
    if (tune && tune->oilPump > 0) oilL += row(oilPumpTable(), tune->oilPump).liters;   // yag pompasi / akumulator
    fail_ = std::make_unique<DrivetrainFailure>(axle, LubeSpec{cfg_.drySump, oilL, 3.0});

    const double ambient = 25.0;
    const bool fDriven = frontShare_ > 0.0, rDriven = frontShare_ < 1.0;
    if (!tune) {
        TireParams slick;                           // tahrikli aks: yapiskan drag slick, dovme jant
        TireParams skinny; skinny.muPeak = 1.0; skinny.wheelMass = 9.0; skinny.B = 10.0; // serbest aks: ince "skinny"
        for (int i = 0; i < 4; ++i) {
            const bool d = i < 2 ? fDriven : rDriven;
            w_.emplace_back(d ? slick : skinny, d ? cfg.slickPsi : 32.0, d ? 55.0 : 30.0, ambient);
        }
    } else {
        // Lastik tipi: sokak / yari-slick / drag slick (tahrikli aks); serbest aks sokak lastigi
        // Sicaklik penceresi: sokak lastigi soguk da tutar (genis pencere), yari-slick ~75 C ister,
        // drag slick dar pencereli (burnout sart). Soguk sokak lastigi ~%97, soguk slick ~%55 tutus.
        TireParams street; street.muPeak = 1.05; street.latGrip = 1.067; street.B = 10.0; street.wheelMass = 16.0;   // yanal ~1.12, boyuna ayni
        street.tempIdeal = 55.0; street.tempWidth = 0.00004;
        TireParams semi;   semi.muPeak = 1.25; semi.latGrip = 1.12;   semi.B = 11.0;   semi.wheelMass = 15.0;
        semi.tempIdeal = 75.0;   semi.tempWidth = 0.00007;
        TireParams slick;  // 1.45, dovme jant
        // Kesin lastik (tireTable): sinifin tutus carpani + teker kutlesi; jant kutlesi
        const TireOpt& TR = row(tireTable(), tune->tireSel > 0 ? tune->tireSel : (int)tune->tires);
        const double wkg = TR.wheelKg + row(rimTable(), tune->rims).wheelKg;
        const double tireWear = 1.0 - 0.35 * std::clamp(tune->wearTires, 0.0, 1.0);        // kel / sertlesmis lastik
        for (TireParams* p : {&street, &semi, &slick}) { p->muPeak *= TR.grip * tireWear; p->wheelMass += wkg; }
        const TireParams* dt = tune->tires == TireType::DragSlick ? &slick : tune->tires == TireType::SemiSlick ? &semi : &street;
        const double defPsi = tune->tires == TireType::DragSlick ? 16.0 : tune->tires == TireType::SemiSlick ? 26.0 : 32.0;
        const double defTemp = tune->tires == TireType::DragSlick ? 55.0 : tune->tires == TireType::SemiSlick ? 45.0 : 35.0;
        // Yol arabasi dengesi: arka aks yanalda %8 fazla tutar (fabrika ayari gibi hafif understeer)
        TireParams dR = *dt; dR.latGrip *= 1.08;
        TireParams semiR = semi, streetR = street; semiR.latGrip *= 1.08; streetR.latGrip *= 1.08;
        for (int i = 0; i < 4; ++i) {
            const bool d = i < 2 ? fDriven : rDriven;
            if (d) w_.emplace_back(i < 2 ? *dt : dR, tune->psi > 0 ? tune->psi : defPsi, defTemp, ambient);
            else if (tune->tires == TireType::SemiSlick) w_.emplace_back(i < 2 ? semi : semiR, 30.0, defTemp, ambient);   // yari-slick 4 teker takim
            else   w_.emplace_back(i < 2 ? street : streetR, 32.0, 30.0, ambient);
        }
    }
    if (cfg.laneAsymmetry) w_[dL_].setSurfaceMu(0.96);

    fuelDensity_ = (cfg_.fuel == FuelType::E85) ? 0.785 : 0.745;
    baseMass_ = (car ? car->massKg : 1080.0) + 75.0 - (tune ? tune->totalWeightKg() : 0);   // kuru arac + surucu - hafifletme
    if (tune && car) baseMass_ += swapMassDelta(*car, *tune) + 45.0 * std::clamp(tune->wearBody, 0.0, 1.0);   // pas / macun
    fuelKg_ = cfg.fuelLiters * fuelDensity_;
    const SuspOpt& SU = row(suspTable(), tune ? tune->susp : 0);
    const bool adj = tune && suspAdjustable(*tune);                         // kurulum ayarlari (ayarli suspansiyon)
    const double hCoG = (car ? 0.36 * car->heightM : 0.50) - (cfg_.drySump ? 0.012 : 0.0) - SU.lowerMm * 0.0008
                      + (adj ? 0.5 * (std::clamp(tune->setRide, -40, 20) + std::clamp(tune->setRideR, -40, 20)) * 0.0008 : 0.0);   // yukseklik: kucuk = alcak agirlik merkezi
    vl_ = VehicleLoad{baseMass_ + fuelKg_, car ? car->wheelbaseM : 2.62, car ? car->widthM * 0.85 : 1.50, hCoG,
                      car ? car->frontWeight : 0.62};
    // Suspansiyon: kasa tipine gore dogal frekans (Hz); yaris araclari sert
    fRide_ = 1.6;
    if (car) {
        switch (car->body) {
        case Body::Super: fRide_ = 2.2; break;   case Body::Roadster: fRide_ = 1.9; break;
        case Body::Muscle: fRide_ = 1.4; break;  case Body::SUV: case Body::Pickup: case Body::Van: fRide_ = 1.2; break;
        case Body::Sedan: case Body::Wagon: fRide_ = 1.45; break; default: fRide_ = 1.7; break;
        }
        if (!car->streetLegal) fRide_ = 2.8;
        fRide_ *= SU.ride * (tune ? 1.0 - 0.30 * std::clamp(tune->wearSusp, 0.0, 1.0) : 1.0);   // bitik amortisor: yumusak
        // (on / arka yay ayari asagida aks basina)
    }
    {
        // Kurulum: on / arka yay (dogal frekans %5 adim) ve amortisor (sonum %8 adim) ayri
        const double kF = adj ? 1.0 + 0.05 * std::clamp(tune->setSpring, -5, 5) : 1.0, kR = adj ? 1.0 + 0.05 * std::clamp(tune->setSpringR, -5, 5) : 1.0;
        SuspensionSetup ss = SuspensionSetup::fromVehicle(vl_.mass, vl_.wheelbase, vl_.track, vl_.hCoG, vl_.frontStatic,
                                                          fRide_ * kF, fRide_ * 1.1 * kR, 0.30, 0.60);
        if (adj) {
            const double dF = 1.0 + 0.08 * std::clamp(tune->setDamp, -5, 5), dR = 1.0 + 0.08 * std::clamp(tune->setDampR, -5, 5);
            ss.front.bumpC *= dF; ss.front.reboundC *= dF; ss.rear.bumpC *= dR; ss.rear.reboundC *= dR;
        }
        if (adj) { ss.arbFront *= 1.0 + 0.2 * std::clamp(tune->setArbF, -3, 3); ss.arbRear *= 1.0 + 0.2 * std::clamp(tune->setArbR, -3, 3); }
        susp_ = std::make_unique<Suspension>(ss);
    }
    for (int i = 0; i < 4; ++i) susp_->setTirePressure(i, w_[i].psi());
    road_ = std::make_unique<RoadProfile>(RoadProfile::preset(cfg.road));
    if (car) {
        const double cd = car->body == Body::Super ? 0.34 : car->body == Body::SUV || car->body == Body::Pickup ? 0.45
                        : car->body == Body::Van ? 0.48 : car->body == Body::Muscle ? 0.40 : 0.34;
        CdA_ = cd * car->widthM * car->heightM * 0.85;
    }
    brakeTotal_ = 4800.0 * (baseMass_ / 1155.0);                         // tam pedal ~1.35 g (sokak lastigi tam basista kilitlenebilir)
    if (tune) {
        brakeTotal_ *= row(brakeTable(), tune->brakes).mul * row(brakeDiscTable(), tune->brakeDisc).mul * row(brakeCaliperTable(), tune->brakeCaliper).mul
                     * (1.0 - 0.5 * std::clamp(tune->wearBrakes, 0.0, 1.0));     // balata x disk x kaliper
        CdA_ *= 1.0 + 0.05 * std::clamp(tune->wearBody, 0.0, 1.0);
        const AeroOpt A = tune->aero == kCustomAero ? customAero(*tune) : row(aeroTable(), tune->aero);
        const AeroOpt& F = row(aeroFrontTable(), tune->aeroFront), &S = row(aeroSideTable(), tune->aeroSide), &U = row(aeroUnderTable(), tune->aeroUnder);
        CdA_ *= A.cd * F.cd * S.cd * U.cd;                                    // arka + on + yan + alt
        dfK_ = (A.downforce + F.downforce + S.downforce + U.downforce) / (27.78 * 27.78);   // N / (m/s)^2
    }
    // Yaw ataleti: ~ m * (dingil/2)^2 * 1.1 (tipik binek: 1200 kg, 2.6 m -> ~2200 kg m^2)
    Iz_ = vl_.mass * std::pow(0.5 * vl_.wheelbase, 2.0) * 1.1 + 0.08 * vl_.mass * vl_.track * vl_.track;
}

double VehicleSim::defaultLaunchRpm() const {
    if (boxType_ == Gearbox::TorqueConverter) return 0.42 * eng_.redlineRpm;   // konvertor stall devri
    if (cfg_.tune && cfg_.tune->launchRpm > 0 && launchControlAvailable(*cfg_.tune))   // launch yazilimiyla ayarlanan 2-step
        return std::clamp((double)cfg_.tune->launchRpm, eng_.idleRpm + 600.0, eng_.redlineRpm - 200.0);
    return cfg_.car ? std::clamp(0.55 * eng_.redlineRpm, 3000.0, 6500.0) : 6500.0;
}

// ABS: kilitlenen (kayma < -0.15) tekerlegin frenini birakir, tutunca geri basar (~10 Hz cevrim).
// TC: cekis tekerlegi kaymasi 0.15'i asinca motor torku kesilir, tutunca geri verilir. Yalniz mevcutsa (fabrika ya
// da ECU kiti) calisir; Faz 1 kurulumu (tune yok, zehra_sim) etkilenmez.
void VehicleSim::updateElectronics(double dt, double brake) {
    if (absAvail_) {
        for (int i = 0; i < 4; ++i) {
            // Anlik kayma (gevsemeli kappa gecikir): -%12'yi asan tekerlegin freni asimla orantili birakilir
            const double s = (w_[i].omega() * w_[i].rEff() - speed()) / std::max(speed(), 2.0);
            if (brake > 0.05 && speed() > 1.5 && s < -absSlip_) absF_[i] = std::max(0.05, absF_[i] - dt * 80.0 * std::min(-s - absSlip_, 1.0));
            else absF_[i] = std::min(1.0, absF_[i] + dt * 6.0);
        }
    }
    if (tcAvail_) {
        // Anlik kayma: tekerlek cevre hizi vs arac hizi (gevsemeli kappa dusuk hizda gecikir, kontrol salinirdi).
        // Oransal: hedefin (0.10) asimiyla orantili hizli kesme, altinda yavas geri verme.
        // Kalkis dostu: dusuk hizda kayma orani patlar (0.5 m/s'de 1 m/s patinaj = %100); izin verilen patinaj en az
        // 2.5 m/s (kontrollu patinaj = en iyi kalkis). Motor kalkis devrinin (2-step / varsayilan) altina dusmez: devir
        // oraya inince kesme durur ve tork hizla geri verilir (bogulma / stop yok). Kesme tabani kalkista %35.
        const double V = std::max(speed(), 2.0);
        double s = 0.0;
        for (int i = 0; i < 4; ++i)
            if ((i < 2 && frontShare_ > 0.0) || (i >= 2 && frontShare_ < 1.0))
                s = std::max(s, (w_[i].omega() * w_[i].rEff() - speed() - std::max(0.0, 2.5 - tcSlip_ * V)) / V);
        const double holdRpm = defaultLaunchRpm();                           // 2-step ayari ya da arac varsayilani
        const bool bogging = pt_->rpm() < holdRpm && speed() < 8.0;          // yalniz kalkis (~30 km/h alti)
        const double floor = speed() < 15.0 ? 0.35 : 0.10;
        if (!tcOn_) tcLim_ = std::min(1.0, tcLim_ + dt * 4.0);
        else if (bogging) tcLim_ = std::min(1.0, tcLim_ + dt * 8.0);
        else tcLim_ = s > tcSlip_ ? std::max(floor, tcLim_ - dt * 25.0 * std::min(s - tcSlip_, 1.0)) : std::min(1.0, tcLim_ + dt * 3.0);
    }
    pt_->setTorqueLimit((tcAvail_ ? tcLim_ : 1.0) * heatLim_);
}

void VehicleSim::updateNitrous(double dt) {
    if (nosHp_ <= 0.0) return;
    PowertrainCore& pt = *pt_;
    const double r = pt.rpm();
    nosActive_ = nosLeft_ > 0.0 && pt.throttle() > 0.9 && r > 3000.0 && r < eng_.redlineRpm && pt.gear() >= 1;
    pt.setTorqueAdd(nosActive_ ? nosHp_ * 7120.9 / std::max(r, 3000.0) : 0.0);
    if (nosActive_) nosLeft_ = std::max(0.0, nosLeft_ - dt);
}

// Motor isisi ve mekanik zorlanma: sogutma suyu sicakligi (isinma: gucun ~%33'u; sogutma: kapasite x hava akisi),
// 108 C ustu guc kaybi (vuruntu geri avansi), 125 C ustu hasar. Dayanim ustu tork motor / sanziman hasari biriktirir.
void VehicleSim::updateHeatAndStress(double dt) {
    if (coolCap_ <= 0.0) return;
    PowertrainCore& pt = *pt_;
    const double P = std::max(0.0, pt.engineTorque()) * pt.rpm() / 9.5493;
    const double air = coolLow_ + std::min(1.0, speed() / 40.0) * (1.4 - coolLow_);
    coolT_ += dt * (P * 0.33 * heatMul_ + 1500.0 - coolCap_ * air * (coolT_ - 30.0)) / 250000.0;   // blok + su + radyator isil kutlesi (fakir karisim sicak)
    coolT_ = std::max(coolT_, 85.0);                                          // termostat
    heatLim_ = coolT_ > 108.0 ? std::clamp(1.0 - (coolT_ - 108.0) * 0.025, 0.55, 1.0) : 1.0;
    if (coolT_ > 125.0) stress_ += dt * 0.02 * (coolT_ - 125.0);
    teF_ += (pt.engineTorque() - teF_) * std::min(1.0, dt / 0.12);
    {   // Lastik asinmasi: boyuna patinaj + yanal kayma hizi (1.5 m/s ustu). 5 s burnout ~%3, 30 s drift ~%2
        const double v = std::fabs(speed());
        for (int i = 0; i < 4; ++i) {
            const WheelSimulation& w = w_[i];
            if (w.Fz() < 100) continue;
            const double sl = std::fabs(w.omega() * w.rEff() - v) + std::fabs(w.slipAngle()) * v * 0.9;
            if (sl > 1.5) tireWear_ += dt * (sl - 1.5) * 1.3e-4;
        }
    }
    if (valveSafeRpm_ > 0 && pt.rpm() > valveSafeRpm_ && !engBlown_) {         // supap atmasi: sinirin ustunde birikir
        stress_ += dt * 0.05 * (pt.rpm() - valveSafeRpm_) / 250.0;
        if (!valveWarned_) { valveWarned_ = true; failEvents_.push_back("SUPAP ATIYOR! DEVIR SINIRI SUPAPLARA FAZLA"); }
    }
    {   // Vuruntu: yuksek yukte oktan acigi. Sensorlu ECU %2.5/oktan avans geri ceker; korumasiz ECU motoru dover
        const double deficit = knockReq_ + std::max(0.0, coolT_ - 100.0) * 0.25 - octane_;
        const double load = teF_ / tmax_;
        knockNow_ = deficit > 0.0 && load > 0.6 && pt.rpm() > 2000.0 && !engBlown_;
        double target = 1.0;
        if (knockNow_) {
            const double sev = deficit * std::min(1.0, (load - 0.6) / 0.4);
            if (knockSensor_) target = std::clamp(1.0 - 0.025 * sev, 0.65, 1.0);
            else stress_ += dt * 0.006 * sev;
            if (!knockWarned_) { knockWarned_ = true; failEvents_.push_back(knockSensor_ ? "VURUNTU: ECU AVANSI GERI CEKTI (OKTAN DUSUK)" : "VURUNTU! MOTOR DOVULUYOR (OKTAN DUSUK)"); }
        }
        knockLim_ += (target - knockLim_) * std::min(1.0, dt / (target < knockLim_ ? 0.15 : 1.5));
        heatLim_ *= knockLim_;
    }
    if (engRating_ > 0 && teF_ > engRating_ && !engBlown_) stress_ += dt * 0.6 * (teF_ / engRating_ - 1.0);
    if (gbRating_ > 0 && teF_ > gbRating_ && !gbBroken_) gbStress_ += dt * 0.8 * (teF_ / gbRating_ - 1.0);
    if (stress_ >= 1.0 && !engBlown_) { engBlown_ = true; failEvents_.push_back("MOTOR PATLADI! (BIYEL / PISTON)"); }
    if (gbStress_ >= 1.0 && !gbBroken_) { gbBroken_ = true; failEvents_.push_back("SANZIMAN KIRILDI!"); }
    if (engBlown_) pt.setIgnitionKilled(true);
    if (gbBroken_) pt.setAxleSnapped(true, true);
}

void VehicleSim::step(double dt, const VehicleInputs& in) {
    if (fuelKg_ <= 0.0 && burnMul_ > 1.0) pt_->setThrottle(0.0);       // maraton: depo bitti, motor tekler / guc yok
    updateElectronics(dt, in.brake);
    updateNitrous(dt);
    updateHeatAndStress(dt);
    if (cfg_.planar) { stepPlanar(dt, in); return; }
    PowertrainCore& pt = *pt_;
    DrivetrainFailure& fail = *fail_;
    suspEvents_.clear();

    // ---- guc aktarma ----
    pt.setAxleSnapped(fail.snapped(0), fail.snapped(1));
    pt.setIgnitionKilled(fail.bearingSpun());
    const double fs = frontShare_;
    const double wLin = drive_ == Drive::AWD ? fs * w_[0].omega() + (1 - fs) * w_[2].omega() : w_[dL_].omega();
    const double wRin = drive_ == Drive::AWD ? fs * w_[1].omega() + (1 - fs) * w_[3].omega() : w_[dL_ + 1].omega();
    pt.step(dt, wLin, wRin, fail.oilPressureFactor());
    {
        const double sh = std::max(fs, 1.0 - fs);
        fail.updateAxles(dt, sh * pt.axleTorqueL(), sh * pt.axleTorqueR());
    }
    fail.updateOil(dt, axF_, 0.0, pt.rpm());

    // ---- suspansiyon (2 kHz) -> dinamik tekerlek yukleri ----
    vl_.mass = baseMass_ + fuelKg_;
    if (++suspCounter_ >= 50) {
        suspCounter_ = 0;
        susp_->step(50 * dt, *road_, dist_, axRaw_, 0.0);
        for (int c = 0; c < 4; ++c) {
            if (susp_->airborne(c) && !wasAir_[c]) suspEvents_.push_back({c, true, dist_});
            if (susp_->onBumpStop(c) && !wasStop_[c]) suspEvents_.push_back({c, false, dist_});
            wasAir_[c] = susp_->airborne(c); wasStop_[c] = susp_->onBumpStop(c);
        }
    }
    const double bias = 0.70;                                          // on agirlikli (oransal valf): arka kilitlenip dondurmesin
    const double pedal = std::pow(std::clamp(in.brake, 0.0, 1.0), 1.6);  // ilerleyici pedal: hafif basis hassas
    const double bF = pedal * brakeTotal_ * bias * 0.5;
    const double bR = pedal * brakeTotal_ * (1 - bias) * 0.5 + in.handbrake * 1500.0;
    double sumFx = 0.0;
    for (int i = 0; i < 4; ++i) {
        w_[i].setNormalLoad(susp_->tireLoad(i) + 0.25 * dfK_ * V_ * V_);
        const double share = i < 2 ? fs : 1.0 - fs;
        const double Ta = share * ((i % 2 == 0) ? pt.axleTorqueL() : pt.axleTorqueR());
        w_[i].step(dt, V_, Ta, (i < 2 ? bF : bR) * absF_[i]);
        sumFx += w_[i].Fx();
        const double h = w_[i].consumeHapticPulse();
        if (h > 0.0) haptic_ = std::max(haptic_, h);
    }

    // ---- govde ----
    const double rho = 1.20;
    const double drag = 0.5 * rho * CdA_ * V_ * V_;
    if (in.held) { axRaw_ = 0.0; V_ = 0.0; }                 // line-lock / el freni tutar
    else {
        axRaw_ = (sumFx - drag) / vl_.mass;
        V_ += axRaw_ * dt;
        if (V_ < 0.0) V_ = 0.0;
    }
    axF_ += (axRaw_ - axF_) * std::min(1.0, dt / 0.08);       // govde pitch gecikmesi (suspansiyon)
    dist_ += V_ * dt;
    fuelKg_ = std::max(0.0, (cfg_.fuelLiters + fuelAdded_) * fuelDensity_ - pt.fuelGrams() * 1e-3 * burnMul_);
}

// ------------------------------------------------------------------------------------------------
// Duzlemsel dinamik: govde ekseninde vx (ileri), vy (sola), r (yaw, sola +). Tekerlek konumlari CoG'ye gore.
//   m (vx' - r vy) = SumFx - suruklemex      m (vy' + r vx) = SumFy - suruklemey      Iz r' = Mz
// Guc aktarma, ariza ve suspansiyon 1B yolla ayni; suspansiyona gercek yanal ivme verilir.
// ------------------------------------------------------------------------------------------------
void VehicleSim::stepPlanar(double dt, const VehicleInputs& in) {
    PowertrainCore& pt = *pt_;
    DrivetrainFailure& fail = *fail_;
    suspEvents_.clear();

    pt.setAxleSnapped(fail.snapped(0), fail.snapped(1));
    pt.setIgnitionKilled(fail.bearingSpun());
    const double fs = frontShare_;
    const double wLin = drive_ == Drive::AWD ? fs * w_[0].omega() + (1 - fs) * w_[2].omega() : w_[dL_].omega();
    const double wRin = drive_ == Drive::AWD ? fs * w_[1].omega() + (1 - fs) * w_[3].omega() : w_[dL_ + 1].omega();
    pt.step(dt, wLin, wRin, fail.oilPressureFactor());
    {
        const double sh = std::max(fs, 1.0 - fs);
        fail.updateAxles(dt, sh * pt.axleTorqueL(), sh * pt.axleTorqueR());
    }
    fail.updateOil(dt, axF_, ayRaw_, pt.rpm());

    vl_.mass = baseMass_ + fuelKg_;
    if (++suspCounter_ >= 50) {
        suspCounter_ = 0;
        susp_->step(50 * dt, *road_, dist_, axRaw_, ayRaw_);
        for (int c = 0; c < 4; ++c) {
            if (susp_->airborne(c) && !wasAir_[c]) suspEvents_.push_back({c, true, dist_});
            if (susp_->onBumpStop(c) && !wasStop_[c]) suspEvents_.push_back({c, false, dist_});
            wasAir_[c] = susp_->airborne(c); wasStop_[c] = susp_->onBumpStop(c);
        }
    }

    const double a = vl_.wheelbase * (1.0 - vl_.frontStatic), b = vl_.wheelbase * vl_.frontStatic, t2 = 0.5 * vl_.track;
    const double xs[4] = {a, a, -b, -b}, ys[4] = {t2, -t2, t2, -t2};
    const double bias = 0.70;                                          // on agirlikli (oransal valf): arka kilitlenip dondurmesin
    const double pedal = std::pow(std::clamp(in.brake, 0.0, 1.0), 1.6);  // ilerleyici pedal: hafif basis hassas
    const double bF = pedal * brakeTotal_ * bias * 0.5;
    const double bR = pedal * brakeTotal_ * (1 - bias) * 0.5 + in.handbrake * 1500.0;
    double Fx = 0.0, Fy = 0.0, Mz = 0.0;
    for (int i = 0; i < 4; ++i) {
        const double d = i < 2 ? in.steer : 0.0, cd = std::cos(d), sd = std::sin(d);
        const double vxi = vx_ - r_ * ys[i], vyi = vy_ + r_ * xs[i];            // tekerlek temas noktasi hizi (govde)
        const double vlong = vxi * cd + vyi * sd, vlat = -vxi * sd + vyi * cd;  // tekerlek ekseni
        w_[i].setNormalLoad(susp_->tireLoad(i));
        const double share = i < 2 ? fs : 1.0 - fs;
        const double Ta = share * ((i % 2 == 0) ? pt.axleTorqueL() : pt.axleTorqueR());
        w_[i].stepPlanar(dt, vlong, vlat, Ta, ((i < 2 ? bF : bR) + std::clamp(in.espBrake[i], 0.0, 1.0) * brakeTotal_ * 0.30) * absF_[i]);
        const double fx = w_[i].Fx(), fy = w_[i].Fy();
        const double bx = fx * cd - fy * sd, by = fx * sd + fy * cd;           // govde eksenine
        Fx += bx; Fy += by;
        Mz += xs[i] * by - ys[i] * bx;
        const double h = w_[i].consumeHapticPulse();
        if (h > 0.0) haptic_ = std::max(haptic_, h);
    }
    const double V = std::sqrt(vx_ * vx_ + vy_ * vy_);
    const double drag = 0.5 * 1.20 * CdA_ * V * V;
    if (V > 1e-6) { Fx -= drag * vx_ / V; Fy -= drag * vy_ / V; }

    if (reverse_ && revThr_ > 0.0 && vx_ > -4.5) Fx -= revThr_ * vl_.mass * 2.8 * std::clamp((vx_ + 4.5) / 1.5, 0.0, 1.0);   // geri cekis
    if (in.held) { vx_ = vy_ = r_ = 0.0; axRaw_ = ayRaw_ = 0.0; }
    else {
        const double m = vl_.mass;
        axRaw_ = Fx / m; ayRaw_ = Fy / m;                                      // hissedilen ivme (suspansiyon, yag)
        // Yer cekiminin yol boyunca bileseni: g sin(atan(egim)). Lastik kuvvetinden gelen axRaw_ yukte kalir
        // (yokusta duran aracta arkaya yuk aktarimi dogru cikar).
        const double gAlong = 9.81 * grade_ / std::sqrt(1.0 + grade_ * grade_);
        vx_ += (axRaw_ - gAlong + r_ * vy_) * dt;
        vy_ += (ayRaw_ - r_ * vx_) * dt;
        r_ += Mz / Iz_ * dt;
        if (vx_ < vxMin()) vx_ = vxMin();                                      // geri vites yoksa geri gitmez
        // Duran arac: kalinti yanal hiz ve yaw sonumlenir (statik surtunme)
        if (vx_ < 0.3 && std::fabs(vy_) < 0.3) { vy_ *= 0.999; r_ *= 0.999; }
    }
    axF_ += (axRaw_ - axF_) * std::min(1.0, dt / 0.08);
    psi_ += r_ * dt;
    const double c = std::cos(psi_), s = std::sin(psi_);
    X_ += (vx_ * c - vy_ * s) * dt;
    Y_ += (vx_ * s + vy_ * c) * dt;
    dist_ += std::sqrt(vx_ * vx_ + vy_ * vy_) * dt;
    V_ = vx_;
    fuelKg_ = std::max(0.0, (cfg_.fuelLiters + fuelAdded_) * fuelDensity_ - pt.fuelGrams() * 1e-3 * burnMul_);
}

// Anlik manifold basinci (bar, gosterge): asiri beslemeli motorda gaz x dolma (turbo: dolma araligi; kompresor: devirle
// dogrusal) x toplam boost; gaz kesikken vakum. Atmosferik motorda 0.
double VehicleSim::boostNow() const {
    if (boostTot_ <= 0.0) return 0.0;
    const double rpm = pt_->rpm(), thr = pt_->throttleEffective(), red = eng_.redlineRpm;
    double spool;
    if (superOnly_) spool = std::clamp(rpm / (0.6 * red), 0.0, 1.0);
    else {
        const double lo = spoolLo_ > 0 ? spoolLo_ : 0.30 * red, hi = spoolHi_ > lo ? spoolHi_ : std::max(lo + 300.0, 0.55 * red);
        const double x = std::clamp((rpm - lo) / (hi - lo), 0.0, 1.0);
        spool = x * x * (3.0 - 2.0 * x);
    }
    return thr * boostTot_ * spool - (1.0 - thr) * 0.65;
}

} // namespace zk
