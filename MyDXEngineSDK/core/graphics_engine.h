#pragma once
#include "skyline_api.h"

#include <array>

#include "my_engine.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <memory>
#include <directxtk12//GraphicsMemory.h>

#include "render_context.h"
#include "rhi/device.h"
#include "rhi/frame.h"
namespace Skyline::D3D12 { class FrameBackend; }
#include "tracy_profiler.h"

class SKYLINE_API GraphicsEngine
{
public:
    GraphicsEngine();
    ~GraphicsEngine() noexcept;

    GraphicsEngine(const GraphicsEngine&) = delete;
    GraphicsEngine& operator=(const GraphicsEngine&) = delete;
    GraphicsEngine(GraphicsEngine&&) = delete;
    GraphicsEngine& operator=(GraphicsEngine&&) = delete;

    bool init(HWND hwnd, UINT frameBufferWidth, UINT frameBufferHeight);
    void resize(UINT frameBufferWidth, UINT frameBufferHeight);

    [[nodiscard]]
      ComPtr<IDXGIFactory7> createDXGIFactory();

    void beginRender();
    void endRender();
    void waitDraw();

    [[nodiscard]]
    [[deprecated("Moved to the D3D12 backend. Use getRhiDevice() or D3D12::nativeFrameDevice(getRhiFrame()) for native integrations.")]]
    ID3D12Device14* getD3DDevice() const noexcept;

    [[nodiscard]]
    UINT getBackBufferIndex() const noexcept;

    [[nodiscard]]
    [[deprecated("Moved to the D3D12 backend. Use D3D12::nativeFrameQueue(getRhiFrame()) for native integrations.")]]
    ID3D12CommandQueue* getCommandQueue() const noexcept;

    [[nodiscard]]
    [[deprecated("Replaced by RHI command recording. Use getRhiCommandContext() or D3D12::nativeFrameCommands(getRhiFrame()) for native integrations.")]]
    ID3D12GraphicsCommandList10* getCommandList() const noexcept;

    [[deprecated("Replaced by RHI command recording. Use getRhiCommandContext().")]]
    RenderContext& getRenderContext();

    [[nodiscard]] Skyline::RHI::AttachmentLayout getBackBufferLayout() const;
    [[nodiscard]] Skyline::RHI::Device& getRhiDevice() const;

    TracyProfiler& getProfiler() noexcept;
    [[nodiscard]] Skyline::RHI::Frame& getRhiFrame() const;
    [[nodiscard]] Skyline::RHI::CommandContext& getRhiCommandContext() const;

    [[nodiscard]]
    UINT getFrameBufferWidth() const noexcept;
    [[nodiscard]]
    UINT GetFrameBufferHeight() const noexcept;

    [[nodiscard]]
    ID3D12Resource* getCurrentBackBuffer() const noexcept;

    [[nodiscard]]
    ID3D12Resource* getDepthStencilBuffer() const noexcept;

    [[nodiscard]]
    D3D12_CPU_DESCRIPTOR_HANDLE getCurrentRenderTargetView() const;
    [[nodiscard]]
    D3D12_CPU_DESCRIPTOR_HANDLE getDepthStencilView() const;

    enum : UINT
    {
        FRAME_BUFFER_COUNT = 2
    };

private:
    [[deprecated("Moved into the D3D12 frame backend. Use init().")]]
    bool createD3DDevice(IDXGIFactory7* dxgiFactory);

    [[deprecated("Moved into the D3D12 frame backend. Use init().")]]
    bool createCommandQueue();
    [[deprecated("Moved into the D3D12 frame backend. Use init().")]]
    bool createCommandList();

    [[deprecated("Moved into the D3D12 frame backend. Use init().")]]
    bool createSwapChain(
        HWND hwnd,
        UINT frameBufferWidth,
        UINT frameBufferHeight,
        IDXGIFactory7* dxgiFactory
    );
    [[deprecated("Moved into the D3D12 frame backend. Use init().")]]
    bool createSynchronizationWithGPUObject();
    [[deprecated("Moved into the D3D12 frame backend. Use resize().")]]
    void createBackBufferViews();
    [[deprecated("Moved into the D3D12 frame backend. Use resize().")]]
    void createDepthStencilBuffer();

    enum GPU_Vender
    {
        GPU_VenderNvidia,
        GPU_VenderAMD,
        GPU_VenderIntel,
        Num_GPUVender
    };

    std::unique_ptr<Skyline::D3D12::FrameBackend> m_backend;
};

extern SKYLINE_API GraphicsEngine* g_graphicsEngine;
