#include "backends/d3d12/d3d12_interop.h"
#include "../rhi_example.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <directx/d3dx12_core.h>
#include <DirectXMath.h>
#include <exception>
#include <imgui.h>
#include <stdexcept>

#include "constant_buffer.h"
#include "graphics_engine.h"
#include "my_engine.h"
#include "pipeline_state.h"
#include "render_pass/render_state.h"
#include "root_signature.h"
#include "rw_structured_buffer.h"
#include "shader.h"
#include "skyline_debugger.h"
#include "system.h"
#include "ui/imgui_system.h"

namespace
{
constexpr UINT ParticleCapacity = 100000;
constexpr UINT ComputeThreadCount = 64;

struct Particle
{
    DirectX::XMFLOAT3 position;
    float life;
    DirectX::XMFLOAT3 velocity;
    float padding;
};

struct ParticleSimulationConstants
{
    float deltaTime;
    UINT particleCount;
    UINT particleCapacity;
    float boundarySize;
    float speed;
    float lifetime;
    UINT reset;
    UINT randomSeed;
};

struct CameraConstants
{
    DirectX::XMFLOAT4X4 viewProjection;
    DirectX::XMFLOAT3 cameraRight;
    float particleSize;
    DirectX::XMFLOAT3 cameraUp;
    float padding;
    DirectX::XMFLOAT4 color;
};

static_assert(sizeof(Particle) == 32, "Particle must match the HLSL structured-buffer layout.");
static_assert(sizeof(ParticleSimulationConstants) == 32,
              "ParticleSimulationConstants must match the HLSL constant-buffer layout.");
static_assert(sizeof(CameraConstants) == 112, "CameraConstants must match the HLSL constant-buffer layout.");

[[deprecated("Replaced by RHI graphics pipeline creation.")]]
D3D12_GRAPHICS_PIPELINE_STATE_DESC createGraphicsPipelineDescription(
    ID3D12RootSignature* rootSignature, ID3DBlob* vertexShader, ID3DBlob* pixelShader)
{
    if (rootSignature == nullptr)
        throw std::invalid_argument("DayX_ComputeShader: A root signature is required.");
    if (vertexShader == nullptr || pixelShader == nullptr)
        throw std::invalid_argument("DayX_ComputeShader: Compiled graphics shaders are required.");

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

[[deprecated("Replaced by Device::createComputePipeline().")]]
D3D12_COMPUTE_PIPELINE_STATE_DESC createComputePipelineDescription(
    ID3D12RootSignature* rootSignature, ID3DBlob* computeShader)
{
    if (rootSignature == nullptr)
        throw std::invalid_argument("DayX_ComputeShader: A root signature is required.");
    if (computeShader == nullptr)
        throw std::invalid_argument("DayX_ComputeShader: A compiled compute shader is required.");

    D3D12_COMPUTE_PIPELINE_STATE_DESC description{};
    description.pRootSignature = rootSignature;
    description.CS = CD3DX12_SHADER_BYTECODE(computeShader);
    return description;
}

CameraConstants createCameraConstants(UINT width, UINT height)
{
    if (width == 0 || height == 0)
        throw std::invalid_argument("DayX_ComputeShader: Frame-buffer dimensions must be non-zero.");

    const float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    const DirectX::XMVECTOR eye = DirectX::XMVectorSet(0.0f, 0.0f, -22.0f, 1.0f);
    const DirectX::XMVECTOR target = DirectX::XMVectorZero();
    const DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    const DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(eye, target, up);
    const DirectX::XMMATRIX projection =
        DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV4, aspectRatio, 0.1f, 100.0f);

    CameraConstants constants{};
    DirectX::XMStoreFloat4x4(&constants.viewProjection, view * projection);
    constants.cameraRight = {1.0f, 0.0f, 0.0f};
    constants.cameraUp = {0.0f, 1.0f, 0.0f};
    constants.particleSize = 0.06f;
    constants.color = {0.2f, 0.65f, 1.0f, 0.85f};
    return constants;
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, LPWSTR commandLine, int showCommand)
{
    try
    {
        SkylineDebugger::Initialize("DayX_ComputeShader");
        initWindow(instance, previous, commandLine, showCommand, TEXT("Compute Shader Particles"));
        if (g_hWnd == nullptr)
            throw std::runtime_error("DayX_ComputeShader: Failed to create the application window.");

        GraphicsEngine graphicsEngine;
        if (!graphicsEngine.init(g_hWnd, FRAME_BUFFER_W, FRAME_BUFFER_H))
            throw std::runtime_error("DayX_ComputeShader: Failed to initialize the graphics engine.");
        SkylineDebugger::ConfigureD3D12(Skyline::D3D12::nativeFrameDevice(graphicsEngine.getRhiFrame()));

        using namespace Skyline::RHI;
        auto& device = graphicsEngine.getRhiDevice();
        BindingLayoutDesc computeLayoutDesc;
        computeLayoutDesc.constantBuffers = {{0}};
        computeLayoutDesc.resources = {{BindingType::StorageResource, 0, 1}};
        auto computeLayout = device.createBindingLayout(computeLayoutDesc);
        BindingLayoutDesc graphicsLayoutDesc;
        graphicsLayoutDesc.constantBuffers = {{0}};
        graphicsLayoutDesc.resources = {{BindingType::SampledResource, 0, 1}};
        auto graphicsLayout = device.createBindingLayout(graphicsLayoutDesc);
        auto computeShader = device.createShader({"assets/shaders/particle_compute.hlsl", "CSMain", Skyline::RHI::ShaderStage::Compute});
        auto computePipeline = device.createComputePipeline({"Particles", computeShader.get(), computeLayout.get()});
        auto vs = device.createShader({"assets/shaders/particle_render.hlsl", "VSMain", Skyline::RHI::ShaderStage::Vertex});
        auto ps = device.createShader({"assets/shaders/particle_render.hlsl", "PSMain", Skyline::RHI::ShaderStage::Pixel});
        auto graphicsDesc = GraphicsPipelineDesc::fullscreen();
        graphicsDesc.vertexShader = vs.get(); graphicsDesc.pixelShader = ps.get(); graphicsDesc.bindingLayout = graphicsLayout.get();
        graphicsDesc.attachments = graphicsEngine.getRhiFrame().attachments();
        graphicsDesc.blend[0] = BlendDesc::alpha(); graphicsDesc.depth.test = true;
        auto graphicsPipeline = device.createGraphicsPipeline(graphicsDesc);

        ParticleSimulationConstants simulationConstants{};
        simulationConstants.particleCount = 10000;
        simulationConstants.particleCapacity = ParticleCapacity;
        simulationConstants.boundarySize = 7.0f;
        simulationConstants.speed = 2.0f;
        simulationConstants.lifetime = 8.0f;
        simulationConstants.reset = 1;
        simulationConstants.randomSeed = 1337;

        CameraConstants cameraConstants = createCameraConstants(
            graphicsEngine.getFrameBufferWidth(), graphicsEngine.GetFrameBufferHeight());

        auto simulationConstantBuffer = Skyline::Examples::constants(device, std::as_bytes(std::span{&simulationConstants, 1}));
        auto cameraConstantBuffer = Skyline::Examples::constants(device, std::as_bytes(std::span{&cameraConstants, 1}));
        auto simView = device.createBufferView(simulationConstantBuffer, {BindingType::ConstantBuffer, 0, sizeof(simulationConstants)});
        auto cameraView = device.createBufferView(cameraConstantBuffer, {BindingType::ConstantBuffer, 0, sizeof(cameraConstants)});
        std::array<std::shared_ptr<Buffer>, GraphicsEngine::FRAME_BUFFER_COUNT> particles;
        std::array<std::unique_ptr<BindingSet>, GraphicsEngine::FRAME_BUFFER_COUNT> computeSets, graphicsSets;
        for (std::size_t index = 0; index < particles.size(); ++index)
        {
            particles[index] = device.createBuffer({sizeof(Particle) * ParticleCapacity, BufferUsage::Storage | BufferUsage::Sampled,
                MemoryType::Gpu, ResourceState::Storage});
            auto uav = device.createBufferView(particles[index], {BindingType::StorageResource, 0, sizeof(Particle) * ParticleCapacity, sizeof(Particle)});
            auto srv = device.createBufferView(particles[index], {BindingType::SampledResource, 0, sizeof(Particle) * ParticleCapacity, sizeof(Particle)});
            computeSets[index] = device.createBindingSet({computeLayout.get(), {{0, 0, simView}, {0, 0, uav}}});
            graphicsSets[index] = device.createBindingSet({graphicsLayout.get(), {{0, 0, cameraView}, {0, 0, srv}}});
        }
        auto& renderContext = graphicsEngine.getRhiCommandContext();

        Skyline::UI::ImGuiSystem imguiSystem;
        imguiSystem.initialize(g_hWnd, graphicsEngine);

        bool paused = false;
        int resetFramesRemaining = GraphicsEngine::FRAME_BUFFER_COUNT;
        std::array<std::chrono::steady_clock::time_point, GraphicsEngine::FRAME_BUFFER_COUNT> previousTimes{};
        const auto initialTime = std::chrono::steady_clock::now();
        previousTimes.fill(initialTime);

        while (dispatchWindowMessage())
        {
            graphicsEngine.beginRender();
            imguiSystem.beginFrame();

            int displayedParticleCount = static_cast<int>(simulationConstants.particleCount);
            if (imguiSystem.beginWindow("Compute shader particles"))
            {
                ImGui::TextUnformatted("Particle motion is simulated entirely on the GPU.");
                ImGui::Separator();
                ImGui::SliderInt("Particle count", &displayedParticleCount, 1,
                                 static_cast<int>(ParticleCapacity), "%d", ImGuiSliderFlags_Logarithmic);
                ImGui::SliderFloat("Spread range", &simulationConstants.boundarySize, 1.0f, 12.0f, "%.1f");
                ImGui::SliderFloat("Movement speed", &simulationConstants.speed, 0.0f, 8.0f, "%.2f");
                ImGui::SliderFloat("Lifetime", &simulationConstants.lifetime, 1.0f, 20.0f, "%.1f s");
                ImGui::SliderFloat("Particle size", &cameraConstants.particleSize, 0.01f, 0.25f, "%.3f");
                ImGui::ColorEdit4("Particle color", &cameraConstants.color.x);
                ImGui::Checkbox("Pause simulation", &paused);
                if (ImGui::Button("Reset particles"))
                {
                    ++simulationConstants.randomSeed;
                    resetFramesRemaining = GraphicsEngine::FRAME_BUFFER_COUNT;
                }
                ImGui::SameLine();
                ImGui::Text("Dispatch groups: %u",
                            (simulationConstants.particleCount + ComputeThreadCount - 1) / ComputeThreadCount);
                ImGui::Text("Frame rate: %.1f FPS", ImGui::GetIO().Framerate);
            }
            imguiSystem.endWindow();
            simulationConstants.particleCount = static_cast<UINT>(displayedParticleCount);

            const UINT frameIndex = graphicsEngine.getBackBufferIndex();
            const auto currentTime = std::chrono::steady_clock::now();
            const float elapsed = std::chrono::duration<float>(currentTime - previousTimes[frameIndex]).count();
            previousTimes[frameIndex] = currentTime;
            simulationConstants.deltaTime = paused ? 0.0f : (std::min)(elapsed, 0.05f);
            simulationConstants.reset = resetFramesRemaining > 0 ? 1u : 0u;

            simulationConstantBuffer->write(0, std::as_bytes(std::span{&simulationConstants, 1}));
            cameraConstantBuffer->write(0, std::as_bytes(std::span{&cameraConstants, 1}));
            renderContext.setPipeline(*computePipeline);
            renderContext.setBindingSet(*computeSets[frameIndex]);
            const UINT dispatchedParticles = simulationConstants.reset != 0
                ? simulationConstants.particleCapacity : simulationConstants.particleCount;
            renderContext.dispatch((dispatchedParticles + ComputeThreadCount - 1) / ComputeThreadCount, 1, 1);
            renderContext.storageBarrier(*particles[frameIndex]);
            renderContext.transition(*particles[frameIndex], ResourceState::ShaderRead);
            renderContext.setPipeline(*graphicsPipeline);
            renderContext.setBindingSet(*graphicsSets[frameIndex]);
            renderContext.draw(6, simulationConstants.particleCount);
            renderContext.transition(*particles[frameIndex], ResourceState::Storage);

            if (resetFramesRemaining > 0)
                --resetFramesRemaining;

            imguiSystem.render();
            graphicsEngine.endRender();
            SkylineDebugger::LogD3D12Messages(Skyline::D3D12::nativeFrameDevice(graphicsEngine.getRhiFrame()));
        }

        graphicsEngine.waitDraw();
        imguiSystem.shutdown();
        SkylineDebugger::Shutdown();
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        SkylineDebugger::ShowFatalError("DayX_ComputeShader initialization failed", error);
        SkylineDebugger::Shutdown();
        return EXIT_FAILURE;
    }
    catch (...)
    {
        SkylineDebugger::ShowFatalError(
            "DayX_ComputeShader initialization failed", "An unknown fatal error occurred.");
        SkylineDebugger::Shutdown();
        return EXIT_FAILURE;
    }
}
