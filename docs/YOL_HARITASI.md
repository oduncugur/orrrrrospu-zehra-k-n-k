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
- [x] 2. Yol: klotoidli viraj, şerit, kenar, izdüşüm, yükseklik/eğim (şehirlerarası ≤%5, dağ ≤%9) (`tests/road_test` [6])
- [x] 3. Trafik: şerit takibi, IDM fren/takip, çarpışma
- [x] 4. Kontrol: Android ivmeölçer eğimi (hassasiyet ayarlı), klavye, dokunmatik
- [x] 5. Görüntü: arkadan perspektif kamera, PS1 tarzı yol/ağaç/direk, trafik, HUD
- [x] 6. Otoban akışı: yakın geçiş (karşı şerit 2x), kombo, hız, karşı şerit, apex; çarpışma cezası (`tests/flow_test`)
- [x] Ek: yol yarışı (YZ rakip) ve 3 km dağ yolu; kariyer ödülü
- [x] Oyuncu-rakip teması: yönlü kutu + impuls (kütle, yaw ataleti, sürtünme) (`tests/contact_test`)
- [ ] Akış ekonomisi oyun testiyle ayarlanmadı; trafik araçları kinematik (çarpınca yeniden doğar)

## Genel: Ayarlar (yapıldı)
- [x] FPS sınırı (ölçüldü: 30/60/144 hedefe ±%0.5), dikey eşitleme (sürücü reddederse yazılımla ekran hızına sınır),
  FPS göstergesi, tam ekran, tam sayı ölçek, ses kanalları, titreşim, eğim, sürüş yardımı/vites varsayılanı,
  km/h-mph, drag ağacı (`tests/settings_test`)

## Genel: Kontroller, karma yarış, ABS/TC (yapıldı, ara taslak)
- [x] Yol ekranı yatay/dikey, analog gaz/fren/debriyaj, şanzımana göre vites kolu (H-desen / P-N-D / sıralı)
- [x] Debriyaj modu: oyuncu / otomatik (bedeli: virajda vites yok, geç kavrama, ödül %75)
- [x] ABS / TC yalnız gerçekte olan araçlarda; olmayana ECU + ELEKTRONİK parçası (`tests/electronics_test`)
- [x] Karma yarış: düzde drag görünümü, virajlı bölümde 3B sürüş, tek yarışta (`tests/road_test` [8])
- [ ] Karma: kalkışta drag ağacı, mesafeye göre ödül, YZ'nin düzlükte drag kalkış planı

---

# SIRADAKİ PLAN (kullanıcı onayıyla, "söyleyince" başlanacak)
Sıra: **1) Oyun döngüsü / kariyer yapısı → 2) Harita detayı → 3) Görsel + ses.** Modifiye derinliği bunlarla
birlikte, parça parça eklenir. Her madde fizikte gerçek bir etkiyle gelir ve testle kilitlenir.

## 1. Oyun döngüsü / kariyer yapısı
- [ ] Hedef ve ilerleme: ligler / şehir bölgeleri (mahalle → şehir → şehirlerarası → dağ → pist), kilitli araçlar/parçalar
- [ ] Rakipler: isimli rakipler ve "patron" yarışları, rakibin bilinen arabası ve parça seti, tekrar maçı
- [ ] Yarış takvimi / "sıradaki yarış" önerisi; günlük/haftalık etkinlikler
- [ ] İtibar (ün) puanı: kazanınca artar, yeni yarışlar ve satıcılar açılır
- [ ] Ekonomi dengesi oyun testiyle (ödül, parça, tamir, yakıt); ikinci el parça alım-satım
- [ ] Bahisli yarış, pink slip (arabasına yarış) — Faz 7 öncesi tek oyunculu sürüm

## 2. Harita detayı ("dümdüz boşlukta yarış yok")
- [ ] Ortamlar: şehir içi (binalar, kavşak, kaldırım, sokak lambası), şehirlerarası (bariyer, tabela, köprü, tünel,
      gişe), dağ yolu (kaya duvar, uçurum bariyeri, viraj levhaları), sahil yolu, sanayi bölgesi
- [ ] Yol detayları: şerit çizgileri, kasis, rögar, yama asfalt, ıslak zemin/su birikintisi (tutuş), toprak banket
- [ ] Gün saati ve hava: gece (far, sokak lambası), yağmur (tutuş düşer), sis
- [ ] Mesafe hissi: dağ/silüet katmanları, ağaç ve bina yoğunluğu, kilometre taşları
- [ ] Bölge haritası ekranı: yarışların yeri, garaj, parça dükkânı, galeri, dyno

## 3. Görsel + ses
- [ ] Araç modelleri (kullanıcı isteğiyle en sona): daha doğru kasa, jant, far/stop, kapı çizgileri
- [ ] Kişiselleştirme görselleri: boya, jant, body kit, kanat, sticker, cam filmi, yükseklik
- [ ] Efektler: duman, kıvılcım, egzoz alevi (anti-lag/patlama), hız bulanıklığı, kamera sarsıntısı
- [ ] Ses: motor sesleri kulakla ayar (v2 geri bildirimi bekliyor), turbo/BOV, vites, lastik, rüzgâr, çarpışma, ortam

## Modifiye listesi (motor swap ve benzeri; kullanıcı isteği)
### Motor
- [ ] **Motor swap**: kataloğun 280 motorundan farklı araca motor nakli (motor takozu/kablaj kiti, ağırlık ve
      ağırlık dağılımı değişir, şanzıman uyumu/adaptör plakası, soğutma yeterliliği)
- [ ] **Frankenstein (blok + kapak)**: farklı blok ve silindir kapağı birleştirme (ör. B18 blok + B16 kapak);
      hacim, sıkıştırma oranı, kam/supap uyumu fizikte (şartname Faz 3.5)
- [ ] Motor revizyonu / stroker: hacim büyütme (strok/çap), dövme piston-biyel (devir ve boost sınırı)
- [ ] Kam mili profilleri (rölanti kalitesi ↔ üst devir gücü), supap yayları ve devir sınırı yükseltme
- [ ] Silindir kapağı porting/polish, büyük supap, ITB dönüşümü (gaz tepkisi + ses)
- [ ] Sıkıştırma oranı / kapak contası kalınlığı (vuruntu riski, yakıt oktanıyla ilişkili)
- [ ] Hafif volan (devir hızlı yükselir, rölanti/kalkış zorlaşır), dengeli krank
- [ ] Yağ sistemi: yağ soğutucu, karter bafılı, kuru karter (var)
- [ ] Soğutma: radyatör, termostat, fan (motor ısısı — Faz 6)
### Aşırı besleme ve yakıt
- [ ] Turbo seçimi: boyut (spool ↔ tepe güç), twin/sıralı turbo, ball-bearing, wastegate ve boost kontrolcüsü,
      vitese göre boost, blow-off valfi (ses), intercooler (ısı emme — Faz 6), su/metanol püskürtme
- [ ] Kompresör kiti: roots / twin-screw / santrifüj (anlık tork ↔ üst devir)
- [ ] Nitro (NOS): kuru / ıslak / kademeli, şişe basıncı ve ısısı, devir/vites penceresi, motor hasarı riski
- [ ] Yakıt sistemi: enjektör, pompa, regülatör, E85 / yarış benzini (kısmen var), flex-fuel
### ECU ve elektronik
- [ ] ECU harita ekranı: 16×16 AFR ve avans tablosu, vuruntu (Faz 6), devir sınırı, kesici tipi
- [ ] Launch control / 2-step ayarı, anti-lag (egzoz patlamaları, turbo ömrü), flat-shift (gazı kesmeden vites)
- [ ] ABS / TC (var), shift light, datalogger (yarış sonrası telemetri grafiği)
### Aktarma
- [ ] **Şanzıman swap**: H-desen → sıralı dogbox / DCT, 5 → 6 vites, otomatik → manuel dönüşümü
- [ ] Vites oranlarını tek tek ayarlama (yakın oranlı kutu), son dişli (var)
- [ ] Debriyaj (var): + çift/üç plaka, seramik balata; quick shifter
- [ ] Diferansiyel (var): + ön/arka ayrı LSD, AWD merkez dağılım ayarı
- [ ] **Çekiş dönüşümü**: FWD → AWD / RWD (ağırlık, kayıp, fiyat), kardan mili, aks (var)
### Şasi, süspansiyon, fren
- [ ] Coilover: yay sertliği, yükseklik, amortisör (bump/rebound); viraj denge çubukları
- [ ] Kamber / kaster / toe ayarı (şartname 2.6), drag için ön-arka yük aktarımı ayarı
- [ ] Wheelie bar ve paraşüt (drag), ağırlık transferi; akü bagaja (ağırlık dağılımı)
- [ ] Şasi güçlendirme: strut bar, roll cage (sertlik + ağırlık + güvenlik), karbon kaput/bagaj, lexan cam
- [ ] Fren: büyük disk/kaliper, balata tipi (ısı ve fade), fren dağılımı ayarı, hidrolik el freni (drift)
### Lastik, jant, aero
- [ ] Lastik ölçüsü/genişliği, drag radial, lastik sınıfı (sokak/yarı-slick/slick var), dövme jant (dönen kütle)
- [ ] Widebody (geniş lastik sığar), kanat/spoiler (downforce ↔ hava direnci), splitter, difüzör
### Yasallık ve bakım
- [ ] Sokak yasallığı: egzoz gürültüsü, nitro, slick → polis/muayene cezası (Faz 6), "pist araçları" romorkla
- [ ] Parça aşınması ve ömrü (turbo, debriyaj, lastik), bakım/tamir maliyeti, arızalı parça
- [ ] Dyno ayar oturumu: harita ince ayarı, güç kazanımı ↔ risk

---

# KULLANICI GERİ BİLDİRİMİ — ÖNCELİKLİ (2026-10-02, oyun testinden)
## Hatalar / his
- [ ] **H-desen kol ile gerçek vites ayrışıyor (hata):** debriyajsız vites denenince kol 5'e gidiyor ama vites girmiyor
      (çıtırtı). Araç düşük viteste kalıyor ("5'te ama hız transferi yok, viraja 1. viteste gibi giriyor"); debriyaja
      basınca 5 aniden giriyor. Çözüm: vites girmezse kol gerçek vitese geri sekmeli (+ titreşim, "DEBRİYAJ!"),
      kol her zaman gerçek vitesi göstermeli; vites göstergesi ile kol çelişmemeli.
- [ ] **Karma: virajlı bölüme geçiş çok ani, fren mesafesi yok.** Çözüm: viraj öncesi hıza göre yaklaşım mesafesi
      (v²/2a + pay; 200 km/h'te ~350-450 m), "VİRAJ 300/200/100 M" levhaları ve HUD uyarısı, kamera geçişinin
      fren noktasından önce bitmesi, viraj girişinde önerilen hız göstergesi.
- [ ] Karma: hız hissi (drag görünümünde arka plan paralaksı, rüzgâr sesi, hız çizgileri); vites/hız aktarımı hissi.
## Arayüz / ekonomi
- [ ] **Parça satın alma önizleme + onay:** ilk dokunuş = önizleme (yeni HP/Nm, tahmini 1/4 mil, aks riski, ağırlık
      farkı, dyno eğrisi karşılaştırması); ikinci dokunuş = "SATIN AL $X?" onayı; geri al/iptal.
- [ ] **Araç satma:** şu an yalnız galeride, yalnız seçili araç ve ancak 2+ araç varken (onaysız). Eklenecek:
      garajdan sat, satış fiyatı dökümü (araç + parçalar - hasar), onay penceresi, ikinci el pazarı/açık artırma.

# MODİFİYE DERİNLİĞİ — "THE STRIP" SEVİYESİ VE ÖTESİ
Kaynak: https://www.thestrip.io/ ve App Store sayfası (300+ araç, 5 seviye, gerçek debriyaj/vites/gaz;
"any crate engine or machine-shop build fits any car"; ayar: vites, son dişli, lastik, boost, NOS, süspansiyon,
ECU haritası; garaj yakıt, ağırlık, yağ, motor aşınması, hasar hatırlar). Hedef: en az bu kadar, fizikte gerçek.
## Motor atölyesi
- [ ] **Hazır (crate) motor**: katalog motorlarını kutudan satın al, her araca tak (takoz/kablaj kiti ücreti)
- [ ] **Atölye yapımı motor (machine shop)**: blok + kapak + krank + biyel + piston + kam + supap + emme manifoldu
      ayrı ayrı seçilir; her parçanın "en iyi olabileceği" bir değer var, kurulum kalitesi tezgâh işçiliğine bağlı
      (rastgele ama ustalık/zamanla iyileşen dağılım). Toleranslar → güç, devir sınırı, ömür.
- [ ] Motor aşınması ve revizyon (var: yatak hasarı) + kompresyon testi, yağ seviyesi/kalitesi, ısınma
## Aşırı besleme / yakıt / ECU
- [ ] Turbo boyutu + A/R, twin/sıralı, wastegate yayı, boost kontrolcü (vitese göre boost), BOV, intercooler
- [ ] Kompresör tipleri, kasnak çapı (boost)
- [ ] Nitro: jet boyutu (shot +HP), kademeli, şişe basıncı/ısıtıcısı, tetikleme devri/vitesi, dolum ücreti
- [ ] ECU haritası: AFR, avans, boost hedefi, devir sınırı, launch devri, vites kesici (flat-shift), anti-lag
## Aktarma / şasi / lastik
- [ ] Her vites oranı ayrı, son dişli (var), debriyaj (var) + hafif volan
- [ ] Süspansiyon: ön/arka sertlik, amortisör (drag'de ağırlık aktarımı), yükseklik, wheelie bar
- [ ] Lastik: tip (var), genişlik, basınç (var), ısıtma (burnout var), drag radial
## Garaj durumu (The Strip'teki gibi kalıcı)
- [ ] Yakıt seviyesi (ağırlık + benzinlik/pit), yağ seviyesi, motor aşınması, kaporta hasarı, parça aşınması
- [ ] Pit stop: yakıt, yağ, nitro dolumu, lastik değişimi
## Ekonomi / araç edinme (The Strip'ten esinle)
- [ ] Çekilmiş araç otoparkı / açık artırma, hurdadan araç alıp restore etme (parça parça), ikinci el parça
- [ ] Pink slip (+ 24 saat geri alma hakkı), bahisli yarış
## İlerleme / modlar (The Strip'ten esinle)
- [ ] Hikâye: bölgeler ve isimli rakipler, turnuvalar, "çağrı" (call-out) yarışları, prestij (sıfırla, kalıcı ün)
- [ ] Hayalet yarış, günün pisti, sınıf bazlı merdiven (Faz 7 ile çevrim içi)
## Kontrol / arayüz seçenekleri (The Strip'te olanlar)
- [ ] Pedal düzeni sol/sağ el seçimi, gösterge stili (klasik ibreli / dijital), vites geçişinde titreşim (var: haptik)
- [ ] CVT şanzıman tipi; oyun kolu tuş atama
