#pragma once
#include "core/my_engine.h"
#include "core/tracy_profiler.h"
#include "rhi/frame.h"
#include "d3d12_command_context.h"
#include <dxgi1_6.h>
#include <directxtk12/GraphicsMemory.h>
#include <array>
class GraphicsEngine;
namespace Skyline::D3D12
{
class FrameBackend final : public RHI::Frame
{
public:
    FrameBackend() = default;
    ~FrameBackend() noexcept;

    FrameBackend(const FrameBackend&) = delete;
    FrameBackend& operator=(const FrameBackend&) = delete;
    FrameBackend(FrameBackend&&) = delete;
    FrameBackend& operator=(FrameBackend&&) = delete;

    bool init(HWND hwnd, UINT frameBufferWidth, UINT frameBufferHeight);


    [[nodiscard]]
      ComPtr<IDXGIFactory7> createDXGIFactory();

    void beginRender();
    void endRender();
    void waitDraw();

    [[nodiscard]]
    ID3D12Device14* getD3DDevice() const noexcept
    {
        return m_d3dDevice.Get();
    }

    [[nodiscard]]
    UINT getBackBufferIndex() const noexcept
    {
        return m_frameIndex;
    }

    [[nodiscard]]
    ID3D12CommandQueue* getCommandQueue() const noexcept
    {
        return m_commandQueue.Get();
    }

    [[nodiscard]]
    ID3D12GraphicsCommandList10* getCommandList() const noexcept
    {
        return m_commandList.Get();
    }

    RenderContext& getRenderContext()
    {
        return m_renderContext;
    }

    [[nodiscard]] Skyline::RHI::AttachmentLayout getBackBufferLayout() const;
    [[nodiscard]] Skyline::RHI::Device& getRhiDevice() const;

    TracyProfiler& getProfiler() noexcept { return m_profiler; }

    [[nodiscard]]
    UINT getFrameBufferWidth() const noexcept
    {
        return m_frameBufferWidth;
    }
    [[nodiscard]]
    UINT GetFrameBufferHeight() const noexcept
    {
        return m_frameBufferHeight;
    }

    [[nodiscard]]
    ID3D12Resource* getCurrentBackBuffer() const noexcept
    {
        return m_renderTargets[m_frameIndex].Get();
    }

    [[nodiscard]]
    ID3D12Resource* getDepthStencilBuffer() const noexcept
    {
        return m_depthStencilBuffer.Get();
    }

    [[nodiscard]]
    D3D12_CPU_DESCRIPTOR_HANDLE getCurrentRenderTargetView() const;
    [[nodiscard]]
    D3D12_CPU_DESCRIPTOR_HANDLE getDepthStencilView() const;

    enum : UINT
    {
        FRAME_BUFFER_COUNT = 2
    };

    RHI::Device& device() const override { return getRhiDevice(); }
    RHI::CommandContext& commands() override { return *m_rhiCommands; }
    RHI::AttachmentLayout attachments() const override { return getBackBufferLayout(); }
    std::uint32_t frameIndex() const noexcept override { return getBackBufferIndex(); }
    std::uint32_t width() const noexcept override { return getFrameBufferWidth(); }
    std::uint32_t height() const noexcept override { return GetFrameBufferHeight(); }
    void begin() override { beginRender(); }
    void end() override { endRender(); }
    void waitIdle() override { waitDraw(); }
    void resize(std::uint32_t width, std::uint32_t height) override;
private:
    friend class ::GraphicsEngine;
    bool createD3DDevice(IDXGIFactory7* dxgiFactory);

    bool createCommandQueue();
    bool createCommandList();

    bool createSwapChain(
        HWND hwnd,
        UINT frameBufferWidth,
        UINT frameBufferHeight,
        IDXGIFactory7* dxgiFactory
    );
    bool createSynchronizationWithGPUObject();
    void createBackBufferViews();
    void createDepthStencilBuffer();

    enum GPU_Vender
    {
        GPU_VenderNvidia,
        GPU_VenderAMD,
        GPU_VenderIntel,
        Num_GPUVender
    };

    ComPtr<ID3D12Device14> m_d3dDevice;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<IDXGISwapChain4> m_swapChain;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    ComPtr<ID3D12Resource> m_depthStencilBuffer;
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12GraphicsCommandList10> m_commandList;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    std::array<ComPtr<ID3D12Resource>, FRAME_BUFFER_COUNT> m_renderTargets;
    ComPtr<ID3D12Fence> m_fence;

    int m_currentBackBufferIndex = 0;
    UINT m_rtvDescriptorSize = 0;
    UINT m_frameIndex = 0;
    HANDLE m_fenceEvent = nullptr;
    UINT64 m_fenceValue = 0;
    UINT m_frameBufferWidth = 0;
    UINT m_frameBufferHeight = 0;

    RenderContext m_renderContext;
    std::unique_ptr<Skyline::RHI::Device> m_rhiDevice;
    std::unique_ptr<CommandContext> m_rhiCommands;
    bool m_frameActive = false;
    TracyProfiler m_profiler;
    std::unique_ptr<DirectX::GraphicsMemory> m_directXTKGfxMemory;
};


SKYLINE_API std::unique_ptr<RHI::Frame> createFrame(const RHI::FrameDesc& desc);
SKYLINE_API ID3D12Device* nativeFrameDevice(const RHI::Frame& frame);
}
