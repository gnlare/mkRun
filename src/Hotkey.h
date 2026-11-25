// Hotkey.h - "Alt+Q" 형식 문자열 <-> (modifiers, vk), 전역 단축키 등록
#pragma once
#include "Common.h"

std::wstring VkName(UINT vk);
UINT NameVk(const std::wstring& t);
bool ParseHotkey(const std::wstring& s, UINT& mods, UINT& vk);
std::wstring FormatHotkey(UINT mods, UINT vk);
bool IsWindowsDefaultHotkey(UINT mods, UINT vk);
bool BindGlobalHotkey(const std::wstring& hk);   // g_hMsg 에 RegisterHotKey + LL 훅
void UnbindGlobalHotkey();                       // 전역 단축키/훅 해제
void SetHotkeyCaptureMode(bool on);              // 설정 창 단축키 입력 중 LL 훅 일시 무시
