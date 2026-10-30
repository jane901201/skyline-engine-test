#include "d3d12_device.h"
#include "d3d12_objects.h"
#include "d3d12_pipeline.h"
#include "d3d12_shader.h"
#include "d3d12_binding.h"
#include "d3d12_resource.h"
#include "d3d12_command_context.h"

#include <stdexcept>

namespace Skyline::D3D12
{
namespace
{
template<class BackendObject, class Object>
const BackendObject& requireObject(const Object* object, const RHI::Device& device, const std::string& name)
{
    if (object == nullptr || &object->device() != &device)
        throw std::invalid_argument(name + ": Required objects must belong to the creating RHI device.");
    const auto* native = dynamic_cast<const BackendObject*>(object);
    if (native == nullptr)
        throw std::invalid_argument(name + ": Object is incompatible with the D3D12 backend.");
    return *native;
}

class Device final : public RHI::Device
{
public:
    explicit Device(ID3D12Device& device) : m_device(&device)
    {
        D3D12_FEATURE_DATA_SHADER_MODEL shaderModel{D3D_SHADER_MODEL_6_4};
        if (FAILED(device.CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel))) ||
            shaderModel.HighestShaderModel < D3D_SHADER_MODEL_6_4)
            throw std::runtime_error("RHI Device: Shader Model 6.4 is required.");
        m_capabilities = {D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT, D3D12_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT, 6, 4};
    }

    const RHI::DeviceCapabilities& capabilities() const noexcept override { return m_capabilities; }

    bool supportsAttachment(RHI::Format format, RHI::SampleDesc samples) const override
    {
        if (format == RHI::Format::Unknown || samples.count == 0 || (samples.count == 1 && samples.quality != 0))
            return false;
        const auto native = toNativeFormat(format);
        D3D12_FEATURE_DATA_FORMAT_SUPPORT support{native};
        if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support))))
            return false;
        const bool depth = format == RHI::Format::D32Float || format == RHI::Format::D24UnormS8Uint;
        const auto required = depth ? D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL : D3D12_FORMAT_SUPPORT1_RENDER_TARGET;
        if ((support.Support1 & required) == 0)
            return false;
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS levels{native, samples.count, D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE, 0};
        return SUCCEEDED(m_device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &levels, sizeof(levels))) &&
               levels.NumQualityLevels > samples.quality;
    }

    std::unique_ptr<RHI::Shader> createShader(const RHI::ShaderDesc& desc) override
    {
        return std::make_unique<Shader>(*this, desc.stage, compileShader(desc));
    }

    std::unique_ptr<RHI::BindingLayout> createBindingLayout(const RHI::BindingLayoutDesc& desc) override
    {
        return std::make_unique<BindingLayout>(*this, desc, D3D12::createBindingLayout(*m_device.Get(), desc));
    }

    std::unique_ptr<RHI::Pipeline> createGraphicsPipeline(const RHI::GraphicsPipelineDesc& desc) override
    {
        const auto& layout = requireObject<BindingLayout>(desc.bindingLayout, *this, desc.debugName);
        const auto& vs = requireObject<Shader>(desc.vertexShader, *this, desc.debugName);
        const auto& ps = requireObject<Shader>(desc.pixelShader, *this, desc.debugName);
        if (vs.stage() != RHI::ShaderStage::Vertex || ps.stage() != RHI::ShaderStage::Pixel)
            throw std::invalid_argument(desc.debugName + ": Graphics shader stages must be Vertex and Pixel.");
        if (desc.attachments.colorCount > m_capabilities.maxColorAttachments)
            throw std::invalid_argument(desc.debugName + ": Too many color attachments.");
        for (std::uint32_t index = 0; index < desc.attachments.colorCount; ++index)
            if (!supportsAttachment(desc.attachments.colorFormats[index], desc.attachments.samples))
                throw std::invalid_argument(desc.debugName + ": Unsupported color format or sample configuration.");
        if (desc.attachments.depthFormat != RHI::Format::Unknown &&
            !supportsAttachment(desc.attachments.depthFormat, desc.attachments.samples))
            throw std::invalid_argument(desc.debugName + ": Unsupported depth format or sample configuration.");
        return std::make_unique<Pipeline>(*this, RHI::PipelineKind::Graphics,
            D3D12::createGraphicsPipeline(*m_device.Get(), layout.native(), vs.bytecode(), ps.bytecode(), desc), layout.native(),
            !layout.description().constantBuffers.empty() || !layout.description().resources.empty(), !desc.vertexInput.empty(), desc.attachments, desc.primitive);
    }

    std::unique_ptr<RHI::Pipeline> createComputePipeline(const RHI::ComputePipelineDesc& desc) override
    {
        const auto& layout = requireObject<BindingLayout>(desc.bindingLayout, *this, desc.debugName);
        const auto& cs = requireObject<Shader>(desc.computeShader, *this, desc.debugName);
        if (cs.stage() != RHI::ShaderStage::Compute)
            throw std::invalid_argument(desc.debugName + ": A Compute shader is required.");
        return std::make_unique<Pipeline>(*this, RHI::PipelineKind::Compute,
            D3D12::createComputePipeline(*m_device.Get(), layout.native(), cs.bytecode(), desc), layout.native(),
            !layout.description().constantBuffers.empty() || !layout.description().resources.empty());
    }

    std::shared_ptr<RHI::Buffer> createBuffer(const RHI::BufferDesc& desc) override
    {
        return std::make_shared<Buffer>(*this, *m_device.Get(), desc);
    }

    std::shared_ptr<RHI::BufferView> createBufferView(std::shared_ptr<RHI::Buffer> buffer, const RHI::BufferViewDesc& desc) override
    {
        requireObject<Buffer>(buffer.get(), *this, "Buffer view");
        return std::make_shared<BufferView>(*this, std::static_pointer_cast<Buffer>(std::move(buffer)), desc);
    }

    std::unique_ptr<RHI::BindingSet> createBindingSet(const RHI::BindingSetDesc& desc) override
    {
        return std::make_unique<BindingSet>(*this, *m_device.Get(), desc);
    }

    std::shared_ptr<RHI::Texture> createTexture(const RHI::TextureDesc& desc) override
    {
        return std::make_shared<Texture>(*this, *m_device.Get(), desc);
    }
    std::shared_ptr<RHI::TextureView> createTextureView(std::shared_ptr<RHI::Texture> texture, const RHI::TextureViewDesc& desc) override
    {
        requireObject<Texture>(texture.get(), *this, "Texture view");
        return std::make_shared<TextureView>(*this, std::static_pointer_cast<Texture>(std::move(texture)), desc);
    }
    std::unique_ptr<RHI::CommandContext> createCommandContext() override
    {
        return std::make_unique<CommandContext>(*this, *m_device.Get());
    }

private:
    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
    RHI::DeviceCapabilities m_capabilities;
};
}

std::unique_ptr<RHI::Device> createDevice(ID3D12Device& device)
{
    return std::make_unique<Device>(device);
}
}
