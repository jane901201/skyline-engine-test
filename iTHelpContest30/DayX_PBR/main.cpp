#include "backends/d3d12/d3d12_interop.h"
#include "../rhi_example.h"
#include <array>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <stdexcept>
#include <string>

#include <DirectXMath.h>
#include <directx/d3dx12_core.h>

#include "constant_buffer.h"
#include "graphics_engine.h"
#include "mesh.h"
#include "pipeline_state.h"
#include "render_context.h"
#include "render_pass.h"
#include "root_signature.h"
#include "shader.h"
#include "skyline_debugger.h"
#include "system.h"

namespace
{
using namespace DirectX;

constexpr XMFLOAT3 cameraPosition{0.0f, 4.0f, -10.0f};
constexpr XMFLOAT3 cameraTarget{0.0f, 1.0f, 0.0f};
constexpr XMFLOAT3 sunDirection{0.45f, 0.72f, 0.53f};
constexpr XMFLOAT3 sunColor{1.0f, 0.91f, 0.78f};
constexpr float sunIntensity = 5.0f;

struct SunConstants
{
    XMFLOAT4X4 inverseView;
    XMFLOAT4X4 inverseProjection;
    XMFLOAT3 direction;
    float radius;
    XMFLOAT3 color;
    float intensity;
};

struct PbrConstants
{
    XMFLOAT4X4 world;
    XMFLOAT4X4 worldViewProjection;
    XMFLOAT3 camera;
    float padding0;
    XMFLOAT3 baseColor;
    float metallic;
    XMFLOAT3 direction;
    float roughness;
    XMFLOAT3 lightColor;
    float lightIntensity;
};

static_assert(sizeof(SunConstants) == 160);
static_assert(sizeof(PbrConstants) == 192);

std::filesystem::path getExecutableDirectory()
{
    wchar_t executablePath[MAX_PATH]{};
    const DWORD pathLength = GetModuleFileNameW(nullptr, executablePath, _countof(executablePath));
    if (pathLength == 0 || pathLength == _countof(executablePath))
        throw std::runtime_error("DayX_PBR: Failed to determine the executable directory.");
    return std::filesystem::path(executablePath).parent_path();
}

XMMATRIX createViewMatrix()
{
    return XMMatrixLookAtLH(XMLoadFloat3(&cameraPosition), XMLoadFloat3(&cameraTarget), XMVectorSet(0, 1, 0, 0));
}

XMMATRIX createProjectionMatrix(UINT width, UINT height)
{
    if (width == 0 || height == 0)
        throw std::runtime_error("DayX_PBR: Frame-buffer dimensions must be non-zero.");
    return XMMatrixPerspectiveFovLH(XMConvertToRadians(55.0f), static_cast<float>(width) / height, 0.1f, 100.0f);
}

[[deprecated("Replaced by RHI graphics pipeline creation.")]]
D3D12_GRAPHICS_PIPELINE_STATE_DESC createPipelineDescription(ID3D12RootSignature* rootSignature,
                                                              ID3DBlob* vertexShader, ID3DBlob* pixelShader,
                                                              bool meshPipeline)
{
    if (rootSignature == nullptr || vertexShader == nullptr || pixelShader == nullptr)
        throw std::invalid_argument("DayX_PBR: Pipeline shaders and root signature are required.");

    static constexpr D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC description{};
    if (meshPipeline)
        description.InputLayout = {inputLayout, _countof(inputLayout)};
    description.pRootSignature = rootSignature;
    description.VS = CD3DX12_SHADER_BYTECODE(vertexShader);
    description.PS = CD3DX12_SHADER_BYTECODE(pixelShader);
    description.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    description.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    description.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    if (!meshPipeline)
    {
        D3D12_RENDER_TARGET_BLEND_DESC& renderTarget = description.BlendState.RenderTarget[0];
        renderTarget.BlendEnable = TRUE;
        renderTarget.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        renderTarget.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        renderTarget.BlendOp = D3D12_BLEND_OP_ADD;
        renderTarget.SrcBlendAlpha = D3D12_BLEND_ONE;
        renderTarget.DestBlendAlpha = D3D12_BLEND_ZERO;
        renderTarget.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        renderTarget.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }
    description.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    description.DepthStencilState.DepthEnable = meshPipeline ? TRUE : FALSE;
    description.DepthStencilState.DepthWriteMask = meshPipeline ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    description.DepthStencilState.StencilEnable = FALSE;
    description.SampleMask = UINT_MAX;
    description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    description.NumRenderTargets = 1;
    description.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    description.SampleDesc.Count = 1;
    return description;
}

class SunPass final : public RenderPass
{
  public:
    void init(const PassInitContext& context) override
    {
        Skyline::RHI::BindingLayoutDesc desc;
        desc.constantBuffers = {{0}};
        auto layout = context.device.createBindingLayout(desc);
        m_rhiPipeline = Skyline::Examples::graphics(context.device, *layout, context.attachments, "assets/shaders/sun.hlsl", {}, true);
        updateConstants(context.width, context.height);
        m_rhiBuffer = Skyline::Examples::constants(context.device, std::as_bytes(std::span{&m_constants, 1}));
        m_rhiBindings = Skyline::Examples::bindConstants(context.device, *layout, m_rhiBuffer);
    }
    void execute(const PassFrameContext& context) override
    {
        updateConstants(context.width, context.height);
        m_rhiBuffer->write(0, std::as_bytes(std::span{&m_constants, 1}));
        context.commands.setPipeline(*m_rhiPipeline);
        context.commands.setBindingSet(*m_rhiBindings);
        context.commands.draw(3);
    }
    [[deprecated("Replaced by init(PassInitContext).")]]
    void init(const RenderPassInitContext& context) override
    {
        m_vertexShader.loadVS("assets/shaders/sun.hlsl", "VSMain");
        m_pixelShader.loadPS("assets/shaders/sun.hlsl", "PSMain");
        m_pipelineState.init(createPipelineDescription(context.rootSignature.get(), m_vertexShader.getCompiledBlob(),
                                                       m_pixelShader.getCompiledBlob(), false));
        updateConstants(context.graphicsEngine.getFrameBufferWidth(), context.graphicsEngine.GetFrameBufferHeight());
        m_constantBuffer.init(sizeof(m_constants), &m_constants);
    }

    [[deprecated("Replaced by execute(PassFrameContext).")]]
    void execute(const RenderFrameContext& context) override
    {
        updateConstants(context.width, context.height);
        m_constantBuffer.copyToVRAM(m_constants);
        context.renderContext.setPipelineState(m_pipelineState);
        context.renderContext.setGraphicsRootConstantBufferView(0, m_constantBuffer.getGPUVirtualAddress());
        context.renderContext.draw(3);
    }

  private:
    void updateConstants(UINT width, UINT height)
    {
        const XMMATRIX inverseView = XMMatrixInverse(nullptr, createViewMatrix());
        const XMMATRIX inverseProjection = XMMatrixInverse(nullptr, createProjectionMatrix(width, height));
        // 引擎與 HLSL 統一使用 row_major，因此直接上傳反矩陣。
        XMStoreFloat4x4(&m_constants.inverseView, inverseView);
        XMStoreFloat4x4(&m_constants.inverseProjection, inverseProjection);
        m_constants.direction = sunDirection;
        m_constants.radius = 0.035f;
        m_constants.color = sunColor;
        m_constants.intensity = 1.5f;
    }

    Shader m_vertexShader;
    Shader m_pixelShader;
    PipelineState m_pipelineState;
    ConstantBuffer m_constantBuffer;
    std::unique_ptr<Skyline::RHI::Pipeline> m_rhiPipeline;
    std::shared_ptr<Skyline::RHI::Buffer> m_rhiBuffer;
    std::unique_ptr<Skyline::RHI::BindingSet> m_rhiBindings;
    SunConstants m_constants{};
};

class PbrPass final : public RenderPass
{
  public:
    void init(const PassInitContext& context) override
    {
        Skyline::RHI::BindingLayoutDesc desc;
        desc.constantBuffers = {{0}};
        auto layout = context.device.createBindingLayout(desc);
        using Skyline::RHI::Format;
        m_rhiPipeline = Skyline::Examples::graphics(context.device, *layout, context.attachments, "assets/shaders/light.hlsl",
            {{"POSITION", 0, Format::Rgb32Float}, {"NORMAL", 0, Format::Rgb32Float, 0, 12}, {"TEXCOORD", 0, Format::Rg32Float, 0, 24}}, false, true);
        const auto directory = getExecutableDirectory() / "assets" / "fbx";
        const std::array<const char*, 3> files{"Plane.fbx", "Cube.fbx", "Sphere.fbx"};
        updateConstants(context.width, context.height);
        for (std::size_t index = 0; index < m_meshes.size(); ++index)
        {
            m_meshes[index].initFromFbxFile((directory / files[index]).string(), context.device);
            m_rhiBuffers[index] = Skyline::Examples::constants(context.device, std::as_bytes(std::span{&m_constants[index], 1}));
            m_rhiBindings[index] = Skyline::Examples::bindConstants(context.device, *layout, m_rhiBuffers[index]);
        }
    }
    void execute(const PassFrameContext& context) override
    {
        updateConstants(context.width, context.height);
        context.commands.setPipeline(*m_rhiPipeline);
        for (std::size_t index = 0; index < m_meshes.size(); ++index)
        {
            m_rhiBuffers[index]->write(0, std::as_bytes(std::span{&m_constants[index], 1}));
            context.commands.setBindingSet(*m_rhiBindings[index]);
            m_meshes[index].draw(context.commands);
        }
    }
    [[deprecated("Replaced by init(PassInitContext).")]]
    void init(const RenderPassInitContext& context) override
    {
        //使用 light.hlsl
        m_vertexShader.loadVS("assets/shaders/light.hlsl", "VSMain");
        m_pixelShader.loadPS("assets/shaders/light.hlsl", "PSMain");
        m_pipelineState.init(createPipelineDescription(context.rootSignature.get(), m_vertexShader.getCompiledBlob(),
                                                       m_pixelShader.getCompiledBlob(), true));
        //使用導入的幾個 fbx 檔
        const std::filesystem::path assetDirectory = getExecutableDirectory() / "assets" / "fbx";
        m_meshes[0].initFromFbxFile((assetDirectory / "Plane.fbx").string());
        m_meshes[1].initFromFbxFile((assetDirectory / "Cube.fbx").string());
        m_meshes[2].initFromFbxFile((assetDirectory / "Sphere.fbx").string());

        updateConstants(context.graphicsEngine.getFrameBufferWidth(), context.graphicsEngine.GetFrameBufferHeight());
        for (std::size_t index = 0; index < m_buffers.size(); ++index)
            m_buffers[index].init(sizeof(PbrConstants), &m_constants[index]);
    }

    [[deprecated("Replaced by execute(PassFrameContext).")]]
    void execute(const RenderFrameContext& context) override
    {
        updateConstants(context.width, context.height);
        context.renderContext.setPipelineState(m_pipelineState);
        for (std::size_t index = 0; index < m_meshes.size(); ++index)
        {
            m_buffers[index].copyToVRAM(m_constants[index]);
            context.renderContext.setGraphicsRootConstantBufferView(0, m_buffers[index].getGPUVirtualAddress());
            m_meshes[index].draw(context.renderContext);
        }
    }

  private:
    void updateConstants(UINT width, UINT height)
    {
        const XMMATRIX viewProjection = createViewMatrix() * createProjectionMatrix(width, height);
        const std::array<XMMATRIX, 3> worlds = {
            XMMatrixScaling(6.0f, 1.0f, 6.0f),
            XMMatrixRotationY(XMConvertToRadians(-20.0f)) * XMMatrixTranslation(-1.7f, 1.0f, 0.0f),
            XMMatrixTranslation(1.7f, 1.0f, 0.0f),
        };
        constexpr std::array<XMFLOAT3, 3> colors = {
            XMFLOAT3{0.38f, 0.42f, 0.46f}, XMFLOAT3{0.92f, 0.24f, 0.08f}, XMFLOAT3{0.95f, 0.71f, 0.20f}};
        constexpr std::array<float, 3> metallic = {0.0f, 0.15f, 1.0f};
        constexpr std::array<float, 3> roughness = {0.72f, 0.30f, 0.16f};

        for (std::size_t index = 0; index < m_constants.size(); ++index)
        {
            XMStoreFloat4x4(&m_constants[index].world, worlds[index]);
            XMStoreFloat4x4(&m_constants[index].worldViewProjection, worlds[index] * viewProjection);
            m_constants[index].camera = cameraPosition;
            m_constants[index].baseColor = colors[index];
            m_constants[index].metallic = metallic[index];
            m_constants[index].direction = sunDirection;
            m_constants[index].roughness = roughness[index];
            m_constants[index].lightColor = sunColor;
            m_constants[index].lightIntensity = sunIntensity;
        }
    }

    Shader m_vertexShader;
    Shader m_pixelShader;
    PipelineState m_pipelineState;
    std::array<Mesh, 3> m_meshes;
    std::array<ConstantBuffer, 3> m_buffers;
    std::unique_ptr<Skyline::RHI::Pipeline> m_rhiPipeline;
    std::array<std::shared_ptr<Skyline::RHI::Buffer>, 3> m_rhiBuffers;
    std::array<std::unique_ptr<Skyline::RHI::BindingSet>, 3> m_rhiBindings;
    std::array<PbrConstants, 3> m_constants{};
};
} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR commandLine, int showCommand)
{
    try
    {
        SkylineDebugger::Initialize("DayX_PBR");

        SKYLINE_LOG(INFO) << "[DayX_PBR] Initializing window...";
        initWindow(hInstance, hPrevInstance, commandLine, showCommand, TEXT("DayX PBR"));
        if (g_hWnd == nullptr)
            throw std::runtime_error("DayX_PBR: Failed to create the application window.");

        SKYLINE_LOG(INFO) << "[DayX_PBR] Initializing graphics engine...";
        GraphicsEngine graphicsEngine;
        graphicsEngine.init(g_hWnd, FRAME_BUFFER_W, FRAME_BUFFER_H);
        SkylineDebugger::ConfigureD3D12(Skyline::D3D12::nativeFrameDevice(graphicsEngine.getRhiFrame()));

        SKYLINE_LOG(INFO) << "[DayX_PBR] Initializing render passes...";
        PassInitContext initContext{graphicsEngine.getRhiDevice(), graphicsEngine.getRhiFrame().attachments(),
            graphicsEngine.getFrameBufferWidth(), graphicsEngine.GetFrameBufferHeight()};
        SunPass sunPass;
        PbrPass pbrPass;
        sunPass.init(initContext);
        pbrPass.init(initContext);

        SKYLINE_LOG(INFO) << "[DayX_PBR] Initialization completed.";
        bool isFirstFrame = true;
        while (dispatchWindowMessage())
        {
            if (isFirstFrame)
                SKYLINE_LOG(INFO) << "[DayX_PBR] Rendering first frame...";

            graphicsEngine.beginRender();
            auto& frame = graphicsEngine.getRhiFrame();
            PassFrameContext frameContext{frame.commands(), frame.frameIndex(), frame.width(), frame.height()};
            sunPass.execute(frameContext);
            pbrPass.execute(frameContext);
            graphicsEngine.endRender();

            SkylineDebugger::LogD3D12Messages(Skyline::D3D12::nativeFrameDevice(graphicsEngine.getRhiFrame()));
            if (isFirstFrame)
            {
                SKYLINE_LOG(INFO) << "[DayX_PBR] First frame presented.";
                isFirstFrame = false;
            }
        }

        SkylineDebugger::Shutdown();
        return EXIT_SUCCESS;
    }
    catch (const std::exception& exception)
    {
        SkylineDebugger::ShowFatalError("DayX_PBR initialization failed", exception);
        SkylineDebugger::Shutdown();
        return EXIT_FAILURE;
    }
    catch (...)
    {
        SkylineDebugger::ShowFatalError("DayX_PBR initialization failed", "An unknown fatal error occurred.");
        SkylineDebugger::Shutdown();
        return EXIT_FAILURE;
    }
}
