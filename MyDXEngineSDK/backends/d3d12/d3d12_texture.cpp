#include "d3d12_resource.h"
#include "d3d12_pipeline.h"
#include <directx/d3dx12_core.h>
#include <stdexcept>

namespace Skyline::D3D12
{
std::uint32_t pixelSize(RHI::Format format)
{
    switch (format)
    {
    case RHI::Format::Rgba8Unorm:
    case RHI::Format::Bgra8Unorm:
    case RHI::Format::R32Float: return 4;
    case RHI::Format::Rgba16Float:
    case RHI::Format::Rg32Float: return 8;
    case RHI::Format::Rgba32Float: return 16;
    default: throw std::invalid_argument("Texture: Unsupported color texture format.");
    }
}

void validateTextureState(const RHI::TextureDesc& desc, RHI::ResourceState state)
{
    using namespace RHI;
    resourceState(state);
    TextureUsage usage;
    switch (state)
    {
    case ResourceState::Common: return;
    case ResourceState::ShaderRead: usage = TextureUsage::Sampled; break;
    case ResourceState::Storage: usage = TextureUsage::Storage; break;
    case ResourceState::CopySource: usage = TextureUsage::CopySource; break;
    case ResourceState::CopyDestination: usage = TextureUsage::CopyDestination; break;
    case ResourceState::RenderTarget: usage = TextureUsage::RenderTarget; break;
    case ResourceState::DepthWrite: usage = TextureUsage::DepthStencil; break;
    default: throw std::invalid_argument(desc.debugName + ": Buffer-only state used for a texture.");
    }
    if (!hasUsage(desc.usage, usage))
        throw std::invalid_argument(desc.debugName + ": State requires matching texture usage.");
}

Texture::Texture(const RHI::Device& owner, ID3D12Device& device, RHI::TextureDesc desc)
    : RHI::Texture(owner), state(desc.initialState), m_desc(std::move(desc))
{
    using namespace RHI;
    const auto bits = static_cast<std::uint32_t>(m_desc.usage);
    if (m_desc.width == 0 || m_desc.height == 0 || m_desc.width > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
        m_desc.height > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION || bits == 0 || (bits & ~63u) != 0)
        throw std::invalid_argument(m_desc.debugName + ": Invalid dimensions or usage.");
    const bool depth = m_desc.format == Format::D32Float || m_desc.format == Format::D24UnormS8Uint;
    if (depth != hasUsage(m_desc.usage, TextureUsage::DepthStencil) ||
        (depth && (hasUsage(m_desc.usage, TextureUsage::Sampled) || hasUsage(m_desc.usage, TextureUsage::Storage) ||
                   hasUsage(m_desc.usage, TextureUsage::RenderTarget))))
        throw std::invalid_argument(m_desc.debugName + ": Depth textures require depth-only views in this backend.");
    if (!depth) pixelSize(m_desc.format);
    validateTextureState(m_desc, m_desc.initialState);
    const auto format = toNativeFormat(m_desc.format);
    D3D12_FEATURE_DATA_FORMAT_SUPPORT support{format};
    if (FAILED(device.CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support))) ||
        !(support.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE2D) ||
        (hasUsage(m_desc.usage, TextureUsage::Sampled) && !(support.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE)) ||
        (hasUsage(m_desc.usage, TextureUsage::Storage) && !(support.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE)) ||
        (hasUsage(m_desc.usage, TextureUsage::RenderTarget) && !(support.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET)) ||
        (depth && !(support.Support1 & D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL)))
        throw std::invalid_argument(m_desc.debugName + ": Texture format does not support the requested usage.");
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    if (hasUsage(m_desc.usage, TextureUsage::Storage)) flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    if (hasUsage(m_desc.usage, TextureUsage::RenderTarget)) flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    if (depth) flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    const auto native = CD3DX12_RESOURCE_DESC::Tex2D(format, m_desc.width, m_desc.height, 1, 1, 1, 0, flags);
    const auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_CLEAR_VALUE clear{};
    clear.Format = format;
    if (depth) clear.DepthStencil.Depth = 1;
    const bool attachment = depth || hasUsage(m_desc.usage, TextureUsage::RenderTarget);
    if (FAILED(device.CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &native, resourceState(state),
        attachment ? &clear : nullptr, IID_PPV_ARGS(&m_resource))))
        throw std::runtime_error(m_desc.debugName + ": Failed to create texture.");
    if (attachment)
    {
        const D3D12_DESCRIPTOR_HEAP_DESC heapDesc{depth ? D3D12_DESCRIPTOR_HEAP_TYPE_DSV : D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            1, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0};
        if (FAILED(device.CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_attachment))))
            throw std::runtime_error(m_desc.debugName + ": Failed to create attachment view.");
        if (depth) device.CreateDepthStencilView(m_resource.Get(), nullptr, m_attachment->GetCPUDescriptorHandleForHeapStart());
        else device.CreateRenderTargetView(m_resource.Get(), nullptr, m_attachment->GetCPUDescriptorHandleForHeapStart());
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE Texture::attachment() const
{
    if (!m_attachment) throw std::invalid_argument("Texture: No attachment view was declared.");
    return m_attachment->GetCPUDescriptorHandleForHeapStart();
}

TextureView::TextureView(const RHI::Device& owner, std::shared_ptr<Texture> texture, RHI::TextureViewDesc desc)
    : RHI::TextureView(owner), m_texture(std::move(texture)), m_desc(desc)
{
    const auto usage = desc.type == RHI::BindingType::SampledResource ? RHI::TextureUsage::Sampled : RHI::TextureUsage::Storage;
    if ((desc.type != RHI::BindingType::SampledResource && desc.type != RHI::BindingType::StorageResource) ||
        !hasUsage(m_texture->description().usage, usage))
        throw std::invalid_argument("Texture view: View type is not permitted by texture usage.");
}

void TextureView::writeDescriptor(ID3D12Device& device, D3D12_CPU_DESCRIPTOR_HANDLE handle) const
{
    if (m_desc.type == RHI::BindingType::SampledResource)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
        desc.Format = toNativeFormat(m_texture->description().format);
        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        desc.Texture2D.MipLevels = 1;
        device.CreateShaderResourceView(m_texture->native(), &desc, handle);
    }
    else
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC desc{};
        desc.Format = toNativeFormat(m_texture->description().format);
        desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        device.CreateUnorderedAccessView(m_texture->native(), nullptr, &desc, handle);
    }
}
}
