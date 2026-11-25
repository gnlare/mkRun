// Config.h - 설정 데이터 구조와 로드/저장
#pragma once
#include "Common.h"

struct Node {
    std::wstring id;
    bool folder = false;
    std::wstring hotkey, name, command;
    std::vector<Node> children;
    bool expanded = true;  // 런타임 전용(설정 창 트리 표시용)
};
struct Keyword { std::wstring keyword, command; };
struct Config {
    bool runOnStartup = false;
    std::wstring globalHotkey = L"Alt+Q";
    std::wstring menuHotkey = L"0";
    std::wstring lang = L"ko";
    COLORREF borderColor = RGB(255, 255, 255);
    COLORREF bgColor = RGB(0, 0, 0);
    std::vector<Keyword> keywords;
    Node root;  // 핫키 트리의 가상 루트 (root.children 이 최상위)
};

inline const wchar_t* Tr(const wchar_t* ko, const wchar_t* en) {
    extern Config g_cfg;
    return g_cfg.lang == L"en" ? en : ko;
}


extern Config g_cfg;    // 실제 적용 중인 설정
extern Config g_work;   // 설정 창 작업 사본

std::wstring NewId();
void AssignIds(Node& n);
std::wstring CfgPath(bool mkdir);   // %APPDATA%/mkRun/settings.json
void LoadConfig();
bool SaveConfig();
bool LoadConfigFrom(const std::wstring& path, Config& out);
bool SaveConfigTo(const Config& cfg, const std::wstring& path);
