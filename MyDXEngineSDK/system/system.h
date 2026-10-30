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
