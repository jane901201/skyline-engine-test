#include "system.h"
#include "graphics_engine.h"
#include "my_engine.h"

#include <stdexcept>


HWND g_hWnd = nullptr;
WindowMessageHandler g_windowMessageHandler = nullptr;

LRESULT CALLBACK MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (g_windowMessageHandler != nullptr)
    {
        const LRESULT result = g_windowMessageHandler(hWnd, msg, wParam, lParam);
        if (result != 0)
            return result;
    }

    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_SIZE:
        if (g_graphicsEngine != nullptr && wParam != SIZE_MINIMIZED)
        {
            g_graphicsEngine->resize(
                static_cast<UINT>(LOWORD(lParam)),
                static_cast<UINT>(HIWORD(lParam)));
        }
        break;
    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }

    return 0;
}


void initWindow(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPWSTR lpCmdLine, int nCmdShow, const TCHAR* appName)
{
    WNDCLASSEX wc =
    {
        sizeof(WNDCLASSEX),     // 結構體的大小。
        CS_CLASSDC,             // 視窗樣式。
                                // 可以在這裡設定捲動條等功能，
                                // 但遊戲通常不需要，因此使用 CS_CLASSDC 即可。
        MsgProc,                // 視窗訊息處理函式，後面會再說明。
        0,                      // 設為 0 即可。
        0,                      // 設為 0 即可。
        GetModuleHandle(NULL),  // 包含此視窗類別處理函式的執行個體控制代碼。
                                // 通常不需要特別處理。
        NULL,                   // 圖示控制代碼。
                                // 若要更換視窗圖示，可修改此處，暫時設為 NULL 即可。
        NULL,                   // 滑鼠游標控制代碼。
                                // 設為 NULL 時使用預設游標。
        NULL,                   // 視窗背景色。
                                // 設為 NULL 時使用預設值。
        NULL,                   // 選單名稱，設為 NULL 即可。
        appName,                // 指定給視窗類別的名稱。
        NULL                    // 設為 NULL 即可。
    };

    RegisterClassEx(&wc);

    g_hWnd = CreateWindow(
        appName,                // 要使用的視窗類別名稱。
                                // 必須與前面建立的視窗類別名稱相同。
        appName,                // 視窗標題。
                                // 可以與視窗類別名稱不同。
        WS_OVERLAPPEDWINDOW,    // 視窗樣式。
                                // 一般遊戲視窗使用 WS_OVERLAPPEDWINDOW 即可。
        0,                      // 視窗初始位置的 X 座標。
        0,                      // 視窗初始位置的 Y 座標。
        FRAME_BUFFER_W,         // 視窗寬度。
        FRAME_BUFFER_H,         // 視窗高度。
        NULL,                   // 父視窗。
                                // 一般遊戲視窗設為 NULL 即可。
        NULL,                   // 選單，目前設為 NULL 即可。
        hInstance,              // 應用程式的執行個體控制代碼。
        NULL                    // 傳遞給視窗的額外建立資料，目前不使用。
    );

    ShowWindow(g_hWnd, nCmdShow);
}

bool dispatchWindowMessage()
{
    MSG msg = { 0 };

    while (WM_QUIT != msg.message) {

        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            break;
        }
    }
    return msg.message != WM_QUIT;
}

void setWindowMessageHandler(WindowMessageHandler handler) noexcept
{
    g_windowMessageHandler = handler;
}
