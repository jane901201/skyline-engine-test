#include <catch2/catch_test_macros.hpp>
#include "backends/d3d12/d3d12_binding.h"

namespace
{
Microsoft::WRL::ComPtr<ID3D12RootSignatureDeserializer> deserialize(
    const Skyline::RHI::BindingLayoutDesc& desc)
{
    const auto bytes = Skyline::D3D12::serializeBindingLayout(desc);
    Microsoft::WRL::ComPtr<ID3D12RootSignatureDeserializer> result;
    REQUIRE(SUCCEEDED(D3D12CreateRootSignatureDeserializer(
        bytes->GetBufferPointer(), bytes->GetBufferSize(), IID_PPV_ARGS(&result))));
    return result;
}
}

TEST_CASE("RHI scene serialization preserves the RenderState binding contract", "[rhi]")
{
    const auto decoded = deserialize(Skyline::RHI::BindingLayoutDesc::scene());
    const auto& native = *decoded->GetRootSignatureDesc();
    REQUIRE(native.Flags == D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    REQUIRE(native.NumParameters == 5);
    for (UINT index = 0; index < 4; ++index)
    {
        const auto& parameter = native.pParameters[index];
        REQUIRE(parameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV);
        REQUIRE(parameter.Descriptor.ShaderRegister == index);
        REQUIRE(parameter.Descriptor.RegisterSpace == 0);
        REQUIRE(parameter.ShaderVisibility == D3D12_SHADER_VISIBILITY_ALL);
    }
    const auto& table = native.pParameters[4];
    REQUIRE(table.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE);
    REQUIRE(table.ShaderVisibility == D3D12_SHADER_VISIBILITY_ALL);
    REQUIRE(table.DescriptorTable.NumDescriptorRanges == 3);
    const D3D12_DESCRIPTOR_RANGE_TYPE types[] = {
        D3D12_DESCRIPTOR_RANGE_TYPE_CBV, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, D3D12_DESCRIPTOR_RANGE_TYPE_UAV};
    const UINT registers[] = {4, 0, 0};
    const UINT counts[] = {4, 32, 8};
    const UINT offsets[] = {0, 4, 36};
    UINT nextOffset = 0;
    for (UINT index = 0; index < 3; ++index)
    {
        const auto& range = table.DescriptorTable.pDescriptorRanges[index];
        REQUIRE(range.RangeType == types[index]);
        REQUIRE(range.BaseShaderRegister == registers[index]);
        REQUIRE(range.RegisterSpace == 0);
        REQUIRE(range.NumDescriptors == counts[index]);
        // APPEND 與明確偏移皆須維持既有資源表的實際位置。
        const UINT offset = range.OffsetInDescriptorsFromTableStart == D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
            ? nextOffset : range.OffsetInDescriptorsFromTableStart;
        REQUIRE(offset == offsets[index]);
        nextOffset = offset + range.NumDescriptors;
    }
    REQUIRE(native.NumStaticSamplers == 1);
    const auto& sampler = native.pStaticSamplers[0];
    REQUIRE(sampler.ShaderRegister == 0);
    REQUIRE(sampler.RegisterSpace == 0);
    REQUIRE(sampler.ShaderVisibility == D3D12_SHADER_VISIBILITY_PIXEL);
    REQUIRE(sampler.Filter == D3D12_FILTER_MIN_MAG_MIP_LINEAR);
    REQUIRE(sampler.AddressU == D3D12_TEXTURE_ADDRESS_MODE_WRAP);
    REQUIRE(sampler.AddressV == D3D12_TEXTURE_ADDRESS_MODE_WRAP);
    REQUIRE(sampler.AddressW == D3D12_TEXTURE_ADDRESS_MODE_WRAP);
}

TEST_CASE("RHI serialization preserves custom spaces visibility and sampler modes", "[rhi]")
{
    using namespace Skyline::RHI;
    BindingLayoutDesc desc;
    desc.allowVertexInput = false;
    desc.constantBuffers = {{2, 3, ShaderVisibility::Vertex}, {4, 5, ShaderVisibility::Pixel},
                            {6, 7, ShaderVisibility::Compute}};
    desc.resources = {{BindingType::SampledResource, 8, 2, 9}};
    desc.samplers = {{10, 11, ShaderVisibility::Compute, SamplerFilter::Point,
                      AddressMode::Clamp, AddressMode::Mirror, AddressMode::Wrap}};
    const auto decoded = deserialize(desc);
    const auto& native = *decoded->GetRootSignatureDesc();
    REQUIRE(native.Flags == D3D12_ROOT_SIGNATURE_FLAG_NONE);
    REQUIRE(native.NumParameters == 4);
    const D3D12_SHADER_VISIBILITY visibility[] = {
        D3D12_SHADER_VISIBILITY_VERTEX, D3D12_SHADER_VISIBILITY_PIXEL, D3D12_SHADER_VISIBILITY_ALL};
    for (UINT index = 0; index < 3; ++index)
    {
        REQUIRE(native.pParameters[index].ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV);
        REQUIRE(native.pParameters[index].Descriptor.ShaderRegister == desc.constantBuffers[index].shaderRegister);
        REQUIRE(native.pParameters[index].Descriptor.RegisterSpace == desc.constantBuffers[index].registerSpace);
        REQUIRE(native.pParameters[index].ShaderVisibility == visibility[index]);
    }
    const auto& table = native.pParameters[3];
    REQUIRE(table.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE);
    REQUIRE(table.ShaderVisibility == D3D12_SHADER_VISIBILITY_ALL);
    REQUIRE(table.DescriptorTable.NumDescriptorRanges == 1);
    const auto& range = table.DescriptorTable.pDescriptorRanges[0];
    REQUIRE(range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV);
    REQUIRE(range.BaseShaderRegister == 8);
    REQUIRE(range.RegisterSpace == 9);
    REQUIRE(range.NumDescriptors == 2);
    REQUIRE(native.NumStaticSamplers == 1);
    const auto& sampler = native.pStaticSamplers[0];
    REQUIRE(sampler.ShaderRegister == 10);
    REQUIRE(sampler.RegisterSpace == 11);
    REQUIRE(sampler.ShaderVisibility == D3D12_SHADER_VISIBILITY_ALL);
    REQUIRE(sampler.Filter == D3D12_FILTER_MIN_MAG_MIP_POINT);
    REQUIRE(sampler.AddressU == D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    REQUIRE(sampler.AddressV == D3D12_TEXTURE_ADDRESS_MODE_MIRROR);
    REQUIRE(sampler.AddressW == D3D12_TEXTURE_ADDRESS_MODE_WRAP);
}

TEST_CASE("RHI empty serialization has no phantom parameters or samplers", "[rhi]")
{
    Skyline::RHI::BindingLayoutDesc desc;
    desc.allowVertexInput = false;
    const auto decoded = deserialize(desc);
    const auto& native = *decoded->GetRootSignatureDesc();
    REQUIRE(native.NumParameters == 0);
    REQUIRE(native.NumStaticSamplers == 0);
    REQUIRE(native.Flags == D3D12_ROOT_SIGNATURE_FLAG_NONE);
}
