#pragma once
#include "skyline_api.h"

#include <Windows.h>

class GraphicsEngine;
extern SKYLINE_API HWND g_hWnd;

using WindowMessageHandler = LRESULT (*)(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

SKYLINE_API void initWindow(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPWSTR lpCmdLine, int nCmdShow, const TCHAR* appName);

SKYLINE_API bool dispatchWindowMessage();

SKYLINE_API void setWindowMessageHandler(WindowMessageHandler handler) noexcept;

// 取出最近一次尚未處理的滑鼠左鍵點擊（Client 區像素座標）；沒有新的點擊時回傳 false
SKYLINE_API bool consumeMouseClick(int& x, int& y) noexcept;
