# ZEHRA KINIK — Devir Notu

> Güncelleme: 2026-10-01 · Dal: `claude/tender-fermat-7w0r3w` · Yerel Windows oturumu (ayarlar + otoban akışı)
> Bu not, projeyi devralan kişinin (ya da yeni bir oturumun) sıfırdan durumu anlayıp kaldığı yerden
> devam edebilmesi için yazıldı. Ürün şartnamesi: ilk mesajdaki "ZEHRA KINIK" spesifikasyonu (Bölüm 1–9).

---

## 1. Kısa durum

| Faz | İçerik | Durum |
|---|---|---|
| 1 | Tekerlek/lastik, güç aktarma, arıza çekirdeği; 0-400 m konsol sim. | ✅ bitti |
| 2 | 324 araç / 280 motor / 127 şanzıman kataloğu, prosedürel ses, süspansiyon + yol | ✅ bitti |
| 2.5 | Grafik istemci iskeleti (Android + Windows + Linux), CI + indirme linki | ✅ bitti |
| 3 | Oynanabilir drag yarışı (yatay ekran) | ⏳ **ara taslak**, oynanabilir |
| 4 | Kariyer: para, garaj, parça dükkânı, galeri, dyno, kayıt | ⏳ **ara taslak**, oynanabilir |
| 5 | Açık yol: viraj fiziği, eğim direksiyonu, trafik, yol yarışı, dağ yolu, otoban akışı | ⏳ **ara taslak**, 6 ölçütün hepsi kodda |
| — | Ayarlar ekranı (FPS sınırı, dikey eşitleme, ses, titreşim, eğim, birim) | ✅ yerelde doğrulandı |
| 6–9 | Isı/arıza/yakıt/maraton, multiplayer, görsel cila, ses/haptik cilası | başlanmadı |

Ayrıntılı ölçütler ve onay kutuları: [`docs/YOL_HARITASI.md`](YOL_HARITASI.md).

**İndirme (her push'ta CI günceller):**
https://github.com/oduncugur/orrrrrospu-zehra-k-n-k/releases/tag/latest-build
— `ZehraKinik.apk` (Android 8.0+, arm64/armv7/x86_64), `ZehraKinik_Windows_x64.zip`, `ZehraKinik_Linux_x64.zip`
(zip'lerde `zehra_game` grafik oyun + `zehra_sim` / `zehra_garage` konsol araçları).

---

## 2. Kullanıcı tercihleri (bunlara uyun)

- İletişim **Türkçe**. Kullanıcı Türk; oyunda yerli araçlar (Tofaş/Anadol/Toros) önemli.
- **Profesyonellik ve her adımı mükemmelleştirerek ilerleme** açıkça istendi: önce doğrula (test, ölçüm,
  ekran görüntüsü), sonra sun. Hata bulunca kök nedeni düzelt, testle kilitle.
- Sunulan şey **bitmemişse "ara taslak" diye etiketle**; bitmiş gibi gösterip "korkutma".
- **Araç modelleri en sona** bırakıldı ("önce oyunun tamamını yap"). Mevcut modeller geçici.
- **Gerçek araç/motor/şanzıman** istendi ama **telifsiz**: teknik veri gerçek, isimler kurgusal (bkz. §6.1).
- Sesler "uzay gemisi gibi" bulunup **daha mekanik** istendi (düzeltildi, v2); dijital vızıltı şikâyeti vardı (giderildi).
  Sesler hiçbir zaman gerçekten dinlenerek doğrulanmadı (bkz. §8).
- Kullanıcı uzun sessizlikten hoşlanmıyor ("NEDEN DURDUN"): uzun işlerde kısa ara durum ver, CI beklerken başka işe geç.

---

## 3. Depo haritası

```
src/
  sim/        Fizik çekirdeği (platformdan bağımsız, deterministik)
    WheelSimulation     Lastik: Pacejka, I_w, κ (gevşeme boylu), PSI→r_eff/sertlik, sıcaklık, flat-spot + haptik darbe
    PowertrainCore      Motor (düşük/yüksek kam, VTEC, ITB/plenum tepki, BSFC yakıt), analog debriyaj (ısırma, fade, stall),
                        şanzıman, 1.5-way plaka LSD (DiffSpec ile açık/2-way/spool)
    DrivetrainFailure   Aks burulma gerilmesi τ=16T/(πd³) → burulma/kırılma; ıslak karter yağsız kalma → yatak sarma
    Suspension          7-DOF sürüş modeli + ISO 8608 yol profili + kasis/çukur/derz; tekerlek havalanması
    VehicleSim          Hepsini birleştiren tek araç sınıfı (drag/konsol/yapay zekâ ortak). Tune uygulanır.
    Tune.h              Parça durumu (lastik, debriyaj, aks, diferansiyel, son dişli, hafifletme, emme/egzoz/ECU, turbo, karter, yakıt)
  garage/     Veri: VehicleTable.inc (324 araç), EngineTable.inc (280), GearboxTable.inc (127); VehicleCatalog (tork eğrisi sentezi);
              LowPolyModel (prosedürel PS1 kasa + OBJ/MTL, malzeme renkleri)
  audio/      ProceduralEngineAudio — sıfır .wav; ateşleme sırası, bank ayrımı, boxer header Δt, egzoz dalga kılavuzu,
              blok çınlaması, supap tıkırtısı, VTEC, ITB, turbo ıslık/flutter, kompresör, dogbox. render() gerçek zamanlı güvenli.
  game/       DragRace (ağaç, faz makinesi, zamanlama, kırmızı ışık, yapay zekâ, şanzıman tipleri),
              Career (para, garaj, parça kataloğu/fiyat, rakip eşleştirme, kayıt/yükleme)
  app/        Platformdan bağımsız uygulama: App (ekran yönetimi, ses karıştırma, haptik/yön geri çağrıları, kariyer),
              Renderer (360x640 / 640x360 piksel-keskin sanal tampon, 2D alfa katman, bitmap font, model önbelleği),
              GarageScreen, DragScreen, MenuScreens (Parça/Galeri/Dyno), Ui.h, GLApi (Android GLES3 / masaüstü GL 3.3 yükleyici)
  main.cpp          zehra_sim (konsol drag sim; regresyon testinin referansı)
  garage_main.cpp   zehra_garage (katalog listesi, WAV/OBJ dışa aktarma)
  desktop_main.cpp  zehra_game (SDL3 platform katmanı)
android/      Gradle projesi; app/src/main/cpp/android_main.cpp = NativeActivity + EGL + AAudio + JNI (yön, titreşim)
tests/        run_regression.sh + regression/ (9 referans çıktı), drag_race_test, tune_test, career_test
third_party/khronos/  GLES3 başlıkları (MIT) — masaüstü GL yükleyicisi için
docs/         YOL_HARITASI.md, DEVIR_NOTU.md (bu dosya)
ornekler/     Örnek WAV sesler (v2), katalog.csv, 6 OBJ; model_sheet.png ESKİ (v1 modeller)
```

Katman kuralı: `sim` → `garage` → `audio`/`game` → `app` → platform (`desktop_main` / `android_main`).
Alt katman üst katmanı include etmez. Oyun mantığı platform koduna yazılmaz.

---

## 4. Derleme, çalıştırma, test

```sh
# Konsol araçları + testler (bağımlılık yok)
cmake -B build && cmake --build build -j8
ctest --test-dir build --output-on-failure       # fizik_regresyon, drag_yarisi, parca_etkileri, kariyer_kayit,
                                                 # duzlemsel_dinamik, acik_yol, ayarlar, otoban_akisi

# Grafik oyun: SDL3 gerekir (statik derleyip gösterin)
git clone --depth 1 --branch release-3.4.16 https://github.com/libsdl-org/SDL
cmake -S SDL -B SDL/build -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TESTS=OFF -DSDL_WAYLAND=OFF -DCMAKE_INSTALL_PREFIX=$HOME/sdl3
cmake --build SDL/build -j8 && cmake --install SDL/build
cmake -B build -DCMAKE_PREFIX_PATH=$HOME/sdl3 && cmake --build build -j8 && ./build/zehra_game

# Windows çapraz derleme: aynı adımlar, -DCMAKE_TOOLCHAIN_FILE=SDL/build-scripts/cmake-toolchain-mingw64-x86_64.cmake
```

- **Yerel Windows (kullanıcının PC'si)**: WinLibs GCC 16.2 + CMake (winget). Kullanıcı klasörü yolunda boşluk
  olduğu için MinGW bağlayıcısı bozulur; çözüm: `C:\zkgcc` bağlantısı (junction) + PATH'te `C:\zkgcc\bin` +
  `CC/CXX=C:\zkgcc\bin\gcc.exe/g++.exe` + **`GCC_EXEC_PREFIX=C:/zkgcc/lib/gcc/`** (junction tek başına yetmez).
  SDL3: `C:\zkgcc_deps\sdl3` → `cmake -B build -G Ninja -DCMAKE_PREFIX_PATH=C:\zkgcc_deps\sdl3`.
  Regresyon betiği Git Bash ile: `bash tests/run_regression.sh build/zehra_sim.exe` (Windows'ta da byte byte aynı).
- **Android** yerelde derlenmiyor: bulut ortamında `dl.google.com` ağ politikasıyla engelli (SDK/NDK inemiyor).
  APK **GitHub Actions**'ta derleniyor (`.github/workflows/build.yml`, runner'da SDK/NDK hazır).
  Yerelde yalnızca stub başlıklarla sözdizimi denetimi yapıldı (JDK `jni.h` ile `AttachCurrentThread(JNIEnv**)`
  imza uyarısı çıkar; NDK'da doğrudur).
- **CI**: her push'ta masaüstü (+ ctest) ve Android derlenir, `latest-build` sürümü yeniden oluşturulur.
  Aynı dalda yeni push eski çalışmayı iptal eder (`concurrency`). Durum: GitHub API
  `repos/oduncugur/orrrrrospu-zehra-k-n-k/actions/runs` (depo herkese açık).

### Test ve otomasyon kancaları (masaüstü `zehra_game`)
| Değişken / argüman | Anlamı |
|---|---|
| `--screenshot=f.ppm --frames=N` | N kare çalıştır, ekran görüntüsü al, çık (Xvfb ile başsız: `xvfb-run -a ...`) |
| `--throttle-frames=M` | ilk M kare gaz basılı |
| `--car-offset=K` | serbest modda araç = 5+K |
| `ZK_SAVE_DIR=dir` | kayıt klasörü (testlerde geçici klasör kullanın) |
| `ZK_START_SCREEN=parts\|gallery\|dyno\|race\|road\|settings` | doğrudan ekranla başla (`race` = kariyer yarışı) |
| `ZK_ROAD_MODE=free\|race\|touge\|flow` | açık yolda modu doğrudan başlat (`flow` = otoban akışı) |
| `--run-seconds=N` | gerçek zamanlı N s çalış, ortalama FPS'i stdout'a yaz, çık (FPS sınırı ölçümü) |
| `ZK_START_DRAG=1` [+ `ZK_AUTOPILOT=1`] | serbest drag yarışı (oyuncu şeridini yapay zekâ sürer) |
| `ZK_KEYS="t:Tuş:1/0,..."` | zamanlı tuş betiği (Throttle, Clutch, Brake, Gear0..6, ShiftUp/Down, Enter) |
| `ZK_TAPS="t:x:y,..."` | zamanlı dokunuş betiği, sanal koordinat (dikey 360×640 / yatay 640×360) |
| `SDL_AUDIO_DRIVER=dummy` | ses cihazı olmayan ortamda |

Klavye: W/↑ gaz, S/↓ fren, Boşluk/Shift debriyaj (analog rampa), 1–6/N vites, E/Q vites ±,
←/→ (A/D) araç / direksiyon, Enter yarış/stage/tekrar, O/F1 ayarlar, F11 tam ekran, Esc geri/çıkış.

### Regresyon testi
`tests/run_regression.sh build/zehra_sim` — 9 `zehra_sim` senaryosunun çıktısı referansla **byte byte** aynı olmalı.
Bilinçli bir fizik değişikliğinden sonra: farkı incele (hareket verisi mi, yalnızca türetilmiş değer mi?),
gerekçelendir, sonra `UPDATE=1 tests/run_regression.sh build/zehra_sim` ve commit mesajında açıkla.
Son güncelleme (424c879): aks boyutlandırması değişti; 9 vakada hareket verisi aynı, yalnızca aks çapı/gerilme değişti.

---

## 5. Mimari notlar

- **Fizik adımı**: konsol/regresyon 10 µs (100 kHz); oyun `DragRace::kStep = 50 µs` (20 kHz). Ölçüm: 100 µs'de
  tekerlek titreşimi +%57, 50 µs'de +%8 → 50 µs seçildi. Masaüstünde iki araç kare başına ~0.6 ms.
  Ekrandaki FPS/MS göstergesi cihaz performansını görmek için.
- **Süspansiyon** 2 kHz (her 50 fizik adımında bir). Yük transferi süspansiyondan gelir.
- **Determinizm**: `DragRace` aynı tohum + aynı girdiyle birebir aynı sonucu verir (test [5]); multiplayer rollback için ön koşul.
  Kayan nokta platformlar arası (ARM/x86) bit-birebir garanti değil — Faz 7'de ele alınmalı.
- **Ses**: `App::renderAudio` ses thread'inde; kilit `try_lock` (asla beklemez); 2 motor sesi + lastik cıyaklaması.
- **Ekranlar** `App::setScreen` ile bir sonraki karede değişir (ekran kendi metodunun içinde silinmesin).
- **Kayıt**: `kariyer.zks`, sürüm + FNV-1a sağlama toplamı, alan aralık denetimi, geçici dosya + rename (atomik).
  Bozuksa yeni oyun başlar (şu an kullanıcıya bildirilmiyor — iyileştirilebilir).

---

## 6. Önemli tasarım kararları ve gerekçeleri

1. **Telif/marka politikası**: teknik veriler gerçek; oyunda görünen marka/model/motor/şanzıman adları kurgusal
   (Honda→Kaze, Toyota→Tsuru, Nissan→Hikari, Porsche→Stahlwerk, Tofaş→Bursa, Anadol→Kınık …). Gerçek adlar yalnızca
   `ref` alanlarında (geliştirici), arayüzde yok (`zehra_garage --dev` gösterir). Kasa modelleri ölçülerden prosedürel,
   logo yok. `~` ile başlayan `ref` değerleri yaklaşık, yayın öncesi doğrulanmalı. Bu hukuki görüş değildir.
2. **Tork eğrisi**: her motorun gerçek tepe tork ve tepe güç noktalarından geçen sentez eğri (`VehicleCatalog.cpp`).
3. **Aks boyutlandırması**: fabrika aksı, stok debriyajın aktarabileceği en yüksek torkta (1.7 × fabrika tepe torku,
   1. vites) stok çeliğin kopma dayanımının %75'i. Önceki formül stok Mustang'in aksını normal kalkışta kırıyordu
   (`tune_test` [6] yakaladı). Yükseltmeler: krom-moly (4340, +%12 çap, ~2.1×), yarış (300M, +%25 çap, ~3.4×).
4. **Spool diferansiyel**: 2500 Nm kilit (sonsuz sert kilit 50 µs adımda sayısal olarak patlar).
5. **Stage'de araç tutma**: oyuncu debriyaja (DCT/otomatikte frene) basana kadar araç frende; rölantide sürünüp
   haksız kırmızı ışık yakılmaz (masaüstü tuş betiği testi yakaladı; `drag_race_test` [2b]).
6. **Reaksiyon süresi** NHRA gibi aracın stage ışığını terk ettiği an ölçülür (rollout dahil → ~0.3–0.5 s tipik).
7. **Yarış mesafesi**: oyunda gerçek çeyrek mil 402.336 m; `zehra_sim` 400 m (Faz 1 spesifikasyonu).
8. **Rakip eşleştirme**: performans endeksi (parçalı güç/kütle × lastik × çekiş) ±%6 → genişleyen bant;
   yapay zekâ 3 parça setinden birini kullanır (stok/sokak/drag).
9. **Başlangıç**: 6.000 $ + Bursa Şahin 1.6 (#217). Fiyatlar güç/yıl/kasa/nadirlikten türetilir.

---

## 7. Faz 5 planı (arşiv — adımların hepsi ara taslak olarak kodda, bkz. sondaki güncellemeler)

Kalan Faz 5 cilası: kaster/toe/amortisör ayarları,
akış modunda ekonomi dengesi. Ardından Faz 6.

**Adım 1 — Düzlemsel dinamik (fizik, test önce):** `VehicleSimConfig`'e `planar` bayrağı; `false` iken mevcut 1B yol
(regresyon birebir kalmalı). `true` iken durum: X, Y, ψ, vx, vy, r. Her tekerlek için gövde hızından
`vlong/vlat` (ön tekerlekte δ direksiyonu), kayma açısı α, yanal Pacejka (B≈11, C≈1.35, E≈0.3, D=μFz),
yanal gevşeme boyu (düşük hız kararlılığı), çekiş elipsi `(Fx/μFz)² + (Fy/μFz)² ≤ 1` (Fy kısılır).
Kuvvetleri gövdeye döndür; `m(v̇x−r·vy)=ΣFx−drag`, `m(v̇y+r·vx)=ΣFy`, `Iz·ṙ=Mz` (Iz ≈ m·(wb/2)²·1.1).
Süspansiyona yanal ivme `ay = v̇y + r·vx` ver. Testler (yazılacak): δ=0'da 1B ile uyum; sabit yarıçaplı
skidpad'de yanal ivme sokak lastiğinde ~0.85–1.0 g; FWD gaz altında understeer; RWD gazla oversteer/spin;
0–5 m/s manevrada NaN yok. Kaster/toe/amortisör ayarları (şartname Bölüm 2.6) bu adımın uzantısı.

**Adım 2 — Yol**: 3D spline (Catmull-Rom), şerit/kenar sorgusu, eğim; mevcut `RoadProfile` pürüzlülüğü üstüne.
**Adım 3 — Trafik**: şerit takip eden kinematik araçlar, AABB/OBB çarpışma.
**Adım 4 — Kontrol**: Android `ASensorManager` (oyun rotasyon vektörü/ivmeölçer) → eğim açısı → δ (ölü bölge, hız
duyarlı oran); masaüstü A/D. **Adım 5 — Görüntü**: arkadan perspektif kamera, yol şeridi mesh'i, çevre, HUD.
**Adım 6 — Mod**: otoban akışı skorlaması.

Ardından şartnamenin kalanı: Faz 6 (intercooler ısı emme/su spreyi, termostat, buji tırnağı/spark blowout,
16×16 ECU AFR/avans + vuruntu, 2-step/anti-lag, BSFC yakıt + depo ağırlığı + benzinlik, polis/sokak yasallığı,
çekici/römork, rektifiye/Frankenstein swap), Faz 7 (rollback netcode, sunucu doğrulaması, pink slip vb.).

---

## 8. Bilinen sorunlar / doğrulanmamışlar

- **Android cihazda hiç çalıştırılmadı** (emülatör yok). CI derlemesi başarılı; ekran yönü (JNI), titreşim
  (VibrationEffect), AAudio, dokunmatik — cihazda denenmeli. Kullanıcı APK'yı kurup geri bildirim verebilir.
- **Sesler dinlenmedi**; yalnızca spektral ölçümle (ateşleme frekansı, yüksek frekans oranı) doğrulandı.
  Kullanıcı geri bildirimi: v1 "uzay gemisi"/dijital vızıltı → v2'de düzeltildi; v2 hakkında geri bildirim bekleniyor.
- **Modeller geçici** (kullanıcı isteğiyle en sona). Bilinen kusurlar: B direği bazı gövdelerde havada, hatchback
  ön ucu. `ornekler/model_sheet.png` v1'i gösteriyor (eski).
- **Ekonomi dengesi** oyun testiyle ayarlanmadı; aynı rakibe "TEKRAR" ile sınırsız ödül kasılabilir.
- **Kayıt bozulursa** yeni oyun başlar; garajda "KAYIT BOZUK - YENI OYUN" uyarısı çıkar, bozuk dosya `.bozuk` olarak saklanır.
- **Yapay zekâ kalkışı sert** (120 ms clutch dump): güçlü parçalı rakipler aks kırabilir — kasıtlı ama dengelenmeli.
- `zehra_sim`'deki sürücü mantığı ile `DragRace::aiDrive` benzer ama ayrı kod (teknik borç; regresyon testiyle birleştirilebilir).
- Veri tablolarında `~` ile işaretli değerler yaklaşık.

---

## 9. Çalışma biçimi (bu projede işe yarayan)

1. Değişiklikten önce referans ölç/kaydet → değişiklik → aynı ölçümle karşılaştır.
2. Her yeni sistemi başsız testle kilitle (`tests/*.cpp`, `ctest`), sonra arayüze bağla.
3. Arayüzü `xvfb-run` + `--screenshot` ile gör; tuş betiğiyle (`ZK_KEYS`) gerçek oyuncu akışını oyna.
4. Commit mesajları Türkçe, "ne + neden + nasıl doğrulandı"; her adım push → CI → `latest-build`.
5. Kullanıcıya sunarken durumu açık yaz: bitti / ara taslak / doğrulanmadı.

## Guncelleme — Faz 5 adim 1 (ara taslak, commit'lendi)
- Duzlemsel dinamik kodda: `VehicleSimConfig::planar`, `VehicleInputs::steer`, `WheelSimulation::stepPlanar`. Drag oyunu hala 1B modu kullanir; oyun etkilenmedi.
- Duzeltme: sokak/yari-slick lastiklerine slick sicaklik penceresi uygulaniyordu (soguk = %55 tutus). Artik sokak 55 C / yari-slick 75 C ideal, genis pencere.
- `tests/planar_test.cpp`: [1] duz cizgi, [3] FWD understeer, [5] tam kilit GECIYOR.
  KALAN: [2] skidpad 0.75 g (hedef 0.80-1.10) ve slick farki yok. Suphe: skidpad FWD aracla 2. viteste; arka aks
  once doyuyor (arka kayma acisi on'un ~2 kati). [4] RWD tam gaz govde kaymasi 4.4 deg (hedef >8).
  Test ctest'e kapali (CMakeLists'te yorum satiri); gecince acilacak.
- Hata ayiklama: `/tmp` altindaki iz araclari kalici degil; test dosyasina `ZK_TRACE` ciktisi eklenerek yeniden uretilebilir.

## Kullanici geri bildirimi — YAPILDI (ara taslak, oyun icinde kulakla onay bekliyor)
- Lastik: `src/audio/TireAudio` (tonal stick-slip + burnout hirlamasi), ornek `ornekler/ses_lastik_burnout_ciglik_v3.wav`.
- Turbo: islik ~7x guclu, blow-off valf, turbo kiti takili NA araclarda da calisir (`setTurboKit`). Ornek `ses_078/089_turbo_v3.wav`.
- PC tus yardimi: garaj + drag ekrani (Android'de gizli).
- Tamir: `OwnedCar::axleBroken/engineWear`, kayitta `dmg=` satiri, garajda TAMIR butonu, career_test [H].

### Ilk not (arsiv)
1. Lastik patinaj sesi "ruzgar" gibi: squeal su an gurultu+rezonator. Duzeltme: slip hizina bagli stick-slip
   titresim (tonal 400-1200 Hz, harmonikli, kaymaya gore frekans/genlik), gurultu payi azaltilacak. Burnout icin ayri "yanma" dokusu.
2. PC'de tuslar ekranda yazmiyor: menu/yaris ekraninda klavye yardim paneli (W/S gaz-fren, Space debriyaj, 1-6/N, E/Q, Esc) + ayarlar sayfasi.
3. Hasarli parca tamiri yok: aks kirilmasi / yatak hasari / debriyaj yanmasi kariyerde kalici olmali;
   garajda "Tamir" (parca fiyatina gore ucret), kayit dosyasina hasar durumu eklenmeli.
4. Turbo sesi duyulmuyor: turbo whistle/flutter kazanci ve boost'a baglanmasi kontrol edilecek (oyun icinde turbolu aracla test;
   ProceduralEngineAudio turbo katmani oyun mikserinde susturuluyor olabilir).

## Guncelleme — Faz 5 adim 2 (ara taslak)
- Planar testler 5/5 geciyor (ctest `duzlemsel_dinamik` acik). Ey=-0.5, yari-slick 4 teker.
- Acik yol: `src/game/RoadPath`, `src/app/RoadScreen.cpp`. Garajda YOL butonu / PgUp. Test: `ZK_START_SCREEN=road ZK_AUTOPILOT=1 ZK_ROAD_LOG=1`.
- Siradaki: trafik araclari, yol tipleri (otoban/sehirlerarasi/dag-touge), yol disi engeller/carpisma,
  jiroskop direksiyon (Android), yarisma modu (rakip + kontrol noktasi), kariyer odulu.

## Guncelleme — Faz 5 adim 3 (ara taslak)
- `RoadCar` (oyuncu+YZ ortak), `RoadSession` (serbest / 4 km yol yarisi, trafik, carpisma, geri sayim, sonuc). Ekran: mod menusu.
- Yarista odul `Career::recordRace` ile (ayni rakip azalmasi gecerli), motor asinmasi kaydedilir.
- Test: `road_test` (ctest `acik_yol`): 3 aracla 6 km kurtarmasiz + trafikli yaris. `ZK_ROAD_MODE=race ZK_AUTOPILOT=1`.
- Bilinen: planar skidpad sokak 0.80 g sinirda; oyuncu-rakip temasi basit itme;
  YZ oyuncuyu sollarken karsi seridi kontrol eder ama agresif degil.
- Sonradan (c9b8ad5, da725e1): trafik IDM ile fren/takip yapar; 3 km dag yolu (touge) yarisi; Android'de
  ivmeolcer egimiyle direksiyon. e904844: `araclar/yerele_aktar.bat` (bulut oturumunu yerel PC'ye aktarir).

## Guncelleme — 2026-10-01: Ayarlar + Faz 5 olcut 6 (otoban akisi)
**Ayarlar** (`src/game/Settings`, `src/app/SettingsScreen.cpp`, `src/app/FramePacer.h`, ctest `ayarlar`):
- Garajda AYAR dugmesi (eski AGAC dugmesinin yeri; agac tipi artik ayarlarda) / masaustunde O veya F1.
- GORUNTU: FPS siniri (30/45/60/75/90/120/144/165/240/sinirsiz), dikey esitleme (kapali/acik/adaptif),
  FPS gostergesi, tam ekran (F11, yalniz masaustu), olcekleme (sigdir / tam sayi piksel-keskin).
  SES: ana / motor / lastik. KONTROL: titresim gucu + egim direksiyonu + egim hassasiyeti (yalniz Android),
  surus yardimi ve vites modu varsayilani (yolda degistirilince de kaydedilir). OYUN: km/h-mph, drag agaci.
- `ayarlar.cfg` kariyer kaydindan ayri, elle duzenlenebilir metin; gecersiz deger en yakin gecerliye cekilir, atomik yazim.
- FPS siniri: son tarih tabanli `FramePacer` (masaustu `SDL_DelayPrecise`, Android `sleep_for`). Olculen:
  30→30.1, 60→60.0, 144→143.5 FPS. Bu PC'nin AMD surucusu dikey esitlemeyi yok sayiyor (swap interval 0 kaliyor):
  oyun bunu algilar, yazilimla ekran yenileme hizina sinirlar (olculen 60.0) ve ayarlarda "YAZILIM" gosterir.
**Otoban akisi** (`src/game/FlowScore`, `RoadSession::Mode::Flow`, ctest `otoban_akisi`):
- 120 s, yogun trafik (24 arac, oyuncunun yakininda yeniden dogar), 3 s geri sayim.
- Skor: yakin gecis (govdeler arasi < 1 m, >= 60 km/h; yakinlik ve hizla artar, karsidan gelen 2x),
  kombo (4 s icinde ardisik gecis, en fazla X5), 90 km/h ustu hiz puani, karsi seritte gitme puani,
  viraj tepesinden (apex) >= 0.45 g yanal ivmeyle gecis. Carpisma: -1000 + kombo sifir; yol disi kombo bitirir.
- Kariyer: odul skor/20 (en fazla $4000), rekor kirilirsa +%50; `Career::bestFlow` kayitta (`flow=` satiri).
- Denge oyun testiyle ayarlanmadi (ara taslak).

## Guncelleme — 2026-10-01: Yol egimi
- `RoadPoint::z/grade`; `RoadPath(..., maxGrade)`: uc sinus toplami + tanh doyumu (uzun sabit yokuslar),
  ayri tohum (viraj programi birebir ayni), ilk 150 m duz. Sehirlerarasi %5, dag yolu %9.
- `VehicleSim::setGrade` (yalniz duzlemsel mod): vx'e -g sin(theta); lastik kuvveti yuk aktariminda kalir.
  Drag 1B fizigi degismedi (regresyon 9/9 ayni). `RoadCar` her kare yol egimini arac yonune izdusurur.
- Cizim: kamera/yol/agac/bitis/araclar yukseklikte, araclar egime gore burun yukari/asagi; yol kenarinda
  yuksekligi izleyen 60 m cim seridi (tepede yol havada kalmaz). Serbest surus HUD'unda EGIM %.
- Test `road_test` [6]: egim siniri, sureklilik, duz baslangic, virajlar degismedi, %8 inis bosta hizlanma
  (3.23 m/s; surtunmesiz 3.91), duzde hareket yok.
- Not: test surucusu (oyuncu-YZ) sollamadigi icin dag yolunda yavas trafige takilip bitiremeyebilir; test bunu sart kosmaz.
- CI (a40a3da): Windows + Linux + **Android APK derlendi** (ayarlar/FPS siniri Android kodu dahil); cihazda denenmedi.

## Guncelleme — 2026-10-01: Oyuncu-rakip temasi (gercek carpisma)
- `src/game/Contact`: iki yonlu kutu (OBB) ayirici eksen testi -> gecme derinligi + normal; temas noktasi
  (digerinin icindeki koseler); kutleyle orantili konum ayirma; normal impuls (esneklik 0.25) + Coulomb
  surtunme (0.4); yaw ataleti dahil. `VehicleSim::applyImpulse/worldVelocity/mass/yawInertia`.
- `RoadSession`: eski "yana it + %3 hiz kes" yerine `resolveContact`. Mesaj: TEMAS / SERT TEMAS (> 6 m/s).
- Test `contact_test` (ctest `arac_temasi`): arkadan carpmada momentum korunur, enerji artmaz, ayrilma e x 10 m/s,
  agir arac az hiz degistirir, donme yok; yan surtmede yanal itme + iki aracta yaw; ayni hizda yan yana: yalniz ayirma.
- Trafik araclari kinematik kalir (carpinca yeniden dogar); onlara impuls yok.

## Guncelleme — 2026-10-02: Tork konvertoru modeli + YZ kalkisinda aks kirma
- Olcum (120 rastgele eslesme, YZ rakip, scratchpad `ai_axle.cpp`): once 3/120 aks kirma, ort ET 13.939 ->
  simdi **0/120**, 120/120 bitirdi, ort ET 13.843.
- Kok neden: otomatik sanziman surtunmeli debriyaj gibi modelliydi; YZ otomatikte de 120 ms "clutch dump"
  yapiyordu, oyuncuda da frende motor bosta 2-step'e cikip birakinca dump oluyordu.
- `PowertrainCore::setConverter`: akiskan kavrama. Pompa torku K w^2 (1 - SR^6), K: stall devrinde (%42 redline)
  tam gaz motor torku; cikis = TR x pompa (TR stall'da 1.9, SR 0.85'te 1); gaz kesmede ters yonde (motor freni);
  pompa torku tepe motor torkunun 1.1 katiyla sinirli (vites darbesini sanziman kavrama elemanlari sinirlar;
  sinirsiz K w^2 redline'da ~4.7 kat cikiyordu). Pedal >= 0.99 = N. Debriyaj isisi yok.
- `VehicleSim`: otomatik araclarda konvertor acik; kalkis devri = stall devri; fabrika aks boyutu 1.9 x tepe tork
  ile (manuelde 1.7). Regresyon 9/9 ayni (regresyon araclarinda otomatik yok).
- `DragRace`: oyuncu/YZ otomatikte frende N + devir stall'da, birakinca konvertor; YZ otomatikte vites aninda
  (0.30 s ara, oyuncu ile ayni; eski 0.35 s "debriyajli" gecis kalkti), DCT kalkisi 150 ms (oyuncu ile ayni).
  Manuel/dogbox/DCT'de "aks hissi": gerilme > %85 kopma ise 80 ms debriyaj kaydir + gaz 0.7.
- `RoadCar`: otomatikte vites karari arac hizindan cikis devri (konvertor kaymasi/patinaj 5. vitese atlatmiyordu).
  0-100 (YZ, sokak): A90 8.7 -> 7.3 s, Hellcat 9.9 -> 8.3 s, SS 454 11.4 -> 9.1 s.
- Test `drag_race_test` [7] konvertor: frende stall devrine yerlesir, tork carpimi 1.9, vites darbesi sinirli,
  N'de tork yok; [8] dort otomatik arac yari slick + slick/parcali kalkista aks kirmaz.
- Stok otomatik ETleri eski modele gore esit ya da daha iyi (slick: A90 13.67 -> 12.94, Hellcat 14.01 -> 13.12;
  SVT eskiden hic bitiremiyordu).

- Hellcat (900 Nm, RWD, sokak lastigi) YZ test surucusuyle 4 km'de 1 kez yoldan cikti (acik yol YZ temposu).
  2026-10-02 tekrar olculdu: #269/#255/#227/#318/#36, yol + dag + 2 karma yolu, tempo 0.55/0.60: kurtarma 0,
  yol disi 0 s, en fazla 8 deg govde kaymasi -> sorun artik yok.

## Guncelleme — 2026-10-02: Gercekci drag ETleri (pist hazirligi + YZ kalkis plani)
Sorun: sokak lastiginde ETler gercegin ~2.5 s ustunde, 60 ft ~3.2 s (A90 15.2 s; gercek ~12.4). Trap hizlari dogruydu.
Olcum: `ideal60.cpp` (YZ'siz en iyi sabit gaz/debriyaj) Mustang sokak lastiginde tepe ivme 0.44 g -> sinir FIZIKTE.
- **Pist hazirligi** (`DragRace.cpp` `trackPrep`): oyundaki drag pistinde zemin tutusu sokak x1.35, yari slick x1.20,
  slick x1.10 (VHT; sokak lastigi en cok kazanir). `zehra_sim` (Faz 1 referansi) duz asfalt: regresyon 9/9 ayni.
- **YZ kalkis plani** (`DragRace::planLaunch`): kalkis devri {%35, %50, %65 redline} x debriyaj birakma {0.12, 0.6 s}
  ayni fizikle 60 ft'e kadar denenir (her aday 2 farkli 2-step evresinde, en kotusu; 1.5 s bekleme); aks gerilmesi
  > %92 kopma, stop ya da bogulma devri (1.5 x rolanti, en az %20 redline) altina dusen aday elenir. Arac+parca
  basina onbellek (mutex). Rakip icin `std::async` ile yaris kurulurken arka planda (masaustu ~1 s); agac fazi
  basinda uygulanir (sabit sim ani -> deterministik). `aiRun`: kalkis sonrasi YZ (yaris ve deneme ortak).
- Geri beslemeli gaz/debriyaj kontrolu denenip reddedildi: dusuk hizda lastik kayma tepkisi ~0.15 s gecikir ->
  salinir; 2-step kalkista acik kalirsa atesleme kesmesi kaymayi titretir (ayrinti: f990d47'deki not).
- **Hata**: yesilden sonra YZ tepki suresinde faz Run oldugu icin gaz 0'a iniyordu -> yarim gazla kalkip bogulma.
- Sonuc (stok, YZ): Civic 15.02 (gercek ~15.2), Mustang GT 13.61 (~13.1), RX-7 13.83 (~13.5-14), A90 12.99 (~12.3),
  McLaren F1 12.02 (~11.1), Sahin 18.17 (~19). 120 eslesme: 120/120 bitirdi, 0 aks kirma, ort ET 13.153 (once 13.843).
- `tune_test` [8] guncellendi: cekis sinirinda kisa son disli 60 ft'i belirgin degistirmez (eski dump'ta kazandiriyordu).
- Telefonda planlama ~3 s surebilir; agac basinda bitmediyse kisa takilma olur (olculmedi).

## Guncelleme — 2026-10-02: Yol kokpiti (yatay/dikey), vites kollari, debriyaj modu, ABS / TC
- `Cockpit`: analog gaz/fren/debriyaj + vites kolu (H-desen / otomatik P-N-D / sirali +/-), yatay ve dikey
  yerlesim. Yol ekrani ayardan YATAY (varsayilan) ya da DIKEY; ekranda direksiyon tusu yok (egim / A-D).
- DEBRIYAJ (H) ayari: OYUNCU (analog pedal; debriyajsiz vites girmez) / OTOMATIK (bedel: virajda R<350 m
  vites degismez, 1.6 kat gec kavrar, odul %75). Sirali: dogbox 35 ms / DCT 60 ms. Otomatik: P/N/D.
- Android egim ekseni ekran donusune gore (Display.getRotation, yarim saniyede bir).
- ABS / TC: `SafetyTable.inc` fabrika donanimi (o yil/versiyon standart; YAKLASIK, yayin oncesi dogrula).
  324 aracin 171'inde ABS, 91'inde TC. Olmayan araca parca dukkani ELEKTRONIK: ABS / ABS+TC (ONCE ECU GEREKLI;
  fabrikada olan satilmaz). Kayit: `elx=abs;tc` satiri.
- Fizik (`VehicleSim::updateElectronics`, yalniz tune'lu oyun kurulumu; regresyon ayni): ABS anlik kayma
  < -%12'de freni orantili birakir; TC cekis kaymasi > %10'da motor torkunu (PowertrainCore::setTorqueLimit)
  orantili keser. Olculen: Sahin+ABS 100-0 50.3 -> 38.8 m, kilitli zaman %94 -> %0; Hellcat sokak lastigi
  TC ile kayma 5.7 -> 0.11, 2 s'de 5.5 -> 7.8 m. Acik yolda yardim dugmesi TC yoksa "TC YOK"; drag'de TC
  YZ'de kapali, oyuncuda ayara gore. Test: `electronics_test` (ctest `abs_tc`).
- Pedal sirasi gercek arac gibi soldan: debriyaj, fren, gaz (debriyajsizda fren en solda).

## Guncelleme — 2026-10-02: KARMA yaris (drag + viraj tek yarista)
- Kullanici tasarimi: sehirlerarasi yarista uzun duzlukte drag (ivme/son hiz), belirli mesafede oyun 3B virajli
  bolume gecer (yavaslama + surus becerisi), virajlar bitince tekrar drag. Uc ayri yaris turu: drag (pist, DragScreen),
  karma (RoadSession::Mode::Karma), yalniz viraj (yol yarisi 4 km / dag yolu).
- `RoadPath::karma`: duz 2.8-3.6 km -> 1-3 virajlik blok (R 70-220 m, %60 S ihtimali) -> duz 1.5-2.5 km -> blok ->
  bitis duzlugu; egim %4 (drag duzlugunde de yokus/inis). `sections()`, `curvyAt(s)`: blogun 120 m oncesi
  (fren bolgesi) - 60 m sonrasi virajli. Trafik yok (kapali yol), rakip kendi (sol) seridinde.
- Tek fizik, iki kamera (`RoadScreen::drawWorld`): duzde yandan dar acili kamera (drag gorunumu; arac seridi kendi
  tutar, oyuncu gaz/debriyaj/vites), virajli bolumde takip kamerasi; gecis 0.9 s smoothstep kamera ucusu.
  HUD: "DRAG VIRAJA N M" / "VIRAJ BOLUMU". Otomatik debriyaj cezasi karma'da virajli bolumde vites kilidi.
- Test `road_test` [8]: iki blok, ilk duzluk >= ~2.8 km, duz bolum gercekten duz, yaris sonuclanir, belirlenimci.
- Bilinen / sonraki: karma odulu normal yol yarisi odulu (mesafeye gore ayarlanmadi); YZ duzlukte de pace 0.6
  (drag'de tam gaz ama vites/kalkis plani yok); drag gorunumunde agac (yesil isik) yerine 3-2-1 geri sayim.

## Guncelleme — 2026-10-02: Oyun testi geri bildirimi (oncelikli 4 madde, ara taslak)
- H-desen kol hatasi: debriyajsiz vites denenince (cıtırtı) kol gercek vitese geri seker (DragScreen + Cockpit::setKnobGear),
  titresim + "DEBRIYAJ!". Onceden kol yeni viteste kaliyor, debriyaja basinca vites aniden giriyordu.
- Karma yaklasim: `RoadPath::kKarmaLead` = 450 m (virajli bolum ilk virajdan bu kadar once baslar); `RoadSection`
  `entry` (ilk viraj) ve `minR`. HUD onerilen giris hizi sqrt(0.8 g Rmin); (v²-vRec²)/(2·7) > kalan-15 ise "FRENE BAS!".
  Levhalar 300/200/100 m + girişte ok. Test road_test [8].
- Parca dukkani: 1. dokunus onizleme (`PartsScreen::statsFor` ile once > sonra), 2. dokunus onay cubugu EVET/VAZGEC.
- Galeri: arac al/sat onay penceresi; satis dokumu `saleQuote` (arac %65 + parca %40 - tamir bedeli, alt sinir %25).
  `repairCostFor(OwnedCar)` serbest fonksiyon. Test career_test [11].
- Test kancasi `ZK_TAPS` (desktop_main, App::tapVirtual).

## Guncelleme — 2026-10-02: Eksikler (ara taslak)
- Parca onizlemesi: `DragRace::estimateQuarter` (YZ kalkis plani + yaristaki fizik/pist hazirligi, tek serit,
  onbellekli) arka plan thread'inde; ~2.3 s (masaustu, ilk kez). Panelde 1/4 mil once > sonra ve guc egrisi
  (gri simdiki, turuncu parcali). Test drag_race_test [9]: tahmin = yaris ETsi (Sahin 18.169 s).
- Garajdan satis: SAT dugmesi (2+ arac), ortak `drawSaleDialog` (galeri ile ayni dokum); Android geri tusu
  pencereyi kapatir (`GarageScreen::modal`).
- Karma: drag agaci (geri sayimin son 1.5 s'si 3 amber, yesil 1 s), oyuncu tepki suresi (arac 30 cm ilerleyince),
  rakip tepkisi 0.15-0.35 s (tohumdan) + `RoadCar::launchRpm` = %30 redline (acik yolda olculdu: 0/30/40/50/60%
  arasinda en iyi 400 m; Aygir S5 GT -1.25 s; otomatiklerde etkisiz). Odul `prizeScale` = uzunluk/4 km (1-2.5).
  Test road_test [8].
- Hiz hissi (drag gorunumu): iki katman paralaks tepe, hiz cizgileri (>12 m/s), ruzgar sesi (`App::wind`: beyaz
  gurultu, 2 kutuplu alcak geciren, genlik ~hiz^2; ekran degisince 0). Ses kulakla dinlenmedi.
