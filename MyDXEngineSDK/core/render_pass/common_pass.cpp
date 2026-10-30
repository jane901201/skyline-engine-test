#include "common_pass.h"
#include "cloudscape_camera.h"

#include <stdexcept>

#include <directx/d3dx12_core.h>

#include "graphics_engine.h"
#include "root_signature.h"
#include "render_context.h"

namespace
{
[[deprecated("Unused after the RHI pipeline refactor. Use GraphicsPipelineDesc::fullscreen().")]]
D3D12_GRAPHICS_PIPELINE_STATE_DESC createPipelineStateDescription(
    ID3D12RootSignature* rootSignature, ID3DBlob* vertexShader, ID3DBlob* pixelShader)
{
    if (rootSignature == nullptr)
    {
        throw std::invalid_argument("CommonPass: Root signature is required.");
    }

    if (vertexShader == nullptr)
    {
        throw std::invalid_argument("CommonPass: Vertex shader is required.");
    }

    if (pixelShader == nullptr)
    {
        throw std::invalid_argument("CommonPass: Pixel shader is required.");
    }

     D3D12_GRAPHICS_PIPELINE_STATE_DESC description{};

    description.InputLayout = {nullptr, 0};
    description.pRootSignature = rootSignature;
    description.VS = CD3DX12_SHADER_BYTECODE(vertexShader);
    description.PS = CD3DX12_SHADER_BYTECODE(pixelShader);

    description.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    description.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    description.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

    description.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    description.DepthStencilState.DepthEnable = FALSE;
    description.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    description.DepthStencilState.StencilEnable = FALSE;

    description.SampleMask = UINT_MAX;
    description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    description.NumRenderTargets = 1;
    description.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    description.SampleDesc.Count = 1;

    return description;
}
}

void CommonPass::updateCameraConstants(UINT width, UINT height)
{
    const float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    const Vector3& cameraPosition = CloudscapeCamera::position;
    const Vector3& cameraTarget = CloudscapeCamera::target;
    const Vector3& worldUp = CloudscapeCamera::up;

    Matrix view;
    view.MakeLookAt(cameraPosition, cameraTarget, worldUp);
    Matrix projection;
    projection.MakeProjectionMatrix(Math::PI / 3.0f, aspectRatio, 0.1f, 1000.0f);

    // 引擎與 HLSL 統一使用 row_major，因此直接上傳反矩陣。
    m_cameraConstants.inverseView.Inverse(view);
    m_cameraConstants.inverseProjection.Inverse(projection);
    m_cameraConstants.cameraPosition = cameraPosition;
    m_cameraConstants.padding = 0.0f;
}

void CommonPass::init(const RenderPassInitContext& context)
{
    m_vertexShader.loadVS("assets/shaders/common/common.hlsl", "VSMain");

    m_pixelShader.loadPS("assets/shaders/common/common.hlsl", "PSMain");

    auto description = Skyline::RHI::GraphicsPipelineDesc::fullscreen();
    description.debugName = "CommonPass";
    description.attachments = context.graphicsEngine.getBackBufferLayout();
    m_pipelineState.init(description, context.graphicsEngine, context.rootSignature, m_vertexShader, m_pixelShader);

    const UINT width = context.graphicsEngine.getFrameBufferWidth();
    const UINT height = context.graphicsEngine.GetFrameBufferHeight();
    if (width == 0 || height == 0)
    {
        throw std::runtime_error("CommonPass: Frame-buffer dimensions must be non-zero during initialization.");
    }

    updateCameraConstants(width, height);
    m_cameraConstantBuffer.init(sizeof(CameraConstants), &m_cameraConstants);
}

void CommonPass::init(const RhiRenderPassInitContext& context)
{
    init(PassInitContext{context.device, context.graphicsEngine.getBackBufferLayout(),
        context.graphicsEngine.getFrameBufferWidth(), context.graphicsEngine.GetFrameBufferHeight()});
}
void CommonPass::init(const PassInitContext& context)
{
    m_rhiVertexShader = context.device.createShader({"assets/shaders/common/common.hlsl", "VSMain", Skyline::RHI::ShaderStage::Vertex});
    m_rhiPixelShader = context.device.createShader({"assets/shaders/common/common.hlsl", "PSMain", Skyline::RHI::ShaderStage::Pixel});
    auto desc = Skyline::RHI::GraphicsPipelineDesc::fullscreen();
    desc.debugName = "CommonPass";
    desc.vertexShader = m_rhiVertexShader.get();
    desc.pixelShader = m_rhiPixelShader.get();
    Skyline::RHI::BindingLayoutDesc bindingDesc;
    bindingDesc.constantBuffers = {{0}};
    auto layout = context.device.createBindingLayout(bindingDesc);
    desc.bindingLayout = layout.get();
    desc.attachments = context.attachments;
    m_rhiPipeline = context.device.createGraphicsPipeline(desc);
    const auto width = context.width;
    const auto height = context.height;
    if (width == 0 || height == 0)
        throw std::runtime_error("CommonPass: Frame-buffer dimensions must be non-zero during initialization.");
    updateCameraConstants(width, height);
    m_rhiResources.init(context.device, *layout, sizeof(CameraConstants));
    m_useRhiResources = true;
}

void CommonPass::executeLegacy(const RenderFrameContext& context)
{
    if (context.width == 0 || context.height == 0)
    {
        return;
    }

#ifdef SKYLINE_ENABLE_TRACY
    TracyD3D12ZoneS(context.graphicsEngine.getProfiler().gpuContext(),
                    context.graphicsEngine.getCommandList(), "Common pass", 4);
#endif

    updateCameraConstants(context.width, context.height);
    m_cameraConstantBuffer.copyToVRAM(m_cameraConstants);

    context.renderContext.setGraphicsPipeline(m_pipelineState);
    context.renderContext.setGraphicsRootConstantBufferView(
        0, m_cameraConstantBuffer.getGPUVirtualAddress());
    context.renderContext.draw(3);
}

void CommonPass::execute(const RenderFrameContext& context)
{
    if (!m_useRhiResources) { executeLegacy(context); return; }
    execute(PassFrameContext{context.graphicsEngine.getRhiCommandContext(),
        context.graphicsEngine.getBackBufferIndex(), context.width, context.height});
}
void CommonPass::execute(const PassFrameContext& context)
{

    if (context.width == 0 || context.height == 0) return;

    const auto frame = context.frameIndex;
    updateCameraConstants(context.width, context.height);
    m_rhiResources.constants[frame]->write(0, std::as_bytes(std::span{&m_cameraConstants, 1}));
    context.commands.setPipeline(*m_rhiPipeline);
    context.commands.setBindingSet(*m_rhiResources.bindings[frame]);
    context.commands.draw(3);
}
