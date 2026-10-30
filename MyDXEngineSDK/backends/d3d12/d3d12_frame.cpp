#include "d3d12_frame.h"

#include <stdexcept>
#include "backends/d3d12/d3d12_pipeline.h"
#include "backends/d3d12/d3d12_device.h"
#include <wrl/client.h>
#include <directx/d3dx12_core.h>

#ifdef SKYLINE_ENABLE_TRACY
#include <tracy/Tracy.hpp>
#endif



namespace Skyline::D3D12
{
Skyline::RHI::Device& FrameBackend::getRhiDevice() const
{
    if (m_rhiDevice == nullptr)
        throw std::logic_error("GraphicsEngine: Initialize the engine before accessing the RHI device.");
    return *m_rhiDevice;
}

Skyline::RHI::AttachmentLayout FrameBackend::getBackBufferLayout() const
{
    if (m_swapChain == nullptr || m_depthStencilBuffer == nullptr)
        throw std::logic_error("GraphicsEngine: Initialize the swap chain before querying attachment layout.");
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    if (FAILED(m_swapChain->GetDesc1(&swapChainDesc)))
        throw std::runtime_error("GraphicsEngine: Failed to query the swap-chain attachment layout.");
    const auto depthDesc = m_depthStencilBuffer->GetDesc();
    if (depthDesc.SampleDesc.Count != swapChainDesc.SampleDesc.Count ||
        depthDesc.SampleDesc.Quality != swapChainDesc.SampleDesc.Quality)
        throw std::logic_error("GraphicsEngine: Color and depth sample layouts must match.");
    Skyline::RHI::AttachmentLayout layout;
    layout.colorCount = 1;
    layout.colorFormats[0] = Skyline::D3D12::fromNativeFormat(swapChainDesc.Format);
    layout.depthFormat = Skyline::D3D12::fromNativeFormat(depthDesc.Format);
    layout.samples = {swapChainDesc.SampleDesc.Count, swapChainDesc.SampleDesc.Quality};
    return layout;
}

FrameBackend::~FrameBackend() noexcept
{
    if (m_commandQueue != nullptr && m_fence != nullptr)
    {
        try
        {
            waitDraw(); // 等待 GPU 完成目前提交的工作
        }
        catch (...)
        {
            //TODO: 解構函式不得讓同步失敗的例外向外傳遞?
        }
    }

    // 如果 Fence Event 已建立則釋放 Windows Handle
    if (m_fenceEvent != nullptr)
        CloseHandle(m_fenceEvent);



}

void FrameBackend::waitDraw()
{
    // 取得這次同步使用的 Fence 值並遞增下一次的值
    const UINT64 fence = m_fenceValue++;

    // 在 Command Queue 插入 Fence Signal
    if (FAILED(m_commandQueue->Signal(m_fence.Get(), fence)))
        throw std::runtime_error("GraphicsEngine: Failed to signal the GPU fence.");

    // 如果 GPU 尚未執行到指定 Fence 值就進行等待
    if (m_fence->GetCompletedValue() < fence)
    {
        if (FAILED(m_fence->SetEventOnCompletion(fence, m_fenceEvent)))
            throw std::runtime_error("GraphicsEngine: Failed to register the GPU fence event.");

        // 等待直到 GPU 完成或發生錯誤
        while (true)
        {
            const DWORD waitResult = WaitForSingleObject(m_fenceEvent, 1000);
            if (waitResult == WAIT_OBJECT_0)
                break;
            if (waitResult != WAIT_TIMEOUT)
                throw std::runtime_error("GraphicsEngine: Failed while waiting for the GPU fence.");
            if (FAILED(m_d3dDevice->GetDeviceRemovedReason()))
                throw std::runtime_error(
                    "GraphicsEngine: The Direct3D device was removed while waiting for the GPU.");
        }
    }
}

bool FrameBackend::init(HWND hwnd, UINT frameBufferWidth, UINT frameBufferHeight)
{
    if (m_rhiDevice) throw std::logic_error("Frame: Already initialized.");
    if (hwnd == nullptr || frameBufferWidth == 0 || frameBufferHeight == 0)
        throw std::invalid_argument("GraphicsEngine: A valid window and framebuffer size are required.");

#ifdef _DEBUG
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        debugController->EnableDebugLayer();
#endif


    m_frameBufferWidth = frameBufferWidth;
    m_frameBufferHeight = frameBufferHeight;

    ComPtr<IDXGIFactory7> dxgiFactory = createDXGIFactory();

    if (dxgiFactory == nullptr)
        throw std::runtime_error("GraphicsEngine: Failed to create the DXGI factory.");
    if (!createD3DDevice(dxgiFactory.Get()))
        throw std::runtime_error("GraphicsEngine: Failed to create a Direct3D 12 device.");
    if (!createCommandQueue())
        throw std::runtime_error("GraphicsEngine: Failed to create the command queue.");

    if (FAILED(m_d3dDevice->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&m_commandAllocator))))
    {
        throw std::runtime_error("GraphicsEngine: Failed to create the command allocator.");
    }

    if (!createSwapChain(hwnd, frameBufferWidth, frameBufferHeight, dxgiFactory.Get()))
        throw std::runtime_error("GraphicsEngine: Failed to create the swap chain.");

    // 建立 RTV Descriptor Heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.NumDescriptors = FRAME_BUFFER_COUNT;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    if (FAILED(m_d3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap))))
        throw std::runtime_error("GraphicsEngine: Failed to create the render-target descriptor heap.");

    m_rtvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    createBackBufferViews();

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDescription{};
    dsvHeapDescription.NumDescriptors = 1;
    dsvHeapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    if (FAILED(m_d3dDevice->CreateDescriptorHeap(
            &dsvHeapDescription,
            IID_PPV_ARGS(&m_dsvHeap))))
    {
        throw std::runtime_error("GraphicsEngine: Failed to create the depth-stencil descriptor heap.");
    }
    createDepthStencilBuffer();

    if (!createCommandList())
        throw std::runtime_error("GraphicsEngine: Failed to create the command list.");
    if (!createSynchronizationWithGPUObject())
        throw std::runtime_error("GraphicsEngine: Failed to create GPU synchronization objects.");

    m_renderContext.init(m_commandList.Get());
    m_profiler.init(m_d3dDevice.Get(), m_commandQueue.Get());
    m_rhiDevice = Skyline::D3D12::createDevice(*m_d3dDevice.Get());
    m_rhiCommands = std::make_unique<CommandContext>(*m_rhiDevice, *m_d3dDevice.Get(), m_commandList.Get());
#ifdef SKYLINE_ENABLE_TRACY
    m_rhiCommands->setProfiler(m_profiler.gpuContext());
#endif
    return true;
}

void FrameBackend::createBackBufferViews()
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT index = 0; index < FRAME_BUFFER_COUNT; ++index)
    {
        if (FAILED(m_swapChain->GetBuffer(index, IID_PPV_ARGS(&m_renderTargets[index]))))
            throw std::runtime_error("GraphicsEngine: Failed to acquire a swap-chain buffer.");
        m_d3dDevice->CreateRenderTargetView(m_renderTargets[index].Get(), nullptr, handle);
        handle.ptr += m_rtvDescriptorSize;
    }
}

void FrameBackend::createDepthStencilBuffer()
{
    const D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    const D3D12_RESOURCE_DESC description = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_D32_FLOAT,
        m_frameBufferWidth,
        m_frameBufferHeight,
        1,
        1,
        1,
        0,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1.0f;

    if (FAILED(m_d3dDevice->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &description,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clearValue,
            IID_PPV_ARGS(&m_depthStencilBuffer))))
    {
        throw std::runtime_error("GraphicsEngine: Failed to create the depth-stencil buffer.");
    }
    m_d3dDevice->CreateDepthStencilView(
        m_depthStencilBuffer.Get(),
        nullptr,
        m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
}

void FrameBackend::resize(UINT frameBufferWidth, UINT frameBufferHeight)
{
    if (m_frameActive) throw std::logic_error("Frame: Finish the active frame before resizing.");
    if (frameBufferWidth == 0 || frameBufferHeight == 0)
        return;
    if (frameBufferWidth == m_frameBufferWidth && frameBufferHeight == m_frameBufferHeight)
        return;

    waitDraw();
    for (auto& renderTarget : m_renderTargets)
    {
        renderTarget.Reset();
    }
    m_depthStencilBuffer.Reset();

    if (FAILED(m_swapChain->ResizeBuffers(
            FRAME_BUFFER_COUNT,
            frameBufferWidth,
            frameBufferHeight,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            0)))
    {
        throw std::runtime_error("GraphicsEngine: Failed to resize the swap chain.");
    }
    m_frameBufferWidth = frameBufferWidth;
    m_frameBufferHeight = frameBufferHeight;
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    createBackBufferViews();
    createDepthStencilBuffer();
}

D3D12_CPU_DESCRIPTOR_HANDLE FrameBackend::getCurrentRenderTargetView() const
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(m_frameIndex) * m_rtvDescriptorSize;
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE FrameBackend::getDepthStencilView() const
{
    return m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
}

ComPtr<IDXGIFactory7> FrameBackend::createDXGIFactory()
{
    ComPtr<IDXGIFactory7> factory;
    UINT factoryFlags = 0;
#ifdef _DEBUG
    factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

    if (FAILED(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&factory))))
        return nullptr;

    return factory;
}

bool FrameBackend::createD3DDevice(IDXGIFactory7* dxgiFactory)
{
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    for (UINT index = 0;
         dxgiFactory->EnumAdapterByGpuPreference(
             index,
             DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
             IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf())) != DXGI_ERROR_NOT_FOUND;
         ++index)
    {
        DXGI_ADAPTER_DESC1 description{};
        adapter->GetDesc1(&description);
        if ((description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
            continue;

        if (SUCCEEDED(D3D12CreateDevice(
            adapter.Get(),
            D3D_FEATURE_LEVEL_12_0,
            IID_PPV_ARGS(&m_d3dDevice))))
        {
            return true;
        }
    }

    return SUCCEEDED(D3D12CreateDevice(
        nullptr,
        D3D_FEATURE_LEVEL_12_0,
        IID_PPV_ARGS(&m_d3dDevice)));
}

bool FrameBackend::createCommandQueue()
{
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    return SUCCEEDED(m_d3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));
}

bool FrameBackend::createSwapChain(
    HWND hwnd,
    UINT frameBufferWidth,
    UINT frameBufferHeight,
    IDXGIFactory7* dxgiFactory)
{
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};

    // 設定 Back Buffer 數量
    swapChainDesc.BufferCount = FRAME_BUFFER_COUNT;

    // 設定每個 Back Buffer 的長寬
    swapChainDesc.Width = frameBufferWidth;
    swapChainDesc.Height = frameBufferHeight;

    //設定格式
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    //表示 Swap Chain Buffer 會被做為 Render Target 使用
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

    // 使用 Flip Model 的 DISCARD 模式
    //Present 後舊 Buffer 不保證保留，適合一般即時渲染
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    // 先回傳 IDXGISwapChain1
    ComPtr<IDXGISwapChain1> swapChain;
    if (FAILED(dxgiFactory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        swapChain.GetAddressOf())))
    {
        return false;
    }

    // 在將 IDXGISwapChain1 查詢成新版本的 IDXGISwapChain4
    if (FAILED(swapChain->QueryInterface(IID_PPV_ARGS(&m_swapChain))))
        return false;

    // 紀錄目前可供渲染的 Back Buffer 索引
    m_currentBackBufferIndex = static_cast<int>(m_swapChain->GetCurrentBackBufferIndex());
    return true;
}

bool FrameBackend::createCommandList()
{
    if (FAILED(m_d3dDevice->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_commandAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(&m_commandList))))
    {
        return false;
    }

    return SUCCEEDED(m_commandList->Close());
}

bool FrameBackend::createSynchronizationWithGPUObject()
{
    //建立初始值為 0 的 Fence，用來追蹤 Command Queue 裡 GPU 命令的完成進度
    if (FAILED(m_d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence))))
        return false;

    m_fenceValue = 1;
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    return m_fenceEvent != nullptr;
}

void FrameBackend::beginRender()
{
    if (m_frameActive) throw std::logic_error("Frame: Finish the active frame first.");
#ifdef SKYLINE_ENABLE_TRACY
    ZoneScoped;
#endif
    m_profiler.beginFrame();
    // 每一幀開始時，向 Swap Chain 查詢目前可寫入的 Back Buffer
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // 重設 Command Allocator，回收上一輪命令記錄使用的記憶體
    if (FAILED(m_commandAllocator->Reset()))
        throw std::runtime_error("GraphicsEngine: Failed to reset the command allocator.");
    if (FAILED(m_commandList->Reset(m_commandAllocator.Get(), nullptr)))
        throw std::runtime_error("GraphicsEngine: Failed to reset the command list.");

    //使用 D3D12_RESOURCE_BARRIER 去做資源狀態轉換
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_renderTargets[m_frameIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &barrier);

    // Viewport 將結果映射到畫面像素範圍
    const D3D12_VIEWPORT viewport{
        0.0f,
        0.0f,
        static_cast<float>(m_frameBufferWidth),
        static_cast<float>(m_frameBufferHeight),
        0.0f,
        1.0f};

    // Scissor Rect 限制實際上顯示的範圍
    const D3D12_RECT scissorRect{
        0,
        0,
        static_cast<LONG>(m_frameBufferWidth),
        static_cast<LONG>(m_frameBufferHeight)};
    m_commandList->RSSetViewports(1, &viewport);
    m_commandList->RSSetScissorRects(1, &scissorRect);

    // 取得目前 Back Buffer 的 RTV，以及深度緩衝區的 DSV
    const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = getCurrentRenderTargetView();
    const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = getDepthStencilView();

    // 將目前 RTV 與 DSV 綁定到 Output Merger
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    //TODO: 設定每幀清除 Render Target 使用的背景顏色 (這一定要?
    constexpr float clearColor[] = {0.08f, 0.10f, 0.14f, 1.0f};
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_commandList->ClearDepthStencilView(
        dsvHandle,
        D3D12_CLEAR_FLAG_DEPTH,
        1.0f,
        0,
        0,
        nullptr);
    m_rhiCommands->beginExternal(getBackBufferLayout());
    m_frameActive = true;
}

void FrameBackend::endRender()
{
    if (!m_frameActive) throw std::logic_error("Frame: begin() must precede end().");
#ifdef SKYLINE_ENABLE_TRACY
    ZoneScoped;
#endif
    //繪圖完畢後，把 Back Buffer 從 RENDER_TARGET 狀態轉回 PRESENT
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_renderTargets[m_frameIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &barrier);

    if (FAILED(m_commandList->Close()))
        throw std::runtime_error("GraphicsEngine: Failed to close the command list.");

    ID3D12CommandList* commandLists[] = {m_commandList.Get()};
    m_commandQueue->ExecuteCommandLists(1, commandLists);
    m_profiler.collect();
    if (FAILED(m_swapChain->Present(1, 0)))
        throw std::runtime_error("GraphicsEngine: Failed to present the frame.");

    // 等待 GPU 完成本幀工作
    waitDraw();
#ifdef SKYLINE_ENABLE_TRACY
    FrameMark;
#endif
    m_rhiCommands->completeExternal();
    m_frameActive = false;
}

std::unique_ptr<RHI::Frame> createFrame(const RHI::FrameDesc& desc)
{
    auto frame = std::make_unique<FrameBackend>();
    frame->init(static_cast<HWND>(desc.nativeWindow), desc.width, desc.height);
    return frame;
}
ID3D12Device* nativeFrameDevice(const RHI::Frame& frame)
{
    const auto* backend = dynamic_cast<const FrameBackend*>(&frame);
    if (!backend) throw std::invalid_argument("D3D12 frame interop: Incompatible backend.");
    return backend->getD3DDevice();
}
}
