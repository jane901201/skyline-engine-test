#include "backends/d3d12/d3d12_interop.h"
#include "../rhi_example.h"
#include <cstdlib>
#include <directx/d3dx12_core.h>
#include <exception>
#include <stdexcept>

#include "constant_buffer.h"
#include "graphics_engine.h"
#include "my_engine.h"
#include "skyline_debugger.h"
#include "system.h"
#include "ui/imgui_system.h"

namespace
{
struct SimpleVertex
{
    float x;
    float y;
    float z;
};

//控制顏色的 struct
struct TriangleConstants
{
    float color[4];
};

[[deprecated("Replaced by RHI factories. Use the RHI Device factories.")]]
void initializeRootSignature(RootSignature& rootSignature)
{
    rootSignature.init(
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP);
}

[[deprecated("Replaced by RHI factories. Use the RHI Device factories.")]]
D3D12_GRAPHICS_PIPELINE_STATE_DESC createPipelineDescription(
    ID3D12RootSignature* rootSignature,
    ID3DBlob* vertexShader,
    ID3DBlob* pixelShader)
{
    if (rootSignature == nullptr)
        throw std::invalid_argument("DayX_ImGui: A root signature is required.");
    if (vertexShader == nullptr)
        throw std::invalid_argument("DayX_ImGui: A vertex shader is required.");
    if (pixelShader == nullptr)
        throw std::invalid_argument("DayX_ImGui: A pixel shader is required.");

    static constexpr D3D12_INPUT_ELEMENT_DESC inputElements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC description{};
    description.InputLayout = {inputElements, _countof(inputElements)};
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

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, LPWSTR commandLine, int showCommand)
{
    try
    {
        SkylineDebugger::Initialize("DayX_ImGui");
        initWindow(instance, previous, commandLine, showCommand, TEXT("DayX_ImGui"));
        if (g_hWnd == nullptr)
            throw std::runtime_error("DayX_ImGui: Failed to create the application window.");

        GraphicsEngine graphicsEngine;
        graphicsEngine.init(g_hWnd, FRAME_BUFFER_W, FRAME_BUFFER_H);
        SkylineDebugger::ConfigureD3D12(Skyline::D3D12::nativeFrameDevice(graphicsEngine.getRhiFrame()));

        auto& device = graphicsEngine.getRhiDevice();
        Skyline::RHI::BindingLayoutDesc layoutDesc;
        layoutDesc.constantBuffers = {{0}};
        auto rootSignature = device.createBindingLayout(layoutDesc);
        auto pipelineState = Skyline::Examples::graphics(device, *rootSignature, graphicsEngine.getBackBufferLayout(),
            "assets/shaders/imgui_triangle.hlsl", {{"POSITION", 0, Skyline::RHI::Format::Rgb32Float}});
        const SimpleVertex vertices[] = {
            {-0.5f, -0.5f, 0.0f},
            {0.0f, 0.5f, 0.0f},
            {0.5f, -0.5f, 0.0f},
        };
        const std::uint16_t indices[] = {0, 1, 2};



        auto [vertexBuffer, indexBuffer] = Skyline::Examples::geometry(device, std::as_bytes(std::span{vertices}), std::as_bytes(std::span{indices}));

        //初始化 Constant Buffer
        TriangleConstants triangleConstants{{1.0f, 0.2f, 0.1f, 1.0f}};
        auto constantBuffer = Skyline::Examples::constants(device, std::as_bytes(std::span{&triangleConstants, 1}));
        auto bindings = Skyline::Examples::bindConstants(device, *rootSignature, constantBuffer);

        auto& renderContext = graphicsEngine.getRhiCommandContext();

        //更新 imgui
        Skyline::UI::ImGuiSystem imguiSystem;
        imguiSystem.initialize(g_hWnd, graphicsEngine);

        while (dispatchWindowMessage())
        {
            graphicsEngine.beginRender();
            imguiSystem.beginFrame();

            if (imguiSystem.beginWindow("Triangle controls"))
                imguiSystem.colorSliders("Triangle color", triangleConstants.color);
            imguiSystem.endWindow();

            constantBuffer->write(0, std::as_bytes(std::span{&triangleConstants, 1}));


            renderContext.setPipeline(*pipelineState);

            renderContext.setVertexBuffer(*vertexBuffer, sizeof(vertices[0]));
            renderContext.setIndexBuffer(*indexBuffer, Skyline::RHI::IndexType::Uint16);
            renderContext.setBindingSet(*bindings);
            renderContext.drawIndexed(3);

            imguiSystem.render();
            graphicsEngine.endRender();

            SkylineDebugger::LogD3D12Messages(Skyline::D3D12::nativeFrameDevice(graphicsEngine.getRhiFrame()));
        }

        imguiSystem.shutdown();
        SkylineDebugger::Shutdown();
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        SkylineDebugger::ShowFatalError("DayX_ImGui initialization failed", error);
        SkylineDebugger::Shutdown();
        return EXIT_FAILURE;
    }
    catch (...)
    {
        SkylineDebugger::ShowFatalError("DayX_ImGui initialization failed", "An unknown fatal error occurred.");
        SkylineDebugger::Shutdown();
        return EXIT_FAILURE;
    }
}
