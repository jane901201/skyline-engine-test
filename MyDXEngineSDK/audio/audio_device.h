#pragma once

#include "audio_types.h"
#include "core/skyline_api.h"

#include <memory>

namespace Skyline::Audio
{
/// <summary>
/// 可播放的音訊實例。
/// </summary>
/// <remarks>
/// 由 Device::createSound 建立。Sound 會延長其所屬裝置的生命週期，
/// 因此即使 Device 先被釋放，Sound 仍可安全使用與銷毀。
/// </remarks>
class SKYLINE_API Sound
{
  public:
    virtual ~Sound();
    Sound(const Sound&) = delete;
    Sound& operator=(const Sound&) = delete;

    /// 從目前位置開始播放
    virtual void play() = 0;
    /// 暫停並保留播放位置
    virtual void pause() = 0;
    /// 停止並回到開頭
    virtual void stop() = 0;

    /// 是否正在播放
    [[nodiscard]] virtual bool isPlaying() const = 0;
    /// 非循環音檔是否已播放到結尾
    [[nodiscard]] virtual bool isFinished() const = 0;

    virtual void setLooping(bool looping) = 0;
    [[nodiscard]] virtual bool looping() const = 0;
    /// volume 必須大於或等於 0
    virtual void setVolume(float volume) = 0;
    [[nodiscard]] virtual float volume() const = 0;
    /// pitch 必須大於 0
    virtual void setPitch(float pitch) = 0;
    [[nodiscard]] virtual float pitch() const = 0;

  protected:
    Sound() = default;
};

/// <summary>
/// 音訊裝置的抽象介面，負責混音與輸出。
/// </summary>
/// <remarks>
/// 新增其他音訊後端時，只需實作 Device 與 Sound，
/// 並提供一個建立裝置的工廠函式（參考 Skyline::MiniAudio::createDevice）。
/// </remarks>
class SKYLINE_API Device
{
  public:
    virtual ~Device();
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    [[nodiscard]] virtual const DeviceInfo& info() const noexcept = 0;
    /// 主音量，必須大於或等於 0
    virtual void setMasterVolume(float volume) = 0;
    [[nodiscard]] virtual float masterVolume() const = 0;
    /// 載入音檔並建立 Sound，失敗時會丟出例外
    [[nodiscard]] virtual std::unique_ptr<Sound> createSound(const SoundDesc& desc) = 0;

  protected:
    Device() = default;
};
} // namespace Skyline::Audio
