// Launcher.cpp
#include "Common.h"
#include "Util.h"
#include "Config.h"
#include "Launcher.h"

HWND g_hMain, g_hEdit, g_hList = nullptr;
bool g_menuOpen = false;

// ----------------------------------------------------------------------------
//  런처 (메인 창 + 캐스케이딩 드롭다운)
// ----------------------------------------------------------------------------
struct Row {
    std::wstring hot, name, command;
    bool folder = false, empty = false;
    const Node* node = nullptr;
};
struct MenuLevel {
    HWND hwnd = nullptr;
    const Node* parent = nullptr;
    std::vector<Row> rows;
    int sel = 0, top = 0;
    bool isHot = false;
};
static std::vector<MenuLevel> g_menus;
static bool g_ignoreChange = false;
static ULONGLONG g_lastShowTick = 0;
static const int ROW_H = 24, MAX_VIS = 10;


static const Node* CurNode() {
    if (g_menus.empty() || !g_menus.back().parent) return &g_cfg.root;
    return g_menus.back().parent;
}

static std::wstring GetEditText() { return GetText(g_hEdit); }
static void SetEditText(const std::wstring& t) {
    g_ignoreChange = true;
    SetWindowTextW(g_hEdit, t.c_str());
    SendMessageW(g_hEdit, EM_SETSEL, (WPARAM)t.size(), (LPARAM)t.size());
    g_ignoreChange = false;
}
static wchar_t MenuTriggerKey() {
    return g_cfg.menuHotkey.empty() ? L'0' : g_cfg.menuHotkey[0];
}
static bool IsHotMode() {
    std::wstring t = GetEditText();
    return !t.empty() && t[0] == MenuTriggerKey();
}
static bool ListVisible() { return !g_menus.empty() && g_menus[0].hwnd && IsWindowVisible(g_menus[0].hwnd); }

static void PopMenusTo(size_t count) {
    while (g_menus.size() > count) {
        if (g_menus.back().hwnd) DestroyWindow(g_menus.back().hwnd);
        g_menus.pop_back();
    }
    g_hList = g_menus.empty() ? nullptr : g_menus[0].hwnd;
}

static int FindMenu(HWND h) {
    for (int i = 0; i < (int)g_menus.size(); i++)
        if (g_menus[i].hwnd == h) return i;
    return -1;
}

static HWND CreateMenuWnd() {
    return CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST, L"mkRunList", L"", WS_POPUP,
                           0, 0, S(200), S(100), g_hMain, nullptr, g_hInst, nullptr);
}

static void BuildHotRowsFor(const Node* parent, std::vector<Row>& out) {
    out.clear();
    for (auto& ch : parent->children) {
        Row r;
        r.hot = ch.hotkey; r.name = ch.name; r.command = ch.command; r.folder = ch.folder; r.node = &ch;
        out.push_back(r);
    }
    if (out.empty()) { Row r; r.empty = true; r.name = Tr(L"(비어 있음)", L"(Empty)"); out.push_back(r); }
}

static void ShowRootHotMenu() {
    PopMenusTo(0);
    MenuLevel ml;
    ml.parent = &g_cfg.root;
    ml.isHot = true;
    BuildHotRowsFor(&g_cfg.root, ml.rows);
    ml.hwnd = CreateMenuWnd();
    g_hList = ml.hwnd;
    RECT rc;
    GetWindowRect(g_hMain, &rc);
    int n = std::min((int)ml.rows.size(), MAX_VIS);
    int h = n * S(ROW_H) + 2;
    SetWindowPos(ml.hwnd, HWND_TOPMOST, rc.left, rc.bottom, rc.right - rc.left, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    g_menus.push_back(std::move(ml));
}

static void BuildKeywordRows(const std::wstring& text) {
    PopMenusTo(0);
    MenuLevel ml;
    ml.parent = nullptr;
    ml.isHot = false;
    std::wstring q = ToLower(text);
    for (auto& k : g_cfg.keywords)
        if (ToLower(k.keyword).find(q) != std::wstring::npos) {
            Row r; r.name = k.keyword; r.command = k.command;
            ml.rows.push_back(r);
        }
    if (ml.rows.empty()) return;
    ml.hwnd = CreateMenuWnd();
    g_hList = ml.hwnd;
    RECT rc;
    GetWindowRect(g_hMain, &rc);
    int n = std::min((int)ml.rows.size(), MAX_VIS);
    int h = n * S(ROW_H) + 2;
    SetWindowPos(ml.hwnd, HWND_TOPMOST, rc.left, rc.bottom, rc.right - rc.left, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    g_menus.push_back(std::move(ml));
}

void HideLauncher() {
    PopMenusTo(0);
    if (g_hMain) ShowWindow(g_hMain, SW_HIDE);
    if (g_hEdit) SetEditText(L"");
}

void ShowLauncher() {
    ULONGLONG now = GetTickCount64();
    if (IsWindowVisible(g_hMain) && now - g_lastShowTick < 150) return;
    g_lastShowTick = now;
    PopMenusTo(0);
    SetEditText(L"");
    POINT pt; GetCursorPos(&pt);
    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfoW(mon, &mi);
    int w = S(200), h = S(30);
    int x = (mi.rcMonitor.left + mi.rcMonitor.right - w) / 2;
    int y = (mi.rcMonitor.top + mi.rcMonitor.bottom - h) / 2;
    SetWindowPos(g_hMain, HWND_TOPMOST, x, y, w, h, SWP_SHOWWINDOW);
    HWND fg = GetForegroundWindow();
    DWORD ft = fg ? GetWindowThreadProcessId(fg, nullptr) : 0, ct = GetCurrentThreadId();
    if (ft && ft != ct) AttachThreadInput(ft, ct, TRUE);
    BringWindowToTop(g_hMain);
    SetForegroundWindow(g_hMain);
    SetActiveWindow(g_hMain);
    SetFocus(g_hEdit);
    if (ft && ft != ct) AttachThreadInput(ft, ct, FALSE);
    if (GetForegroundWindow() != g_hMain) {
        SwitchToThisWindow(g_hMain, TRUE);
        SetForegroundWindow(g_hMain);
        SetFocus(g_hEdit);
    }
}

static void SyncEditFromMenus() {
    std::wstring t(1, MenuTriggerKey());
    for (size_t i = 1; i < g_menus.size(); i++) t += g_menus[i].parent->hotkey;
    SetEditText(t);
}

static void EnterFolder(const Node* n, int folderRowIdx) {
    if (g_menus.empty()) return;
    MenuLevel& par = g_menus.back();
    if (folderRowIdx >= 0 && folderRowIdx < (int)par.rows.size()) {
        par.sel = folderRowIdx;
        if (par.sel < par.top) par.top = par.sel;
        if (par.sel >= par.top + MAX_VIS) par.top = par.sel - MAX_VIS + 1;
        InvalidateRect(par.hwnd, nullptr, FALSE);
    }
    RECT prc;
    GetWindowRect(par.hwnd, &prc);
    int visIdx = std::max(0, par.sel - par.top);
    int y = prc.top + 1 + visIdx * S(ROW_H);
    int w = prc.right - prc.left;

    MenuLevel ml;
    ml.parent = n;
    ml.isHot = true;
    BuildHotRowsFor(n, ml.rows);
    ml.hwnd = CreateMenuWnd();
    int nh = std::min((int)ml.rows.size(), MAX_VIS) * S(ROW_H) + 2;

    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfoW(MonitorFromWindow(par.hwnd, MONITOR_DEFAULTTOPRIMARY), &mi);
    int x = prc.right;
    if (x + w > mi.rcMonitor.right) x = std::max((int)mi.rcMonitor.left, (int)prc.left - w);
    if (y + nh > mi.rcMonitor.bottom) y = std::max((int)mi.rcMonitor.top, (int)mi.rcMonitor.bottom - nh);

    SetWindowPos(ml.hwnd, HWND_TOPMOST, x, y, w, nh, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    g_menus.push_back(std::move(ml));
    SetEditText(GetEditText() + n->hotkey);
}
static void GoUp() {
    if (g_menus.size() <= 1) {  // 최상위에서 뒤로 = 핫키 모드 종료
        SetEditText(L"");
        PopMenusTo(0);
        return;
    }
    PopMenusTo(g_menus.size() - 1);
    SyncEditFromMenus();
}
static void ExecuteRow(int i) {
    if (g_menus.empty()) return;
    MenuLevel& cur = g_menus.back();
    if (i < 0 || i >= (int)cur.rows.size()) return;
    Row r = cur.rows[i];
    if (r.empty) return;
    if (cur.isHot && r.folder) { EnterFolder(r.node, i); return; }
    HideLauncher();
    RunCommand(r.command);
}
static void HandleDigit(wchar_t c) {
    if (g_menus.empty()) return;
    MenuLevel& cur = g_menus.back();
    wint_t lc = towlower(c);
    for (int i = 0; i < (int)cur.rows.size(); i++) {
        const Row& r = cur.rows[i];
        if (!r.empty && r.node && r.node->hotkey.size() == 1 && towlower(r.node->hotkey[0]) == lc) {
            if (r.folder) EnterFolder(r.node, i);
            else { std::wstring cmd = r.command; HideLauncher(); RunCommand(cmd); }
            return;
        }
    }
    MessageBeep(MB_OK);
}
static void MoveSel(int d) {
    if (g_menus.empty()) return;
    MenuLevel& cur = g_menus.back();
    if (!IsWindowVisible(cur.hwnd) || cur.rows.empty()) return;
    int n = (int)cur.rows.size();
    cur.sel = (cur.sel + d + n) % n;
    if (cur.sel < cur.top) cur.top = cur.sel;
    if (cur.sel >= cur.top + MAX_VIS) cur.top = cur.sel - MAX_VIS + 1;
    InvalidateRect(cur.hwnd, nullptr, FALSE);
}
static void OnEditChanged() {
    if (g_ignoreChange) return;
    std::wstring t = GetEditText();
    if (t.empty()) {
        PopMenusTo(0);
        return;
    }
    wchar_t mk = MenuTriggerKey();
    if (t[0] == mk) {
        std::wstring mks(1, mk);
        if (t != mks) SetEditText(mks);
        ShowRootHotMenu();
        return;
    }
    BuildKeywordRows(t);
}

// 텍스트 박스 서브클래스 -------------------------------------------------------
static LRESULT CALLBACK EditProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    switch (m) {
    case WM_CONTEXTMENU:
        ShowCtxMenu(g_hMain);
        return 0;
    case WM_CUT: case WM_PASTE: case WM_CLEAR:
        if (IsHotMode()) return 0;
        break;
    case WM_KEYDOWN: {
        bool hot = IsHotMode();
        switch (w) {
        case VK_ESCAPE:
            if (ListVisible()) {
                PopMenusTo(0);
                if (hot) SetEditText(L"");
            } else HideLauncher();
            return 0;
        case VK_UP: MoveSel(-1); return 0;
        case VK_DOWN: MoveSel(1); return 0;
        case VK_RETURN:
            if (!g_menus.empty()) ExecuteRow(g_menus.back().sel);
            return 0;
        case VK_BACK:
            if (hot) { GoUp(); return 0; }
            break;
        case VK_LEFT:
            if (hot) { GoUp(); return 0; }
            break;
        case VK_RIGHT:
            if (hot) {
                if (!g_menus.empty()) {
                    const MenuLevel& cur = g_menus.back();
                    if (cur.sel >= 0 && cur.sel < (int)cur.rows.size() && cur.rows[cur.sel].folder)
                        EnterFolder(cur.rows[cur.sel].node, cur.sel);
                }
                return 0;
            }
            break;
        case VK_DELETE: case VK_HOME: case VK_END: case VK_INSERT:
            if (hot) return 0;
            break;
        }
        break;
    }
    case WM_SYSCHAR:
        return 0;
    case WM_CHAR:
        if (w == VK_RETURN || w == VK_ESCAPE) return 0;
        if (IsHotMode()) {
            if (w >= 32 && w != 127 && w != L' ') HandleDigit((wchar_t)w);
            return 0;
        }
        break;
    }
    return DefSubclassProc(h, m, w, l);
}

// 드롭다운 리스트 창 -----------------------------------------------------------
static void PaintList(HWND h) {
    int mi = FindMenu(h);
    if (mi < 0) { PAINTSTRUCT ps; BeginPaint(h, &ps); EndPaint(h, &ps); return; }
    const MenuLevel& ml = g_menus[mi];
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(h, &ps);
    RECT rc; GetClientRect(h, &rc);
    HDC dc = CreateCompatibleDC(hdc);
    HBITMAP bm = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    HGDIOBJ ob = SelectObject(dc, bm);
    HBRUSH bb = CreateSolidBrush(g_cfg.bgColor);
    FillRect(dc, &rc, bb);
    DeleteObject(bb);
    HGDIOBJ of = SelectObject(dc, g_fontList);
    SetBkMode(dc, TRANSPARENT);
    int rh = S(ROW_H);
    int n = std::min((int)ml.rows.size() - ml.top, MAX_VIS);
    for (int k = 0; k < n; k++) {
        int i = ml.top + k;
        const Row& r = ml.rows[i];
        RECT rr = {1, 1 + k * rh, rc.right - 1, 1 + (k + 1) * rh};
        if (i == ml.sel && !r.empty) {
            HBRUSH sb = CreateSolidBrush(RGB(0, 90, 170));
            FillRect(dc, &rr, sb);
            DeleteObject(sb);
        }
        RECT nr = rr;
        if (ml.isHot && !r.empty) {
            RECT hr = {rr.left + S(4), rr.top, rr.left + S(30), rr.bottom};
            SetTextColor(dc, RGB(120, 200, 255));
            DrawTextW(dc, r.hot.c_str(), -1, &hr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            HPEN pen = CreatePen(PS_SOLID, 1, RGB(110, 110, 110));
            HGDIOBJ op = SelectObject(dc, pen);
            MoveToEx(dc, rr.left + S(33), rr.top + S(4), nullptr);
            LineTo(dc, rr.left + S(33), rr.bottom - S(4));
            SelectObject(dc, op);
            DeleteObject(pen);
            nr.left = rr.left + S(40);
        } else nr.left = rr.left + S(8);
        nr.right = rr.right - (r.folder ? S(26) : S(6));
        SetTextColor(dc, r.empty ? RGB(130, 130, 130) : RGB(255, 255, 255));
        DrawTextW(dc, r.name.c_str(), -1, &nr, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        if (ml.isHot && r.folder) {  // 하위 항목 있음 아이콘 (▶)
            int cx = rr.right - S(14), cy = (rr.top + rr.bottom) / 2;
            POINT pts[3] = {{cx - S(3), cy - S(5)}, {cx - S(3), cy + S(5)}, {cx + S(4), cy}};
            HBRUSH tb = CreateSolidBrush(RGB(255, 255, 255));
            HPEN tp = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
            HGDIOBJ obr = SelectObject(dc, tb), opn = SelectObject(dc, tp);
            Polygon(dc, pts, 3);
            SelectObject(dc, obr); SelectObject(dc, opn);
            DeleteObject(tb); DeleteObject(tp);
        }
    }
    HBRUSH fb = CreateSolidBrush(g_cfg.borderColor);
    FrameRect(dc, &rc, fb);
    DeleteObject(fb);
    SelectObject(dc, of);
    BitBlt(hdc, 0, 0, rc.right, rc.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, ob);
    DeleteObject(bm);
    DeleteDC(dc);
    EndPaint(h, &ps);
}

static int RowAt(const MenuLevel& ml, int y) {
    int i = ml.top + (y - 1) / S(ROW_H);
    return (i >= 0 && i < (int)ml.rows.size() && y >= 1) ? i : -1;
}
static LRESULT CALLBACK ListProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: PaintList(h); return 0;
    case WM_MOUSEMOVE: {
        int mi = FindMenu(h);
        if (mi >= 0) {
            int i = RowAt(g_menus[mi], GET_Y_LPARAM(l));
            if (i >= 0 && i != g_menus[mi].sel) { g_menus[mi].sel = i; InvalidateRect(h, nullptr, FALSE); }
        }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int mi = FindMenu(h);
        if (mi >= 0) {
            int i = RowAt(g_menus[mi], GET_Y_LPARAM(l));
            if (i >= 0) {
                PopMenusTo((size_t)mi + 1);
                if (g_menus[mi].isHot) SyncEditFromMenus();
                g_menus[mi].sel = i;
                ExecuteRow(i);
            }
        }
        return 0;
    }
    case WM_MOUSEWHEEL: {
        int mi = FindMenu(h);
        if (mi >= 0) {
            int d = GET_WHEEL_DELTA_WPARAM(w) > 0 ? -1 : 1;
            int maxTop = std::max(0, (int)g_menus[mi].rows.size() - MAX_VIS);
            g_menus[mi].top = std::max(0, std::min(maxTop, g_menus[mi].top + d));
            InvalidateRect(h, nullptr, FALSE);
        }
        return 0;
    }
    }
    return DefWindowProcW(h, m, w, l);
}

// 메인 입력 창 ---------------------------------------------------------------
static LRESULT CALLBACK MainProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        HBRUSH bk = CreateSolidBrush(g_cfg.bgColor), wb = CreateSolidBrush(g_cfg.borderColor);
        FillRect(dc, &rc, bk);
        FrameRect(dc, &rc, wb);              // 외곽선 2px
        InflateRect(&rc, -1, -1);
        FrameRect(dc, &rc, wb);
        DeleteObject(bk); DeleteObject(wb);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_CTLCOLOREDIT: {
        static HBRUSH br = nullptr;
        static COLORREF brCol = CLR_INVALID;
        if (!br || brCol != g_cfg.bgColor) {
            if (br) DeleteObject(br);
            br = CreateSolidBrush(g_cfg.bgColor);
            brCol = g_cfg.bgColor;
        }
        SetTextColor((HDC)w, RGB(255, 255, 255));
        SetBkColor((HDC)w, g_cfg.bgColor);
        return (LRESULT)br;
    }
    case WM_COMMAND:
        if (LOWORD(w) == 1 && HIWORD(w) == EN_CHANGE) OnEditChanged();
        return 0;
    case WM_SETFOCUS:
        if (g_hEdit) SetFocus(g_hEdit);
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(w) == WA_INACTIVE && !g_menuOpen && GetTickCount64() - g_lastShowTick > 150) HideLauncher();  // 포커스 아웃 → 숨김
        return 0;
    case WM_RBUTTONUP:
        ShowCtxMenu(h);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}


// ----------------------------------------------------------------------------
//  초기화: 클래스 등록 + 메인 창/입력 박스/드롭다운 창 생성
// ----------------------------------------------------------------------------
bool Launcher_Init(HINSTANCE hInst) {
    WNDCLASSEXW wc = {sizeof(wc)};
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = g_icoBig;
    wc.hIconSm = g_icoSmall;
    wc.lpfnWndProc = MainProc;
    wc.lpszClassName = L"mkRunMain";
    RegisterClassExW(&wc);
    wc.lpfnWndProc = ListProc;
    wc.lpszClassName = L"mkRunList";
    RegisterClassExW(&wc);

    g_hMain = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, L"mkRunMain", L"mkRun", WS_POPUP,
                              0, 0, S(200), S(30), nullptr, nullptr, hInst, nullptr);
    HDC dc = GetDC(g_hMain);
    HGDIOBJ of = SelectObject(dc, g_fontEdit);
    TEXTMETRICW tm = {};
    GetTextMetricsW(dc, &tm);
    SelectObject(dc, of);
    ReleaseDC(g_hMain, dc);
    int eh = std::max(S(14), (int)tm.tmHeight + S(2));
    int ey = (S(30) - eh) / 2;
    g_hEdit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                              S(8), ey, S(184), eh, g_hMain, (HMENU)1, hInst, nullptr);
    SendMessageW(g_hEdit, WM_SETFONT, (WPARAM)g_fontEdit, TRUE);
    SetWindowSubclass(g_hEdit, EditProc, 1, 0);
    return g_hMain && g_hEdit;
}
