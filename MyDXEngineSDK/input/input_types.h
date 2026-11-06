#pragma once

#include <cstddef>

namespace Skyline::Input
{
/// <summary>
/// 鍵盤按鍵。與平台無關，各後端負責把平台的按鍵碼轉成此列舉。
/// </summary>
/// <remarks>
/// Shift、Control、Alt 不區分左右。
/// </remarks>
enum class Key : std::size_t
{
    Unknown = 0,

    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,

    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

    Left, Right, Up, Down,

    Space, Enter, Escape, Tab, Backspace,
    Insert, Delete, Home, End, PageUp, PageDown,

    Shift, Control, Alt,

    // 必須放在最後，代表按鍵總數
    Count
};

/// <summary>
/// 滑鼠按鍵。
/// </summary>
enum class MouseButton : std::size_t
{
    Left = 0,
    Right,
    Middle,
    // 側邊按鍵（上一頁、下一頁）
    X1,
    X2,

    // 必須放在最後，代表按鍵總數
    Count
};

inline constexpr std::size_t keyCount = static_cast<std::size_t>(Key::Count);
inline constexpr std::size_t mouseButtonCount = static_cast<std::size_t>(MouseButton::Count);
} // namespace Skyline::Input
