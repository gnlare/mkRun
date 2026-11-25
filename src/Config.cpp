// Config.cpp
#include "Common.h"
#include "Util.h"
#include "Json.h"
#include "Config.h"

// ----------------------------------------------------------------------------
//  설정 데이터
// ----------------------------------------------------------------------------
Config g_cfg;   // 실제 적용 중인 설정
Config g_work;  // 설정 창 작업 사본
static int g_idCounter = 0;
std::wstring NewId() { return std::to_wstring(++g_idCounter); }

void AssignIds(Node& n) {
    for (auto& c : n.children) { c.id = NewId(); AssignIds(c); }
}

static void LoadNode(const JV& j, Node& n) {
    auto gs = [&](const char* k) -> std::wstring {
        const JV* v = j.get(k);
        if (!v) return L"";
        if (v->t == JV::Str) return U8ToW(v->s);
        if (v->t == JV::Num) return std::to_wstring((long long)v->n);
        return L"";
    };
    n.folder = (gs("type") == L"folder");
    n.hotkey = gs("hotkey");
    n.name = gs("name");
    n.command = n.folder ? L"" : gs("command");
    const JV* ch = j.get("children");
    if (ch && ch->t == JV::Arr)
        for (auto& c : ch->vals) {
            if (c.t != JV::Obj) continue;
            Node k;
            LoadNode(c, k);
            n.children.push_back(std::move(k));
        }
}

static void WriteNode(std::string& o, const Node& n, int ind) {
    std::string pad(ind, ' '), p2(ind + 2, ' ');
    o += pad + "{\n";
    o += p2 + "\"id\": ";       JEsc(o, WToU8(n.id));      o += ",\n";
    o += p2 + "\"type\": ";     JEsc(o, n.folder ? "folder" : "item"); o += ",\n";
    o += p2 + "\"hotkey\": ";   JEsc(o, WToU8(n.hotkey));  o += ",\n";
    o += p2 + "\"name\": ";     JEsc(o, WToU8(n.name));    o += ",\n";
    o += p2 + "\"command\": ";
    if (n.folder) o += "null"; else JEsc(o, WToU8(n.command));
    o += ",\n";
    o += p2 + "\"children\": ";
    if (n.children.empty()) o += "[]";
    else {
        o += "[\n";
        for (size_t i = 0; i < n.children.size(); i++) {
            WriteNode(o, n.children[i], ind + 4);
            o += (i + 1 < n.children.size()) ? ",\n" : "\n";
        }
        o += p2 + "]";
    }
    o += "\n" + pad + "}";
}

std::wstring CfgPath(bool mk) {
    wchar_t p[MAX_PATH] = {};
    SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, p);
    std::wstring d = std::wstring(p) + L"\\mkRun";
    if (mk) CreateDirectoryW(d.c_str(), nullptr);
    return d + L"\\settings.json";
}

static bool ReadAll(const std::wstring& path, std::string& out) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD sz = GetFileSize(f, nullptr), rd = 0;
    out.assign(sz, 0);
    BOOL ok = sz ? ReadFile(f, &out[0], sz, &rd, nullptr) : TRUE;
    CloseHandle(f);
    if (!ok) return false;
    if (out.size() >= 3 && (unsigned char)out[0] == 0xEF && (unsigned char)out[1] == 0xBB && (unsigned char)out[2] == 0xBF)
        out.erase(0, 3);
    return true;
}
static bool WriteAll(const std::wstring& path, const std::string& data) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD wr = 0;
    BOOL ok = WriteFile(f, data.data(), (DWORD)data.size(), &wr, nullptr);
    CloseHandle(f);
    return ok != FALSE;
}

static Config DefaultConfig() {
    Config c;
    c.keywords.push_back({L"메모장", L"notepad.exe"});
    c.keywords.push_back({L"calc", L"calc.exe"});
    Node a; a.folder = false; a.hotkey = L"1"; a.name = L"메모장"; a.command = L"notepad.exe";
    Node f; f.folder = true; f.hotkey = L"2"; f.name = L"도구";
    Node b; b.hotkey = L"1"; b.name = L"계산기"; b.command = L"calc.exe";
    f.children.push_back(b);
    c.root.folder = true;
    c.root.children.push_back(a);
    c.root.children.push_back(f);
    AssignIds(c.root);
    return c;
}

static std::string ColorToHex(COLORREF c) {
    char buf[16];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X", GetRValue(c), GetGValue(c), GetBValue(c));
    return buf;
}
static bool HexToColor(const std::string& s, COLORREF& out) {
    if (s.size() != 7 || s[0] != '#') return false;
    unsigned int r = 0, g = 0, b = 0;
    if (sscanf_s(s.c_str() + 1, "%02x%02x%02x", &r, &g, &b) != 3) return false;
    out = RGB(r, g, b);
    return true;
}

bool LoadConfigFrom(const std::wstring& path, Config& out) {
    std::string txt;
    JV root;
    bool ok = ReadAll(path, txt);
    if (ok) { JParser ps(txt); ok = ps.val(root) && root.t == JV::Obj; }
    if (!ok) return false;
    Config c;
    c.root.folder = true;
    if (const JV* g = root.get("general")) {
        if (const JV* v = g->get("run_on_startup")) c.runOnStartup = (v->t == JV::Bool && v->b);
        if (const JV* v = g->get("global_hotkey")) if (v->t == JV::Str && !v->s.empty()) c.globalHotkey = U8ToW(v->s);
        if (const JV* v = g->get("menu_hotkey")) if (v->t == JV::Str && !v->s.empty()) c.menuHotkey = U8ToW(v->s);
        if (const JV* v = g->get("lang")) if (v->t == JV::Str && (v->s == "ko" || v->s == "en")) c.lang = U8ToW(v->s);
    }
    if (const JV* a = root.get("appearance")) {
        if (const JV* v = a->get("border_color")) if (v->t == JV::Str) HexToColor(v->s, c.borderColor);
        if (const JV* v = a->get("bg_color")) if (v->t == JV::Str) HexToColor(v->s, c.bgColor);
    }
    if (const JV* k = root.get("keywords"); k && k->t == JV::Arr)
        for (auto& e : k->vals) {
            if (e.t != JV::Obj) continue;
            const JV* a = e.get("keyword"); const JV* b = e.get("command");
            if (a && b && a->t == JV::Str && b->t == JV::Str) c.keywords.push_back({U8ToW(a->s), U8ToW(b->s)});
        }
    if (const JV* h = root.get("hotkeys"); h && h->t == JV::Arr)
        for (auto& e : h->vals) {
            if (e.t != JV::Obj) continue;
            Node n;
            LoadNode(e, n);
            c.root.children.push_back(std::move(n));
        }
    AssignIds(c.root);
    out = std::move(c);
    return true;
}

void LoadConfig() {
    if (!LoadConfigFrom(CfgPath(false), g_cfg))
        g_cfg = DefaultConfig();
}

bool SaveConfigTo(const Config& cfg, const std::wstring& path) {
    std::string o = "{\n  \"general\": {\n    \"run_on_startup\": ";
    o += cfg.runOnStartup ? "true" : "false";
    o += ",\n    \"global_hotkey\": ";
    JEsc(o, WToU8(cfg.globalHotkey));
    o += ",\n    \"menu_hotkey\": ";
    JEsc(o, WToU8(cfg.menuHotkey));
    o += ",\n    \"lang\": ";
    JEsc(o, WToU8(cfg.lang));
    o += "\n  },\n  \"appearance\": {\n    \"border_color\": ";
    JEsc(o, ColorToHex(cfg.borderColor));
    o += ",\n    \"bg_color\": ";
    JEsc(o, ColorToHex(cfg.bgColor));
    o += "\n  },\n  \"keywords\": ";
    if (cfg.keywords.empty()) o += "[]";
    else {
        o += "[\n";
        for (size_t i = 0; i < cfg.keywords.size(); i++) {
            o += "    { \"keyword\": "; JEsc(o, WToU8(cfg.keywords[i].keyword));
            o += ", \"command\": ";     JEsc(o, WToU8(cfg.keywords[i].command));
            o += (i + 1 < cfg.keywords.size()) ? " },\n" : " }\n";
        }
        o += "  ]";
    }
    o += ",\n  \"hotkeys\": ";
    if (cfg.root.children.empty()) o += "[]";
    else {
        o += "[\n";
        for (size_t i = 0; i < cfg.root.children.size(); i++) {
            WriteNode(o, cfg.root.children[i], 4);
            o += (i + 1 < cfg.root.children.size()) ? ",\n" : "\n";
        }
        o += "  ]";
    }
    o += "\n}\n";
    return WriteAll(path, o);
}

bool SaveConfig() {
    return SaveConfigTo(g_cfg, CfgPath(true));
}

