// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef IA_UTF8_HPP
#define IA_UTF8_HPP
#include <cstdint>
#include <string>
#include <vector>
namespace utf8 {
struct Glyph { uint32_t cp; size_t begin; size_t end; };
inline Glyph next(const std::string& s, size_t pos) {
    const size_t start = pos;
    if (pos >= s.size()) return {0, pos, pos};
    const auto b = static_cast<unsigned char>(s[pos++]);
    uint32_t cp = b;
    int n = 0;
    if (b >= 0xc2 && b <= 0xdf) { cp = b & 31; n = 1; }
    else if (b >= 0xe0 && b <= 0xef) { cp = b & 15; n = 2; }
    else if (b >= 0xf0 && b <= 0xf4) { cp = b & 7; n = 3; }
    else if (b >= 0x80) return {0xfffd, start, pos};
    for (int i = 0; i < n; ++i) {
        if (pos >= s.size() || (static_cast<unsigned char>(s[pos]) & 0xc0) != 0x80)
            return {0xfffd, start, start + 1};
        cp = (cp << 6) | (static_cast<unsigned char>(s[pos++]) & 63);
    }
    if ((n == 1 && cp < 0x80) || (n == 2 && cp < 0x800) ||
        (n == 3 && cp < 0x10000) || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
        return {0xfffd, start, start + 1};
    return {cp, start, pos};
}
inline bool wide(uint32_t cp) {
    return cp >= 0x1100 && (cp <= 0x115f || cp == 0x2329 || cp == 0x232a ||
        (cp >= 0x2e80 && cp <= 0xa4cf) || (cp >= 0xac00 && cp <= 0xd7a3) ||
        (cp >= 0xf900 && cp <= 0xfaff) || (cp >= 0xfe10 && cp <= 0xfe6f) ||
        (cp >= 0xff01 && cp <= 0xff60) || (cp >= 0xffe0 && cp <= 0xffe6) || cp >= 0x1f300);
}
inline int columns(uint32_t cp) {
    if (cp == '\n' || cp == '\r' || (cp >= 0x300 && cp <= 0x36f)) return 0;
    return wide(cp) ? 2 : 1;
}
inline size_t width(const std::string& s) {
    size_t w = 0;
    for (size_t p = 0; p < s.size();) { auto g = next(s, p); w += columns(g.cp); p = g.end; }
    return w;
}
inline size_t visible_width(const std::string& s) {
    size_t w = 0;
    for (size_t p = 0; p < s.size();) {
        if (s[p] == '{') { auto end = s.find('}', p); if (end != std::string::npos) {
            if (s.substr(p, end-p+1) == "{_}") ++w;
            p = end + 1; continue;
        }}
        auto g = next(s, p); w += columns(g.cp); p = g.end;
    }
    return w;
}
inline bool has_unicode(const std::string& s) {
    for (unsigned char c : s) if (c >= 0x80) return true;
    return false;
}
inline void pop_back(std::string& s) {
    if (s.empty()) return;
    size_t p = s.size()-1;
    while (p > 0 && (static_cast<unsigned char>(s[p]) & 0xc0) == 0x80) --p;
    s.resize(p);
}
}
#endif
