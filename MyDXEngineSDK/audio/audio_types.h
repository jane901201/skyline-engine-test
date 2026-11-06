#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>

namespace Skyline::Audio
{
/// <summary>
/// 建立音訊裝置時的設定。
/// </summary>
struct DeviceDesc
{
    /// 取樣率（Hz），0 代表使用系統預設值
    std::uint32_t sampleRate = 0;
    /// 聲道數，0 代表使用系統預設值
    std::uint32_t channels = 0;
    /// 為 true 時不開啟實體音訊裝置，只執行混音（用於 CI 與自動化測試）
    bool headless = false;
};

/// <summary>
/// 音訊裝置實際生效的設定。
/// </summary>
struct DeviceInfo
{
    std::uint32_t sampleRate = 0;
    std::uint32_t channels = 0;
    /// 後端名稱，例如 "MiniAudio"
    std::string_view backendName;
    /// 後端實際使用的系統音訊 API，例如 "WASAPI"、"Null"
    std::string_view systemApiName;
};

/// <summary>
/// 音檔的載入方式。
/// </summary>
enum class SoundLoadMode
{
    /// 建立時一次解碼到記憶體，適合短音效
    Decode,
    /// 播放時邊讀邊解碼，適合音樂與長音檔
    Stream,
};

/// <summary>
/// 建立 Sound 時的設定。
/// </summary>
struct SoundDesc
{
    /// 音檔路徑（必填）
    std::filesystem::path path;
    SoundLoadMode loadMode = SoundLoadMode::Decode;
    bool looping = false;
    /// 音量，必須大於或等於 0，1 為原始音量
    float volume = 1.0f;
    /// 音高倍率，必須大於 0，1 為原始音高
    float pitch = 1.0f;
};
} // namespace Skyline::Audio
