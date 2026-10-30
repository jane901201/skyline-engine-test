#include "backends/d3d12/d3d12_interop.h"
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <directx/d3dx12_core.h>


#include "graphics_engine.h"
#include "my_engine.h"
#include "skyline_debugger.h"
#include "system.h"

namespace
{
    [[deprecated("Replaced by RHI binding ownership. Use device.createBindingLayout().")]]
    void initRootSignature(RootSignature& rs)
    {
        rs.init(D3D12_FILTER_MIN_MAG_MIP_LINEAR,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP);
    }

    [[deprecated("Unused after the RHI pipeline refactor. Use GraphicsPipelineDesc and GraphicsEngine::getBackBufferLayout().")]]
    D3D12_GRAPHICS_PIPELINE_STATE_DESC createSkyPipelineStateDescription(
        ID3D12RootSignature* rootSignature,
        ID3DBlob* vertexShader,
        ID3DBlob* pixelShader)
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

        // 把 ID3DBlob 裡的 vertex shader 跟 pixel shader 放進 description
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

    [[deprecated("Replaced by the RHI pipeline refactor. Use initPipelineState with an explicit GraphicsEngine.")]]
    void initPipelineState(PipelineState& pipelineState, RootSignature& rs, Shader& vs, Shader& ps)
    {
        const D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc =
            createSkyPipelineStateDescription(rs.get(), vs.getCompiledBlob(), ps.getCompiledBlob());
        pipelineState.init(psoDesc);
    }

    [[deprecated("Replaced by RHI Device factories. Use device.createGraphicsPipeline(desc).")]]
    void initPipelineState(PipelineState& pipelineState, const GraphicsEngine& engine,
                           const RootSignature& layout, const Shader& vs, const Shader& ps)
    {
        auto desc = Skyline::RHI::GraphicsPipelineDesc::fullscreen();
        desc.debugName = "DayX_Sky";
        desc.attachments = engine.getBackBufferLayout();
        pipelineState.init(desc, engine, layout, vs, ps);
    }



}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    try
    {
        SkylineDebugger::Initialize("DayX_Sky");

        // 基礎圖形初始化
        SKYLINE_LOG(INFO) << "[DayX_Sky] Initializing window...";
        initWindow(hInstance, hPrevInstance, lpCmdLine, nCmdShow, TEXT("Game"));
        if (g_hWnd == nullptr)
            throw std::runtime_error("DayX_Sky: Failed to create the application window.");

        SKYLINE_LOG(INFO) << "[DayX_Sky] Initializing graphics engine...";
        GraphicsEngine graphicsEngine;
        graphicsEngine.init(g_hWnd, FRAME_BUFFER_W, FRAME_BUFFER_H);
        SkylineDebugger::ConfigureD3D12(Skyline::D3D12::nativeFrameDevice(graphicsEngine.getRhiFrame()));

        // 1. RootSignature 初始化
        SKYLINE_LOG(INFO) << "[DayX_Sky] Initializing root signature...";
        auto& device = graphicsEngine.getRhiDevice();
        auto bindingLayout = device.createBindingLayout({});

        SKYLINE_LOG(INFO) << "[DayX_Sky] Compiling shaders...";
        auto vs = device.createShader({"assets/shaders/sky_gradient.hlsl", "VSMain", Skyline::RHI::ShaderStage::Vertex});
        auto ps = device.createShader({"assets/shaders/sky_gradient.hlsl", "PSMain", Skyline::RHI::ShaderStage::Pixel});

        SKYLINE_LOG(INFO) << "[DayX_Sky] Creating pipeline state...";
        auto pipelineDesc = Skyline::RHI::GraphicsPipelineDesc::fullscreen();
        pipelineDesc.debugName = "DayX_Sky";
        pipelineDesc.vertexShader = vs.get();
        pipelineDesc.pixelShader = ps.get();
        pipelineDesc.bindingLayout = bindingLayout.get();
        pipelineDesc.attachments = graphicsEngine.getBackBufferLayout();
        auto pipeline = device.createGraphicsPipeline(pipelineDesc);

        SKYLINE_LOG(INFO) << "[DayX_Sky] Initialization completed.";
        auto& renderContext = graphicsEngine.getRhiCommandContext();

        bool isFirstFrame = true;
        while (dispatchWindowMessage())
        {
            if (isFirstFrame)
                SKYLINE_LOG(INFO) << "[DayX_Sky] Rendering first frame...";

            graphicsEngine.beginRender();

            renderContext.setPipeline(*pipeline);
            renderContext.setPrimitiveTopology(Skyline::RHI::PrimitiveType::Triangle);
            renderContext.draw(3);

            graphicsEngine.endRender();

            SkylineDebugger::LogD3D12Messages(Skyline::D3D12::nativeFrameDevice(graphicsEngine.getRhiFrame()));
            if (isFirstFrame)
            {
                SKYLINE_LOG(INFO) << "[DayX_Sky] First frame presented.";
                isFirstFrame = false;
            }
        }

        SkylineDebugger::Shutdown();
        return EXIT_SUCCESS;
    }
    catch (const std::exception& exception)
    {
        SkylineDebugger::ShowFatalError("DayX_Sky initialization failed", exception);
        SkylineDebugger::Shutdown();
        return EXIT_FAILURE;
    }
    catch (...)
    {
        SkylineDebugger::ShowFatalError("DayX_Sky initialization failed", "An unknown fatal error occurred.");
        SkylineDebugger::Shutdown();
        return EXIT_FAILURE;
    }

}
