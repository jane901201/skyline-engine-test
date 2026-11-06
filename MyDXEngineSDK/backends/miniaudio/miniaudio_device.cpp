#include "miniaudio_device.h"

// miniaudio 為單一標頭檔函式庫，只在這個檔案內展開實作，且不對外公開其型別
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <stdexcept>
#include <string>

namespace Skyline::MiniAudio
{
namespace
{
// 將 miniaudio 的錯誤碼轉為例外
void check(ma_result result, const char* what)
{
    if (result != MA_SUCCESS)
        throw std::runtime_error(std::string("MiniAudio: ") + what + " failed: " + ma_result_description(result));
}

void validateVolume(float volume, const char* what)
{
    if (!(volume >= 0.0f))
        throw std::invalid_argument(std::string("MiniAudio: ") + what + " must be greater than or equal to zero.");
}

void validatePitch(float pitch)
{
    if (!(pitch > 0.0f))
        throw std::invalid_argument("MiniAudio: Pitch must be greater than zero.");
}

/// <summary>
/// 持有 ma_context 與 ma_engine，由 Device 與所有 Sound 共同持有，確保引擎晚於所有 Sound 釋放。
/// </summary>
class EngineState
{
  public:
    explicit EngineState(const Audio::DeviceDesc& desc)
    {
        ma_context_config contextConfig = ma_context_config_init();
        // headless 時只啟用 Null 後端，不會開啟實體音訊裝置
        ma_backend nullBackend[] = {ma_backend_null};
        check(ma_context_init(desc.headless ? nullBackend : nullptr, desc.headless ? 1 : 0, &contextConfig, &m_context),
              "context initialization");

        ma_engine_config engineConfig = ma_engine_config_init();
        engineConfig.pContext = &m_context;
        engineConfig.sampleRate = desc.sampleRate;
        engineConfig.channels = desc.channels;
        const ma_result engineResult = ma_engine_init(&engineConfig, &m_engine);
        if (engineResult != MA_SUCCESS)
        {
            // 建構失敗時不會執行解構函式，需自行釋放已建立的 context
            ma_context_uninit(&m_context);
            check(engineResult, "engine initialization");
        }

        m_info.sampleRate = ma_engine_get_sample_rate(&m_engine);
        m_info.channels = ma_engine_get_channels(&m_engine);
        m_info.backendName = "MiniAudio";
        m_info.systemApiName = ma_get_backend_name(m_context.backend);
    }

    ~EngineState()
    {
        ma_engine_uninit(&m_engine);
        ma_context_uninit(&m_context);
    }

    EngineState(const EngineState&) = delete;
    EngineState& operator=(const EngineState&) = delete;

    ma_engine& engine() noexcept
    {
        return m_engine;
    }
    const ma_engine& engine() const noexcept
    {
        return m_engine;
    }
    const Audio::DeviceInfo& info() const noexcept
    {
        return m_info;
    }

  private:
    ma_context m_context{};
    ma_engine m_engine{};
    Audio::DeviceInfo m_info;
};

class MiniAudioSound final : public Audio::Sound
{
  public:
    MiniAudioSound(std::shared_ptr<EngineState> state, const Audio::SoundDesc& desc) : m_state(std::move(state))
    {
        ma_uint32 flags = MA_SOUND_FLAG_NO_SPATIALIZATION;
        flags |= desc.loadMode == Audio::SoundLoadMode::Stream ? MA_SOUND_FLAG_STREAM : MA_SOUND_FLAG_DECODE;

#if defined(_WIN32)
        // Windows 上 path 內部是 UTF-16，使用寬字元版本避免非 ASCII 路徑出錯
        const ma_result result =
            ma_sound_init_from_file_w(&m_state->engine(), desc.path.c_str(), flags, nullptr, nullptr, &m_sound);
#else
        const ma_result result =
            ma_sound_init_from_file(&m_state->engine(), desc.path.string().c_str(), flags, nullptr, nullptr, &m_sound);
#endif
        if (result != MA_SUCCESS)
            throw std::runtime_error("MiniAudio: Failed to load sound '" + desc.path.string() +
                                     "': " + ma_result_description(result));

        ma_sound_set_looping(&m_sound, desc.looping ? MA_TRUE : MA_FALSE);
        ma_sound_set_volume(&m_sound, desc.volume);
        ma_sound_set_pitch(&m_sound, desc.pitch);
    }

    ~MiniAudioSound() override
    {
        ma_sound_uninit(&m_sound);
    }

    void play() override
    {
        check(ma_sound_start(&m_sound), "sound start");
    }
    void pause() override
    {
        check(ma_sound_stop(&m_sound), "sound pause");
    }
    void stop() override
    {
        check(ma_sound_stop(&m_sound), "sound stop");
        check(ma_sound_seek_to_pcm_frame(&m_sound, 0), "sound seek");
    }

    bool isPlaying() const override
    {
        // miniaudio 的 is_playing 只代表「已啟動」，播到結尾後狀態更新有時間差，
        // 因此非循環音檔播完（at_end）時一律視為沒有在播放
        return ma_sound_is_playing(&m_sound) == MA_TRUE && ma_sound_at_end(&m_sound) != MA_TRUE;
    }
    bool isFinished() const override
    {
        return ma_sound_at_end(&m_sound) == MA_TRUE;
    }

    void setLooping(bool looping) override
    {
        ma_sound_set_looping(&m_sound, looping ? MA_TRUE : MA_FALSE);
    }
    bool looping() const override
    {
        return ma_sound_is_looping(&m_sound) == MA_TRUE;
    }
    void setVolume(float volume) override
    {
        validateVolume(volume, "Volume");
        ma_sound_set_volume(&m_sound, volume);
    }
    float volume() const override
    {
        return ma_sound_get_volume(&m_sound);
    }
    void setPitch(float pitch) override
    {
        validatePitch(pitch);
        ma_sound_set_pitch(&m_sound, pitch);
    }
    float pitch() const override
    {
        return ma_sound_get_pitch(&m_sound);
    }

  private:
    // 先持有引擎，確保 m_sound 釋放時引擎仍然有效
    std::shared_ptr<EngineState> m_state;
    ma_sound m_sound{};
};

class MiniAudioDevice final : public Audio::Device
{
  public:
    explicit MiniAudioDevice(const Audio::DeviceDesc& desc) : m_state(std::make_shared<EngineState>(desc))
    {
    }

    const Audio::DeviceInfo& info() const noexcept override
    {
        return m_state->info();
    }

    void setMasterVolume(float volume) override
    {
        validateVolume(volume, "Master volume");
        check(ma_engine_set_volume(&m_state->engine(), volume), "set master volume");
    }
    float masterVolume() const override
    {
        return ma_engine_get_volume(&m_state->engine());
    }

    std::unique_ptr<Audio::Sound> createSound(const Audio::SoundDesc& desc) override
    {
        if (desc.path.empty())
            throw std::invalid_argument("MiniAudio: Sound path is required.");
        validateVolume(desc.volume, "Volume");
        validatePitch(desc.pitch);
        return std::make_unique<MiniAudioSound>(m_state, desc);
    }

  private:
    std::shared_ptr<EngineState> m_state;
};
} // namespace

std::unique_ptr<Audio::Device> createDevice(const Audio::DeviceDesc& desc)
{
    return std::make_unique<MiniAudioDevice>(desc);
}
} // namespace Skyline::MiniAudio
