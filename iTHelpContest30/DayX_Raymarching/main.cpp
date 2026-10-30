#include "backends/d3d12/d3d12_interop.h"
#include "../rhi_example.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <stdexcept>

#include <directx/d3dcommon.h>
#include <directx/d3dx12_core.h>

#include "constant_buffer.h"
#include "graphics_engine.h"
#include "my_engine.h"
#include "skyline_debugger.h"
#include "system.h"

namespace
{
struct ShaderConstants
{
    float mousePosition[2];
    float time;
    float aspectRatio;
};

static_assert(sizeof(ShaderConstants) == 16);

[[deprecated("Replaced by RHI factories. Use Skyline::Examples::graphics().")]]
D3D12_GRAPHICS_PIPELINE_STATE_DESC createPipelineDescription(
    ID3D12RootSignature* rootSignature, ID3DBlob* vertexShader, ID3DBlob* pixelShader)
{
    if (rootSignature == nullptr || vertexShader == nullptr || pixelShader == nullptr)
        throw std::invalid_argument("DayX_Raymarching: Root signature and shaders are required.");

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

void updateMousePosition(ShaderConstants& constants)
{
    POINT cursor{};
    if (!GetCursorPos(&cursor) || !ScreenToClient(g_hWnd, &cursor))
        return;

    RECT clientRect{};
    if (!GetClientRect(g_hWnd, &clientRect))
        return;

    const float width = static_cast<float>(clientRect.right - clientRect.left);
    const float height = static_cast<float>(clientRect.bottom - clientRect.top);
    if (width <= 0.0f || height <= 0.0f)
        return;

    constants.mousePosition[0] = std::clamp(static_cast<float>(cursor.x) / width, 0.0f, 1.0f);
    constants.mousePosition[1] = std::clamp(static_cast<float>(cursor.y) / height, 0.0f, 1.0f);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, LPWSTR commandLine, int showCommand)
{
    try
    {
        SkylineDebugger::Initialize("DayX_Raymarching");
        initWindow(instance, previous, commandLine, showCommand, TEXT("DayX Ray Marching PBR"));
        if (g_hWnd == nullptr)
            throw std::runtime_error("DayX_Raymarching: Failed to create the application window.");

        GraphicsEngine graphicsEngine;
        graphicsEngine.init(g_hWnd, FRAME_BUFFER_W, FRAME_BUFFER_H);
        SkylineDebugger::ConfigureD3D12(Skyline::D3D12::nativeFrameDevice(graphicsEngine.getRhiFrame()));

        auto& device = graphicsEngine.getRhiDevice();
        Skyline::RHI::BindingLayoutDesc layoutDesc;
        layoutDesc.constantBuffers = {{0}};
        auto rootSignature = device.createBindingLayout(layoutDesc);
        auto pipelineState = Skyline::Examples::graphics(device, *rootSignature,
            graphicsEngine.getBackBufferLayout(), "assets/shaders/pixel_shader.hlsl");
        ShaderConstants constants{{0.5f, 0.5f}, 0.0f,
                                  static_cast<float>(FRAME_BUFFER_W) / static_cast<float>(FRAME_BUFFER_H)};
        auto constantBuffer = Skyline::Examples::constants(device, std::as_bytes(std::span{&constants, 1}));
        auto bindings = Skyline::Examples::bindConstants(device, *rootSignature, constantBuffer);
        const auto startTime = std::chrono::steady_clock::now();

        auto& renderContext = graphicsEngine.getRhiCommandContext();
        while (dispatchWindowMessage())
        {
            constants.time =
                std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime).count();
            constants.aspectRatio = static_cast<float>(graphicsEngine.getFrameBufferWidth()) /
                                    static_cast<float>(graphicsEngine.GetFrameBufferHeight());
            updateMousePosition(constants);
            constantBuffer->write(0, std::as_bytes(std::span{&constants, 1}));

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
        SkylineDebugger::ShowFatalError("DayX_Raymarching initialization failed", error);
        SkylineDebugger::Shutdown();
        return EXIT_FAILURE;
    }
    catch (...)
    {
        SkylineDebugger::ShowFatalError("DayX_Raymarching initialization failed",
                                        "An unknown fatal error occurred.");
        SkylineDebugger::Shutdown();
        return EXIT_FAILURE;
    }
}
