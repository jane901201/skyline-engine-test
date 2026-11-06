#pragma once

#include "core/skyline_api.h"
#include "input/input_device.h"

#include <Windows.h>

#include <memory>

namespace Skyline::Win32
{
/// <summary>
/// 建立監聽指定視窗的 Win32 輸入裝置（鍵盤、滑鼠，滑鼠位移使用 Raw Input）。
/// </summary>
/// <remarks>
/// 透過 SetWindowSubclass 掛接，不會取代視窗原本的訊息處理函式，因此可與 ImGui 等並存。
/// window 為空或註冊 Raw Input 失敗時會丟出例外。
/// 輸入裝置必須與視窗在同一個執行緒使用。
/// 視窗先被銷毀時，裝置會自動解除掛接，之後 state() 仍可讀取（不再更新）。
/// </remarks>
SKYLINE_API std::unique_ptr<Input::Device> createInputDevice(HWND window);
} // namespace Skyline::Win32
