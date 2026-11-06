#pragma once

#include "input_types.h"
#include "math/vector.h"

#include <bitset>
#include <cassert>

namespace Skyline::Input
{
/// <summary>
/// 與平台無關的輸入狀態，後端把事件餵進來，遊戲邏輯從這裡查詢。
/// </summary>
/// <remarks>
/// 運作方式：
/// 1. 後端在訊息處理時呼叫 setXxx / addXxx，這些事件只會更新「即時狀態」。
/// 2. 每個 Frame 開頭呼叫一次 update()，把即時狀態整理成這個 Frame 的快照。
/// 3. isDown、wasPressed、wasReleased、mouseDelta 等查詢都讀取快照，
///    所以同一個 Frame 內多次查詢的結果一致。
///
/// 如果一個按鍵在兩次 update() 之間被按下又放開，wasPressed 與 wasReleased 都會是 true，
/// 不會因為處理太慢而漏掉短按。
///
/// 此類別只含標準型別，全部是 inline，不需要匯出。
/// </remarks>
class State
{
  public:
    // ----- 查詢（讀取最近一次 update() 的快照） -----

    [[nodiscard]] bool isDown(Key key) const noexcept
    {
        return m_frameKeyDown.test(index(key));
    }
    // 這個 Frame 內剛被按下
    [[nodiscard]] bool wasPressed(Key key) const noexcept
    {
        return m_framePressed.test(index(key));
    }
    // 這個 Frame 內剛被放開
    [[nodiscard]] bool wasReleased(Key key) const noexcept
    {
        return m_frameReleased.test(index(key));
    }

    [[nodiscard]] bool isDown(MouseButton button) const noexcept
    {
        return m_frameMouseDown.test(index(button));
    }
    [[nodiscard]] bool wasPressed(MouseButton button) const noexcept
    {
        return m_frameMousePressed.test(index(button));
    }
    [[nodiscard]] bool wasReleased(MouseButton button) const noexcept
    {
        return m_frameMouseReleased.test(index(button));
    }

    // 滑鼠在視窗 Client 區的位置（像素）
    [[nodiscard]] const Vector2& mousePosition() const noexcept
    {
        return m_frameMousePosition;
    }
    // 這個 Frame 的滑鼠位移（來自裝置的原始位移，不受螢幕邊界限制）
    [[nodiscard]] const Vector2& mouseDelta() const noexcept
    {
        return m_frameMouseDelta;
    }
    // 這個 Frame 的滾輪位移，單位為格數，向前滾為正
    [[nodiscard]] float mouseWheel() const noexcept
    {
        return m_frameMouseWheel;
    }

    // ----- 事件輸入（由後端呼叫） -----

    void setKeyDown(Key key, bool down) noexcept
    {
        const std::size_t i = index(key);
        if (m_liveKeyDown.test(i) == down)
            return; // 按住不放時系統會連續送出按下事件，這裡忽略重複的狀態
        m_liveKeyDown.set(i, down);
        (down ? m_livePressed : m_liveReleased).set(i);
    }
    void setMouseButtonDown(MouseButton button, bool down) noexcept
    {
        const std::size_t i = index(button);
        if (m_liveMouseDown.test(i) == down)
            return;
        m_liveMouseDown.set(i, down);
        (down ? m_liveMousePressed : m_liveMouseReleased).set(i);
    }
    void setMousePosition(float x, float y) noexcept
    {
        m_liveMousePosition.Set(x, y);
    }
    void addMouseDelta(float dx, float dy) noexcept
    {
        m_liveMouseDelta.x += dx;
        m_liveMouseDelta.y += dy;
    }
    void addMouseWheel(float notches) noexcept
    {
        m_liveMouseWheel += notches;
    }

    /// <summary>
    /// 放開所有按下中的按鍵與滑鼠按鍵。視窗失去焦點時使用，避免按鍵卡住。
    /// </summary>
    void releaseAll() noexcept
    {
        for (std::size_t i = 0; i < keyCount; ++i)
            setKeyDown(static_cast<Key>(i), false);
        for (std::size_t i = 0; i < mouseButtonCount; ++i)
            setMouseButtonDown(static_cast<MouseButton>(i), false);
    }

    /// <summary>
    /// 把即時狀態整理成這個 Frame 的快照，並清除已累積的邊緣與位移。每個 Frame 呼叫一次。
    /// </summary>
    void update() noexcept
    {
        m_frameKeyDown = m_liveKeyDown;
        m_framePressed = m_livePressed;
        m_frameReleased = m_liveReleased;
        m_livePressed.reset();
        m_liveReleased.reset();

        m_frameMouseDown = m_liveMouseDown;
        m_frameMousePressed = m_liveMousePressed;
        m_frameMouseReleased = m_liveMouseReleased;
        m_liveMousePressed.reset();
        m_liveMouseReleased.reset();

        m_frameMousePosition = m_liveMousePosition;
        m_frameMouseDelta = m_liveMouseDelta;
        m_frameMouseWheel = m_liveMouseWheel;
        m_liveMouseDelta.Set(0.0f, 0.0f);
        m_liveMouseWheel = 0.0f;
    }

  private:
    static std::size_t index(Key key) noexcept
    {
        assert(key < Key::Count);
        return static_cast<std::size_t>(key);
    }
    static std::size_t index(MouseButton button) noexcept
    {
        assert(button < MouseButton::Count);
        return static_cast<std::size_t>(button);
    }

    // 即時狀態：事件到達時立刻更新
    std::bitset<keyCount> m_liveKeyDown;
    std::bitset<keyCount> m_livePressed;
    std::bitset<keyCount> m_liveReleased;
    std::bitset<mouseButtonCount> m_liveMouseDown;
    std::bitset<mouseButtonCount> m_liveMousePressed;
    std::bitset<mouseButtonCount> m_liveMouseReleased;
    Vector2 m_liveMousePosition;
    Vector2 m_liveMouseDelta;
    float m_liveMouseWheel = 0.0f;

    // Frame 快照：update() 時從即時狀態複製
    std::bitset<keyCount> m_frameKeyDown;
    std::bitset<keyCount> m_framePressed;
    std::bitset<keyCount> m_frameReleased;
    std::bitset<mouseButtonCount> m_frameMouseDown;
    std::bitset<mouseButtonCount> m_frameMousePressed;
    std::bitset<mouseButtonCount> m_frameMouseReleased;
    Vector2 m_frameMousePosition;
    Vector2 m_frameMouseDelta;
    float m_frameMouseWheel = 0.0f;
};
} // namespace Skyline::Input
