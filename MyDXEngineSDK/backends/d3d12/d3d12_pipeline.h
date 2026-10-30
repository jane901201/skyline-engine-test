#pragma once

#include "core/skyline_api.h"
#include "rhi/pipeline.h"

#include <directx/d3d12.h>
#include <wrl/client.h>
#include <string>
#include <vector>

namespace Skyline::D3D12
{
SKYLINE_API RHI::Format fromNativeFormat(DXGI_FORMAT format);
SKYLINE_API DXGI_FORMAT toNativeFormat(RHI::Format format);

SKYLINE_API Microsoft::WRL::ComPtr<ID3D12PipelineState> createGraphicsPipeline(
    ID3D12Device& device, ID3D12RootSignature* layout, D3D12_SHADER_BYTECODE vertexShader,
    D3D12_SHADER_BYTECODE pixelShader, const RHI::GraphicsPipelineDesc& desc);
SKYLINE_API Microsoft::WRL::ComPtr<ID3D12PipelineState> createComputePipeline(
    ID3D12Device& device, ID3D12RootSignature* layout, D3D12_SHADER_BYTECODE computeShader,
    const RHI::ComputePipelineDesc& desc);

// 持有語意名稱與輸入配置，確保建立 PSO 前的原生指標有效。
class SKYLINE_API GraphicsPipelineDescription
{
public:
    explicit GraphicsPipelineDescription(const RHI::GraphicsPipelineDesc& desc);
    GraphicsPipelineDescription(const GraphicsPipelineDescription&) = delete;
    GraphicsPipelineDescription& operator=(const GraphicsPipelineDescription&) = delete;
    GraphicsPipelineDescription(GraphicsPipelineDescription&&) = delete;
    GraphicsPipelineDescription& operator=(GraphicsPipelineDescription&&) = delete;

    [[nodiscard]] D3D12_GRAPHICS_PIPELINE_STATE_DESC get() const;

private:
    D3D12_GRAPHICS_PIPELINE_STATE_DESC m_description{};
    std::vector<std::string> m_semantics;
    std::vector<D3D12_INPUT_ELEMENT_DESC> m_input;
};
}
