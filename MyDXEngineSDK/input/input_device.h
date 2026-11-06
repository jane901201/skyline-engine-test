#pragma once

#include "core/skyline_api.h"
#include "input_state.h"

namespace Skyline::Input
{
/// <summary>
/// 輸入裝置的抽象介面。
/// </summary>
/// <remarks>
/// 使用方式：每個 Frame 先處理完平台訊息，再呼叫 update()，之後用 state() 查詢。
/// 新增其他輸入後端時，實作此介面並提供一個建立裝置的工廠函式
/// （參考 Skyline::Win32::createInputDevice）。
/// </remarks>
class SKYLINE_API Device
{
  public:
    virtual ~Device();
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    /// 結算這個 Frame 的輸入，必須每個 Frame 呼叫一次
    virtual void update() = 0;
    /// 最近一次 update() 的輸入快照
    [[nodiscard]] virtual const State& state() const noexcept = 0;

  protected:
    Device() = default;
};
} // namespace Skyline::Input
