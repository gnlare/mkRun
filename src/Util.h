// Util.h - 문자열 변환, 시작프로그램 등록, 명령 실행, 아이콘 생성
#pragma once
#include "Common.h"

std::wstring U8ToW(const std::string& s);
std::string WToU8(const std::wstring& w);
std::wstring Trim(const std::wstring& s);
std::wstring GetText(HWND h);
std::wstring ToLower(std::wstring s);

void SetStartup(bool on);                    // HKCU\...\Run 등록/해제
bool ParseCommandTarget(const std::wstring& cmdIn, std::wstring& file, std::wstring& args);
void RunCommand(const std::wstring& cmd);    // 파일/폴더/URL/인자 포함 명령 실행
HICON GetCommandIcon(const std::wstring& cmd, bool isFolder);  // 작은 아이콘 추출
HICON MakeIcon(int size);                    // "mk" 아이콘을 코드로 생성
