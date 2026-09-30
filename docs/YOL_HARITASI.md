# ZEHRA KINIK — Yol Haritası

Her faz, "bitti" ölçütleri sağlanmadan kapanmaz: derleme uyarısız, ilgili regresyon testleri geçer,
oyun içi ekran görüntüsü/ölçüm ile doğrulanır, CI yeşil ve indirme linki güncel.

| Faz | İçerik | Durum |
|---|---|---|
| 1 | Tekerlek/lastik, güç aktarma, arıza çekirdeği; 0-400 m konsol simülasyonu | ✅ |
| 2 | 324 araç / 280 motor / 127 şanzıman kataloğu, prosedürel ses, süspansiyon + yol | ✅ |
| 2.5 | Grafik istemci iskeleti (Android + Windows + Linux), garaj ekranı, CI + indirme linki | ✅ |
| **3** | **Oynanabilir drag yarışı (yan mod / yatay ekran)** | ⏳ ara taslak oynanabilir, cila sürüyor |
| 4 | Kariyer ve ekonomi: para, garaj, parça/tuning, swap, dyno, kayıt | ⏳ ara taslak oynanabilir |
| 5 | Dik mod: 3D spline yol, jiroskop direksiyon, trafik, virajlar, yanal lastik modeli | |
| 6 | Isı, arıza ve yakıt sistemleri; şehirlerarası maraton, benzinlik, polis, çekici | |
| 7 | Çok oyunculu: deterministik rollback netcode, sunucu doğrulaması, modlar | |
| 8 | Görsel cila: araç modelleri, dokular, çevre, efektler (duman, akkor manifold) | |
| 9 | Ses/haptik cilası, ayarlar, yerelleştirme, mağaza hazırlığı | |

## Faz 3 — Drag yarışı: bitti ölçütleri
1. `VehicleSim`: araç fiziği (güç aktarma, 4 tekerlek, süspansiyon, arıza) tek sınıfta; `zehra_sim` onu kullanır
   ve 0-400 m sonuçları yeniden düzenleme öncesiyle birebir aynıdır (regresyon testi).
2. Yarış akışı: araç seçimi → burnout kutusu → pre-stage/stage → Christmas tree (sportsman .500 / pro .400)
   → yarış → sonuç ekranı. Kırmızı ışık, stop etme, aks kırılması doğru işlenir.
3. Zaman ölçümü: reaksiyon, 60 ft, 330 ft, 1/8 mil ET + hız, 1000 ft, 1/4 mil ET + trap hızı.
4. Kontroller: analog debriyaj (ısırma noktası), analog gaz, fren; H-desen vites kolu (sürükle),
   DCT/dogbox/otomatik için vites pedalları; masaüstünde klavye karşılıkları.
5. Rakip: aynı fizikle çalışan yapay zekâ sürücü (reaksiyon ve vites zamanlaması değişken).
6. Görüntü: 640x360 sanal tampon, yan görünüm, paralaks arka plan, pist, ağaç, iki araç, HUD
   (devir, vites, hız, vites ışığı, zamanlar).
7. Ses: oyuncu motoru yük altında, rakip motor uzaklığa göre kısık.
8. Android'de yarışta yatay, garajda dikey ekran yönü.

### Faz 3 durum (ara taslak)
- [x] 1. `VehicleSim` + regresyon testi (9 senaryo byte byte aynı)
- [x] 2. Yarış akışı, kırmızı ışık, stall, aks kırılması (`tests/drag_race_test`)
- [x] 3. Zaman fişi: RT, 60 ft, 330 ft, 1/8 + hız, 1000 ft, 1/4 + trap
- [x] 4. Kontroller: analog debriyaj/gaz, fren, H-desen kol, pedallar, klavye (masaüstünde betikli testle doğrulandı)
- [x] 5. Yapay zekâ rakip
- [x] 6. Görüntü: 640x360 yan görünüm, paralaks, ağaç, duman, egzoz alevi, HUD, sonuç ekranı
- [x] 7. Ses: iki motor, rakip uzaklığa göre kısık
- [ ] 8. Android ekran yönü: kodlandı, cihazda doğrulanmadı
- [x] Lastik cıyaklaması, haptik (kalkış, vites, kesici, aks, kırmızı ışık, flat-spot), ağaç tipi seçimi
- [x] Performans göstergesi (FPS + kare başına fizik süresi; masaüstünde 2 araç 20 kHz = 0.6 ms)
- [ ] Cila: burnout su havuzu, ağaç ışığı sesi

## Faz 4 — Kariyer ve ekonomi: bitti ölçütleri
1. Kayıt sistemi: para, sahip olunan araçlar ve her aracın parça/ayar durumu; Android iç depolama,
   masaüstünde kullanıcı klasörü. Bozuk/eksik kayıtta güvenli varsayılan. Birim testli (yaz-oku-karşılaştır).
2. Galeri: 324 aracın fiyatı (güç, yıl, nadirlik, kasa tipinden türetilir), satın alma / satma.
   Başlangıç: mütevazı bir yerli araç + az para.
3. Parça dükkânı: her parça fizikte gerçek bir parametreyi değiştirir (lastik, debriyaj, aks, diferansiyel,
   son dişli, hafifletme, emme/egzoz, ECU, turbo kiti, kuru karter). Parçanın etkisi birim testle ölçülür
   (ör. slick lastik 60 ft'i kısaltır, krom-moly aks kırılmaz).
4. Dyno ekranı: motorun güç/tork eğrisi (parçalarla birlikte) grafik olarak.
5. Yarış ödülleri: rakip zorluğuna göre para; kazanç/kayıp kaydı.

### Faz 4 durum (ara taslak)
- [x] 1. Kayıt: sürümlü, sağlama toplamlı, atomik; Android iç depolama / masaüstü kullanıcı klasörü (`tests/career_test`)
- [x] 2. Galeri: 324 araç fiyatı, satın alma / satma
- [x] 3. Parça dükkânı: 12 kategori, fizikte gerçek etki (`tests/tune_test`), aks riski tahmini
- [x] 4. Dyno: parçalı vs stok güç/tork eğrisi, sesli çekiş
- [x] 5. Yarış ödülü + performansa göre rakip eşleştirme (yapay zekâ parça setiyle)
- [ ] Denge: ödül/fiyat ekonomisi oyun testiyle ayarlanacak; aynı rakibe tekrar yarışta ödül azalması
- [ ] Frankenstein motor swap (Faz 3.5 şartnamesi: blok + kapak) — Faz 6 motor atölyesiyle birlikte

## Faz 5 — Dik mod (viraj, jiroskop, trafik): bitti ölçütleri
1. Düzlemsel araç dinamiği: yanal Pacejka (kayma açısı), çekiş elipsi, yaw; drag modunun 1B fiziği birebir
   korunur (regresyon). Testler: düz çizgide 1B ile uyum, sabit yarıçaplı skidpad'de yanal ivme lastik
   sınırında (sokak ~0.85-1.0 g), FWD'de gaz altında understeer, RWD'de gazla oversteer/spin, düşük hızda
   sayısal kararlılık.
2. Yol: 3D spline (viraj + eğim), şeritler, kenar; pozisyon/şerit sorgusu.
3. Trafik: şerit takip eden araçlar, basit çarpışma.
4. Kontrol: Android jiroskop/ivmeölçer eğimi = direksiyon; masaüstünde klavye; dokunmatik gaz/fren.
5. Görüntü: arkadan perspektif kamera, PS1 tarzı yol ve çevre, trafik araçları, HUD.
6. Mod: otoban akışı (skor: yakın geçiş, hız, apex), çarpışmada ceza.

### Faz 5 durum (ara taslak)
- [x] 1. Düzlemsel dinamik: yanal Pacejka, çekiş elipsi, yaw; 1B drag fiziği regresyonla korunuyor (`tests/planar_test`)
- [ ] 2. Yol: klotoidli viraj, şerit, kenar, izdüşüm var (`tests/road_test`); **eğim yok**
- [x] 3. Trafik: şerit takibi, IDM fren/takip, çarpışma
- [x] 4. Kontrol: Android ivmeölçer eğimi (hassasiyet ayarlı), klavye, dokunmatik
- [x] 5. Görüntü: arkadan perspektif kamera, PS1 tarzı yol/ağaç/direk, trafik, HUD
- [x] 6. Otoban akışı: yakın geçiş (karşı şerit 2x), kombo, hız, karşı şerit, apex; çarpışma cezası (`tests/flow_test`)
- [x] Ek: yol yarışı (YZ rakip) ve 3 km dağ yolu; kariyer ödülü
- [ ] Oyuncu-rakip teması gerçek çarpışma fiziği değil (basit itme); akış ekonomisi oyun testiyle ayarlanmadı

## Genel: Ayarlar (yapıldı)
- [x] FPS sınırı (ölçüldü: 30/60/144 hedefe ±%0.5), dikey eşitleme (sürücü reddederse yazılımla ekran hızına sınır),
  FPS göstergesi, tam ekran, tam sayı ölçek, ses kanalları, titreşim, eğim, sürüş yardımı/vites varsayılanı,
  km/h-mph, drag ağacı (`tests/settings_test`)
