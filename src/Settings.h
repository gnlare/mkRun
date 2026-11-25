// Settings.h - 설정 창 (탭: 숫자 핫키 / 키워드 / 프로그램)
#pragma once
#include "Common.h"

bool Settings_Init(HINSTANCE hInst);   // 윈도우 클래스 등록
void OpenSettings(int tab);            // 0: 숫자 핫키, 1: 키워드, 2: 프로그램
