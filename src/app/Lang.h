// ZEHRA KINIK - Dil destegi. Kaynak dil Turkce (buyuk harf, piksel font). Ceviri ekrana yazarken uygulanir
// (Renderer::text): once tam cumle tablosu, yoksa kelime kelime sozluk (sayilar / noktalama korunur). Boylece
// kodda her metne dokunmadan tum arayuz cevrilir; dinamik metinler ("VIRAJ 192 M") da kelime duzeyinde cevrilir.
// Piksel font Latin + Kiril + Yunan buyuk harf: Latin alfabeli diller aksansiz yazilir (DEUTSCH: UE/OE/AE).
#pragma once
#include <string>

namespace zk {

enum class Lang { TR = 0, EN, DE, ES, FR, IT, PT, RU, UK, EL, PL, NL, ID, Count };
const char* langName(Lang l);                 // ayarlar ekraninda (kendi dilinde)
// Metni cevirir (TR ise aynen dondurur). Sonuclar onbellekte (kare basina tekrar eden metinler ucuz).
const std::string& translate(Lang l, const std::string& s);

} // namespace zk
