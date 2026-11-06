#pragma once

#include <cstdint>

namespace Skyline
{
/// <summary>
/// 遊戲時間，對應 Unity 的 Time 類別。由 GameWorld::Tick 更新。
/// </summary>
/// <remarks>
/// 可以由外部設定的欄位：timeScale、fixedDeltaTime、maximumDeltaTime。
/// 其餘欄位由 GameWorld 維護，請視為唯讀。
/// </remarks>
struct GameTime
{
    float timeScale = 1.0f;           ///< 時間縮放；0 代表暫停（Update 仍會被呼叫，但 deltaTime 為 0，FixedUpdate 不會）
    float fixedDeltaTime = 0.02f;     ///< FixedUpdate 的固定步長（秒），必須大於 0
    float maximumDeltaTime = 1.0f / 3.0f; ///< 單次 Tick 能使用的最大真實時間（秒），避免卡頓後 FixedUpdate 連跑太多次

    float deltaTime = 0.0f;           ///< 本次 Tick 套用 timeScale 之後的時間（FixedUpdate 內為 fixedDeltaTime）
    float unscaledDeltaTime = 0.0f;   ///< 本次 Tick 未套用 timeScale 的時間（已受 maximumDeltaTime 限制）
    double time = 0.0;                ///< 遊戲開始至今的縮放後時間（秒）
    double unscaledTime = 0.0;        ///< 遊戲開始至今的未縮放時間（秒）
    std::uint64_t frameCount = 0;     ///< 已完成的 Tick 次數
};
} // namespace Skyline
