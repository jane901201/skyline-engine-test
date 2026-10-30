#include "backends/d3d12/d3d12_interop.h"
#include "../rhi_example.h"
#include <cstdlib>
#include <directx/d3dx12_core.h>
#include <exception>
#include <stdexcept>

#include "constant_buffer.h"
#include "graphics_engine.h"
#include "math/vector.h"
#include "render_context.h"
#include "skyline_debugger.h"
#include "system.h"

namespace
{
// 定義傳給太陽 Shader 的 Constant Buffer 資料
struct SunConstants
{
    // 太陽在螢幕座標中的中心位置
    Vector2 sunPosition;

    // 太陽圓盤半徑
    float sunRadius;

    // 太陽亮度強度
    float sunIntensity;

    // 太陽顏色
    Vector3 sunColor;

    // 畫面寬高比，用來修正太陽形狀
    float aspectRatio;
};

// 確認 C++ 結構大小與 HLSL Constant Buffer layout 一致
static_assert(sizeof(SunConstants) == 32, "SunConstants must match the HLSL constant-buffer layout.");

[[deprecated("Replaced by RHI factories. Use Skyline::Examples::graphics().")]]
void initRootSignature(RootSignature& rs)
{
    rs.init(D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP);
}

[[deprecated("Replaced by RHI factories. Use Skyline::Examples::graphics().")]]
D3D12_GRAPHICS_PIPELINE_STATE_DESC createSkyPipelineStateDescription(ID3D12RootSignature* rootSignature,
                                                                     ID3DBlob* vertexShader, ID3DBlob* pixelShader)
{
    if (rootSignature == nullptr)
        throw std::invalid_argument("SkyPipeline: Root signature is required.");
    if (vertexShader == nullptr)
        throw std::invalid_argument("SkyPipeline: Vertex shader is required.");
    if (pixelShader == nullptr)
        throw std::invalid_argument("SkyPipeline: Pixel shader is required.");

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

// 建立繪製太陽使用的 Graphics Pipeline State 設定
[[deprecated("Replaced by RHI factories. Use Skyline::Examples::graphics().")]]
D3D12_GRAPHICS_PIPELINE_STATE_DESC createLightStateDescription(ID3D12RootSignature* rootSignature,
                                                               ID3DBlob* vertexShader, ID3DBlob* pixelShader)
{
    if (rootSignature == nullptr)
        throw std::invalid_argument("LightPipeline: Root signature is required.");
    if (vertexShader == nullptr)
        throw std::invalid_argument("LightPipeline: Vertex shader is required.");
    if (pixelShader == nullptr)
        throw std::invalid_argument("LightPipeline: Pixel shader is required.");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC description{};

    // Fullscreen Triangle 由 SV_VertexID 產生，因此不需要 Vertex Buffer Input Layout
    description.InputLayout = {nullptr, 0};

    description.pRootSignature = rootSignature;

    // 設定太陽使用的 Vertex Shader 與 Pixel Shader
    description.VS = CD3DX12_SHADER_BYTECODE(vertexShader);
    description.PS = CD3DX12_SHADER_BYTECODE(pixelShader);

    description.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    description.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    // 使用 Alpha Blending 讓太陽與光暈疊加在天空背景上
    description.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

    // 取得第一個 Render Target 的 Blend 設定
    auto& rt = description.BlendState.RenderTarget[0];

    // 啟用顏色混合
    rt.BlendEnable = TRUE;

    // Source 顏色乘上 Pixel Shader 輸出的 Alpha
    rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;

    // 背景顏色乘上 1 - Source Alpha
    rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;

    // 將 Source 與 Destination 顏色相加
    rt.BlendOp = D3D12_BLEND_OP_ADD;

    // Alpha 輸出直接使用 Source Alpha
    rt.SrcBlendAlpha = D3D12_BLEND_ONE;

    // 不混合 Destination Alpha
    rt.DestBlendAlpha = D3D12_BLEND_ZERO;

    // 使用加法產生最終 Alpha
    rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;

    // 允許寫入 RGBA 所有 Color Channel
    rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

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

void initPipelineState(PipelineState& pipelineState, RootSignature& rs, Shader& vs, Shader& ps)
{
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc =
        createSkyPipelineStateDescription(rs.get(), vs.getCompiledBlob(), ps.getCompiledBlob());
    pipelineState.init(psoDesc);
}

// 初始化太陽專用的 Pipeline State
void initLightPipelineState(PipelineState& pipelineState, RootSignature& rs, Shader& vs, Shader& ps)
{
    // 建立包含 Alpha Blending 設定的太陽 Pipeline 描述
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc =
        createLightStateDescription(rs.get(), vs.getCompiledBlob(), ps.getCompiledBlob());

    // 建立太陽使用的 Pipeline State Object
    pipelineState.init(psoDesc);
}
} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    try
    {
        SkylineDebugger::Initialize("DayX_Light");

        SKYLINE_LOG(INFO) << "[DayX_Light] Initializing window...";
        initWindow(hInstance, hPrevInstance, lpCmdLine, nCmdShow, TEXT("Game"));
        if (g_hWnd == nullptr)
            throw std::runtime_error("DayX_Light: Failed to create the application window.");

        SKYLINE_LOG(INFO) << "[DayX_Light] Initializing graphics engine...";
        GraphicsEngine graphicsEngine;
        graphicsEngine.init(g_hWnd, FRAME_BUFFER_W, FRAME_BUFFER_H);

        // 設定太陽初始位置、大小、亮度、顏色與畫面比例
        SunConstants sunConstants{Vector2{-0.55f, 0.25f}, 0.08f, 1.5f, Vector3{1.0f, 0.85f, 0.55f},
                                  static_cast<float>(FRAME_BUFFER_W) / static_cast<float>(FRAME_BUFFER_H)};

        // 建立儲存 SunConstants 的 Constant Buffer
        auto& device = graphicsEngine.getRhiDevice();
        auto sunConstantBuffer = Skyline::Examples::constants(device, std::as_bytes(std::span{&sunConstants, 1}));

        SkylineDebugger::ConfigureD3D12(Skyline::D3D12::nativeFrameDevice(graphicsEngine.getRhiFrame()));

        SKYLINE_LOG(INFO) << "[DayX_Light] Initializing root signature...";
        auto skyLayout = device.createBindingLayout({});
        Skyline::RHI::BindingLayoutDesc definition;
        definition.constantBuffers = {{0}};
        auto sunLayout = device.createBindingLayout(definition);
        auto skyPipelineState = Skyline::Examples::graphics(device, *skyLayout, graphicsEngine.getBackBufferLayout(), "assets/shaders/sky_gradient.hlsl");
        auto sunPipelineState = Skyline::Examples::graphics(device, *sunLayout, graphicsEngine.getBackBufferLayout(), "assets/shaders/sun_2D.hlsl", {}, true);
        auto bindings = Skyline::Examples::bindConstants(device, *sunLayout, sunConstantBuffer);
        auto& renderContext = graphicsEngine.getRhiCommandContext();

        bool isFirstFrame = true;
        while (dispatchWindowMessage())
        {
            if (isFirstFrame)
                SKYLINE_LOG(INFO) << "[DayX_Light] Rendering first frame...";

            graphicsEngine.beginRender();

            // 每幀取得目前 Frame Buffer 的實際寬高比
            sunConstants.aspectRatio = static_cast<float>(graphicsEngine.getFrameBufferWidth()) /
                                       static_cast<float>(graphicsEngine.GetFrameBufferHeight());

            // 將更新後的太陽參數同步到 GPU
            sunConstantBuffer->write(0, std::as_bytes(std::span{&sunConstants, 1}));




            renderContext.setPipeline(*skyPipelineState);
            renderContext.draw(3);

            // 切換成支援 Alpha Blending 的太陽 Pipeline
            renderContext.setPipeline(*sunPipelineState);

            // 將 SunConstants 綁定到 Root Parameter 0 對應的 Constant Buffer
            renderContext.setBindingSet(*bindings);

            // 繪製 Fullscreen Triangle，並由 Pixel Shader 決定太陽實際顯示區域
            renderContext.draw(3);

            graphicsEngine.endRender();

            SkylineDebugger::LogD3D12Messages(Skyline::D3D12::nativeFrameDevice(graphicsEngine.getRhiFrame()));
            if (isFirstFrame)
            {
                SKYLINE_LOG(INFO) << "[DayX_Light] First frame presented.";
                isFirstFrame = false;
            }
        }

        SkylineDebugger::Shutdown();
        return EXIT_SUCCESS;
    }
    catch (const std::exception& exception)
    {
        SkylineDebugger::ShowFatalError("DayX_Light initialization failed", exception);
        SkylineDebugger::Shutdown();
        return EXIT_FAILURE;
    }
    catch (...)
    {
        SkylineDebugger::ShowFatalError("DayX_Light initialization failed", "An unknown fatal error occurred.");
        SkylineDebugger::Shutdown();
        return EXIT_FAILURE;
    }
}
