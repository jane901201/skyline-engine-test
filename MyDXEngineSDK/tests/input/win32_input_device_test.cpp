#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "backends/win32/win32_input_device.h"

#include <stdexcept>

namespace
{
using namespace Skyline;
using Input::Key;
using Input::MouseButton;

LRESULT CALLBACK testWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProcW(window, message, wParam, lParam);
}

// 建立一個不顯示的測試視窗。SendMessage 在同一執行緒會直接呼叫視窗處理函式，所以測試是同步且可重現的
class TestWindow
{
  public:
    TestWindow()
    {
        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = testWindowProc;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.lpszClassName = L"SkylineInputTestWindow";
        // 重複註冊（ERROR_CLASS_ALREADY_EXISTS）時沿用既有的類別
        RegisterClassExW(&windowClass);

        m_window = CreateWindowExW(0, windowClass.lpszClassName, L"SkylineInputTest", WS_OVERLAPPEDWINDOW, 0, 0, 200, 200,
                                   nullptr, nullptr, windowClass.hInstance, nullptr);
        REQUIRE(m_window != nullptr);
    }
    ~TestWindow()
    {
        if (m_window != nullptr)
            DestroyWindow(m_window);
    }
    TestWindow(const TestWindow&) = delete;
    TestWindow& operator=(const TestWindow&) = delete;

    HWND handle() const
    {
        return m_window;
    }
    // 模擬視窗被外部銷毀，之後不需要再由此物件銷毀
    void destroy()
    {
        DestroyWindow(m_window);
        m_window = nullptr;
    }

  private:
    HWND m_window = nullptr;
};
} // namespace

TEST_CASE("Win32 input device requires a window", "[input][win32]")
{
    CHECK_THROWS_AS(Win32::createInputDevice(nullptr), std::invalid_argument);
}

TEST_CASE("Win32 input device translates keyboard messages", "[input][win32]")
{
    TestWindow window;
    const auto device = Win32::createInputDevice(window.handle());

    SendMessageW(window.handle(), WM_KEYDOWN, 'W', 0);
    SendMessageW(window.handle(), WM_KEYDOWN, VK_LEFT, 0);
    SendMessageW(window.handle(), WM_KEYDOWN, VK_F5, 0);
    SendMessageW(window.handle(), WM_KEYDOWN, '7', 0);
    SendMessageW(window.handle(), WM_KEYDOWN, VK_SHIFT, 0);
    device->update();

    const auto& state = device->state();
    CHECK(state.isDown(Key::W));
    CHECK(state.wasPressed(Key::W));
    CHECK(state.isDown(Key::Left));
    CHECK(state.isDown(Key::F5));
    CHECK(state.isDown(Key::Num7));
    CHECK(state.isDown(Key::Shift));
    CHECK_FALSE(state.isDown(Key::A));

    SendMessageW(window.handle(), WM_KEYUP, 'W', 0);
    device->update();
    CHECK_FALSE(state.isDown(Key::W));
    CHECK(state.wasReleased(Key::W));
    CHECK(state.isDown(Key::Left));
}

TEST_CASE("Win32 input device treats system key messages like normal keys", "[input][win32]")
{
    TestWindow window;
    const auto device = Win32::createInputDevice(window.handle());

    SendMessageW(window.handle(), WM_SYSKEYDOWN, VK_MENU, 0);
    device->update();
    CHECK(device->state().isDown(Key::Alt));

    SendMessageW(window.handle(), WM_SYSKEYUP, VK_MENU, 0);
    device->update();
    CHECK_FALSE(device->state().isDown(Key::Alt));
}

TEST_CASE("Win32 input device ignores unknown keys", "[input][win32]")
{
    TestWindow window;
    const auto device = Win32::createInputDevice(window.handle());

    SendMessageW(window.handle(), WM_KEYDOWN, VK_OEM_3, 0);
    device->update();
    CHECK_FALSE(device->state().isDown(Key::Unknown));
}

TEST_CASE("Win32 input device translates mouse buttons and position", "[input][win32]")
{
    TestWindow window;
    const auto device = Win32::createInputDevice(window.handle());

    SendMessageW(window.handle(), WM_MOUSEMOVE, 0, MAKELPARAM(30, 40));
    SendMessageW(window.handle(), WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(30, 40));
    SendMessageW(window.handle(), WM_RBUTTONDOWN, MK_RBUTTON, MAKELPARAM(30, 40));
    SendMessageW(window.handle(), WM_XBUTTONDOWN, MAKEWPARAM(0, XBUTTON2), MAKELPARAM(30, 40));
    device->update();

    const auto& state = device->state();
    CHECK(state.isDown(MouseButton::Left));
    CHECK(state.wasPressed(MouseButton::Left));
    CHECK(state.isDown(MouseButton::Right));
    CHECK(state.isDown(MouseButton::X2));
    CHECK_FALSE(state.isDown(MouseButton::Middle));
    CHECK_FALSE(state.isDown(MouseButton::X1));
    CHECK(state.mousePosition().x == 30.0f);
    CHECK(state.mousePosition().y == 40.0f);

    SendMessageW(window.handle(), WM_LBUTTONUP, 0, MAKELPARAM(30, 40));
    device->update();
    CHECK_FALSE(state.isDown(MouseButton::Left));
    CHECK(state.wasReleased(MouseButton::Left));
}

TEST_CASE("Win32 input device supports negative mouse coordinates", "[input][win32]")
{
    TestWindow window;
    const auto device = Win32::createInputDevice(window.handle());

    // 按住滑鼠拖曳到視窗左上方之外時，座標為負值
    SendMessageW(window.handle(), WM_MOUSEMOVE, MK_LBUTTON,
                 MAKELPARAM(static_cast<WORD>(static_cast<short>(-12)), static_cast<WORD>(static_cast<short>(-3))));
    device->update();
    CHECK(device->state().mousePosition().x == -12.0f);
    CHECK(device->state().mousePosition().y == -3.0f);
}

TEST_CASE("Win32 input device converts wheel delta to notches", "[input][win32]")
{
    TestWindow window;
    const auto device = Win32::createInputDevice(window.handle());

    SendMessageW(window.handle(), WM_MOUSEWHEEL, MAKEWPARAM(0, WHEEL_DELTA), 0);
    SendMessageW(window.handle(), WM_MOUSEWHEEL, MAKEWPARAM(0, static_cast<WORD>(-WHEEL_DELTA / 2)), 0);
    device->update();
    CHECK_THAT(device->state().mouseWheel(), Catch::Matchers::WithinAbs(0.5, 1e-6));
}

TEST_CASE("Win32 input device releases everything when the window loses focus", "[input][win32]")
{
    TestWindow window;
    const auto device = Win32::createInputDevice(window.handle());

    SendMessageW(window.handle(), WM_KEYDOWN, 'W', 0);
    SendMessageW(window.handle(), WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(1, 1));
    device->update();
    REQUIRE(device->state().isDown(Key::W));

    SendMessageW(window.handle(), WM_KILLFOCUS, 0, 0);
    device->update();
    CHECK_FALSE(device->state().isDown(Key::W));
    CHECK_FALSE(device->state().isDown(MouseButton::Left));
    CHECK(device->state().wasReleased(Key::W));
}

TEST_CASE("Win32 input device does not intercept messages", "[input][win32]")
{
    TestWindow window;
    const auto device = Win32::createInputDevice(window.handle());

    // 訊息仍會交給視窗原本的處理函式：WM_GETTEXTLENGTH 由預設處理函式回傳標題長度
    const LRESULT length = SendMessageW(window.handle(), WM_GETTEXTLENGTH, 0, 0);
    CHECK(length == static_cast<LRESULT>(wcslen(L"SkylineInputTest")));
}

TEST_CASE("Win32 input device stops listening after it is destroyed", "[input][win32]")
{
    TestWindow window;
    {
        const auto device = Win32::createInputDevice(window.handle());
        SendMessageW(window.handle(), WM_KEYDOWN, 'W', 0);
    }
    // 裝置已解除掛接，後續訊息不應造成存取已釋放的物件
    SendMessageW(window.handle(), WM_KEYDOWN, 'W', 0);
    SUCCEED();
}

TEST_CASE("Win32 input device survives the window being destroyed first", "[input][win32]")
{
    TestWindow window;
    const auto device = Win32::createInputDevice(window.handle());
    SendMessageW(window.handle(), WM_KEYDOWN, 'W', 0);

    window.destroy();
    device->update();
    // 視窗銷毀後仍可讀取最後的狀態，且解構裝置時不會再操作已不存在的視窗
    CHECK(device->state().isDown(Key::W));
}
