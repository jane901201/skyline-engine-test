#pragma once

#include "binding.h"
#include "pipeline.h"
#include "shader.h"
#include "resource.h"
#include <memory>

namespace Skyline::RHI
{
class CommandContext;
struct DeviceCapabilities
{
    std::uint32_t maxColorAttachments = 0;
    std::uint32_t maxVertexAttributes = 0;
    std::uint32_t shaderModelMajor = 0;
    std::uint32_t shaderModelMinor = 0;
};

class SKYLINE_API Device
{
public:
    virtual ~Device();
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    [[nodiscard]] virtual const DeviceCapabilities& capabilities() const noexcept = 0;
    [[nodiscard]] virtual bool supportsAttachment(Format format, SampleDesc samples = {}) const = 0;
    [[nodiscard]] virtual std::unique_ptr<Shader> createShader(const ShaderDesc& desc) = 0;
    [[nodiscard]] virtual std::unique_ptr<BindingLayout> createBindingLayout(const BindingLayoutDesc& desc) = 0;
    [[nodiscard]] virtual std::unique_ptr<Pipeline> createGraphicsPipeline(const GraphicsPipelineDesc& desc) = 0;
    [[nodiscard]] virtual std::unique_ptr<Pipeline> createComputePipeline(const ComputePipelineDesc& desc) = 0;
    [[nodiscard]] virtual std::shared_ptr<Buffer> createBuffer(const BufferDesc& desc) = 0;
    [[nodiscard]] virtual std::shared_ptr<BufferView> createBufferView(
        std::shared_ptr<Buffer> buffer, const BufferViewDesc& desc) = 0;
    [[nodiscard]] virtual std::unique_ptr<BindingSet> createBindingSet(const BindingSetDesc& desc) = 0;
    [[nodiscard]] virtual std::shared_ptr<Texture> createTexture(const TextureDesc& desc) = 0;
    [[nodiscard]] virtual std::shared_ptr<TextureView> createTextureView(std::shared_ptr<Texture> texture, const TextureViewDesc& desc = {}) = 0;
    [[nodiscard]] virtual std::unique_ptr<CommandContext> createCommandContext() = 0;

protected:
    Device() = default;
};
}
