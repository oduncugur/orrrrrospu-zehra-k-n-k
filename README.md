# ZEHRA KINIK — Çekirdek Simülasyon Motoru

Saf C++17 (SDL3 / OpenGL ES 3.0 katmanları sonraki fazlarda). Bu depo **Faz 1**'i içerir: dış bağımlılığı olmayan,
deterministik güç aktarma ve lastik fiziği çekirdeği ile terminalde çalışan 0-400 m drag simülasyonu.

## Derleme ve çalıştırma
```sh
cmake -B build && cmake --build build
./build/zehra_sim                 # krom-moly aks, ıslak karter, 100 oktan
./build/zehra_sim --stock-axles   # clutch dump'ta aks kırılır
./build/zehra_sim --oil=3.0 --e85 --dry-sump --csv=telemetri.csv
./build/zehra_sim --help
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
