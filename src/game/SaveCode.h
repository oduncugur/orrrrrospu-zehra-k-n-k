// ZEHRA KINIK - Kayit kodu: kariyer kaydi (Career::serialize metni, sonunda checksum) -> "ZK1-" + base64.
// Telefon <-> bilgisayar arasi panodan tasima icin; sunucu gerekmez. Bozuk / eksik kod Career::parse'ta reddedilir.
#pragma once
#include <string>

namespace zk {

inline std::string base64Encode(const std::string& in) {
    static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((in.size() + 2) / 3 * 4);
    size_t i = 0;
    for (; i + 2 < in.size(); i += 3) {
        const unsigned v = (unsigned char)in[i] << 16 | (unsigned char)in[i + 1] << 8 | (unsigned char)in[i + 2];
        out += t[v >> 18 & 63]; out += t[v >> 12 & 63]; out += t[v >> 6 & 63]; out += t[v & 63];
    }
    if (i < in.size()) {
        const unsigned v = (unsigned char)in[i] << 16 | (i + 1 < in.size() ? (unsigned char)in[i + 1] << 8 : 0);
        out += t[v >> 18 & 63]; out += t[v >> 12 & 63];
        out += i + 1 < in.size() ? t[v >> 6 & 63] : '=';
        out += '=';
    }
    return out;
}

// Gecersiz karakter / bicimde false (bosluk ve satir sonlari atlanir: kopyalarken bolunmus kod)
inline bool base64Decode(const std::string& in, std::string& out) {
    auto val = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        return c == '+' ? 62 : c == '/' ? 63 : -1;
    };
    out.clear();
    unsigned acc = 0; int bits = 0;
    for (char c : in) {
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;
        if (c == '=') break;
        const int v = val(c);
        if (v < 0) return false;
        acc = acc << 6 | (unsigned)v; bits += 6;
        if (bits >= 8) { bits -= 8; out += (char)(acc >> bits & 0xFF); }
    }
    return true;
}

inline std::string makeSaveCode(const std::string& saveText) { return "ZK1-" + base64Encode(saveText); }

// Koddan kayit metni (onek ve base64 denetimi; asil dogrulama Career::parse checksum'u)
inline bool readSaveCode(std::string code, std::string& saveText) {
    const size_t p = code.find("ZK1-");
    if (p == std::string::npos) return false;
    code = code.substr(p + 4);
    return base64Decode(code, saveText) && !saveText.empty();
}

} // namespace zk
