// ZEHRA KINIK - Garaj araci: katalog listeleme, arac bilgisi, prosedurel ses (WAV onizleme) ve
// low-poly model (OBJ) disa aktarimi.
//   ./zehra_garage --list                 tum araclar
//   ./zehra_garage --info=5               arac detay + tork egrisi + vites oranlari
//   ./zehra_garage --car=5 --wav=a.wav    sesi render et (rolanti -> redline -> kesici -> gaz kesme)
//   ./zehra_garage --car=5 --obj=out/     modeli disa aktar
//   ./zehra_garage --export=out/ [--no-wav]  tum katalogu disa aktar
//   --dev : gercek arac/motor referanslarini da goster (yalniz gelistirici)
#include "audio/ProceduralEngineAudio.h"
#include "garage/LowPolyModel.h"
#include "garage/VehicleCatalog.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>

using namespace zk;

static void printRow(const VehicleDef& v, bool dev) {
    const EngineDef& e = engineTable()[v.engine];
    const GearboxDef& g = gearboxTable()[v.gearbox];
    std::printf("%3d %-10s %-26s %4d %-8s %-3s %5.0f kg %-6s %-13s %-9s %4.1fL %4.0f HP %-11s %s%s", v.id, v.brand.c_str(),
                v.model.c_str(), v.year, bodyName(v.body), driveName(v.drive), v.massKg, e.code, layoutName(e.layout),
                inductionName(e.induction), e.displacementL, e.powerHp, gearboxName(g.type),
                v.streetLegal ? "" : "[YARIS/ROMORK] ", dev ? "" : "\n");
    if (dev) std::printf(" <- %s | %s | %s\n", v.ref.c_str(), e.ref, g.ref);
}

static bool ensureDir(const std::string& d) {
    struct stat st{};
    if (stat(d.c_str(), &st) == 0) return S_ISDIR(st.st_mode);
    return mkdir(d.c_str(), 0755) == 0;
}

static void exportCar(const VehicleDef& v, const std::string& dir, bool wav, bool obj) {
    char base[64];
    std::snprintf(base, sizeof base, "%03d", v.id);
    if (obj) {
        const LowPolyMesh m = buildVehicleMesh(v);
        writeMtl(m, dir + "/" + base + ".mtl");
        writeObj(m, dir + "/" + base + ".obj", std::string(base) + ".mtl", v.fullName());
    }
    if (wav) writeWav16(dir + "/" + base + ".wav", renderRevDemo(v, 44100), 44100);
}

int main(int argc, char** argv) {
    bool dev = false, list = false, noWav = false;
    int car = 0, info = 0;
    std::string wav, objDir, exportDir;
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (!std::strcmp(a, "--dev")) dev = true;
        else if (!std::strcmp(a, "--list")) list = true;
        else if (!std::strcmp(a, "--no-wav")) noWav = true;
        else if (!std::strncmp(a, "--car=", 6)) car = std::atoi(a + 6);
        else if (!std::strncmp(a, "--info=", 7)) info = std::atoi(a + 7);
        else if (!std::strncmp(a, "--wav=", 6)) wav = a + 6;
        else if (!std::strncmp(a, "--obj=", 6)) objDir = a + 6;
        else if (!std::strncmp(a, "--export=", 9)) exportDir = a + 9;
        else { std::fprintf(stderr, "Bilinmeyen secenek: %s (bkz. kaynak basligi)\n", a); return 1; }
    }
    const auto& cat = vehicleCatalog();
    if (argc == 1 || list) {
        std::printf("ZEHRA KINIK garaj katalogu: %zu arac, %zu motor, %zu sanziman\n", cat.size(), engineTable().size(),
                    gearboxTable().size());
        for (const auto& v : cat) printRow(v, dev);
        return 0;
    }
    if (info) {
        const VehicleDef* v = findVehicle(info);
        if (!v) { std::fprintf(stderr, "Arac yok: %d\n", info); return 1; }
        printRow(*v, dev);
        const EngineDef& e = engineTable()[v->engine];
        const GearboxDef& g = gearboxTable()[v->gearbox];
        const EngineSpec s = buildEngineSpec(*v);
        std::printf("Motor: %s, %d silindir, %.2f L, %d subap/sil, redline %.0f, atesleme %s%s\n", e.code, e.cylinders,
                    e.displacementL, e.valvesPerCyl, e.redline, e.firingOrder, e.unequalHeaders ? ", esitsiz header" : "");
        std::printf("Veri: %.0f HP @ %.0f, %.0f Nm @ %.0f%s\n", e.powerHp, e.powerRpm, e.torqueNm, e.torqueRpm,
                    e.variableCam ? (" | kam gecisi " + std::to_string((int)e.camSwitchRpm) + " rpm").c_str() : "");
        std::printf("Tork egrisi (RPM: Nm dusuk/yuksek kam):\n");
        for (size_t i = 0; i < s.lowCam.size(); i += 4)
            if (s.lowCam[i].first <= e.redline)
                std::printf("  %5.0f: %5.0f / %5.0f\n", s.lowCam[i].first, s.lowCam[i].second, s.highCam[i].second);
        std::printf("Sanziman: %s (%s), oranlar:", g.code, gearboxName(g.type));
        for (double r : g.ratios) std::printf(" %.3f", r);
        std::printf(" | son disli %.3f\n", g.finalDrive);
        ProceduralEngineAudio a(*v);
        std::printf("Ses imzasi: %s\n", a.signature().c_str());
        const LowPolyMesh m = buildVehicleMesh(*v);
        std::printf("Model: %zu vertex, %zu ucgen\n", m.verts.size(), m.tris.size());
        return 0;
    }
    if (car) {
        const VehicleDef* v = findVehicle(car);
        if (!v) { std::fprintf(stderr, "Arac yok: %d\n", car); return 1; }
        if (!wav.empty()) {
            writeWav16(wav, renderRevDemo(*v, 44100), 44100);
            std::printf("Ses yazildi: %s (%s)\n", wav.c_str(), v->fullName().c_str());
        }
        if (!objDir.empty()) {
            if (!ensureDir(objDir)) { std::fprintf(stderr, "Klasor olusturulamadi: %s\n", objDir.c_str()); return 1; }
            exportCar(*v, objDir, false, true);
            std::printf("Model yazildi: %s/%03d.obj\n", objDir.c_str(), v->id);
        }
        return 0;
    }
    if (!exportDir.empty()) {
        if (!ensureDir(exportDir)) { std::fprintf(stderr, "Klasor olusturulamadi: %s\n", exportDir.c_str()); return 1; }
        FILE* idx = std::fopen((exportDir + "/katalog.csv").c_str(), "w");
        if (idx) std::fprintf(idx, "id,marka,model,yil,kasa,cekis,kg,motor,duzen,induksiyon,litre,hp,sanziman\n");
        for (const auto& v : cat) {
            exportCar(v, exportDir, !noWav, true);
            const EngineDef& e = engineTable()[v.engine];
            if (idx) std::fprintf(idx, "%d,%s,%s,%d,%s,%s,%.0f,%s,%s,%s,%.2f,%.0f,%s\n", v.id, v.brand.c_str(),
                                  v.model.c_str(), v.year, bodyName(v.body), driveName(v.drive), v.massKg, e.code,
                                  layoutName(e.layout), inductionName(e.induction), e.displacementL, e.powerHp,
                                  gearboxTable()[v.gearbox].code);
            std::printf("\r%3d/%zu", v.id, cat.size()); std::fflush(stdout);
        }
        if (idx) std::fclose(idx);
        std::printf("\nDisa aktarildi: %s\n", exportDir.c_str());
        return 0;
    }
    return 0;
}
