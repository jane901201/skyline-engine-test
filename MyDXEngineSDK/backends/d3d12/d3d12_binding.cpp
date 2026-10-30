#include "d3d12_binding.h"

#include <directx/d3dx12_core.h>
#include <d3dcompiler.h>
#include <limits>
#include <stdexcept>
#include <vector>

namespace Skyline::D3D12
{
namespace
{
D3D12_SHADER_VISIBILITY visibility(RHI::ShaderVisibility value)
{
    switch (value)
    {
    case RHI::ShaderVisibility::All:
    case RHI::ShaderVisibility::Compute: return D3D12_SHADER_VISIBILITY_ALL;
    case RHI::ShaderVisibility::Vertex: return D3D12_SHADER_VISIBILITY_VERTEX;
    case RHI::ShaderVisibility::Pixel: return D3D12_SHADER_VISIBILITY_PIXEL;
    }
    throw std::invalid_argument("Binding layout: Unsupported shader visibility.");
}

D3D12_DESCRIPTOR_RANGE_TYPE rangeType(RHI::BindingType value)
{
    switch (value)
    {
    case RHI::BindingType::ConstantBuffer: return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    case RHI::BindingType::SampledResource: return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    case RHI::BindingType::StorageResource: return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    }
    throw std::invalid_argument("Binding layout: Unsupported resource type.");
}

D3D12_TEXTURE_ADDRESS_MODE addressMode(RHI::AddressMode value)
{
    switch (value)
    {
    case RHI::AddressMode::Wrap: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    case RHI::AddressMode::Clamp: return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    case RHI::AddressMode::Mirror: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    }
    throw std::invalid_argument("Binding layout: Unsupported sampler address mode.");
}

bool overlaps(std::uint32_t first, std::uint32_t count, std::uint32_t otherFirst, std::uint32_t otherCount)
{
    return std::uint64_t(first) < std::uint64_t(otherFirst) + otherCount &&
           std::uint64_t(otherFirst) < std::uint64_t(first) + count;
}
}

void validateBindingLayout(const RHI::BindingLayoutDesc& desc)
{
    const auto fail = [&desc](const char* message) {
        throw std::invalid_argument(desc.debugName + ": " + message);
    };
    if (desc.constantBuffers.size() > (desc.resources.empty() ? 32u : 31u))
        fail("Constant buffers and resource table exceed the D3D12 binding budget.");
    std::uint64_t totalDescriptors = 0;
    for (std::size_t index = 0; index < desc.constantBuffers.size(); ++index)
    {
        const auto& slot = desc.constantBuffers[index];
        visibility(slot.visibility);
        for (std::size_t other = 0; other < index; ++other)
            if (slot.shaderRegister == desc.constantBuffers[other].shaderRegister &&
                slot.registerSpace == desc.constantBuffers[other].registerSpace)
                fail("Constant-buffer registers overlap.");
    }
    for (std::size_t index = 0; index < desc.resources.size(); ++index)
    {
        const auto& range = desc.resources[index];
        rangeType(range.type);
        if (range.count == 0 || range.count == std::numeric_limits<std::uint32_t>::max() ||
            std::uint64_t(range.shaderRegister) + range.count > (std::uint64_t(1) << 32))
            fail("Resource ranges require a finite non-zero count and valid register span.");
        totalDescriptors += range.count;
        if (totalDescriptors > D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1)
            fail("Resource descriptor count exceeds the supported heap size.");
        for (std::size_t other = 0; other < index; ++other)
        {
            const auto& previous = desc.resources[other];
            if (range.type == previous.type && range.registerSpace == previous.registerSpace &&
                overlaps(range.shaderRegister, range.count, previous.shaderRegister, previous.count))
                fail("Resource registers overlap.");
        }
        if (range.type == RHI::BindingType::ConstantBuffer)
            for (const auto& slot : desc.constantBuffers)
                if (slot.registerSpace == range.registerSpace && overlaps(range.shaderRegister, range.count, slot.shaderRegister, 1))
                    fail("Resource constant buffers overlap constant-buffer slots.");
    }
    if (desc.samplers.size() > D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE)
        fail("Too many static samplers.");
    for (std::size_t index = 0; index < desc.samplers.size(); ++index)
    {
        const auto& sampler = desc.samplers[index];
        visibility(sampler.visibility);
        addressMode(sampler.addressU);
        addressMode(sampler.addressV);
        addressMode(sampler.addressW);
        if (sampler.filter != RHI::SamplerFilter::Linear && sampler.filter != RHI::SamplerFilter::Point)
            fail("Unsupported sampler filter.");
        for (std::size_t other = 0; other < index; ++other)
            if (sampler.shaderRegister == desc.samplers[other].shaderRegister &&
                sampler.registerSpace == desc.samplers[other].registerSpace)
                fail("Sampler registers overlap.");
    }
}

Microsoft::WRL::ComPtr<ID3DBlob> serializeBindingLayout(const RHI::BindingLayoutDesc& desc)
{
    validateBindingLayout(desc);
    std::vector<CD3DX12_ROOT_PARAMETER> parameters(desc.constantBuffers.size() + (desc.resources.empty() ? 0 : 1));
    for (std::size_t index = 0; index < desc.constantBuffers.size(); ++index)
    {
        const auto& slot = desc.constantBuffers[index];
        parameters[index].InitAsConstantBufferView(slot.shaderRegister, slot.registerSpace, visibility(slot.visibility));
    }
    std::vector<CD3DX12_DESCRIPTOR_RANGE> ranges(desc.resources.size());
    for (std::size_t index = 0; index < desc.resources.size(); ++index)
    {
        const auto& range = desc.resources[index];
        ranges[index].Init(rangeType(range.type), range.count, range.shaderRegister, range.registerSpace);
    }
    if (!ranges.empty())
        parameters.back().InitAsDescriptorTable(static_cast<UINT>(ranges.size()), ranges.data());

    std::vector<D3D12_STATIC_SAMPLER_DESC> samplers(desc.samplers.size());
    for (std::size_t index = 0; index < desc.samplers.size(); ++index)
    {
        const auto& source = desc.samplers[index];
        auto& sampler = samplers[index];
        sampler.ShaderRegister = source.shaderRegister;
        sampler.RegisterSpace = source.registerSpace;
        sampler.ShaderVisibility = visibility(source.visibility);
        sampler.Filter = source.filter == RHI::SamplerFilter::Linear ? D3D12_FILTER_MIN_MAG_MIP_LINEAR : D3D12_FILTER_MIN_MAG_MIP_POINT;
        sampler.AddressU = addressMode(source.addressU);
        sampler.AddressV = addressMode(source.addressV);
        sampler.AddressW = addressMode(source.addressW);
        sampler.MaxAnisotropy = 1;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
    }
    D3D12_ROOT_SIGNATURE_DESC native{};
    native.NumParameters = static_cast<UINT>(parameters.size());
    native.pParameters = parameters.data();
    native.NumStaticSamplers = static_cast<UINT>(samplers.size());
    native.pStaticSamplers = samplers.data();
    native.Flags = desc.allowVertexInput ? D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT : D3D12_ROOT_SIGNATURE_FLAG_NONE;
    Microsoft::WRL::ComPtr<ID3DBlob> serialized, errors;
    if (FAILED(D3D12SerializeRootSignature(&native, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors)))
    {
        std::string message = desc.debugName + ": Failed to serialize binding layout.";
        if (errors)
            message.append(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize());
        throw std::runtime_error(message);
    }
    return serialized;
}

Microsoft::WRL::ComPtr<ID3D12RootSignature> createBindingLayout(ID3D12Device& device, const RHI::BindingLayoutDesc& desc)
{
    const auto serialized = serializeBindingLayout(desc);
    Microsoft::WRL::ComPtr<ID3D12RootSignature> layout;
    const auto result = device.CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&layout));
    if (FAILED(result))
        throw std::runtime_error(desc.debugName + ": Failed to create binding layout; HRESULT " + std::to_string(result));
    return layout;
}
}
