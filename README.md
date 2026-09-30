# ZEHRA KINIK — Çekirdek Simülasyon Motoru

Saf C++17 (SDL3 / OpenGL ES 3.0 katmanları sonraki fazlarda). Bu depo **Faz 1**'i içerir: dış bağımlılığı olmayan,
deterministik güç aktarma ve lastik fiziği çekirdeği ile terminalde çalışan 0-400 m drag simülasyonu.

## Derleme ve çalıştırma
```sh
cmake -B build && cmake --build build
./build/zehra_sim                 # krom-moly aks, ıslak karter, 100 oktan
./build/zehra_sim --stock-axles   # clutch dump'ta aks kırılır
./build/zehra_sim --oil=3.0 --e85 --dry-sump --csv=telemetri.csv
./build/zehra_sim --car=227 --road=sehir   # katalogdan araç, şehir içi bozuk yol
./build/zehra_sim --help

./build/zehra_garage --list               # 324 araçlık garaj
./build/zehra_garage --info=5             # motor verisi, tork eğrisi, vites oranları, ses imzası
./build/zehra_garage --car=5 --wav=ek.wav # prosedürel motor sesi (rölanti → redline → kesici → gaz kesme)
./build/zehra_garage --export=cikti/      # tüm araçlar: OBJ+MTL model, WAV önizleme, katalog.csv
```

## Modüller (`src/sim`)
| Sınıf | İçerik |
|---|---|
| `WheelSimulation` | `I_w·dω/dt = T_axle − T_brake − F_x·r_eff − C_rr·F_z·r_eff`, `I_w = ½·m·r²`, Pacejka Magic Formula, κ (spesifikasyon formülü + gevşeme boylu transient κ), PSI’ya bağlı `r_eff` ve yanak burulması, 80–105 °C sıcaklık penceresi, yük hassasiyeti, ABS’siz kilitlenmede flat-spot ve tur başına haptik vuruntu |
| `VehicleLoad` | `ΔF_z_long = m·a_x·h/L`, `ΔF_z_lat = m·a_y·h/W_track`, kuru karterde düşük CoG |
| `PowertrainCore` | Düşük/yüksek kam eğrileri, VTEC geçişi (devir + yağ basıncı + yük, histerezisli), ITB (4 ms) ve plenum (70 ms) gaz tepkisi, 8V/16V/20V nefeslenme, kademeli subap, conta kalınlığından CR ve Otto verimi, BSFC (`dm/dt = HP·BSFC/3600`), DFCO, analog debriyaj ısırma noktası + balata ısısı/fade + stall, 2-step, 1.5-Way plaka LSD (45°/60° ramp) |
| `DrivetrainFailure` | Aks burulma gerilmesi `τ = 16T/(πd³)` → kalıcı burulma / kırılma; ıslak karterde G’ye bağlı pickup açığa çıkması, yağ basıncı düşümü, devir² ile ölçeklenen yatak hasarı → 3 s’de yatak sarma |

Entegrasyon 100 kHz sabit alt adım (debriyaj ve LSD kilit sertliği için); aynı girdiler aynı sonucu üretir (rollback netcode için ön koşul).

## Eklenen mühendislik detayları
- Lastik gevşeme boyu (relaxation length) ile transient kayma: düşük hızda sayısal kararlılık, gerçekçi lastik gecikmesi.
- Yük hassasiyeti (F_z arttıkça μ düşer), yüksek devirde slick lastik santrifüj büyümesi.
- Gövde pitch gecikmesi (yük transferi süspansiyon üzerinden 80 ms’de oturur).
- Debriyaj balatası ısınması ve fade, şanzıman verimi (sürüşte η, motor freninde 1/η).
- Tüketilen yakıt kütlesi araç kütlesinden düşülür.

## Faz 2: Garaj, prosedürel ses, low-poly model, süspansiyon

### Araç kataloğu (`src/garage`)
- **324 gerçek araç, 280 gerçek motor, 127 gerçek şanzıman** (`VehicleTable.inc`, `EngineTable.inc`, `GearboxTable.inc`).
- Teknik veriler gerçektir: silindir düzeni, hacim, supap sayısı, ateşleme sırası, güç/tork ve devirleri, redline,
  kademeli kam geçiş devri, turbo/kompresör, vites oranları ve son dişli, ağırlık, ölçüler, ağırlık dağılımı.
- Tork eğrisi, motorun gerçek tepe tork ve tepe güç noktalarından geçecek şekilde üretilir (`buildEngineSpec`).
- `zehra_sim --car=N` bu verilerle FWD/RWD/AWD drag simülasyonu yapar (aks çapı, debriyaj kapasitesi,
  vites geçiş tipi — H-desen, tork konvertörü, DCT, dogbox 35 ms ateşleme kesme — araçtan türetilir).

**Telif / marka politikası:** oyunda görünen marka, model, motor ve şanzıman kodları kurgusaldır
(ör. Honda → *Kaze*, Porsche → *Stahlwerk*, Tofaş → *Bursa*, Anadol → *Kınık*). Gerçek isimler yalnızca `ref`
alanlarında geliştirici referansı olarak durur, arayüzde gösterilmez (`zehra_garage --dev` ile görülebilir).
Kasa modelleri gerçek tasarım kopyası değildir; ölçülerden prosedürel üretilir, logo içermez.
`~` ile başlayan `ref` değerleri yaklaşıktır ve yayın öncesi doğrulanmalıdır. Bu bir hukuki görüş değildir;
yayın öncesi bir fikri mülkiyet avukatına danışılması önerilir.

### Prosedürel motor sesi (`src/audio`) — sıfır .wav varlığı
- Ateşleme olayları gerçek ateşleme sırasından krank açısına yerleştirilir, `f_fire = RPM·silindir/120`.
- Bank ayrımı (tek/çift silindir ya da ilk/ikinci yarı) + bank başına farklı boru boyu: crossplane V8'de
  bank başına eşitsiz aralık (homurtu), flatplane'de eşit aralık (çığlık).
- Boxer eşitsiz header: silindir başına Δt gecikmesi (rumble). 90° odd-fire V6: 90°/150° aralık.
  Rotari: eksantrik mil turu başına rotor sayısı kadar ateşleme.
- Egzoz borusu: bank başına dalga kılavuzu (delay line, açık uç yansıması, kayıp) + susturucu alçak geçiren.
- VTEC/kam geçişi parlaklık ve emme sesi sıçraması, ITB emme homurtusu, turbo ıslığı, flutter
  `sin(ωt)·e^(−λt)`, kompresör uğultusu, dogbox inlemesi `f = mil devri × diş`, gaz kesmede patlamalar.
- `ProceduralEngineAudio::render()` bellek ayırmaz; SDL3 ses geri çağrısında doğrudan kullanılabilir.

### Low-poly model (`LowPolyModel`)
Kasa tipine göre istasyon kesitleri gerçek uzunluk/genişlik/yükseklik/dingil ölçüleriyle loft edilir;
8 kenarlı PS1 tarzı tekerler, far/stop, yarış araçlarında kanat ve farsızlık, kompresörde kaput scoop'u.
Araç başına ~180 vertex / ~300 üçgen. OBJ+MTL olarak dışa aktarılır.

### Süspansiyon ve yol bozuklukları (`src/sim/Suspension`)
- 7 serbestlik dereceli sürüş modeli: gövde heave/pitch/roll + 4 yaysız kütle.
- Köşe başına helezon yay, çift yönlü (bump/rebound) dijresif amortisör, progresif takoz, amortisör tam uzama
  limiti (iç tekerlek kalkması), ön/arka viraj denge çubuğu.
- Lastik dikey yayı PSI'ya bağlı ve yalnız basıya çalışır: tekerlek yerden kesilebilir (>15 ms havalanma sayılır),
  temas izi zarflama filtresi.
- Yük transferi artık süspansiyon dinamiğinden doğar (pitch/roll gecikmesi, sekme); `WheelSimulation`'a
  anlık `F_z` olarak verilir — havadaki tekerlek çekiş üretemez.
- Yol: ISO 8608 pürüzlülük sınıfı (A–H) + kasis, çukur, rögar, dilatasyon derzi, arnavut kaldırımı, tümsek.
  Hazır profiller: `drag`, `otoban`, `sehir`, `koy`, `dag`. Deterministik tohum (rollback netcode için).
