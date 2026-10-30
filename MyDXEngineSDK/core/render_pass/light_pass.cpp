#include "light_pass.h"
#include "cloudscape_camera.h"

#include <stdexcept>

#include <directx/d3dx12_core.h>

#include "graphics_engine.h"
#include "root_signature.h"
#include "render_context.h"

namespace
{
[[deprecated("Unused after the RHI pipeline refactor. Use GraphicsPipelineDesc::fullscreen().")]]
D3D12_GRAPHICS_PIPELINE_STATE_DESC createPipelineStateDescription(ID3D12RootSignature* rootSignature,
                                                                  ID3DBlob* vertexShader, ID3DBlob* pixelShader)
{
    if (rootSignature == nullptr)
    {
        throw std::invalid_argument("LightPass: Root signature is required.");
    }

    if (vertexShader == nullptr)
    {
        throw std::invalid_argument("LightPass: Vertex shader is required.");
    }

    if (pixelShader == nullptr)
    {
        throw std::invalid_argument("LightPass: Pixel shader is required.");
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC description{};

    description.InputLayout = {nullptr, 0};
    description.pRootSignature = rootSignature;
    description.VS = CD3DX12_SHADER_BYTECODE(vertexShader);
    description.PS = CD3DX12_SHADER_BYTECODE(pixelShader);

    description.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    description.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    description.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

    D3D12_RENDER_TARGET_BLEND_DESC& renderTarget = description.BlendState.RenderTarget[0];

    renderTarget.BlendEnable = TRUE;
    renderTarget.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    renderTarget.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    renderTarget.BlendOp = D3D12_BLEND_OP_ADD;
    renderTarget.SrcBlendAlpha = D3D12_BLEND_ONE;
    renderTarget.DestBlendAlpha = D3D12_BLEND_ZERO;
    renderTarget.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    renderTarget.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

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

LightPass::LightPass(const CloudSettings& settings)
{
    setSettings(settings);
}

void LightPass::setSettings(const CloudSettings& settings)
{
    m_constants.sunDirection = settings.sunDirection;
    m_constants.sunDirection.Normalize();
    m_constants.sunRadius = 0.055f;
    m_constants.sunColor = settings.sunColor;
    m_constants.sunIntensity = settings.sunIntensity;
}

void LightPass::updateCameraConstants(UINT width, UINT height)
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
    m_constants.inverseView.Inverse(view);
    m_constants.inverseProjection.Inverse(projection);
}

void LightPass::init(const RenderPassInitContext& context)
{
    m_vertexShader.loadVS("assets/shaders/sun.hlsl", "VSMain");

    m_pixelShader.loadPS("assets/shaders/sun.hlsl", "PSMain");

    auto description = Skyline::RHI::GraphicsPipelineDesc::fullscreen();
    description.debugName = "LightPass";
    description.attachments = context.graphicsEngine.getBackBufferLayout();
    description.blend[0] = Skyline::RHI::BlendDesc::alpha();
    // 太陽沿用原本的 Alpha 覆寫方式，與雲層的 Alpha 累積不同。
    description.blend[0].destinationAlpha = Skyline::RHI::BlendFactor::Zero;
    m_pipelineState.init(description, context.graphicsEngine, context.rootSignature, m_vertexShader, m_pixelShader);

    const UINT width = context.graphicsEngine.getFrameBufferWidth();
    const UINT height = context.graphicsEngine.GetFrameBufferHeight();
    if (width == 0 || height == 0)
        throw std::runtime_error("LightPass: Frame-buffer dimensions must be non-zero during initialization.");

    updateCameraConstants(width, height);
    m_constantBuffer.init(sizeof(SunConstants), &m_constants);
}

void LightPass::init(const RhiRenderPassInitContext& context)
{
    init(PassInitContext{context.device, context.graphicsEngine.getBackBufferLayout(),
        context.graphicsEngine.getFrameBufferWidth(), context.graphicsEngine.GetFrameBufferHeight()});
}
void LightPass::init(const PassInitContext& context)
{
    m_rhiVertexShader = context.device.createShader({"assets/shaders/sun.hlsl", "VSMain", Skyline::RHI::ShaderStage::Vertex});
    m_rhiPixelShader = context.device.createShader({"assets/shaders/sun.hlsl", "PSMain", Skyline::RHI::ShaderStage::Pixel});
    auto desc = Skyline::RHI::GraphicsPipelineDesc::fullscreen();
    desc.debugName = "LightPass";
    desc.vertexShader = m_rhiVertexShader.get();
    desc.pixelShader = m_rhiPixelShader.get();
    Skyline::RHI::BindingLayoutDesc bindingDesc;
    bindingDesc.constantBuffers = {{0}};
    auto layout = context.device.createBindingLayout(bindingDesc);
    desc.bindingLayout = layout.get();
    desc.attachments = context.attachments;
    desc.blend[0] = Skyline::RHI::BlendDesc::alpha();
    desc.blend[0].destinationAlpha = Skyline::RHI::BlendFactor::Zero;
    m_rhiPipeline = context.device.createGraphicsPipeline(desc);
    const auto width = context.width;
    const auto height = context.height;
    if (width == 0 || height == 0)
        throw std::runtime_error("LightPass: Frame-buffer dimensions must be non-zero during initialization.");
    updateCameraConstants(width, height);
    m_rhiResources.init(context.device, *layout, sizeof(SunConstants));
    m_useRhiResources = true;
}

void LightPass::executeLegacy(const RenderFrameContext& context)
{
    if (context.height == 0)
    {
        return;
    }

#ifdef SKYLINE_ENABLE_TRACY
    TracyD3D12ZoneS(context.graphicsEngine.getProfiler().gpuContext(),
                    context.graphicsEngine.getCommandList(), "Light pass", 4);
#endif

    updateCameraConstants(context.width, context.height);
    m_constantBuffer.copyToVRAM(m_constants);

    context.renderContext.setGraphicsPipeline(m_pipelineState);
    context.renderContext.setGraphicsRootConstantBufferView(0, m_constantBuffer.getGPUVirtualAddress());
    context.renderContext.draw(3);
}


void LightPass::execute(const RenderFrameContext& context)
{
    if (!m_useRhiResources) { executeLegacy(context); return; }
    execute(PassFrameContext{context.graphicsEngine.getRhiCommandContext(),
        context.graphicsEngine.getBackBufferIndex(), context.width, context.height});
}
void LightPass::execute(const PassFrameContext& context)
{

    if (context.width == 0 || context.height == 0) return;

    const auto frame = context.frameIndex;
    updateCameraConstants(context.width, context.height);
    m_rhiResources.constants[frame]->write(0, std::as_bytes(std::span{&m_constants, 1}));
    context.commands.setPipeline(*m_rhiPipeline);
    context.commands.setBindingSet(*m_rhiResources.bindings[frame]);
    context.commands.draw(3);
}
