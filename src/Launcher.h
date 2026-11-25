// Launcher.h - 메인 입력 창(200x30) + 드롭다운 리스트
#pragma once
#include "Common.h"

extern HWND g_hMain, g_hEdit, g_hList;
extern bool g_menuOpen;           // 컨텍스트 메뉴가 열려 있는 동안 포커스 아웃 숨김 방지

bool Launcher_Init(HINSTANCE hInst);   // 윈도우 클래스 등록 + 창 생성
void ShowLauncher();                   // 활성 모니터 중앙에 표시 + 포커스
void HideLauncher();
void ShowCtxMenu(HWND owner);          // main.cpp 에 구현 (트레이/메인 창 공용 우클릭 메뉴)
