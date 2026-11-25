// Hotkey.cpp
#include "Common.h"
#include "Util.h"
#include "Hotkey.h"

// ----------------------------------------------------------------------------
//  전역 단축키 문자열 <-> (modifiers, vk)
// ----------------------------------------------------------------------------
struct KeyName { const wchar_t* n; UINT vk; };
static const KeyName kNames[] = {
    {L"Space", VK_SPACE}, {L"Enter", VK_RETURN}, {L"Tab", VK_TAB}, {L"Esc", VK_ESCAPE},
    {L"Backspace", VK_BACK}, {L"Insert", VK_INSERT}, {L"Delete", VK_DELETE},
    {L"Home", VK_HOME}, {L"End", VK_END}, {L"PageUp", VK_PRIOR}, {L"PageDown", VK_NEXT},
    {L"Up", VK_UP}, {L"Down", VK_DOWN}, {L"Left", VK_LEFT}, {L"Right", VK_RIGHT}};

std::wstring VkName(UINT vk) {
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) return std::wstring(1, (wchar_t)vk);
    if (vk >= VK_F1 && vk <= VK_F24) return L"F" + std::to_wstring(vk - VK_F1 + 1);
    for (auto& k : kNames) if (k.vk == vk) return k.n;
    return L"";
}
UINT NameVk(const std::wstring& t) {
    if (t.size() == 1) {
        wchar_t c = (wchar_t)towupper(t[0]);
        if ((c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9')) return (UINT)c;
    }
    if ((t[0] == L'F' || t[0] == L'f') && t.size() >= 2 && t.size() <= 3) {
        int n = _wtoi(t.c_str() + 1);
        if (n >= 1 && n <= 24) return VK_F1 + n - 1;
    }
    for (auto& k : kNames) if (_wcsicmp(t.c_str(), k.n) == 0) return k.vk;
    return 0;
}
bool ParseHotkey(const std::wstring& s, UINT& mods, UINT& vk) {
    mods = 0; vk = 0;
    size_t st = 0;
    for (;;) {
        size_t p = s.find(L'+', st);
        std::wstring t = Trim(s.substr(st, p == std::wstring::npos ? std::wstring::npos : p - st));
        if (t.empty()) return false;
        if (!_wcsicmp(t.c_str(), L"ctrl") || !_wcsicmp(t.c_str(), L"control")) mods |= MOD_CONTROL;
        else if (!_wcsicmp(t.c_str(), L"alt")) mods |= MOD_ALT;
        else if (!_wcsicmp(t.c_str(), L"shift")) mods |= MOD_SHIFT;
        else if (!_wcsicmp(t.c_str(), L"win")) mods |= MOD_WIN;
        else { if (vk) return false; vk = NameVk(t); if (!vk) return false; }
        if (p == std::wstring::npos) break;
        st = p + 1;
    }
    return vk != 0 && mods != 0;
}
std::wstring FormatHotkey(UINT mods, UINT vk) {
    std::wstring r;
    if (mods & MOD_CONTROL) r += L"Ctrl+";
    if (mods & MOD_ALT) r += L"Alt+";
    if (mods & MOD_SHIFT) r += L"Shift+";
    if (mods & MOD_WIN) r += L"Win+";
    return r + VkName(vk);
}

bool IsWindowsDefaultHotkey(UINT mods, UINT vk) {
    if (mods & MOD_WIN) return true;  // Win 조합키는 대부분 윈도우 기본 단축키
    if (mods == MOD_ALT && (vk == VK_TAB || vk == VK_ESCAPE || vk == VK_F4 || vk == VK_SPACE)) return true;
    if (mods == (MOD_ALT | MOD_SHIFT) && vk == VK_TAB) return true;
    if (mods == MOD_CONTROL && vk == VK_ESCAPE) return true;
    if (mods == (MOD_CONTROL | MOD_SHIFT) && vk == VK_ESCAPE) return true;
    if (mods == (MOD_CONTROL | MOD_ALT) && vk == VK_DELETE) return true;
    return false;
}
static HHOOK g_kbHook = nullptr;
static UINT g_hkMods = 0, g_hkVk = 0;
static bool g_hkHeld = false;
static bool g_capturing = false;

static bool ModifiersMatch(UINT mods) {
    bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    bool win = ((GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000) != 0;
    return ctrl == ((mods & MOD_CONTROL) != 0) &&
           alt == ((mods & MOD_ALT) != 0) &&
           shift == ((mods & MOD_SHIFT) != 0) &&
           win == ((mods & MOD_WIN) != 0);
}

static LRESULT CALLBACK LowLevelKeyProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && !g_capturing && g_hkVk != 0) {
        const KBDLLHOOKSTRUCT* kb = (const KBDLLHOOKSTRUCT*)lParam;
        if (!(kb->flags & LLKHF_INJECTED) && kb->vkCode == g_hkVk) {
            if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
                if (ModifiersMatch(g_hkMods)) {
                    if (!g_hkHeld) {
                        g_hkHeld = true;
                        PostMessageW(g_hMsg, WM_APP_HOTKEY, 0, 0);
                    }
                    return 1;
                }
            } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
                g_hkHeld = false;
            }
        }
    }
    return CallNextHookEx(g_kbHook, code, wParam, lParam);
}

void SetHotkeyCaptureMode(bool on) { g_capturing = on; }

void UnbindGlobalHotkey() {
    UnregisterHotKey(g_hMsg, ID_HOTKEY);
    if (g_kbHook) {
        UnhookWindowsHookEx(g_kbHook);
        g_kbHook = nullptr;
    }
    g_hkMods = 0;
    g_hkVk = 0;
    g_hkHeld = false;
}

bool BindGlobalHotkey(const std::wstring& hk) {
    UINT m, v;
    if (!ParseHotkey(hk, m, v)) return false;
    if (!RegisterHotKey(g_hMsg, ID_HOTKEY, m | MOD_NOREPEAT, v)) return false;
    g_hkMods = m;
    g_hkVk = v;
    g_hkHeld = false;
    if (!g_kbHook)
        g_kbHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyProc, g_hInst, 0);
    return true;
}

