// Json.h - 최소 JSON 파서/작성기 (헤더 온리)
#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// ----------------------------------------------------------------------------
//  최소 JSON 파서 / 작성기
// ----------------------------------------------------------------------------
struct JV {
    enum T { Null, Bool, Num, Str, Arr, Obj } t = Null;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<std::string> keys;  // Obj
    std::vector<JV> vals;           // Arr / Obj
    const JV* get(const char* k) const {
        if (t != Obj) return nullptr;
        for (size_t i = 0; i < keys.size(); i++)
            if (keys[i] == k) return &vals[i];
        return nullptr;
    }
};

struct JParser {
    const std::string& s;
    size_t p = 0;
    explicit JParser(const std::string& x) : s(x) {}
    void ws() { while (p < s.size() && (s[p] == ' ' || s[p] == '\t' || s[p] == '\r' || s[p] == '\n')) p++; }
    bool lit(const char* w) {
        size_t n = strlen(w);
        if (s.compare(p, n, w) == 0) { p += n; return true; }
        return false;
    }
    static void putUtf8(std::string& o, unsigned c) {
        if (c < 0x80) o += (char)c;
        else if (c < 0x800) { o += (char)(0xC0 | (c >> 6)); o += (char)(0x80 | (c & 0x3F)); }
        else if (c < 0x10000) { o += (char)(0xE0 | (c >> 12)); o += (char)(0x80 | ((c >> 6) & 0x3F)); o += (char)(0x80 | (c & 0x3F)); }
        else { o += (char)(0xF0 | (c >> 18)); o += (char)(0x80 | ((c >> 12) & 0x3F)); o += (char)(0x80 | ((c >> 6) & 0x3F)); o += (char)(0x80 | (c & 0x3F)); }
    }
    bool hex4(unsigned& v) {
        if (p + 4 > s.size()) return false;
        v = 0;
        for (int i = 0; i < 4; i++) {
            char c = s[p++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= c - '0';
            else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
            else return false;
        }
        return true;
    }
    bool str(std::string& out) {
        if (p >= s.size() || s[p] != '"') return false;
        p++;
        while (p < s.size()) {
            char c = s[p++];
            if (c == '"') return true;
            if (c != '\\') { out += c; continue; }
            if (p >= s.size()) return false;
            char e = s[p++];
            switch (e) {
            case 'n': out += '\n'; break;
            case 't': out += '\t'; break;
            case 'r': out += '\r'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case '/': case '\\': case '"': out += e; break;
            case 'u': {
                unsigned cp;
                if (!hex4(cp)) return false;
                if (cp >= 0xD800 && cp < 0xDC00 && p + 1 < s.size() && s[p] == '\\' && s[p + 1] == 'u') {
                    size_t sv = p;
                    p += 2;
                    unsigned lo;
                    if (hex4(lo) && lo >= 0xDC00 && lo < 0xE000) cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    else p = sv;
                }
                putUtf8(out, cp);
                break;
            }
            default: return false;
            }
        }
        return false;
    }
    bool val(JV& v, int depth = 0) {
        ws();
        if (p >= s.size() || depth > 64) return false;
        char c = s[p];
        if (c == '{') {
            v.t = JV::Obj; p++; ws();
            if (p < s.size() && s[p] == '}') { p++; return true; }
            for (;;) {
                ws();
                std::string k;
                if (!str(k)) return false;
                ws();
                if (p >= s.size() || s[p] != ':') return false;
                p++;
                JV x;
                if (!val(x, depth + 1)) return false;
                v.keys.push_back(k);
                v.vals.push_back(std::move(x));
                ws();
                if (p < s.size() && s[p] == ',') { p++; continue; }
                if (p < s.size() && s[p] == '}') { p++; return true; }
                return false;
            }
        }
        if (c == '[') {
            v.t = JV::Arr; p++; ws();
            if (p < s.size() && s[p] == ']') { p++; return true; }
            for (;;) {
                JV x;
                if (!val(x, depth + 1)) return false;
                v.vals.push_back(std::move(x));
                ws();
                if (p < s.size() && s[p] == ',') { p++; continue; }
                if (p < s.size() && s[p] == ']') { p++; return true; }
                return false;
            }
        }
        if (c == '"') { v.t = JV::Str; return str(v.s); }
        if (lit("true")) { v.t = JV::Bool; v.b = true; return true; }
        if (lit("false")) { v.t = JV::Bool; v.b = false; return true; }
        if (lit("null")) { v.t = JV::Null; return true; }
        char* e;
        double d = strtod(s.c_str() + p, &e);
        if (e == s.c_str() + p) return false;
        v.t = JV::Num; v.n = d; p = (size_t)(e - s.c_str());
        return true;
    }
};

inline void JEsc(std::string& o, const std::string& s) {
    o += '"';
    for (unsigned char c : s) {
        switch (c) {
        case '"': o += "\\\""; break;
        case '\\': o += "\\\\"; break;
        case '\n': o += "\\n"; break;
        case '\r': o += "\\r"; break;
        case '\t': o += "\\t"; break;
        default:
            if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
            else o += (char)c;
        }
    }
    o += '"';
}

