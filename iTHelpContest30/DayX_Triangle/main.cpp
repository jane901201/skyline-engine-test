#include "backends/d3d12/d3d12_interop.h"
#include <cstdlib>
#include <directx/d3dx12_core.h>
#include <exception>
#include <stdexcept>

#include "graphics_engine.h"
#include "my_engine.h"
#include "skyline_debugger.h"
#include "system.h"

namespace
{
    // 定義三角形單一頂點的位置資料
    struct SimpleVertex
    {
        float x, y, z;
    };

    // 初始化三角形使用的 Root Signature
    [[deprecated("Replaced by RHI binding ownership. Use device.createBindingLayout().")]]
    void initRootSignature(RootSignature & rs)
    {
        rs.init(D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                D3D12_TEXTURE_ADDRESS_MODE_WRAP);
    }

    // 建立繪製三角形需要的 Graphics Pipeline State 描述
    [[deprecated("Unused after the RHI pipeline refactor. Use GraphicsPipelineDesc and GraphicsEngine::getBackBufferLayout().")]]
    D3D12_GRAPHICS_PIPELINE_STATE_DESC createTrianglePipelineStateDescription(
        ID3D12RootSignature * rootSignature, ID3DBlob * vertexShader, ID3DBlob * pixelShader)
    {
        // 確認有傳入有效的 Root Signature
        if (rootSignature == nullptr)
            throw std::invalid_argument("TrianglePipeline: Root signature is required.");

        // 確認有傳入有效的 Vertex Shader
        if (vertexShader == nullptr)
            throw std::invalid_argument("TrianglePipeline: Vertex shader is required.");

        // 確認有傳入有效的 Pixel Shader
        if (pixelShader == nullptr)
            throw std::invalid_argument("TrianglePipeline: Pixel shader is required.");

        // 定義 Vertex Buffer 中 POSITION 資料的格式
        static constexpr D3D12_INPUT_ELEMENT_DESC inputElementDescriptions[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        };

        // 建立並初始化 Graphics Pipeline State 描述
        D3D12_GRAPHICS_PIPELINE_STATE_DESC description{};

        // 設定 Vertex Shader 使用的 Input Layout
        description.InputLayout = {inputElementDescriptions, _countof(inputElementDescriptions)};

        // 設定 Pipeline 使用的 Root Signature
        description.pRootSignature = rootSignature;

        // 設定 Vertex Shader Bytecode
        description.VS = CD3DX12_SHADER_BYTECODE(vertexShader);

        // 設定 Pixel Shader Bytecode
        description.PS = CD3DX12_SHADER_BYTECODE(pixelShader);

        // 使用 Direct3D 12 預設 Rasterizer 設定
        description.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

        // 關閉三角形面的 Cull
        description.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

        // 使用 Direct3D 12 預設 Blend 設定
        description.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

        // 使用 Direct3D 12 預設 Depth Stencil 設定
        description.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

        // 關閉 Depth Test
        description.DepthStencilState.DepthEnable = FALSE;

        // 禁止寫入 Depth Buffer
        description.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

        // 關閉 Stencil Test
        description.DepthStencilState.StencilEnable = FALSE;

        // 允許所有 Multisampling Sample 被處理
        description.SampleMask = UINT_MAX;

        // 指定 Pipeline 使用 Triangle 類型的 Primitive
        description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

        // 指定同時使用一個 Render Target
        description.NumRenderTargets = 1;

        // 設定第一個 Render Target 的像素格式
        description.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

        // 設定 Depth Stencil Buffer 的格式
        description.DSVFormat = DXGI_FORMAT_D32_FLOAT;

        // 設定每個 Pixel 只使用一個 Sample
        description.SampleDesc.Count = 1;

        // 回傳設定完成的 Graphics Pipeline State 描述
        return description;
    }

    // 建立並初始化三角形使用的 Pipeline State
    [[deprecated("Replaced by the RHI pipeline refactor. Use initPipelineState with an explicit GraphicsEngine.")]]
    void initPipelineState(PipelineState & pipelineState, RootSignature & rs, Shader & vs, Shader & ps)
    {
        // 使用 Root Signature 與 Shader 建立 Graphics Pipeline State 描述
        const D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc =
            createTrianglePipelineStateDescription(rs.get(), vs.getCompiledBlob(), ps.getCompiledBlob());

        // 使用建立好的描述初始化 Pipeline State
        pipelineState.init(psoDesc);
    }

    [[deprecated("Replaced by RHI Device factories. Use device.createGraphicsPipeline(desc).")]]
    void initPipelineState(PipelineState& pipelineState, const GraphicsEngine& engine,
                           const RootSignature& layout, const Shader& vs, const Shader& ps)
    {
        // 範例只描述頂點格式與渲染意圖，原生 PSO 欄位由後端轉換。
        auto desc = Skyline::RHI::GraphicsPipelineDesc::fullscreen();
        desc.debugName = "DayX_Triangle";
        desc.vertexInput = {{"POSITION", 0, Skyline::RHI::Format::Rgb32Float}};
        desc.attachments = engine.getBackBufferLayout();
        pipelineState.init(desc, engine, layout, vs, ps);
    }
}

// Windows 應用程式的主進入點
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    try
    {
        // 初始化 Skyline Debugger
        SkylineDebugger::Initialize("DayX_Triangle");

        // 建立應用程式視窗
        initWindow(hInstance, hPrevInstance, lpCmdLine, nCmdShow, TEXT("Game"));

        // 確認應用程式視窗建立成功
        if (g_hWnd == nullptr)
            throw std::runtime_error("DayX_Triangle: Failed to create the application window.");

        // 建立 Graphics Engine
        GraphicsEngine graphicsEngine;

        // 使用視窗與 Frame Buffer 大小初始化 Graphics Engine
        graphicsEngine.init(g_hWnd, FRAME_BUFFER_W, FRAME_BUFFER_H);

        // 將 D3D12 Device 設定給 Skyline Debugger
        SkylineDebugger::ConfigureD3D12(Skyline::D3D12::nativeFrameDevice(graphicsEngine.getRhiFrame()));

        // 由 RHI 裝置建立並擁有 Shader、綁定配置與管線。
        auto& device = graphicsEngine.getRhiDevice();
        auto bindingLayout = device.createBindingLayout({});
        auto vs = device.createShader({"assets/shaders/triangle.hlsl", "VSMain", Skyline::RHI::ShaderStage::Vertex});
        auto ps = device.createShader({"assets/shaders/triangle.hlsl", "PSMain", Skyline::RHI::ShaderStage::Pixel});
        auto pipelineDesc = Skyline::RHI::GraphicsPipelineDesc::fullscreen();
        pipelineDesc.debugName = "DayX_Triangle";
        pipelineDesc.vertexShader = vs.get();
        pipelineDesc.pixelShader = ps.get();
        pipelineDesc.bindingLayout = bindingLayout.get();
        pipelineDesc.vertexInput = {{"POSITION", 0, Skyline::RHI::Format::Rgb32Float}};
        pipelineDesc.attachments = graphicsEngine.getBackBufferLayout();
        auto pipeline = device.createGraphicsPipeline(pipelineDesc);

        // 建立三角形的三個頂點位置
        SimpleVertex vertices[] = {
            {-0.5f, -0.5f, 0.0f},
            {0.0f, 0.5f, 0.0f},
            {0.5f, -0.5f, 0.0f},
        };

        // 建立三角形使用的 Vertex Buffer
        auto triangleVB = device.createBuffer({sizeof(vertices), Skyline::RHI::BufferUsage::Vertex | Skyline::RHI::BufferUsage::CopyDestination});

        // 定義三角形使用的三個 Index
        uint16_t indices[] = {0, 1, 2};

        // 建立三角形使用的 Index Buffer
        auto triangleIB = device.createBuffer({sizeof(indices), Skyline::RHI::BufferUsage::Index | Skyline::RHI::BufferUsage::CopyDestination});
        auto uploads = device.createCommandContext();
        uploads->begin();
        uploads->uploadBuffer(*triangleVB, 0, std::as_bytes(std::span{vertices}), Skyline::RHI::ResourceState::Vertex);
        uploads->uploadBuffer(*triangleIB, 0, std::as_bytes(std::span{indices}), Skyline::RHI::ResourceState::Index);
        uploads->submitAndWait();

        // 取得 Graphics Engine 使用的 Render Context
        auto& renderContext = graphicsEngine.getRhiCommandContext();

        // 將目前 Graphics Engine 的 Command List 設定給 Render Context


        // 持續處理視窗訊息並執行每一幀的繪製
        while (dispatchWindowMessage())
        {
            // 開始目前 Frame 的 Rendering
            graphicsEngine.beginRender();

            // 命令轉接層一次綁定管線與它保留的配置。
            renderContext.setPipeline(*pipeline);

            // 設定 Input Assembler 使用 Triangle List
            renderContext.setPrimitiveTopology(Skyline::RHI::PrimitiveType::Triangle);

            // 將三角形 Vertex Buffer 綁定到 Input Assembler
            renderContext.setVertexBuffer(*triangleVB, sizeof(SimpleVertex));

            // 將三角形 Index Buffer 綁定到 Input Assembler
            renderContext.setIndexBuffer(*triangleIB, Skyline::RHI::IndexType::Uint16);

            // 使用三個 Index 繪製三角形
            renderContext.drawIndexed(3);

            // 結束目前 Frame 並提交繪製結果
            graphicsEngine.endRender();

            // 將 Direct3D 12 Debug Layer 訊息輸出到 Debugger
            SkylineDebugger::LogD3D12Messages(Skyline::D3D12::nativeFrameDevice(graphicsEngine.getRhiFrame()));
        }

        // 關閉 Skyline Debugger
        SkylineDebugger::Shutdown();

        // 程式正常結束
        return EXIT_SUCCESS;
    }
    catch (const std::exception& exception)
    {
        // 顯示標準例外造成的致命錯誤
        SkylineDebugger::ShowFatalError("DayX_Triangle initialization failed", exception);

        // 關閉 Skyline Debugger
        SkylineDebugger::Shutdown();

        // 回傳程式執行失敗
        return EXIT_FAILURE;
    }
    catch (...)
    {
        SkylineDebugger::ShowFatalError("DayX_Triangle initialization failed", "An unknown fatal error occurred.");
        SkylineDebugger::Shutdown();

        return EXIT_FAILURE;
    }
}
