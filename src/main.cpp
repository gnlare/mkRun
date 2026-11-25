// main.cpp - 진입점, 트레이 아이콘, 전역 핫키 수신 창, 우클릭 메뉴
#include "Common.h"
#include "Util.h"
#include "Hotkey.h"
#include "Config.h"
#include "Launcher.h"
#include "Settings.h"

#ifdef _MSC_VER
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

HINSTANCE g_hInst;
int g_dpi = 96;
HWND g_hMsg;
HICON g_icoBig, g_icoSmall;
HFONT g_fontEdit, g_fontList, g_fontUI;

static NOTIFYICONDATAW g_nid;
static UINT g_msgTaskbarCreated;

// ----------------------------------------------------------------------------
//  트레이 / 컨텍스트 메뉴 / 메시지 창
// ----------------------------------------------------------------------------
static void AddTray() {
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_hMsg;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY;
    g_nid.hIcon = g_icoSmall;
    lstrcpynW(g_nid.szTip, L"mkRun", ARRAYSIZE(g_nid.szTip));
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

void ShowCtxMenu(HWND owner) {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, IDM_HOTKEYS, Tr(L"메뉴 핫키 지정", L"Menu Hotkeys"));
    AppendMenuW(menu, MF_STRING, IDM_ADDKW, Tr(L"키워드 추가", L"Add Keyword"));
    AppendMenuW(menu, MF_STRING, IDM_SETTINGS, Tr(L"설정", L"Settings"));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_EXIT, Tr(L"종료", L"Exit"));
    POINT pt; GetCursorPos(&pt);
    g_menuOpen = true;
    SetForegroundWindow(owner);
    int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, owner, nullptr);
    g_menuOpen = false;
    PostMessageW(owner, WM_NULL, 0, 0);
    DestroyMenu(menu);
    switch (cmd) {
    case IDM_ADDKW: OpenSettings(1); break;     // 키워드 탭
    case IDM_HOTKEYS: OpenSettings(0); break;   // 숫자 핫키 탭
    case IDM_SETTINGS: OpenSettings(2); break;  // 프로그램 탭
    case IDM_EXIT: DestroyWindow(g_hMsg); break;
    }
}

static LRESULT CALLBACK MsgProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == g_msgTaskbarCreated) { AddTray(); return 0; }
    switch (m) {
    case WM_HOTKEY:
        if (w == ID_HOTKEY) ShowLauncher();
        return 0;
    case WM_APP_HOTKEY:
        ShowLauncher();
        return 0;
    case WM_TRAY:
        if (LOWORD(l) == WM_RBUTTONUP) ShowCtxMenu(h);
        else if (LOWORD(l) == WM_LBUTTONUP) ShowLauncher();
        return 0;
    case WM_DESTROY:
        UnbindGlobalHotkey();
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}


int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    g_hInst = hInst;
    SetProcessDPIAware();
    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"mkRun_SingleInstance_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;

    HDC sdc = GetDC(nullptr);
    g_dpi = GetDeviceCaps(sdc, LOGPIXELSX);
    ReleaseDC(nullptr, sdc);

    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES | ICC_WIN95_CLASSES};
    InitCommonControlsEx(&icc);

    LoadConfig();
    if (GetFileAttributesW(CfgPath(false).c_str()) == INVALID_FILE_ATTRIBUTES) SaveConfig();

    g_icoBig = MakeIcon(32);
    g_icoSmall = MakeIcon(16);
    g_fontEdit = CreateFontW(-S(11), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Malgun Gothic");
    g_fontList = CreateFontW(-S(11), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Malgun Gothic");
    g_fontUI = CreateFontW(-S(11), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Malgun Gothic");

    // 핫키/트레이 메시지 수신용 숨김 창
    WNDCLASSEXW wc = {sizeof(wc)};
    wc.hInstance = hInst;
    wc.lpfnWndProc = MsgProc;
    wc.lpszClassName = L"mkRunMsg";
    wc.hIcon = g_icoBig;
    wc.hIconSm = g_icoSmall;
    RegisterClassExW(&wc);
    g_msgTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    g_hMsg = CreateWindowExW(0, L"mkRunMsg", L"mkRun", WS_OVERLAPPED, 0, 0, 0, 0, nullptr, nullptr, hInst, nullptr);

    if (!Launcher_Init(hInst) || !Settings_Init(hInst)) return 1;

    AddTray();

    if (!BindGlobalHotkey(g_cfg.globalHotkey)) {
        MessageBoxW(nullptr, Tr(L"전역 단축키를 등록할 수 없습니다.\n설정의 '프로그램' 탭에서 다른 단축키를 지정해 주세요.",
                                L"Failed to register global hotkey.\nPlease assign another hotkey in Settings -> Program."),
                    L"mkRun", MB_OK | MB_ICONWARNING | MB_TOPMOST);
        OpenSettings(2);
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    CloseHandle(mutex);
    return (int)msg.wParam;
}
