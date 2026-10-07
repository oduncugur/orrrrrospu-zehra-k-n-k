# Play Store yayin kontrol listesi

Hazir (repoda):
- targetSdk 35, compileSdk 36, minSdk 26
- 64-bit (arm64-v8a, x86_64) + armeabi-v7a
- 16 KB sayfa boyutu uyumlu native kutuphane
- Uygulama simgeleri (klasik turuncu, acik dunya mavi) + 512x512 magaza simgeleri: store/*_icon_512.png
- Imzali AAB: ZK_KEYSTORE_B64 / ZK_KEYSTORE_PASS secret'lari girilince CI "play-store-aab" artifact'i uretir
- Gizlilik politikasi metni: store/GIZLILIK.md (bir web sayfasinda yayinlanip linki Play Console'a girilmeli)

Senin yapman gerekenler (Play Console):
1. Gelistirici hesabi (25 USD) ve kimlik dogrulamasi
2. Keystore uretip GitHub secret'larina ekle (Play App Signing acik olsun)
3. Yeni kisisel hesaplar: 12 test kullanicisiyla 14 gun kapali test zorunlu
4. Icerik derecelendirme anketi, veri guvenligi formu ("veri toplanmaz"), hedef kitle
5. Magaza gorselleri: 1024x500 one cikan gorsel, en az 2 ekran goruntusu
6. Paket adlari: com.zehrakinik.game ve com.zehrakinik.game.world (iki ayri uygulama)
