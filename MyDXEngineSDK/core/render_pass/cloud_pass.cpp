#include "cloud_pass.h"
#include "cloudscape_camera.h"

#include <directx/d3dx12_core.h>
#include <stdexcept>

#include "graphics_engine.h"
#include "render_context.h"
#include "root_signature.h"

namespace
{
constexpr UINT computeThreadCount = 64;

[[deprecated("Unused after the RHI pipeline refactor. Use GraphicsPipelineDesc::fullscreen().")]]
D3D12_GRAPHICS_PIPELINE_STATE_DESC createGraphicsDescription(
    ID3D12RootSignature* rootSignature, ID3DBlob* vertexShader, ID3DBlob* pixelShader)
{
    if (rootSignature == nullptr || vertexShader == nullptr || pixelShader == nullptr)
        throw std::invalid_argument("CloudPass: A root signature and compiled graphics shaders are required.");

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
    renderTarget.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
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

CloudPass::CloudPass(const CloudSettings& settings) : m_settings(settings)
{
    setSettings(settings);
}

void CloudPass::setSettings(const CloudSettings& settings)
{
    m_settings = settings;
    m_cloudConstants.cloudCount = settings.cloudCount;
    m_cloudConstants.cloudBottomHeight = settings.bottomHeight;
    m_cloudConstants.cloudTopHeight = settings.topHeight;
    m_cloudConstants.cloudCellSize = settings.cellSize;
    m_cloudConstants.densityScale = settings.densityScale;
    m_cloudConstants.cloudSpeed = settings.speed;
    m_cloudConstants.extinction = settings.extinction;
    m_cloudConstants.rayMarchSteps = settings.rayMarchSteps;
    m_cloudConstants.color = settings.color;
    m_cloudConstants.coverage = settings.coverage;
    m_cloudConstants.densityMultiplier = settings.densityMultiplier;
    m_cloudConstants.baseNoiseScale = settings.baseNoiseScale;
    m_cloudConstants.detailNoiseScale = settings.detailNoiseScale;
    m_cloudConstants.detailErosion = settings.detailErosion;
    m_cloudConstants.bottomFadeDistance = settings.bottomFadeDistance;
    m_cloudConstants.topFadeDistance = settings.topFadeDistance;
    m_cloudConstants.lightAbsorption = settings.lightAbsorption;
    m_cloudConstants.noiseIterations = settings.noiseIterations;
    m_cloudConstants.lightMarchSteps = settings.lightMarchSteps;
    m_cloudConstants.lightStepSize = settings.lightStepSize;
    m_cloudConstants.padding = 0.0f;
    m_cloudConstants.sunDirection = settings.sunDirection;
    m_cloudConstants.sunDirection.Normalize();
    m_cloudConstants.sunIntensity = settings.sunIntensity;
    m_cloudConstants.sunColor = settings.sunColor;
    m_cloudConstants.sunPadding = 0.0f;
}

void CloudPass::updateCameraConstants(UINT width, UINT height)
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

void CloudPass::init(const RenderPassInitContext& context)
{
    m_rootSignature = context.rootSignature.get();
    if (m_rootSignature == nullptr)
        throw std::runtime_error("CloudPass: Root signature is required.");

    m_computeShader.loadCS("assets/shaders/cloud.hlsl", "CSMain");
    m_vertexShader.loadVS("assets/shaders/cloud.hlsl", "VSMain");
    m_pixelShader.loadPS("assets/shaders/cloud.hlsl", "PSMain");

    Skyline::RHI::ComputePipelineDesc computeDescription;
    computeDescription.debugName = "CloudPass compute";
    m_computePipelineState.init(computeDescription, context.graphicsEngine, context.rootSignature, m_computeShader);
    auto graphicsDescription = Skyline::RHI::GraphicsPipelineDesc::fullscreen();
    graphicsDescription.debugName = "CloudPass graphics";
    graphicsDescription.attachments = context.graphicsEngine.getBackBufferLayout();
    graphicsDescription.blend[0] = Skyline::RHI::BlendDesc::alpha();
    m_graphicsPipelineState.init(graphicsDescription, context.graphicsEngine, context.rootSignature,
                                m_vertexShader, m_pixelShader);

    const UINT width = context.graphicsEngine.getFrameBufferWidth();
    const UINT height = context.graphicsEngine.GetFrameBufferHeight();
    if (width == 0 || height == 0)
        throw std::runtime_error("CloudPass: Frame-buffer dimensions must be non-zero during initialization.");

    updateCameraConstants(width, height);
    m_cameraConstantBuffer.init(sizeof(CameraConstants), &m_cameraConstants);
    m_cloudConstantBuffer.init(sizeof(CloudConstants), &m_cloudConstants);
    m_cloudBuffer.Init(sizeof(CloudData), static_cast<int>(m_cloudConstants.cloudCount), nullptr);

    ID3D12Device* device = context.graphicsEngine.getD3DDevice();
    m_renderState.init(device);
    m_renderState.setRWStructuredBuffer(0, 0, m_cloudBuffer);

    m_startTime = std::chrono::steady_clock::now();
}

void CloudPass::init(const RhiRenderPassInitContext& context)
{
    init(PassInitContext{context.device, context.graphicsEngine.getBackBufferLayout(),
        context.graphicsEngine.getFrameBufferWidth(), context.graphicsEngine.GetFrameBufferHeight()});
}
void CloudPass::init(const PassInitContext& context)
{
    using namespace Skyline::RHI;
    BindingLayoutDesc bindingDesc;
    bindingDesc.constantBuffers = {{0}, {1}};
    bindingDesc.resources = {{BindingType::SampledResource, 0, 1}, {BindingType::StorageResource, 0, 1}};
    auto layout = context.device.createBindingLayout(bindingDesc);
    m_rhiComputeShader = context.device.createShader({"assets/shaders/cloud.hlsl", "CSMain", Skyline::RHI::ShaderStage::Compute});
    m_rhiVertexShader = context.device.createShader({"assets/shaders/cloud.hlsl", "VSMain", Skyline::RHI::ShaderStage::Vertex});
    m_rhiPixelShader = context.device.createShader({"assets/shaders/cloud.hlsl", "PSMain", Skyline::RHI::ShaderStage::Pixel});
    Skyline::RHI::ComputePipelineDesc compute;
    compute.debugName = "CloudPass compute";
    compute.computeShader = m_rhiComputeShader.get();
    compute.bindingLayout = layout.get();
    m_rhiComputePipeline = context.device.createComputePipeline(compute);
    auto graphics = Skyline::RHI::GraphicsPipelineDesc::fullscreen();
    graphics.debugName = "CloudPass graphics";
    graphics.vertexShader = m_rhiVertexShader.get();
    graphics.pixelShader = m_rhiPixelShader.get();
    graphics.bindingLayout = layout.get();
    graphics.attachments = context.attachments;
    graphics.blend[0] = Skyline::RHI::BlendDesc::alpha();
    m_rhiGraphicsPipeline = context.device.createGraphicsPipeline(graphics);

    const auto width = context.width;
    const auto height = context.height;
    if (width == 0 || height == 0)
        throw std::runtime_error("CloudPass: Frame-buffer dimensions must be non-zero during initialization.");
    updateCameraConstants(width, height);
    const std::uint64_t cloudSize = std::uint64_t(sizeof(CloudData)) * m_cloudConstants.cloudCount;
    for (std::size_t frame = 0; frame < Skyline::RhiPassResources::frameCount; ++frame)
    {
        m_rhiCameraBuffers[frame] = context.device.createBuffer({sizeof(CameraConstants), BufferUsage::Constant,
            MemoryType::Upload, ResourceState::GenericRead});
        m_rhiCloudConstants[frame] = context.device.createBuffer({sizeof(CloudConstants), BufferUsage::Constant,
            MemoryType::Upload, ResourceState::GenericRead});
        m_rhiCloudBuffers[frame] = context.device.createBuffer({cloudSize, BufferUsage::Sampled | BufferUsage::Storage,
            MemoryType::Gpu, ResourceState::Storage});
        auto camera = context.device.createBufferView(m_rhiCameraBuffers[frame], {BindingType::ConstantBuffer, 0, sizeof(CameraConstants)});
        auto constants = context.device.createBufferView(m_rhiCloudConstants[frame], {BindingType::ConstantBuffer, 0, sizeof(CloudConstants)});
        auto sampled = context.device.createBufferView(m_rhiCloudBuffers[frame], {BindingType::SampledResource, 0, cloudSize, sizeof(CloudData)});
        auto storage = context.device.createBufferView(m_rhiCloudBuffers[frame], {BindingType::StorageResource, 0, cloudSize, sizeof(CloudData)});
        m_rhiBindings[frame] = context.device.createBindingSet({layout.get(),
            {{0, 0, camera}, {1, 0, constants}, {0, 0, sampled}, {0, 0, storage}}});
    }
    m_startTime = std::chrono::steady_clock::now();
    m_useRhiResources = true;
}

void CloudPass::executeLegacy(const RenderFrameContext& context)
{
    if (context.width == 0 || context.height == 0)
        return;

#ifdef SKYLINE_ENABLE_TRACY
    TracyD3D12ZoneS(context.graphicsEngine.getProfiler().gpuContext(),
                    context.graphicsEngine.getCommandList(), "Cloud pass", 4);
#endif

    const UINT frameIndex = g_graphicsEngine->getBackBufferIndex();
    ID3D12Resource* cloudResource = m_cloudBuffer.getD3DResource();

    updateCameraConstants(context.width, context.height);
    m_cloudConstants.time = std::chrono::duration<float>(std::chrono::steady_clock::now() - m_startTime).count();
    m_cameraConstantBuffer.copyToVRAM(m_cameraConstants);
    m_cloudConstantBuffer.copyToVRAM(m_cloudConstants);

    context.renderContext.setComputePipeline(m_computePipelineState);
    m_renderState.setConstantBuffer(context.renderContext, ShaderStage::Compute, 1, m_cloudConstantBuffer);
    m_renderState.applyCompute(context.renderContext, frameIndex);
    context.renderContext.dispatch((m_cloudConstants.cloudCount + computeThreadCount - 1) / computeThreadCount, 1, 1);

    D3D12_RESOURCE_BARRIER uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(cloudResource);
    context.renderContext.resourceBarrier(uavBarrier);
    D3D12_RESOURCE_BARRIER toShaderResource = CD3DX12_RESOURCE_BARRIER::Transition(
        cloudResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    context.renderContext.resourceBarrier(toShaderResource);

    context.renderContext.setGraphicsPipeline(m_graphicsPipelineState);
    m_renderState.setConstantBuffer(context.renderContext, ShaderStage::Graphics, 0, m_cameraConstantBuffer);
    m_renderState.setConstantBuffer(context.renderContext, ShaderStage::Graphics, 1, m_cloudConstantBuffer);
    m_renderState.applyGraphics(context.renderContext, frameIndex);
    context.renderContext.draw(3);

    D3D12_RESOURCE_BARRIER toUnorderedAccess = CD3DX12_RESOURCE_BARRIER::Transition(
        cloudResource,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    context.renderContext.resourceBarrier(toUnorderedAccess);
}

void CloudPass::execute(const RenderFrameContext& context)
{
    if (!m_useRhiResources) { executeLegacy(context); return; }
    execute(PassFrameContext{context.graphicsEngine.getRhiCommandContext(),
        context.graphicsEngine.getBackBufferIndex(), context.width, context.height});
}
void CloudPass::execute(const PassFrameContext& context)
{

    if (context.width == 0 || context.height == 0) return;

    using Skyline::RHI::ResourceState;
    const auto frame = context.frameIndex;
    updateCameraConstants(context.width, context.height);
    m_cloudConstants.time = std::chrono::duration<float>(std::chrono::steady_clock::now() - m_startTime).count();
    m_rhiCameraBuffers[frame]->write(0, std::as_bytes(std::span{&m_cameraConstants, 1}));
    m_rhiCloudConstants[frame]->write(0, std::as_bytes(std::span{&m_cloudConstants, 1}));
    context.commands.setPipeline(*m_rhiComputePipeline);
    context.commands.setBindingSet(*m_rhiBindings[frame]);
    context.commands.dispatch((m_cloudConstants.cloudCount + computeThreadCount - 1) / computeThreadCount, 1, 1);
    context.commands.storageBarrier(*m_rhiCloudBuffers[frame]);
    context.commands.transition(*m_rhiCloudBuffers[frame], ResourceState::ShaderRead);
    context.commands.setPipeline(*m_rhiGraphicsPipeline);
    context.commands.setBindingSet(*m_rhiBindings[frame]);
    context.commands.draw(3);
    context.commands.transition(*m_rhiCloudBuffers[frame], ResourceState::Storage);
}
