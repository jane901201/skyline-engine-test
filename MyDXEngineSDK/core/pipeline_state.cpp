#include "pipeline_state.h"
#include <stdexcept>
#include "graphics_engine.h"
#include "root_signature.h"
#include "shader.h"
#include "backends/d3d12/d3d12_pipeline.h"
#include "backends/d3d12/d3d12_interop.h"

void PipelineState::init(std::unique_ptr<Skyline::RHI::Pipeline> pipeline)
{
    if (pipeline == nullptr)
        throw std::invalid_argument("PipelineState: An owned RHI pipeline is required.");
    auto* layout = Skyline::D3D12::nativePipelineLayout(*pipeline);
    auto* native = Skyline::D3D12::nativePipeline(*pipeline);
    m_bindingLayout = layout;
    m_pipelineState = native;
    m_ownedPipeline = std::move(pipeline);
}

namespace
{
void validateShader(const Shader& shader, Skyline::RHI::ShaderStage stage, const std::string& name)
{
    if (shader.getCompiledBlob() == nullptr || shader.getStage() != stage)
        throw std::invalid_argument(name + ": A compiled shader with the required stage is needed.");
}
}

void PipelineState::init(const Skyline::RHI::GraphicsPipelineDesc& desc, const GraphicsEngine& engine,
                         const RootSignature& layout, const Shader& vertexShader, const Shader& pixelShader)
{
    if (engine.getD3DDevice() == nullptr)
        throw std::invalid_argument(desc.debugName + ": Graphics device is not initialized.");
    if (layout.get() == nullptr)
        throw std::invalid_argument(desc.debugName + ": An initialized binding layout is required.");
    validateShader(vertexShader, Skyline::RHI::ShaderStage::Vertex, desc.debugName);
    validateShader(pixelShader, Skyline::RHI::ShaderStage::Pixel, desc.debugName);

    m_pipelineState = Skyline::D3D12::createGraphicsPipeline(*engine.getD3DDevice(), layout.get(),
        {vertexShader.getCompiledBlob()->GetBufferPointer(), vertexShader.getCompiledBlob()->GetBufferSize()},
        {pixelShader.getCompiledBlob()->GetBufferPointer(), pixelShader.getCompiledBlob()->GetBufferSize()}, desc);
    m_bindingLayout = layout.get();
    m_ownedPipeline.reset();
}

void PipelineState::init(const Skyline::RHI::ComputePipelineDesc& desc, const GraphicsEngine& engine,
                         const RootSignature& layout, const Shader& computeShader)
{
    if (engine.getD3DDevice() == nullptr)
        throw std::invalid_argument(desc.debugName + ": Graphics device is not initialized.");
    if (layout.get() == nullptr)
        throw std::invalid_argument(desc.debugName + ": An initialized binding layout is required.");
    validateShader(computeShader, Skyline::RHI::ShaderStage::Compute, desc.debugName);
    m_pipelineState = Skyline::D3D12::createComputePipeline(*engine.getD3DDevice(), layout.get(),
        {computeShader.getCompiledBlob()->GetBufferPointer(), computeShader.getCompiledBlob()->GetBufferSize()}, desc);
    m_bindingLayout = layout.get();
    m_ownedPipeline.reset();
}

void PipelineState::init(D3D12_GRAPHICS_PIPELINE_STATE_DESC desc)
{
    if (g_graphicsEngine == nullptr || g_graphicsEngine->getD3DDevice() == nullptr)
        throw std::runtime_error("PipelineState: Graphics device is not initialized.");

    const auto d3dDevice = g_graphicsEngine->getD3DDevice();

    if (auto hr = d3dDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_pipelineState)); FAILED(hr))
    {
        throw std::runtime_error("PipelineState: Failed to create the graphics pipeline state.");
    }
    m_bindingLayout = desc.pRootSignature;
    m_ownedPipeline.reset();
}

void PipelineState::init(D3D12_COMPUTE_PIPELINE_STATE_DESC desc)
{
    if (g_graphicsEngine == nullptr || g_graphicsEngine->getD3DDevice() == nullptr)
        throw std::runtime_error("PipelineState: Graphics device is not initialized.");

    auto d3dDevice = g_graphicsEngine->getD3DDevice();
    auto hr = d3dDevice->CreateComputePipelineState(&desc, IID_PPV_ARGS(&m_pipelineState));
    if (FAILED(hr))
    {
        throw std::runtime_error("PipelineState: Failed to create the compute pipeline state.");
    }
    m_bindingLayout = desc.pRootSignature;
    m_ownedPipeline.reset();
}
