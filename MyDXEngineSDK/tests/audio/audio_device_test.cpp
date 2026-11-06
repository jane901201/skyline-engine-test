#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "backends/miniaudio/miniaudio_device.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>

namespace
{
using namespace Skyline;

// 以小端序寫入整數
template <class T> void writeLittleEndian(std::ofstream& file, T value)
{
    for (std::size_t i = 0; i < sizeof(T); ++i)
        file.put(static_cast<char>((static_cast<std::uint64_t>(value) >> (8 * i)) & 0xFF));
}

// 產生一段 16-bit 單聲道 PCM 的正弦波 WAV，測試不需要依賴外部音檔
std::filesystem::path writeTestWav(const char* name, double seconds)
{
    constexpr std::uint32_t sampleRate = 48000;
    const auto sampleCount = static_cast<std::uint32_t>(sampleRate * seconds);
    const std::uint32_t dataBytes = sampleCount * sizeof(std::int16_t);

    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream file(path, std::ios::binary);
    file.write("RIFF", 4);
    writeLittleEndian<std::uint32_t>(file, 36 + dataBytes);
    file.write("WAVEfmt ", 8);
    writeLittleEndian<std::uint32_t>(file, 16);
    writeLittleEndian<std::uint16_t>(file, 1); // PCM
    writeLittleEndian<std::uint16_t>(file, 1); // 單聲道
    writeLittleEndian<std::uint32_t>(file, sampleRate);
    writeLittleEndian<std::uint32_t>(file, sampleRate * sizeof(std::int16_t));
    writeLittleEndian<std::uint16_t>(file, sizeof(std::int16_t));
    writeLittleEndian<std::uint16_t>(file, 16);
    file.write("data", 4);
    writeLittleEndian<std::uint32_t>(file, dataBytes);
    for (std::uint32_t i = 0; i < sampleCount; ++i)
    {
        const double sample = std::sin(2.0 * 3.14159265358979 * 440.0 * i / sampleRate) * 0.25;
        writeLittleEndian<std::uint16_t>(file, static_cast<std::uint16_t>(static_cast<std::int16_t>(sample * 32767.0)));
    }
    return path;
}

std::unique_ptr<Audio::Device> createHeadlessDevice()
{
    Audio::DeviceDesc desc;
    desc.headless = true;
    return MiniAudio::createDevice(desc);
}
} // namespace

TEST_CASE("Audio headless device reports its effective settings", "[audio]")
{
    const auto device = createHeadlessDevice();
    const auto& info = device->info();
    CHECK(info.sampleRate > 0);
    CHECK(info.channels > 0);
    CHECK(info.backendName == "MiniAudio");
    CHECK(info.systemApiName == "Null");
}

TEST_CASE("Audio headless device honors the requested format", "[audio]")
{
    Audio::DeviceDesc desc;
    desc.headless = true;
    desc.sampleRate = 44100;
    desc.channels = 2;
    const auto device = MiniAudio::createDevice(desc);
    CHECK(device->info().sampleRate == 44100);
    CHECK(device->info().channels == 2);
}

TEST_CASE("Audio master volume is validated", "[audio]")
{
    const auto device = createHeadlessDevice();
    CHECK_THAT(device->masterVolume(), Catch::Matchers::WithinAbs(1.0, 1e-6));
    device->setMasterVolume(0.5f);
    CHECK_THAT(device->masterVolume(), Catch::Matchers::WithinAbs(0.5, 1e-6));
    CHECK_THROWS_AS(device->setMasterVolume(-0.1f), std::invalid_argument);
    CHECK_THAT(device->masterVolume(), Catch::Matchers::WithinAbs(0.5, 1e-6));
}

TEST_CASE("Audio createSound rejects invalid descriptions", "[audio]")
{
    const auto device = createHeadlessDevice();
    const auto wav = writeTestWav("skyline_audio_invalid.wav", 0.1);

    CHECK_THROWS_AS(device->createSound({}), std::invalid_argument);

    Audio::SoundDesc negativeVolume{wav};
    negativeVolume.volume = -1.0f;
    CHECK_THROWS_AS(device->createSound(negativeVolume), std::invalid_argument);

    Audio::SoundDesc zeroPitch{wav};
    zeroPitch.pitch = 0.0f;
    CHECK_THROWS_AS(device->createSound(zeroPitch), std::invalid_argument);

    Audio::SoundDesc missing{std::filesystem::temp_directory_path() / "skyline_audio_does_not_exist.wav"};
    CHECK_THROWS_AS(device->createSound(missing), std::runtime_error);

    std::filesystem::remove(wav);
}

TEST_CASE("Audio Sound plays pauses and stops", "[audio]")
{
    const auto device = createHeadlessDevice();
    const auto wav = writeTestWav("skyline_audio_playback.wav", 5.0);

    for (const auto mode : {Audio::SoundLoadMode::Decode, Audio::SoundLoadMode::Stream})
    {
        Audio::SoundDesc desc{wav};
        desc.loadMode = mode;
        const auto sound = device->createSound(desc);

        CHECK_FALSE(sound->isPlaying());
        sound->play();
        CHECK(sound->isPlaying());
        sound->pause();
        CHECK_FALSE(sound->isPlaying());
        sound->play();
        sound->stop();
        CHECK_FALSE(sound->isPlaying());
        CHECK_FALSE(sound->isFinished());
    }

    std::filesystem::remove(wav);
}

TEST_CASE("Audio Sound properties round-trip and are validated", "[audio]")
{
    const auto device = createHeadlessDevice();
    const auto wav = writeTestWav("skyline_audio_properties.wav", 0.1);

    Audio::SoundDesc desc{wav};
    desc.volume = 0.25f;
    desc.pitch = 1.5f;
    desc.looping = true;
    const auto sound = device->createSound(desc);

    CHECK_THAT(sound->volume(), Catch::Matchers::WithinAbs(0.25, 1e-6));
    CHECK_THAT(sound->pitch(), Catch::Matchers::WithinAbs(1.5, 1e-6));
    CHECK(sound->looping());

    sound->setLooping(false);
    sound->setVolume(0.75f);
    sound->setPitch(2.0f);
    CHECK_FALSE(sound->looping());
    CHECK_THAT(sound->volume(), Catch::Matchers::WithinAbs(0.75, 1e-6));
    CHECK_THAT(sound->pitch(), Catch::Matchers::WithinAbs(2.0, 1e-6));

    CHECK_THROWS_AS(sound->setVolume(-1.0f), std::invalid_argument);
    CHECK_THROWS_AS(sound->setPitch(0.0f), std::invalid_argument);
    CHECK_THAT(sound->volume(), Catch::Matchers::WithinAbs(0.75, 1e-6));
    CHECK_THAT(sound->pitch(), Catch::Matchers::WithinAbs(2.0, 1e-6));

    std::filesystem::remove(wav);
}

TEST_CASE("Audio non-looping Sound reaches its end", "[audio]")
{
    const auto device = createHeadlessDevice();
    const auto wav = writeTestWav("skyline_audio_finish.wav", 0.1);
    const auto sound = device->createSound({wav});

    sound->play();
    // Null 後端以即時速度消耗音訊，給足夠時間讓 0.1 秒的音檔播完
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!sound->isFinished() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    CHECK(sound->isFinished());
    CHECK_FALSE(sound->isPlaying());

    std::filesystem::remove(wav);
}

TEST_CASE("Audio Sound can outlive the Device that created it", "[audio]")
{
    const auto wav = writeTestWav("skyline_audio_lifetime.wav", 0.5);

    std::unique_ptr<Audio::Sound> sound;
    {
        const auto device = createHeadlessDevice();
        sound = device->createSound({wav});
    }
    // Device 已釋放，Sound 仍持有引擎，因此仍可操作與銷毀
    sound->play();
    CHECK(sound->isPlaying());
    sound.reset();

    std::filesystem::remove(wav);
}
