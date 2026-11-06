#pragma once

#include "audio/audio_device.h"
#include "core/skyline_api.h"

#include <memory>

namespace Skyline::MiniAudio
{
/// <summary>
/// 建立以 miniaudio 為後端的音訊裝置。
/// </summary>
/// <remarks>
/// 初始化失敗時會丟出 std::runtime_error。
/// </remarks>
SKYLINE_API std::unique_ptr<Audio::Device> createDevice(const Audio::DeviceDesc& desc = {});
} // namespace Skyline::MiniAudio
