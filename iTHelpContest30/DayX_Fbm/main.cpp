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

namespace
{
struct ShaderConstants
{
    float resolution[2];
};

[[deprecated("Replaced by RHI factories. Use Skyline::Examples::graphics().")]]
D3D12_GRAPHICS_PIPELINE_STATE_DESC createPipelineDescription(
    ID3D12RootSignature* rootSignature, ID3DBlob* vertexShader, ID3DBlob* pixelShader)
{
    if (rootSignature == nullptr)
        throw std::invalid_argument("DayX_Fbm: Root signature is required.");
    if (vertexShader == nullptr)
        throw std::invalid_argument("DayX_Fbm: Vertex shader is required.");
    if (pixelShader == nullptr)
        throw std::invalid_argument("DayX_Fbm: Pixel shader is required.");

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

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, LPWSTR commandLine, int showCommand)
{
    try
    {
        SkylineDebugger::Initialize("DayX_Fbm");
        initWindow(instance, previous, commandLine, showCommand, TEXT("DayX_Fbm"));
        if (g_hWnd == nullptr)
            throw std::runtime_error("DayX_Fbm: Failed to create the application window.");

        GraphicsEngine graphicsEngine;
        graphicsEngine.init(g_hWnd, FRAME_BUFFER_W, FRAME_BUFFER_H);
        SkylineDebugger::ConfigureD3D12(Skyline::D3D12::nativeFrameDevice(graphicsEngine.getRhiFrame()));

        auto& device = graphicsEngine.getRhiDevice();
        Skyline::RHI::BindingLayoutDesc layoutDesc;
        layoutDesc.constantBuffers = {{0}};
        auto rootSignature = device.createBindingLayout(layoutDesc);
        auto pipelineState = Skyline::Examples::graphics(device, *rootSignature,
            graphicsEngine.getBackBufferLayout(), "assets/shaders/fbm.hlsl");
        ShaderConstants constants{{static_cast<float>(FRAME_BUFFER_W), static_cast<float>(FRAME_BUFFER_H)}};
        auto constantBuffer = Skyline::Examples::constants(device, std::as_bytes(std::span{&constants, 1}));
        auto bindings = Skyline::Examples::bindConstants(device, *rootSignature, constantBuffer);

        auto& renderContext = graphicsEngine.getRhiCommandContext();
        while (dispatchWindowMessage())
        {
            graphicsEngine.beginRender();

            renderContext.setPipeline(*pipelineState);

            renderContext.setBindingSet(*bindings);
            renderContext.draw(3);
            graphicsEngine.endRender();
        }

        SkylineDebugger::Shutdown();
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        SkylineDebugger::ShowFatalError("DayX_Fbm initialization failed", error);
        SkylineDebugger::Shutdown();
        return EXIT_FAILURE;
    }
    catch (...)
    {
        SkylineDebugger::ShowFatalError("DayX_Fbm initialization failed", "An unknown fatal error occurred.");
        SkylineDebugger::Shutdown();
        return EXIT_FAILURE;
    }
}
