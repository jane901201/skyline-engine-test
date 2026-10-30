#pragma once
#include "skyline_api.h"
#include "rhi/pipeline.h"
#include <directx/d3d12.h>
#include <wrl/client.h>
#include <memory>

class GraphicsEngine;
class RootSignature;
class Shader;

class SKYLINE_API PipelineState
{
public:
    // 相容命令層暫時持有中立管線；原生 COM 參考只用於既有指令綁定。
    void init(std::unique_ptr<Skyline::RHI::Pipeline> pipeline);
    [[nodiscard]] ID3D12RootSignature* getBindingLayout() const noexcept { return m_bindingLayout.Get(); }
    // 第一階段借用既有裝置、Shader 與綁定物件，避免查詢全域裝置。
    [[deprecated("Replaced by RHI Device ownership. Use device.createGraphicsPipeline(desc).")]]
    void init(const Skyline::RHI::GraphicsPipelineDesc& desc, const GraphicsEngine& engine,
              const RootSignature& layout, const Shader& vertexShader, const Shader& pixelShader);
    [[deprecated("Replaced by RHI Device ownership. Use device.createComputePipeline(desc).")]]
    void init(const Skyline::RHI::ComputePipelineDesc& desc, const GraphicsEngine& engine,
              const RootSignature& layout, const Shader& computeShader);

    // 初始化 Graphics Pipeline State
    [[deprecated("Replaced by the RHI pipeline refactor. Use init(GraphicsPipelineDesc, engine, layout, vertexShader, pixelShader).")]]
    void init(D3D12_GRAPHICS_PIPELINE_STATE_DESC decs);
    // 初始化 Compute Pipeline State
    [[deprecated("Replaced by the RHI pipeline refactor. Use init(ComputePipelineDesc, engine, layout, computeShader).")]]
    void init(D3D12_COMPUTE_PIPELINE_STATE_DESC desc);

    [[nodiscard]]
    ID3D12PipelineState* get() const
    {
        return m_pipelineState.Get();
    }

private:
    std::unique_ptr<Skyline::RHI::Pipeline> m_ownedPipeline;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_bindingLayout;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
};


