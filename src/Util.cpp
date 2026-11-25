// Util.cpp
#include "Common.h"
#include "Util.h"

// ----------------------------------------------------------------------------
//  문자열 유틸
// ----------------------------------------------------------------------------
std::wstring U8ToW(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}
std::string WToU8(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}
std::wstring Trim(const std::wstring& s) {
    size_t a = 0, b = s.size();
    while (a < b && iswspace(s[a])) a++;
    while (b > a && iswspace(s[b - 1])) b--;
    return s.substr(a, b - a);
}
std::wstring GetText(HWND h) {
    int n = GetWindowTextLengthW(h);
    std::wstring s(n + 1, 0);
    GetWindowTextW(h, &s[0], n + 1);
    s.resize(n);
    return s;
}
std::wstring ToLower(std::wstring s) {
    for (auto& c : s) c = (wchar_t)towlower(c);
    return s;
}


// ----------------------------------------------------------------------------
//  시작프로그램 등록 / 명령 실행
// ----------------------------------------------------------------------------
void SetStartup(bool on) {
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &k) != ERROR_SUCCESS) return;
    if (on) {
        wchar_t p[MAX_PATH];
        GetModuleFileNameW(nullptr, p, MAX_PATH);
        std::wstring v = L"\"" + std::wstring(p) + L"\"";
        RegSetValueExW(k, L"mkRun", 0, REG_SZ, (const BYTE*)v.c_str(), (DWORD)((v.size() + 1) * sizeof(wchar_t)));
    } else RegDeleteValueW(k, L"mkRun");
    RegCloseKey(k);
}

bool ParseCommandTarget(const std::wstring& cmdIn, std::wstring& file, std::wstring& args) {
    file.clear();
    args.clear();
    std::wstring cmd = Trim(cmdIn);
    if (cmd.empty()) return false;
    wchar_t buf[4096];
    DWORD n = ExpandEnvironmentStringsW(cmd.c_str(), buf, 4096);
    if (n > 0 && n <= 4096) cmd = buf;

    if (cmd[0] == L'"') {
        size_t q = cmd.find(L'"', 1);
        if (q == std::wstring::npos) file = cmd.substr(1);
        else { file = cmd.substr(1, q - 1); args = Trim(cmd.substr(q + 1)); }
    } else if (cmd.find(L"://") != std::wstring::npos || GetFileAttributesW(cmd.c_str()) != INVALID_FILE_ATTRIBUTES) {
        file = cmd;
    } else {
        size_t sp = cmd.find(L' ');
        if (sp == std::wstring::npos) file = cmd;
        else { file = cmd.substr(0, sp); args = Trim(cmd.substr(sp + 1)); }
    }
    return !file.empty();
}

void RunCommand(const std::wstring& cmdIn) {
    std::wstring file, args;
    if (!ParseCommandTarget(cmdIn, file, args)) return;
    DWORD a = GetFileAttributesW(file.c_str());
    if (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY)) {
        while (file.size() > 3 && (file.back() == L'\\' || file.back() == L'/')) file.pop_back();
        std::wstring param = L"\"" + file + L"\"";
        HINSTANCE r = ShellExecuteW(nullptr, L"open", L"explorer.exe", param.c_str(), nullptr, SW_SHOWNORMAL);
        if ((INT_PTR)r <= 32)
            MessageBoxW(nullptr, (L"실행할 수 없습니다:\n" + Trim(cmdIn)).c_str(), L"mkRun", MB_OK | MB_ICONWARNING | MB_TOPMOST);
        return;
    }
    std::wstring dir;
    if (a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY)) {
        size_t sl = file.find_last_of(L"\\/");
        if (sl != std::wstring::npos) dir = file.substr(0, sl);
    }
    HINSTANCE r = ShellExecuteW(nullptr, L"open", file.c_str(), args.empty() ? nullptr : args.c_str(),
                                dir.empty() ? nullptr : dir.c_str(), SW_SHOWNORMAL);
    if ((INT_PTR)r <= 32)
        MessageBoxW(nullptr, (L"실행할 수 없습니다:\n" + Trim(cmdIn)).c_str(), L"mkRun", MB_OK | MB_ICONWARNING | MB_TOPMOST);
}

HICON GetCommandIcon(const std::wstring& cmdIn, bool isFolder) {
    SHFILEINFOW sfi = {};
    if (isFolder) {
        if (SHGetFileInfoW(L"folder", FILE_ATTRIBUTE_DIRECTORY, &sfi, sizeof(sfi),
                           SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES))
            return sfi.hIcon;
        return nullptr;
    }
    std::wstring file, args;
    if (ParseCommandTarget(cmdIn, file, args)) {
        if (file.find(L"://") != std::wstring::npos) {
            if (SHGetFileInfoW(L".html", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
                               SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES))
                return sfi.hIcon;
        } else {
            if (GetFileAttributesW(file.c_str()) == INVALID_FILE_ATTRIBUTES) {
                wchar_t resolved[MAX_PATH];
                if (SearchPathW(nullptr, file.c_str(), L".exe", MAX_PATH, resolved, nullptr))
                    file = resolved;
            }
            if (SHGetFileInfoW(file.c_str(), 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_SMALLICON))
                return sfi.hIcon;
        }
    }
    if (SHGetFileInfoW(L".exe", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
                       SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES))
        return sfi.hIcon;
    return nullptr;
}

// ----------------------------------------------------------------------------
//  아이콘 (코드로 생성)
// ----------------------------------------------------------------------------
HICON MakeIcon(int sz) {
    HDC sdc = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(sdc);
    HBITMAP color = CreateCompatibleBitmap(sdc, sz, sz);
    HBITMAP mask = CreateBitmap(sz, sz, 1, 1, nullptr);
    HGDIOBJ oldBmp = SelectObject(dc, color);
    RECT r = {0, 0, sz, sz};
    HBRUSH bk = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(dc, &r, bk);
    DeleteObject(bk);
    HBRUSH wb = CreateSolidBrush(RGB(255, 255, 255));
    FrameRect(dc, &r, wb);
    DeleteObject(wb);
    HFONT f = CreateFontW(-sz * 5 / 8, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Segoe UI");
    HGDIOBJ oldFont = SelectObject(dc, f);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    DrawTextW(dc, L"mk", -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, oldFont);
    DeleteObject(f);
    SelectObject(dc, mask);
    PatBlt(dc, 0, 0, sz, sz, BLACKNESS);
    SelectObject(dc, oldBmp);
    ICONINFO ii = {TRUE, 0, 0, mask, color};
    HICON ic = CreateIconIndirect(&ii);
    DeleteObject(color);
    DeleteObject(mask);
    DeleteDC(dc);
    ReleaseDC(nullptr, sdc);
    return ic;
}

