#include "win32_input_device.h"

#include <CommCtrl.h>

#include <stdexcept>

namespace Skyline::Win32
{
namespace
{
using Input::Key;
using Input::MouseButton;

// 子類別化使用的識別碼，同一個視窗上不可重複
constexpr UINT_PTR kSubclassId = 1;

// HID 使用頁面與用途：一般桌面（0x01）的滑鼠（0x02）
constexpr USHORT kHidUsagePageGeneric = 0x01;
constexpr USHORT kHidUsageMouse = 0x02;

// 將 Windows 虛擬鍵碼轉成與平台無關的 Key，不認得的按鍵回傳 Key::Unknown
Key translateVirtualKey(WPARAM virtualKey) noexcept
{
    if (virtualKey >= 'A' && virtualKey <= 'Z')
        return static_cast<Key>(static_cast<std::size_t>(Key::A) + (virtualKey - 'A'));
    if (virtualKey >= '0' && virtualKey <= '9')
        return static_cast<Key>(static_cast<std::size_t>(Key::Num0) + (virtualKey - '0'));
    if (virtualKey >= VK_F1 && virtualKey <= VK_F12)
        return static_cast<Key>(static_cast<std::size_t>(Key::F1) + (virtualKey - VK_F1));

    switch (virtualKey)
    {
    case VK_LEFT:
        return Key::Left;
    case VK_RIGHT:
        return Key::Right;
    case VK_UP:
        return Key::Up;
    case VK_DOWN:
        return Key::Down;
    case VK_SPACE:
        return Key::Space;
    case VK_RETURN:
        return Key::Enter;
    case VK_ESCAPE:
        return Key::Escape;
    case VK_TAB:
        return Key::Tab;
    case VK_BACK:
        return Key::Backspace;
    case VK_INSERT:
        return Key::Insert;
    case VK_DELETE:
        return Key::Delete;
    case VK_HOME:
        return Key::Home;
    case VK_END:
        return Key::End;
    case VK_PRIOR:
        return Key::PageUp;
    case VK_NEXT:
        return Key::PageDown;
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
        return Key::Shift;
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
        return Key::Control;
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
        return Key::Alt;
    default:
        return Key::Unknown;
    }
}

class Win32InputDevice final : public Input::Device
{
  public:
    explicit Win32InputDevice(HWND window) : m_window(window)
    {
        if (m_window == nullptr)
            throw std::invalid_argument("Win32InputDevice: A window handle is required.");

        // 以 Raw Input 取得滑鼠的原始位移（不受螢幕邊界與滑鼠加速度影響）
        RAWINPUTDEVICE mouse{};
        mouse.usUsagePage = kHidUsagePageGeneric;
        mouse.usUsage = kHidUsageMouse;
        mouse.dwFlags = 0;
        mouse.hwndTarget = m_window;
        if (!RegisterRawInputDevices(&mouse, 1, sizeof(mouse)))
            throw std::runtime_error("Win32InputDevice: Failed to register the raw input mouse device.");

        if (!SetWindowSubclass(m_window, &Win32InputDevice::subclassProc, kSubclassId,
                               reinterpret_cast<DWORD_PTR>(this)))
        {
            unregisterRawInput();
            throw std::runtime_error("Win32InputDevice: Failed to subclass the window.");
        }

        // 以目前游標位置作為初始滑鼠座標
        POINT cursor{};
        if (GetCursorPos(&cursor) && ScreenToClient(m_window, &cursor))
            m_state.setMousePosition(static_cast<float>(cursor.x), static_cast<float>(cursor.y));
    }

    ~Win32InputDevice() override
    {
        // m_window 為空代表視窗已先被銷毀，掛接已在 WM_NCDESTROY 時解除
        if (m_window != nullptr)
            RemoveWindowSubclass(m_window, &Win32InputDevice::subclassProc, kSubclassId);
        unregisterRawInput();
    }

    void update() override
    {
        m_state.update();
    }

    const Input::State& state() const noexcept override
    {
        return m_state;
    }

  private:
    static LRESULT CALLBACK subclassProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR,
                                         DWORD_PTR refData)
    {
        auto* self = reinterpret_cast<Win32InputDevice*>(refData);
        self->observe(window, message, wParam, lParam);

        if (message == WM_NCDESTROY)
        {
            RemoveWindowSubclass(window, &Win32InputDevice::subclassProc, kSubclassId);
            self->m_window = nullptr;
        }
        // 只觀察訊息，不攔截；交還給視窗原本的處理函式
        return DefSubclassProc(window, message, wParam, lParam);
    }

    void observe(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            onKey(wParam, true);
            break;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            onKey(wParam, false);
            break;

        case WM_LBUTTONDOWN:
            onMouseButton(window, MouseButton::Left, true, wParam);
            break;
        case WM_LBUTTONUP:
            onMouseButton(window, MouseButton::Left, false, wParam);
            break;
        case WM_RBUTTONDOWN:
            onMouseButton(window, MouseButton::Right, true, wParam);
            break;
        case WM_RBUTTONUP:
            onMouseButton(window, MouseButton::Right, false, wParam);
            break;
        case WM_MBUTTONDOWN:
            onMouseButton(window, MouseButton::Middle, true, wParam);
            break;
        case WM_MBUTTONUP:
            onMouseButton(window, MouseButton::Middle, false, wParam);
            break;
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
        {
            const WORD which = GET_XBUTTON_WPARAM(wParam);
            if (which == XBUTTON1)
                onMouseButton(window, MouseButton::X1, message == WM_XBUTTONDOWN, wParam);
            else if (which == XBUTTON2)
                onMouseButton(window, MouseButton::X2, message == WM_XBUTTONDOWN, wParam);
            break;
        }

        case WM_MOUSEMOVE:
            // lParam 的低、高 word 是 Client 區座標，以 short 解讀以支援負值（拖曳到視窗外時）
            m_state.setMousePosition(static_cast<float>(static_cast<short>(LOWORD(lParam))),
                                     static_cast<float>(static_cast<short>(HIWORD(lParam))));
            break;
        case WM_MOUSEWHEEL:
            m_state.addMouseWheel(static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / static_cast<float>(WHEEL_DELTA));
            break;
        case WM_INPUT:
            onRawInput(lParam);
            break;

        case WM_KILLFOCUS:
            // 失去焦點後收不到放開事件，這裡主動放開所有按鍵避免卡住
            m_state.releaseAll();
            break;
        default:
            break;
        }
    }

    void onKey(WPARAM virtualKey, bool down)
    {
        // 不認得的按鍵直接忽略，不寫入狀態
        const Key key = translateVirtualKey(virtualKey);
        if (key != Key::Unknown)
            m_state.setKeyDown(key, down);
    }

    void onMouseButton(HWND window, MouseButton button, bool down, WPARAM wParam)
    {
        m_state.setMouseButtonDown(button, down);

        // 按住期間捕捉滑鼠，這樣拖曳到視窗外再放開也能收到放開事件
        if (down)
        {
            SetCapture(window);
        }
        else
        {
            constexpr WPARAM anyButton = MK_LBUTTON | MK_RBUTTON | MK_MBUTTON | MK_XBUTTON1 | MK_XBUTTON2;
            if ((wParam & anyButton) == 0)
                ReleaseCapture();
        }
    }

    void onRawInput(LPARAM lParam)
    {
        RAWINPUT raw{};
        UINT size = sizeof(raw);
        // 取不到資料（例如非滑鼠裝置的資料較大）時直接略過，不在視窗訊息處理中丟出例外
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) ==
            static_cast<UINT>(-1))
            return;

        if (raw.header.dwType == RIM_TYPEMOUSE && (raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0)
            m_state.addMouseDelta(static_cast<float>(raw.data.mouse.lLastX), static_cast<float>(raw.data.mouse.lLastY));
    }

    static void unregisterRawInput() noexcept
    {
        RAWINPUTDEVICE mouse{};
        mouse.usUsagePage = kHidUsagePageGeneric;
        mouse.usUsage = kHidUsageMouse;
        mouse.dwFlags = RIDEV_REMOVE;
        mouse.hwndTarget = nullptr; // RIDEV_REMOVE 時必須為空
        RegisterRawInputDevices(&mouse, 1, sizeof(mouse));
    }

    HWND m_window;
    Input::State m_state;
};
} // namespace

std::unique_ptr<Input::Device> createInputDevice(HWND window)
{
    return std::make_unique<Win32InputDevice>(window);
}
} // namespace Skyline::Win32
