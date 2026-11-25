// Common.h - 공통 헤더: 윈도우 헤더, 컨트롤 ID, 전역 핸들/DPI 헬퍼
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwctype>
#include <string>
#include <utility>
#include <vector>

enum {
    ID_HOTKEY = 1,
    WM_TRAY = WM_APP + 1,
    WM_APP_HOTKEY = WM_APP + 2,
    IDM_ADDKW = 1001, IDM_HOTKEYS, IDM_SETTINGS, IDM_EXIT,
    // 설정 창 컨트롤
    IDC_TAB = 2001, IDC_SAVE, IDC_CLOSE,
    IDC_LVHOT = 2010, IDC_EDHNAME, IDC_EDHKEY, IDC_EDHCMD,
    IDC_ADDFOLDER, IDC_ADDITEM, IDC_APPLYH, IDC_DELH,
    IDC_LVKW = 2030, IDC_EDKWK, IDC_EDKWC, IDC_KWADD, IDC_KWMOD, IDC_KWDEL,
    IDC_LBCAT = 2050, IDC_CHKSTART, IDC_EDHOTKEY, IDC_EDMENUHOTKEY, IDC_EXPORT, IDC_IMPORT,
    IDC_BTNBORDERCLR, IDC_BTNBGCLR, IDC_LANGKO, IDC_LANGEN, IDC_EDHELP
};

extern HINSTANCE g_hInst;
extern int g_dpi;
inline int S(int v) { return MulDiv(v, g_dpi, 96); }  // 96 DPI 기준 px -> 현재 DPI px

extern HWND g_hMsg;                       // 트레이/핫키 수신용 숨김 창
extern HICON g_icoBig, g_icoSmall;
extern HFONT g_fontEdit, g_fontList, g_fontUI;
