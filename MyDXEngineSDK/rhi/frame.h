#pragma once

#include "command_context.h"
#include "device.h"

namespace Skyline::RHI
{
struct FrameDesc
{
    void* nativeWindow = nullptr;
    std::uint32_t width = 0, height = 0;
};
class SKYLINE_API Frame
{
public:
    virtual ~Frame();
    [[nodiscard]] virtual Device& device() const = 0;
    [[nodiscard]] virtual CommandContext& commands() = 0;
    [[nodiscard]] virtual AttachmentLayout attachments() const = 0;
    [[nodiscard]] virtual std::uint32_t frameIndex() const noexcept = 0;
    [[nodiscard]] virtual std::uint32_t width() const noexcept = 0;
    [[nodiscard]] virtual std::uint32_t height() const noexcept = 0;
    virtual void begin() = 0;
    virtual void end() = 0;
    virtual void waitIdle() = 0;
    virtual void resize(std::uint32_t width, std::uint32_t height) = 0;
};
}
