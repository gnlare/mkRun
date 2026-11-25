// Settings.cpp
#include "Common.h"
#include "Util.h"
#include "Hotkey.h"
#include "Config.h"
#include "Launcher.h"
#include "Settings.h"

static HWND g_hSettings = nullptr;

// ----------------------------------------------------------------------------
//  설정 창
// ----------------------------------------------------------------------------
struct HRow { Node* node; Node* parent; int depth; };
static std::vector<HRow> g_hrows;
static std::vector<HWND> g_pages[4];
static std::vector<HWND> g_catGeneral, g_catAppearance;
static HWND g_hTab, g_lvHot, g_edHName, g_edHKey, g_edHCmd, g_lvKw, g_edKwK, g_edKwC, g_chkStart, g_edHotkey, g_edMenuHotkey;
static HWND g_lbCat, g_btnBorderClr, g_btnBgClr, g_btnLangKo, g_btnLangEn;
static bool g_keepWork = false;
static const wchar_t* WTr(const wchar_t* ko, const wchar_t* en) { return g_work.lang == L"en" ? en : ko; }
static HIMAGELIST g_imlHot = nullptr;
static HWND g_edInline = nullptr, g_inlineLv = nullptr;
static int g_inlineRow = -1, g_inlineCol = -1;
static bool g_inlineBusy = false;
static bool g_refreshing = false;
static int g_dragRow = -1, g_dropRow = -1;
static HWND g_dragLv = nullptr;
static const UINT_PTR IDT_FLASH = 1;
static bool g_flashName = false, g_flashCmd = false;
static int g_flashCount = 0;
static HBRUSH g_flashBrush = nullptr;

static HWND Mk(int page, const wchar_t* cls, const wchar_t* txt, DWORD st, int x, int y, int w, int h, int id, DWORD ex = 0) {
    HWND c = CreateWindowExW(ex, cls, txt, WS_CHILD | WS_CLIPSIBLINGS | st, S(x), S(y), S(w), S(h),
                             g_hSettings, (HMENU)(INT_PTR)id, g_hInst, nullptr);
    if (page >= 0) g_pages[page].push_back(c);
    return c;
}
static HWND MkCat(std::vector<HWND>& cat, const wchar_t* cls, const wchar_t* txt, DWORD st, int x, int y, int w, int h, int id, DWORD ex = 0) {
    HWND c = Mk(2, cls, txt, st, x, y, w, h, id, ex);
    cat.push_back(c);
    return c;
}
static void AddCol(HWND lv, int idx, const wchar_t* t, int w) {
    LVCOLUMNW c = {};
    c.mask = LVCF_TEXT | LVCF_WIDTH;
    c.pszText = const_cast<wchar_t*>(t);
    c.cx = S(w);
    ListView_InsertColumn(lv, idx, &c);
}
static void CloseInlineEdit(bool commit);

static void SwitchCat(int cat) {
    for (HWND c : g_catGeneral) ShowWindow(c, cat == 0 ? SW_SHOW : SW_HIDE);
    for (HWND c : g_catAppearance) ShowWindow(c, cat == 1 ? SW_SHOW : SW_HIDE);
}

static void SwitchTab(int t) {
    CloseInlineEdit(true);
    for (int p = 0; p < 4; p++)
        for (HWND c : g_pages[p]) ShowWindow(c, p == t ? SW_SHOW : SW_HIDE);
    if (t == 2 && g_lbCat) {
        int cat = (int)SendMessageW(g_lbCat, LB_GETCURSEL, 0, 0);
        SwitchCat(cat < 0 ? 0 : cat);
    }
}

// --- 숫자 핫키 탭 -------------------------------------------------------------
static void Flatten(Node& parent, int depth) {
    for (auto& c : parent.children) {
        g_hrows.push_back({&c, &parent, depth});
        if (c.folder && c.expanded) Flatten(c, depth + 1);
    }
}
static int HotSel() { return ListView_GetNextItem(g_lvHot, -1, LVNI_SELECTED); }

static void LoadHotEdits() {
    int i = HotSel();
    if (i < 0 || i >= (int)g_hrows.size()) {
        SetWindowTextW(g_edHName, L""); SetWindowTextW(g_edHKey, L""); SetWindowTextW(g_edHCmd, L"");
        EnableWindow(g_edHCmd, TRUE);
        return;
    }
    Node* n = g_hrows[i].node;
    SetWindowTextW(g_edHName, n->name.c_str());
    SetWindowTextW(g_edHKey, n->hotkey.c_str());
    SetWindowTextW(g_edHCmd, n->command.c_str());
    EnableWindow(g_edHCmd, n->folder ? FALSE : TRUE);  // 폴더는 명령 입력 불가
}

static void RefreshHot(const std::wstring& selId) {
    g_refreshing = true;
    SendMessageW(g_lvHot, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(g_lvHot);
    if (g_imlHot) ImageList_RemoveAll(g_imlHot);
    g_hrows.clear();
    Flatten(g_work.root, 0);
    int sel = -1;
    for (int i = 0; i < (int)g_hrows.size(); i++) {
        Node* n = g_hrows[i].node;
        int imgIdx = -1;
        if (g_imlHot) {
            HICON ico = GetCommandIcon(n->command, n->folder);
            if (ico) {
                imgIdx = ImageList_AddIcon(g_imlHot, ico);
                DestroyIcon(ico);
            }
        }
        std::wstring t = (n->folder ? (n->expanded ? L"[-] " : L"[+] ") : L"") + n->name;
        LVITEMW it = {};
        it.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_INDENT;
        it.iItem = i;
        it.iImage = imgIdx;
        it.iIndent = g_hrows[i].depth;
        it.pszText = const_cast<wchar_t*>(t.c_str());
        ListView_InsertItem(g_lvHot, &it);
        ListView_SetItemText(g_lvHot, i, 1, const_cast<wchar_t*>(n->hotkey.c_str()));
        ListView_SetItemText(g_lvHot, i, 2, const_cast<wchar_t*>(n->command.c_str()));
        if (!selId.empty() && n->id == selId) sel = i;
    }
    if (sel >= 0) {
        ListView_SetItemState(g_lvHot, sel, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(g_lvHot, sel, FALSE);
    }
    SendMessageW(g_lvHot, WM_SETREDRAW, TRUE, 0);
    g_refreshing = false;
    LoadHotEdits();
}

static bool HotkeyUsed(Node* parent, const std::wstring& hk, const Node* except) {
    for (auto& c : parent->children) {
        if (&c == except) continue;
        if (c.hotkey.size() == 1 && hk.size() == 1) {
            if (towlower(c.hotkey[0]) == towlower(hk[0])) return true;
        } else if (c.hotkey == hk) return true;
    }
    return false;
}

static std::wstring AutoHotkey(Node* parent) {
    for (wchar_t c = L'1'; c <= L'9'; c++) {
        std::wstring t(1, c);
        if (!HotkeyUsed(parent, t, nullptr)) return t;
    }
    if (!HotkeyUsed(parent, L"0", nullptr)) return L"0";
    for (wchar_t c = L'a'; c <= L'z'; c++) {
        std::wstring t(1, c);
        if (!HotkeyUsed(parent, t, nullptr)) return t;
    }
    return L"";
}

static void StartEmptyFlash(bool nameEmpty, bool cmdEmpty) {
    g_flashName = nameEmpty;
    g_flashCmd = cmdEmpty;
    g_flashCount = 6;
    if (!g_flashBrush) g_flashBrush = CreateSolidBrush(RGB(255, 190, 190));
    SetTimer(g_hSettings, IDT_FLASH, 120, nullptr);
    if (g_flashName) InvalidateRect(g_edHName, nullptr, TRUE);
    if (g_flashCmd) InvalidateRect(g_edHCmd, nullptr, TRUE);
}

static void AddHot(bool folder) {
    std::wstring name = Trim(GetText(g_edHName));
    std::wstring hk = Trim(GetText(g_edHKey));
    std::wstring cmd = Trim(GetText(g_edHCmd));
    bool badName = name.empty();
    bool badCmd = !folder && cmd.empty();
    if (badName || badCmd) {
        StartEmptyFlash(badName, badCmd);
        SetFocus(badName ? g_edHName : g_edHCmd);
        return;
    }

    int i = HotSel();
    Node* parent = &g_work.root;
    if (i >= 0 && i < (int)g_hrows.size()) {
        Node* sel = g_hrows[i].node;
        if (sel->folder) { parent = sel; sel->expanded = true; }  // 선택 폴더의 하위로
        else parent = g_hrows[i].parent;                           // 항목 선택 시 같은 레벨
    }

    if (hk.empty()) {
        hk = AutoHotkey(parent);
        if (hk.empty()) {
            MessageBoxW(g_hSettings, L"이 뎁스에는 더 이상 추가할 수 없습니다. (사용 가능한 핫키 없음)", L"mkRun", MB_OK | MB_ICONINFORMATION);
            return;
        }
    } else {
        if (hk.size() != 1 || iswspace(hk[0])) {
            MessageBoxW(g_hSettings, L"핫키는 공백이 아닌 한 글자여야 합니다.", L"mkRun", MB_OK | MB_ICONWARNING);
            return;
        }
        if (HotkeyUsed(parent, hk, nullptr)) {
            MessageBoxW(g_hSettings, L"같은 뎁스에 이미 사용 중인 핫키입니다.", L"mkRun", MB_OK | MB_ICONWARNING);
            return;
        }
    }

    Node n;
    n.id = NewId();
    n.folder = folder;
    n.hotkey = hk;
    n.name = name;
    n.command = folder ? L"" : cmd;
    parent->children.push_back(n);
    RefreshHot(n.id);
    SetFocus(g_edHName);
    SendMessageW(g_edHName, EM_SETSEL, 0, -1);
}

static void ApplyHot() {
    int i = HotSel();
    if (i < 0 || i >= (int)g_hrows.size()) return;
    HRow r = g_hrows[i];
    std::wstring name = Trim(GetText(g_edHName)), hk = Trim(GetText(g_edHKey)), cmd = Trim(GetText(g_edHCmd));
    if (name.empty()) { MessageBoxW(g_hSettings, L"이름을 입력하세요.", L"mkRun", MB_OK | MB_ICONWARNING); return; }
    if (hk.size() != 1 || iswspace(hk[0])) {
        MessageBoxW(g_hSettings, L"핫키는 공백이 아닌 한 글자여야 합니다.", L"mkRun", MB_OK | MB_ICONWARNING); return;
    }
    if (HotkeyUsed(r.parent, hk, r.node)) {
        MessageBoxW(g_hSettings, L"같은 뎁스에 이미 사용 중인 핫키입니다.", L"mkRun", MB_OK | MB_ICONWARNING); return;
    }
    if (!r.node->folder && cmd.empty()) {
        MessageBoxW(g_hSettings, L"명령을 입력하세요.", L"mkRun", MB_OK | MB_ICONWARNING); return;
    }
    r.node->name = name;
    r.node->hotkey = hk;
    if (!r.node->folder) r.node->command = cmd;
    RefreshHot(r.node->id);
}

static void DelHot() {
    int i = HotSel();
    if (i < 0 || i >= (int)g_hrows.size()) return;
    HRow r = g_hrows[i];
    if (r.node->folder && !r.node->children.empty() &&
        MessageBoxW(g_hSettings, L"하위 항목도 모두 삭제됩니다. 계속할까요?", L"mkRun", MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;
    for (auto it = r.parent->children.begin(); it != r.parent->children.end(); ++it)
        if (&*it == r.node) { r.parent->children.erase(it); break; }
    RefreshHot(L"");
}

static void ToggleHot(int i, int forceState /*-1 토글, 0 접기, 1 펼치기*/) {
    if (i < 0 || i >= (int)g_hrows.size()) return;
    Node* n = g_hrows[i].node;
    if (!n->folder) return;
    n->expanded = forceState < 0 ? !n->expanded : (forceState == 1);
    std::wstring id = n->id;
    RefreshHot(id);
}

// --- 키워드 탭 ----------------------------------------------------------------
static int KwSel() { return ListView_GetNextItem(g_lvKw, -1, LVNI_SELECTED); }
static void LoadKwEdits() {
    int i = KwSel();
    if (i < 0 || i >= (int)g_work.keywords.size()) { SetWindowTextW(g_edKwK, L""); SetWindowTextW(g_edKwC, L""); return; }
    SetWindowTextW(g_edKwK, g_work.keywords[i].keyword.c_str());
    SetWindowTextW(g_edKwC, g_work.keywords[i].command.c_str());
}
static void RefreshKw(int sel) {
    g_refreshing = true;
    ListView_DeleteAllItems(g_lvKw);
    for (int i = 0; i < (int)g_work.keywords.size(); i++) {
        LVITEMW it = {};
        it.mask = LVIF_TEXT; it.iItem = i;
        it.pszText = const_cast<wchar_t*>(g_work.keywords[i].keyword.c_str());
        ListView_InsertItem(g_lvKw, &it);
        ListView_SetItemText(g_lvKw, i, 1, const_cast<wchar_t*>(g_work.keywords[i].command.c_str()));
    }
    if (sel >= 0 && sel < (int)g_work.keywords.size())
        ListView_SetItemState(g_lvKw, sel, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    g_refreshing = false;
    LoadKwEdits();
}
static bool ReadKwInput(std::wstring& k, std::wstring& c) {
    k = Trim(GetText(g_edKwK));
    c = Trim(GetText(g_edKwC));
    if (k.empty() || c.empty()) {
        MessageBoxW(g_hSettings, L"키워드와 명령을 모두 입력하세요.", L"mkRun", MB_OK | MB_ICONWARNING);
        return false;
    }
    if (k[0] >= L'0' && k[0] <= L'9') {  // 첫 글자 숫자 금지
        MessageBoxW(g_hSettings, L"키워드의 첫 글자는 숫자로 시작할 수 없습니다.\n(숫자 0으로 시작하면 핫키 모드로 동작합니다.)", L"mkRun", MB_OK | MB_ICONWARNING);
        return false;
    }
    return true;
}
static void AddKw() {
    std::wstring k, c;
    if (!ReadKwInput(k, c)) return;
    g_work.keywords.push_back({k, c});
    RefreshKw((int)g_work.keywords.size() - 1);
    SetWindowTextW(g_edKwK, L""); SetWindowTextW(g_edKwC, L"");
    SetFocus(g_edKwK);
}
static void ModKw() {
    int i = KwSel();
    if (i < 0) return;
    std::wstring k, c;
    if (!ReadKwInput(k, c)) return;
    g_work.keywords[i] = {k, c};
    RefreshKw(i);
}
static void DelKw() {
    int i = KwSel();
    if (i < 0 || i >= (int)g_work.keywords.size()) return;
    g_work.keywords.erase(g_work.keywords.begin() + i);
    RefreshKw(std::min(i, (int)g_work.keywords.size() - 1));
}

// --- 리스트뷰 인라인 직접 수정 --------------------------------------------------
static void CloseInlineEdit(bool commit) {
    if (!g_edInline || g_inlineBusy) return;
    g_inlineBusy = true;
    HWND ed = g_edInline, lv = g_inlineLv;
    int row = g_inlineRow, col = g_inlineCol;
    std::wstring val = Trim(GetText(ed));
    g_edInline = nullptr; g_inlineLv = nullptr; g_inlineRow = -1; g_inlineCol = -1;
    DestroyWindow(ed);

    if (commit) {
        if (lv == g_lvHot && row >= 0 && row < (int)g_hrows.size()) {
            HRow r = g_hrows[row];
            if (col == 0) {
                if (val.empty()) MessageBoxW(g_hSettings, L"이름을 입력하세요.", L"mkRun", MB_OK | MB_ICONWARNING);
                else { r.node->name = val; RefreshHot(r.node->id); }
            } else if (col == 1) {
                if (val.size() != 1 || iswspace(val[0]))
                    MessageBoxW(g_hSettings, L"핫키는 공백이 아닌 한 글자여야 합니다.", L"mkRun", MB_OK | MB_ICONWARNING);
                else if (HotkeyUsed(r.parent, val, r.node))
                    MessageBoxW(g_hSettings, L"같은 뎁스에 이미 사용 중인 핫키입니다.", L"mkRun", MB_OK | MB_ICONWARNING);
                else { r.node->hotkey = val; RefreshHot(r.node->id); }
            } else if (col == 2 && !r.node->folder) {
                if (val.empty()) MessageBoxW(g_hSettings, L"명령을 입력하세요.", L"mkRun", MB_OK | MB_ICONWARNING);
                else { r.node->command = val; RefreshHot(r.node->id); }
            }
        } else if (lv == g_lvKw && row >= 0 && row < (int)g_work.keywords.size()) {
            if (col == 0) {
                if (val.empty()) MessageBoxW(g_hSettings, L"키워드를 입력하세요.", L"mkRun", MB_OK | MB_ICONWARNING);
                else if (val[0] >= L'0' && val[0] <= L'9')
                    MessageBoxW(g_hSettings, L"키워드의 첫 글자는 숫자로 시작할 수 없습니다.\n(숫자 0으로 시작하면 핫키 모드로 동작합니다.)", L"mkRun", MB_OK | MB_ICONWARNING);
                else { g_work.keywords[row].keyword = val; RefreshKw(row); }
            } else if (col == 1) {
                if (val.empty()) MessageBoxW(g_hSettings, L"명령을 입력하세요.", L"mkRun", MB_OK | MB_ICONWARNING);
                else { g_work.keywords[row].command = val; RefreshKw(row); }
            }
        }
    }
    g_inlineBusy = false;
}

static LRESULT CALLBACK InlineEditProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    switch (m) {
    case WM_GETDLGCODE: return DLGC_WANTALLKEYS;
    case WM_KEYDOWN:
        if (w == VK_RETURN) {
            HWND lv = g_inlineLv;
            CloseInlineEdit(true);
            if (lv) SetFocus(lv);
            return 0;
        }
        if (w == VK_ESCAPE) {
            HWND lv = g_inlineLv;
            CloseInlineEdit(false);
            if (lv) SetFocus(lv);
            return 0;
        }
        break;
    case WM_CHAR:
        if (g_inlineLv == g_lvHot && g_inlineCol == 1 && w >= 32 && w != 127) {
            wchar_t ch = (wchar_t)w;
            if (ch == L' ') { MessageBeep(MB_OK); return 0; }
            wchar_t buf[2] = {ch, 0};
            SetWindowTextW(h, buf);
            SendMessageW(h, EM_SETSEL, 1, 1);
            return 0;
        }
        break;
    case WM_KILLFOCUS:
        CloseInlineEdit(true);
        return 0;
    }
    return DefSubclassProc(h, m, w, l);
}

static bool IsDescendant(const Node* anc, const Node* target) {
    if (anc == target) return true;
    for (const auto& c : anc->children)
        if (IsDescendant(&c, target)) return true;
    return false;
}

static void MoveHotRow(int srcIdx, int dstIdx) {
    if (srcIdx < 0 || srcIdx >= (int)g_hrows.size() || srcIdx == dstIdx) return;
    HRow src = g_hrows[srcIdx];
    Node* newParent = &g_work.root;
    int insertPos = (int)g_work.root.children.size();

    if (dstIdx >= 0 && dstIdx < (int)g_hrows.size()) {
        HRow dst = g_hrows[dstIdx];
        if (src.node->folder && IsDescendant(src.node, dst.node)) return;
        if (dst.node->folder) {
            if (src.parent == dst.node) return;
            newParent = dst.node;
            dst.node->expanded = true;
            insertPos = (int)newParent->children.size();
        } else {
            newParent = dst.parent;
            insertPos = 0;
            for (int i = 0; i < (int)newParent->children.size(); i++) {
                if (&newParent->children[i] == dst.node) { insertPos = i; break; }
            }
        }
    }

    std::wstring hk = src.node->hotkey;
    if (src.parent != newParent && HotkeyUsed(newParent, hk, nullptr)) {
        hk = AutoHotkey(newParent);
        if (hk.empty()) {
            MessageBoxW(g_hSettings, L"대상 폴더(뎁스)에 빈 핫키가 없어 이동할 수 없습니다.", L"mkRun", MB_OK | MB_ICONWARNING);
            return;
        }
    }

    Node moved = *src.node;
    moved.hotkey = hk;
    std::wstring movedId = moved.id;

    int srcPos = -1;
    for (int i = 0; i < (int)src.parent->children.size(); i++) {
        if (&src.parent->children[i] == src.node) { srcPos = i; break; }
    }
    if (srcPos < 0) return;
    src.parent->children.erase(src.parent->children.begin() + srcPos);
    if (src.parent == newParent && srcPos < insertPos) insertPos--;
    insertPos = std::max(0, std::min(insertPos, (int)newParent->children.size()));
    newParent->children.insert(newParent->children.begin() + insertPos, moved);
    RefreshHot(movedId);
}

static void MoveKwRow(int srcIdx, int dstIdx) {
    int n = (int)g_work.keywords.size();
    if (srcIdx < 0 || srcIdx >= n || dstIdx < 0 || dstIdx >= n || srcIdx == dstIdx) return;
    Keyword moved = std::move(g_work.keywords[srcIdx]);
    g_work.keywords.erase(g_work.keywords.begin() + srcIdx);
    g_work.keywords.insert(g_work.keywords.begin() + dstIdx, std::move(moved));
    RefreshKw(dstIdx);
}

static void EndDrag(bool commit) {
    if (g_dragRow < 0 || !g_dragLv) return;
    HWND lv = g_dragLv;
    int src = g_dragRow, dst = g_dropRow;
    if (g_dropRow >= 0) ListView_SetItemState(lv, g_dropRow, 0, LVIS_DROPHILITED);
    g_dragRow = -1; g_dropRow = -1; g_dragLv = nullptr;
    if (GetCapture() == lv) ReleaseCapture();
    if (commit) {
        if (lv == g_lvHot) MoveHotRow(src, dst);
        else if (lv == g_lvKw) MoveKwRow(src, dst);
    }
}

static LRESULT CALLBACK LvScrollProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    if ((m == WM_VSCROLL || m == WM_HSCROLL || m == WM_MOUSEWHEEL) && g_edInline && g_inlineLv == h)
        CloseInlineEdit(true);
    if (g_dragLv == h && g_dragRow >= 0) {
        if (m == WM_MOUSEMOVE) {
            LVHITTESTINFO ht = {};
            ht.pt = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
            int hit = ListView_HitTest(h, &ht);
            if (hit != g_dropRow) {
                if (g_dropRow >= 0) ListView_SetItemState(h, g_dropRow, 0, LVIS_DROPHILITED);
                g_dropRow = hit;
                if (g_dropRow >= 0) ListView_SetItemState(h, g_dropRow, LVIS_DROPHILITED, LVIS_DROPHILITED);
            }
            return 0;
        }
        if (m == WM_LBUTTONUP) { EndDrag(true); return 0; }
        if (m == WM_RBUTTONDOWN || (m == WM_KEYDOWN && w == VK_ESCAPE)) { EndDrag(false); return 0; }
        if (m == WM_CAPTURECHANGED) { EndDrag(false); return 0; }
    }
    return DefSubclassProc(h, m, w, l);
}

static void StartInlineEdit(HWND lv, int row, int col) {
    CloseInlineEdit(true);
    if (row < 0 || col < 0) return;
    std::wstring txt;
    if (lv == g_lvHot) {
        if (row >= (int)g_hrows.size() || col > 2) return;
        Node* n = g_hrows[row].node;
        if (col == 2 && n->folder) return;
        txt = (col == 0) ? n->name : (col == 1 ? n->hotkey : n->command);
    } else if (lv == g_lvKw) {
        if (row >= (int)g_work.keywords.size() || col > 1) return;
        txt = (col == 0) ? g_work.keywords[row].keyword : g_work.keywords[row].command;
    } else return;

    ListView_EnsureVisible(lv, row, FALSE);
    RECT rc = {};
    if (col == 0) ListView_GetSubItemRect(lv, row, 0, LVIR_LABEL, &rc);
    else ListView_GetSubItemRect(lv, row, col, LVIR_BOUNDS, &rc);

    DWORD st = WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | (lv == g_lvHot && col == 1 ? ES_CENTER : 0);
    int w = std::max(S(40), (int)(rc.right - rc.left));
    int h = rc.bottom - rc.top;
    g_inlineLv = lv; g_inlineRow = row; g_inlineCol = col;
    g_edInline = CreateWindowExW(0, L"EDIT", txt.c_str(), st, rc.left, rc.top, w, h, lv, nullptr, g_hInst, nullptr);
    SendMessageW(g_edInline, WM_SETFONT, (WPARAM)g_fontUI, TRUE);
    if (lv == g_lvHot && col == 1) SendMessageW(g_edInline, EM_LIMITTEXT, 1, 0);
    SetWindowSubclass(g_edInline, InlineEditProc, 3, 0);
    SetFocus(g_edInline);
    SendMessageW(g_edInline, EM_SETSEL, 0, -1);
}

// --- 단축키 단일 문자 입력 박스 (기존 값 지우지 않고 바로 덮어쓰기) ----------------
static LRESULT CALLBACK SingleKeyEditProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    if (m == WM_CHAR && w >= 32 && w != 127) {
        wchar_t ch = (wchar_t)w;
        if (ch == L' ') { MessageBeep(MB_OK); return 0; }
        wchar_t buf[2] = {ch, 0};
        SetWindowTextW(h, buf);
        SendMessageW(h, EM_SETSEL, 1, 1);
        return 0;
    }
    return DefSubclassProc(h, m, w, l);
}

// --- 프로그램 탭: 단축키 캡처 박스 ----------------------------------------------
static LRESULT CALLBACK HkEditProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    switch (m) {
    case WM_SETFOCUS:
        SetHotkeyCaptureMode(true);
        break;
    case WM_KILLFOCUS:
        SetHotkeyCaptureMode(false);
        break;
    case WM_KEYDOWN: case WM_SYSKEYDOWN: {
        UINT vk = (UINT)w;
        if (vk == VK_CONTROL || vk == VK_SHIFT || vk == VK_MENU || vk == VK_LWIN || vk == VK_RWIN) return 0;
        UINT mods = 0;
        if (GetKeyState(VK_CONTROL) & 0x8000) mods |= MOD_CONTROL;
        if (GetKeyState(VK_MENU) & 0x8000) mods |= MOD_ALT;
        if (GetKeyState(VK_SHIFT) & 0x8000) mods |= MOD_SHIFT;
        if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) mods |= MOD_WIN;
        if (VkName(vk).empty() || !mods) { MessageBeep(MB_OK); return 0; }
        if (IsWindowsDefaultHotkey(mods, vk)) {
            MessageBeep(MB_ICONWARNING);
            MessageBoxW(g_hSettings, L"윈도우 기본 단축키와 겹치는 키는 사용할 수 없습니다.", L"mkRun", MB_OK | MB_ICONWARNING);
            return 0;
        }
        SetWindowTextW(h, FormatHotkey(mods, vk).c_str());
        return 0;
    }
    case WM_CHAR: case WM_SYSCHAR: case WM_KEYUP: case WM_SYSKEYUP:
    case WM_CONTEXTMENU: case WM_PASTE: case WM_CUT:
        return 0;
    }
    return DefSubclassProc(h, m, w, l);
}

// --- 저장 -----------------------------------------------------------------------
static bool CheckCommands(const Node& n, std::wstring& bad) {
    for (auto& c : n.children) {
        if (!c.folder && Trim(c.command).empty()) { bad = c.name; return false; }
        if (!CheckCommands(c, bad)) return false;
    }
    return true;
}

static void DoSave() {
    CloseInlineEdit(true);
    std::wstring bad;
    if (!CheckCommands(g_work.root, bad)) {
        MessageBoxW(g_hSettings, (L"명령이 비어 있는 핫키 항목이 있습니다: " + bad).c_str(), L"mkRun", MB_OK | MB_ICONWARNING);
        return;
    }
    UINT m, v;
    std::wstring hk = Trim(GetText(g_edHotkey));
    if (!ParseHotkey(hk, m, v)) {
        MessageBoxW(g_hSettings, L"글로벌 핫키가 올바르지 않습니다. (예: Alt+Q)", L"mkRun", MB_OK | MB_ICONWARNING);
        return;
    }
    if (IsWindowsDefaultHotkey(m, v)) {
        MessageBoxW(g_hSettings, L"윈도우 기본 단축키와 겹치는 키는 사용할 수 없습니다.", L"mkRun", MB_OK | MB_ICONWARNING);
        return;
    }
    std::wstring mhk = Trim(GetText(g_edMenuHotkey));
    if (mhk.size() != 1) {
        MessageBoxW(g_hSettings, L"메뉴 핫키 호출 키는 1글자로 입력해 주세요. (기본: 0)", L"mkRun", MB_OK | MB_ICONWARNING);
        return;
    }
    g_work.runOnStartup = Button_GetCheck(g_chkStart) == BST_CHECKED;
    g_work.globalHotkey = FormatHotkey(m, v);
    g_work.menuHotkey = mhk;

    HideLauncher();  // g_cfg 를 교체하므로 런처 상태 초기화
    // 기존 Hotkey Hook 해제 후 신규 등록 (충돌 방지)
    UnregisterHotKey(g_hMsg, ID_HOTKEY);
    if (!BindGlobalHotkey(g_work.globalHotkey)) {
        MessageBoxW(g_hSettings, L"단축키를 등록할 수 없습니다. 다른 프로그램이 사용 중일 수 있습니다.\n이전 단축키를 유지합니다.",
                    L"mkRun", MB_OK | MB_ICONWARNING);
        g_work.globalHotkey = g_cfg.globalHotkey;
        if (!BindGlobalHotkey(g_work.globalHotkey)) BindGlobalHotkey(L"Alt+Q");
        SetWindowTextW(g_edHotkey, g_work.globalHotkey.c_str());
    }
    g_cfg = g_work;
    if (!SaveConfig())
        MessageBoxW(g_hSettings, L"설정 파일을 저장하지 못했습니다.", L"mkRun", MB_OK | MB_ICONERROR);
    SetStartup(g_cfg.runOnStartup);
}

static void DoExport() {
    wchar_t path[MAX_PATH] = L"settings.json";
    OPENFILENAMEW ofn = {sizeof(ofn)};
    ofn.hwndOwner = g_hSettings;
    ofn.lpstrFilter = L"JSON 파일 (*.json)\0*.json\0모든 파일 (*.*)\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"json";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return;
    if (!SaveConfigTo(g_cfg, path))
        MessageBoxW(g_hSettings, L"설정 파일을 내보내지 못했습니다.", L"mkRun", MB_OK | MB_ICONERROR);
    else
        MessageBoxW(g_hSettings, L"설정을 내보냈습니다.", L"mkRun", MB_OK | MB_ICONINFORMATION);
}

static void DoImport() {
    wchar_t path[MAX_PATH] = {};
    OPENFILENAMEW ofn = {sizeof(ofn)};
    ofn.hwndOwner = g_hSettings;
    ofn.lpstrFilter = L"JSON 파일 (*.json)\0*.json\0모든 파일 (*.*)\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return;

    Config loaded;
    if (!LoadConfigFrom(path, loaded)) {
        MessageBoxW(g_hSettings, L"설정 파일을 읽을 수 없습니다. 올바른 settings.json 파일인지 확인해 주세요.", L"mkRun", MB_OK | MB_ICONERROR);
        return;
    }

    HideLauncher();
    UnbindGlobalHotkey();
    if (!BindGlobalHotkey(loaded.globalHotkey)) {
        MessageBoxW(g_hSettings, L"가져온 설정의 글로벌 핫키를 등록할 수 없어 기본값(Alt+Q)을 시도합니다.", L"mkRun", MB_OK | MB_ICONWARNING);
        if (!BindGlobalHotkey(L"Alt+Q")) BindGlobalHotkey(g_cfg.globalHotkey);
        else loaded.globalHotkey = L"Alt+Q";
    }
    g_cfg = loaded;
    g_work = loaded;
    if (!SaveConfig())
        MessageBoxW(g_hSettings, L"프로그램 설정 파일을 덮어쓰지 못했습니다.", L"mkRun", MB_OK | MB_ICONERROR);
    SetStartup(g_cfg.runOnStartup);

    Button_SetCheck(g_chkStart, g_work.runOnStartup ? BST_CHECKED : BST_UNCHECKED);
    SetWindowTextW(g_edHotkey, g_work.globalHotkey.c_str());
    SetWindowTextW(g_edMenuHotkey, g_work.menuHotkey.c_str());
    Button_SetCheck(g_btnLangKo, g_work.lang == L"en" ? BST_UNCHECKED : BST_CHECKED);
    Button_SetCheck(g_btnLangEn, g_work.lang == L"en" ? BST_CHECKED : BST_UNCHECKED);
    InvalidateRect(g_btnBorderClr, nullptr, TRUE);
    InvalidateRect(g_btnBgClr, nullptr, TRUE);
    RefreshHot(L"");
    RefreshKw(-1);
    MessageBoxW(g_hSettings, WTr(L"설정을 가져왔습니다.", L"Settings imported."), L"mkRun", MB_OK | MB_ICONINFORMATION);
}

static void SetLanguage(const wchar_t* lang) {
    if (g_work.lang == lang) return;
    g_work.lang = lang;
    g_cfg.lang = lang;
    g_keepWork = true;
    DestroyWindow(g_hSettings);
    OpenSettings(2);
    g_keepWork = false;
}

static void PickColor(COLORREF& col, HWND btn) {
    static COLORREF cust[16] = {};
    CHOOSECOLORW cc = {sizeof(cc)};
    cc.hwndOwner = g_hSettings;
    cc.rgbResult = col;
    cc.lpCustColors = cust;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (ChooseColorW(&cc)) {
        col = cc.rgbResult;
        InvalidateRect(btn, nullptr, TRUE);
    }
}

// --- 창 생성 --------------------------------------------------------------------
static void BuildSettingsUI() {
    // 탭: 메뉴 핫키 / 키워드 / 프로그램 / 도움말
    g_hTab = Mk(-1, WC_TABCONTROLW, L"", WS_VISIBLE | WS_TABSTOP, 8, 8, 534, 26, IDC_TAB);
    const wchar_t* names[4] = {
        WTr(L"메뉴 핫키", L"Menu Hotkeys"),
        WTr(L"키워드", L"Keywords"),
        WTr(L"프로그램", L"Program"),
        WTr(L"도움말", L"Help")
    };
    for (int i = 0; i < 4; i++) {
        TCITEMW ti = {};
        ti.mask = TCIF_TEXT;
        ti.pszText = const_cast<wchar_t*>(names[i]);
        TabCtrl_InsertItem(g_hTab, i, &ti);
    }
    const DWORD LVST = LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_BORDER | WS_TABSTOP;
    const DWORD EDST = ES_AUTOHSCROLL | WS_TABSTOP;
    const DWORD BTN = BS_PUSHBUTTON | WS_TABSTOP;

    // ---- 페이지 0: 메뉴 핫키 ----
    g_lvHot = Mk(0, WC_LISTVIEWW, L"", LVST | LVS_SHAREIMAGELISTS, 18, 44, 514, 340, IDC_LVHOT);
    ListView_SetExtendedListViewStyle(g_lvHot, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    g_imlHot = ImageList_Create(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), ILC_COLOR32 | ILC_MASK, 16, 16);
    ListView_SetImageList(g_lvHot, g_imlHot, LVSIL_SMALL);
    SetWindowSubclass(g_lvHot, LvScrollProc, 4, 0);
    AddCol(g_lvHot, 0, WTr(L"항목", L"Item"), 230);
    AddCol(g_lvHot, 1, WTr(L"핫키", L"Key"), 54);
    AddCol(g_lvHot, 2, WTr(L"명령", L"Command"), 210);
    Mk(0, L"STATIC", WTr(L"이름", L"Name"), SS_LEFT, 18, 400, 36, 20, 0);
    g_edHName = Mk(0, L"EDIT", L"", EDST, 56, 396, 120, 24, IDC_EDHNAME, WS_EX_CLIENTEDGE);
    Mk(0, L"STATIC", WTr(L"핫키", L"Key"), SS_LEFT, 184, 400, 32, 20, 0);
    g_edHKey = Mk(0, L"EDIT", L"", EDST | ES_CENTER, 218, 396, 36, 24, IDC_EDHKEY, WS_EX_CLIENTEDGE);
    SendMessageW(g_edHKey, EM_LIMITTEXT, 1, 0);
    SetWindowSubclass(g_edHKey, SingleKeyEditProc, 6, 0);
    Mk(0, L"STATIC", WTr(L"명령", L"Cmd"), SS_LEFT, 262, 400, 34, 20, 0);
    g_edHCmd = Mk(0, L"EDIT", L"", EDST, 298, 396, 234, 24, IDC_EDHCMD, WS_EX_CLIENTEDGE);
    Mk(0, L"BUTTON", WTr(L"폴더 추가", L"Add Folder"), BTN, 18, 432, 90, 28, IDC_ADDFOLDER);
    Mk(0, L"BUTTON", WTr(L"항목 추가", L"Add Item"), BTN, 114, 432, 90, 28, IDC_ADDITEM);
    Mk(0, L"BUTTON", WTr(L"수정 적용", L"Apply"), BTN, 210, 432, 90, 28, IDC_APPLYH);
    Mk(0, L"BUTTON", WTr(L"삭제", L"Delete"), BTN, 306, 432, 90, 28, IDC_DELH);

    // ---- 페이지 1: 키워드 ----
    g_lvKw = Mk(1, WC_LISTVIEWW, L"", LVST, 18, 44, 514, 340, IDC_LVKW);
    ListView_SetExtendedListViewStyle(g_lvKw, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    SetWindowSubclass(g_lvKw, LvScrollProc, 5, 0);
    AddCol(g_lvKw, 0, WTr(L"키워드", L"Keyword"), 150);
    AddCol(g_lvKw, 1, WTr(L"명령", L"Command"), 344);
    g_edKwK = Mk(1, L"EDIT", L"", EDST, 18, 396, 150, 24, IDC_EDKWK, WS_EX_CLIENTEDGE);
    g_edKwC = Mk(1, L"EDIT", L"", EDST, 176, 396, 356, 24, IDC_EDKWC, WS_EX_CLIENTEDGE);
    Mk(1, L"BUTTON", WTr(L"추가", L"Add"), BTN, 18, 432, 90, 28, IDC_KWADD);
    Mk(1, L"BUTTON", WTr(L"수정", L"Modify"), BTN, 114, 432, 90, 28, IDC_KWMOD);
    Mk(1, L"BUTTON", WTr(L"삭제", L"Delete"), BTN, 210, 432, 90, 28, IDC_KWDEL);

    // ---- 페이지 2: 프로그램 (좌: 카테고리 / 우: 세부 설정) ----
    g_lbCat = Mk(2, L"LISTBOX", L"", LBS_NOTIFY | WS_BORDER | WS_TABSTOP, 18, 44, 130, 344, IDC_LBCAT);
    SendMessageW(g_lbCat, LB_ADDSTRING, 0, (LPARAM)WTr(L"일반", L"General"));
    SendMessageW(g_lbCat, LB_ADDSTRING, 0, (LPARAM)WTr(L"외형", L"Appearance"));
    SendMessageW(g_lbCat, LB_SETCURSEL, 0, 0);

    // 카테고리 0: 일반
    HWND grp = MkCat(g_catGeneral, L"BUTTON", WTr(L"실행", L"Startup & Hotkeys"), BS_GROUPBOX, 158, 44, 374, 160, 0);
    g_chkStart = MkCat(g_catGeneral, L"BUTTON", WTr(L"윈도우 시작 시 실행", L"Run on Windows startup"), BS_AUTOCHECKBOX | WS_TABSTOP, 174, 76, 330, 24, IDC_CHKSTART);
    MkCat(g_catGeneral, L"STATIC", WTr(L"글로벌 핫키", L"Global Hotkey"), SS_LEFT, 174, 118, 150, 20, 0);
    g_edHotkey = MkCat(g_catGeneral, L"EDIT", L"", ES_AUTOHSCROLL | ES_CENTER | WS_TABSTOP, 334, 114, 160, 24, IDC_EDHOTKEY, WS_EX_CLIENTEDGE);
    SetWindowSubclass(g_edHotkey, HkEditProc, 2, 0);
    MkCat(g_catGeneral, L"STATIC", WTr(L"메뉴 핫키 호출 키", L"Menu Trigger Key"), SS_LEFT, 174, 154, 150, 20, 0);
    g_edMenuHotkey = MkCat(g_catGeneral, L"EDIT", L"", ES_AUTOHSCROLL | ES_CENTER | WS_TABSTOP, 334, 150, 160, 24, IDC_EDMENUHOTKEY, WS_EX_CLIENTEDGE);
    SendMessageW(g_edMenuHotkey, EM_LIMITTEXT, 1, 0);
    SetWindowSubclass(g_edMenuHotkey, SingleKeyEditProc, 7, 0);
    SetWindowPos(grp, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    HWND grpData = MkCat(g_catGeneral, L"BUTTON", WTr(L"설정 데이터", L"Settings Data"), BS_GROUPBOX, 158, 216, 374, 80, 0);
    MkCat(g_catGeneral, L"BUTTON", WTr(L"내보내기", L"Export"), BTN, 174, 246, 110, 28, IDC_EXPORT);
    MkCat(g_catGeneral, L"BUTTON", WTr(L"가져오기", L"Import"), BTN, 294, 246, 110, 28, IDC_IMPORT);
    SetWindowPos(grpData, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    HWND grpLang = MkCat(g_catGeneral, L"BUTTON", WTr(L"언어", L"Language"), BS_GROUPBOX, 158, 308, 374, 80, 0);
    g_btnLangKo = MkCat(g_catGeneral, L"BUTTON", L"한국어", BS_AUTORADIOBUTTON | BS_PUSHLIKE | WS_GROUP | WS_TABSTOP, 174, 338, 110, 28, IDC_LANGKO);
    g_btnLangEn = MkCat(g_catGeneral, L"BUTTON", L"English", BS_AUTORADIOBUTTON | BS_PUSHLIKE | WS_TABSTOP, 294, 338, 110, 28, IDC_LANGEN);
    SetWindowPos(grpLang, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    // 카테고리 1: 외형
    HWND grpApp = MkCat(g_catAppearance, L"BUTTON", WTr(L"색 변경", L"Colors"), BS_GROUPBOX, 158, 44, 374, 120, 0);
    MkCat(g_catAppearance, L"STATIC", WTr(L"외곽선", L"Border Color"), SS_LEFT, 174, 80, 140, 20, 0);
    g_btnBorderClr = MkCat(g_catAppearance, L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP, 334, 76, 80, 24, IDC_BTNBORDERCLR);
    MkCat(g_catAppearance, L"STATIC", WTr(L"배경", L"Background Color"), SS_LEFT, 174, 118, 140, 20, 0);
    g_btnBgClr = MkCat(g_catAppearance, L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP, 334, 114, 80, 24, IDC_BTNBGCLR);
    SetWindowPos(grpApp, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    // ---- 페이지 3: 도움말 ----
    const wchar_t* helpKo =
        L"[기본 사용법]\r\n"
        L"• 글로벌 핫키(기본: Alt+Q)를 누르면 런처 입력창이 열립니다.\r\n"
        L"• 키워드를 입력하고 Enter를 누르면 연결된 프로그램/URL이 실행됩니다.\r\n"
        L"• 메뉴 핫키 호출 키(기본: 0)를 입력하면 계층형 메뉴가 열립니다.\r\n"
        L"• ESC 또는 최상위에서 ← / Backspace를 누르면 메뉴가 닫힙니다.\r\n"
        L"• 트레이 아이콘 우클릭으로 설정 창 열기 및 종료가 가능합니다.\r\n\r\n"
        L"[메뉴 핫키]\r\n"
        L"• 선택한 폴더의 하위에 추가됩니다. (선택 없음 → 최상위, 항목 선택 → 같은 레벨)\r\n"
        L"• 핫키를 비우고 추가하면 1~9, 0, a~z 순으로 자동 배정됩니다.\r\n"
        L"• 셀 더블클릭 또는 F2로 직접 수정, Delete 키로 삭제, 드래그로 순서/폴더 이동이 가능합니다.\r\n"
        L"• 폴더 이름 1열 더블클릭 또는 ← / → 키로 폴더를 접고 펼칠 수 있습니다.\r\n\r\n"
        L"[키워드]\r\n"
        L"• 키워드의 첫 글자는 숫자로 시작할 수 없습니다.\r\n"
        L"• 셀 더블클릭/F2로 수정, Delete 키로 삭제, 드래그로 순서 변경이 가능합니다.\r\n"
        L"• 명령 예: chrome.exe, C:\\Tools\\app.exe -arg, https://example.com";
    const wchar_t* helpEn =
        L"[Basic Usage]\r\n"
        L"• Press the Global Hotkey (default: Alt+Q) to open the launcher.\r\n"
        L"• Type a keyword and press Enter to launch the command or URL.\r\n"
        L"• Type the Menu Trigger Key (default: 0) to open the cascading hotkey menu.\r\n"
        L"• Press ESC or Left/Backspace at the root menu to close and reset.\r\n"
        L"• Right-click the tray icon to open Settings or exit mkRun.\r\n\r\n"
        L"[Menu Hotkeys]\r\n"
        L"• New items are added under the selected folder (or at the same level for items).\r\n"
        L"• Leave Key empty to auto-assign in order: 1-9, 0, then a-z.\r\n"
        L"• Double-click or press F2 to edit inline, Delete to remove, drag to reorder/move.\r\n"
        L"• Double-click a folder name or press Left/Right to collapse/expand.\r\n\r\n"
        L"[Keywords]\r\n"
        L"• Keywords cannot start with a digit.\r\n"
        L"• Double-click or press F2 to edit inline, Delete to remove, drag to reorder.\r\n"
        L"• Examples: chrome.exe, C:\\Tools\\app.exe -arg, https://example.com";
    Mk(3, L"EDIT", WTr(helpKo, helpEn), WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
       18, 44, 514, 424, IDC_EDHELP);

    // ---- 공통 하단 버튼 ----
    Mk(-1, L"BUTTON", WTr(L"저장", L"Save"), BTN | WS_VISIBLE, 350, 482, 90, 28, IDC_SAVE);
    Mk(-1, L"BUTTON", WTr(L"닫기", L"Close"), BTN | WS_VISIBLE, 452, 482, 90, 28, IDC_CLOSE);

    // 글꼴 적용
    EnumChildWindows(g_hSettings, [](HWND c, LPARAM) -> BOOL { SendMessageW(c, WM_SETFONT, (WPARAM)g_fontUI, TRUE); return TRUE; }, 0);

    // 값 채우기
    Button_SetCheck(g_chkStart, g_work.runOnStartup ? BST_CHECKED : BST_UNCHECKED);
    SetWindowTextW(g_edHotkey, g_work.globalHotkey.c_str());
    SetWindowTextW(g_edMenuHotkey, g_work.menuHotkey.c_str());
    Button_SetCheck(g_btnLangKo, g_work.lang == L"en" ? BST_UNCHECKED : BST_CHECKED);
    Button_SetCheck(g_btnLangEn, g_work.lang == L"en" ? BST_CHECKED : BST_UNCHECKED);
    RefreshHot(L"");
    RefreshKw(-1);
}

static LRESULT CALLBACK SettingsProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE:
        g_hSettings = h;
        BuildSettingsUI();
        return 0;
    case WM_COMMAND:
        switch (LOWORD(w)) {
        case IDC_SAVE: DoSave(); break;
        case IDC_CLOSE: DestroyWindow(h); break;
        case IDC_ADDFOLDER: AddHot(true); break;
        case IDC_ADDITEM: AddHot(false); break;
        case IDC_APPLYH: ApplyHot(); break;
        case IDC_DELH: DelHot(); break;
        case IDC_KWADD: AddKw(); break;
        case IDC_KWMOD: ModKw(); break;
        case IDC_KWDEL: DelKw(); break;
        case IDC_EXPORT: DoExport(); break;
        case IDC_IMPORT: DoImport(); break;
        case IDC_BTNBORDERCLR: PickColor(g_work.borderColor, g_btnBorderClr); break;
        case IDC_BTNBGCLR: PickColor(g_work.bgColor, g_btnBgClr); break;
        case IDC_LANGKO: SetLanguage(L"ko"); break;
        case IDC_LANGEN: SetLanguage(L"en"); break;
        case IDC_LBCAT:
            if (HIWORD(w) == LBN_SELCHANGE) SwitchCat((int)SendMessageW(g_lbCat, LB_GETCURSEL, 0, 0));
            break;
        }
        return 0;
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* ds = (DRAWITEMSTRUCT*)l;
        if (ds->CtlID == IDC_BTNBORDERCLR || ds->CtlID == IDC_BTNBGCLR) {
            COLORREF col = (ds->CtlID == IDC_BTNBORDERCLR) ? g_work.borderColor : g_work.bgColor;
            HBRUSH br = CreateSolidBrush(col);
            FillRect(ds->hDC, &ds->rcItem, br);
            DeleteObject(br);
            HBRUSH fb = CreateSolidBrush(RGB(120, 120, 120));
            FrameRect(ds->hDC, &ds->rcItem, fb);
            DeleteObject(fb);
            return TRUE;
        }
        break;
    }
    case WM_CTLCOLOREDIT: {
        HWND ed = (HWND)l;
        if ((g_flashCount % 2 == 1) && ((ed == g_edHName && g_flashName) || (ed == g_edHCmd && g_flashCmd))) {
            SetBkColor((HDC)w, RGB(255, 190, 190));
            return (LRESULT)g_flashBrush;
        }
        break;
    }
    case WM_TIMER:
        if (w == IDT_FLASH) {
            g_flashCount--;
            if (g_flashName) InvalidateRect(g_edHName, nullptr, TRUE);
            if (g_flashCmd) InvalidateRect(g_edHCmd, nullptr, TRUE);
            if (g_flashCount <= 0) {
                KillTimer(h, IDT_FLASH);
                g_flashName = g_flashCmd = false;
            }
            return 0;
        }
        break;
    case WM_NOTIFY: {
        NMHDR* nh = (NMHDR*)l;
        if (nh->idFrom == IDC_TAB && nh->code == TCN_SELCHANGE) SwitchTab(TabCtrl_GetCurSel(g_hTab));
        else if (nh->idFrom == IDC_LVHOT) {
            if (nh->code == LVN_ITEMCHANGED && !g_refreshing) LoadHotEdits();
            else if (nh->code == NM_DBLCLK) {
                NMITEMACTIVATE* nm = (NMITEMACTIVATE*)l;
                if (nm->iItem >= 0 && nm->iItem < (int)g_hrows.size()) {
                    if (nm->iSubItem == 0 && g_hrows[nm->iItem].node->folder) ToggleHot(nm->iItem, -1);
                    else StartInlineEdit(g_lvHot, nm->iItem, nm->iSubItem);
                }
            } else if (nh->code == LVN_BEGINDRAG) {
                NMLISTVIEW* nlv = (NMLISTVIEW*)l;
                if (nlv->iItem >= 0 && nlv->iItem < (int)g_hrows.size()) {
                    CloseInlineEdit(true);
                    g_dragLv = g_lvHot;
                    g_dragRow = nlv->iItem;
                    g_dropRow = -1;
                    SetCapture(g_lvHot);
                }
            } else if (nh->code == LVN_KEYDOWN) {
                WORD vk = ((NMLVKEYDOWN*)l)->wVKey;
                if (vk == VK_RIGHT) ToggleHot(HotSel(), 1);
                else if (vk == VK_LEFT) ToggleHot(HotSel(), 0);
                else if (vk == VK_F2) StartInlineEdit(g_lvHot, HotSel(), 0);
                else if (vk == VK_DELETE) DelHot();
            }
        } else if (nh->idFrom == IDC_LVKW) {
            if (nh->code == LVN_ITEMCHANGED && !g_refreshing) LoadKwEdits();
            else if (nh->code == NM_DBLCLK) {
                NMITEMACTIVATE* nm = (NMITEMACTIVATE*)l;
                if (nm->iItem >= 0) StartInlineEdit(g_lvKw, nm->iItem, nm->iSubItem);
            } else if (nh->code == LVN_BEGINDRAG) {
                NMLISTVIEW* nlv = (NMLISTVIEW*)l;
                if (nlv->iItem >= 0 && nlv->iItem < (int)g_work.keywords.size()) {
                    CloseInlineEdit(true);
                    g_dragLv = g_lvKw;
                    g_dragRow = nlv->iItem;
                    g_dropRow = -1;
                    SetCapture(g_lvKw);
                }
            } else if (nh->code == LVN_KEYDOWN) {
                WORD vk = ((NMLVKEYDOWN*)l)->wVKey;
                if (vk == VK_F2) StartInlineEdit(g_lvKw, KwSel(), 0);
                else if (vk == VK_DELETE) DelKw();
            }
        }
        return 0;
    }
    case WM_CLOSE:
        CloseInlineEdit(true);
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        EndDrag(false);
        KillTimer(h, IDT_FLASH);
        g_flashName = g_flashCmd = false; g_flashCount = 0;
        if (g_flashBrush) { DeleteObject(g_flashBrush); g_flashBrush = nullptr; }
        g_edInline = nullptr; g_inlineLv = nullptr; g_inlineRow = -1; g_inlineCol = -1; g_inlineBusy = false;
        SetHotkeyCaptureMode(false);
        if (g_imlHot) { ImageList_Destroy(g_imlHot); g_imlHot = nullptr; }
        g_hSettings = nullptr;
        for (auto& p : g_pages) p.clear();
        g_catGeneral.clear();
        g_catAppearance.clear();
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

void OpenSettings(int tab) {
    HideLauncher();
    if (!g_hSettings) {
        if (!g_keepWork) g_work = g_cfg;
        POINT pt; GetCursorPos(&pt);
        MONITORINFO mi = {sizeof(mi)};
        GetMonitorInfoW(MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY), &mi);
        DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        RECT r = {0, 0, S(550), S(522)};
        AdjustWindowRectEx(&r, style, FALSE, 0);
        int w = r.right - r.left, h = r.bottom - r.top;
        int x = (mi.rcWork.left + mi.rcWork.right - w) / 2, y = (mi.rcWork.top + mi.rcWork.bottom - h) / 2;
        CreateWindowExW(0, L"mkRunSettings", WTr(L"mkRun 설정", L"mkRun Settings"), style, x, y, w, h, nullptr, nullptr, g_hInst, nullptr);
    }
    if (!g_hSettings) return;
    TabCtrl_SetCurSel(g_hTab, tab);
    SwitchTab(tab);
    ShowWindow(g_hSettings, IsIconic(g_hSettings) ? SW_RESTORE : SW_SHOW);
    SetForegroundWindow(g_hSettings);
    if (tab == 1) SetFocus(g_edKwK);
}


// ----------------------------------------------------------------------------
//  초기화: 설정 창 클래스 등록
// ----------------------------------------------------------------------------
bool Settings_Init(HINSTANCE hInst) {
    WNDCLASSEXW wc = {sizeof(wc)};
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = g_icoBig;
    wc.hIconSm = g_icoSmall;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpfnWndProc = SettingsProc;
    wc.lpszClassName = L"mkRunSettings";
    return RegisterClassExW(&wc) != 0;
}
