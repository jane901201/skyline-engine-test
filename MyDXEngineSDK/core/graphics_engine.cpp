#include "graphics_engine.h"
#include "backends/d3d12/d3d12_frame.h"
#include <stdexcept>

GraphicsEngine* g_graphicsEngine = nullptr;
GraphicsEngine::GraphicsEngine() = default;
GraphicsEngine::~GraphicsEngine() noexcept
{
    m_backend.reset();
    if (g_graphicsEngine == this) g_graphicsEngine = nullptr;
}
bool GraphicsEngine::init(HWND window, UINT width, UINT height)
{
    if (m_backend) throw std::logic_error("GraphicsEngine: Already initialized.");
    auto backend = std::make_unique<Skyline::D3D12::FrameBackend>();
    backend->init(window, width, height);
    m_backend = std::move(backend);
    g_graphicsEngine = this;
    return true;
}
Skyline::RHI::Frame& GraphicsEngine::getRhiFrame() const
{
    if (!m_backend) throw std::logic_error("GraphicsEngine: Initialize the engine before accessing its frame.");
    return *m_backend;
}
Skyline::RHI::Device& GraphicsEngine::getRhiDevice() const { return getRhiFrame().device(); }
Skyline::RHI::CommandContext& GraphicsEngine::getRhiCommandContext() const { return getRhiFrame().commands(); }
Skyline::RHI::AttachmentLayout GraphicsEngine::getBackBufferLayout() const { return getRhiFrame().attachments(); }
void GraphicsEngine::beginRender() { getRhiFrame().begin(); }
void GraphicsEngine::endRender() { getRhiFrame().end(); }
void GraphicsEngine::waitDraw() { getRhiFrame().waitIdle(); }
void GraphicsEngine::resize(UINT width, UINT height) { getRhiFrame().resize(width, height); }
ID3D12Device14* GraphicsEngine::getD3DDevice() const noexcept { return m_backend ? m_backend->getD3DDevice() : nullptr; }
ID3D12CommandQueue* GraphicsEngine::getCommandQueue() const noexcept { return m_backend ? m_backend->getCommandQueue() : nullptr; }
ID3D12GraphicsCommandList10* GraphicsEngine::getCommandList() const noexcept { return m_backend ? m_backend->getCommandList() : nullptr; }
UINT GraphicsEngine::getBackBufferIndex() const noexcept { return m_backend ? m_backend->frameIndex() : 0; }
UINT GraphicsEngine::getFrameBufferWidth() const noexcept { return m_backend ? m_backend->width() : 0; }
UINT GraphicsEngine::GetFrameBufferHeight() const noexcept { return m_backend ? m_backend->height() : 0; }
ID3D12Resource* GraphicsEngine::getCurrentBackBuffer() const noexcept { return m_backend ? m_backend->getCurrentBackBuffer() : nullptr; }
ID3D12Resource* GraphicsEngine::getDepthStencilBuffer() const noexcept { return m_backend ? m_backend->getDepthStencilBuffer() : nullptr; }
RenderContext& GraphicsEngine::getRenderContext() { (void)getRhiFrame(); return m_backend->getRenderContext(); }
TracyProfiler& GraphicsEngine::getProfiler() noexcept { return m_backend->getProfiler(); }
D3D12_CPU_DESCRIPTOR_HANDLE GraphicsEngine::getCurrentRenderTargetView() const { (void)getRhiFrame(); return m_backend->getCurrentRenderTargetView(); }
D3D12_CPU_DESCRIPTOR_HANDLE GraphicsEngine::getDepthStencilView() const { (void)getRhiFrame(); return m_backend->getDepthStencilView(); }
ComPtr<IDXGIFactory7> GraphicsEngine::createDXGIFactory() { Skyline::D3D12::FrameBackend factory; return factory.createDXGIFactory(); }
// 舊私有初始化函式保留供 API 遷移追蹤；實際初始化由後端負責。
bool GraphicsEngine::createD3DDevice(IDXGIFactory7* factory) { return m_backend->createD3DDevice(factory); }
bool GraphicsEngine::createCommandQueue() { return m_backend->createCommandQueue(); }
bool GraphicsEngine::createCommandList() { return m_backend->createCommandList(); }
bool GraphicsEngine::createSwapChain(HWND window, UINT width, UINT height, IDXGIFactory7* factory) { return m_backend->createSwapChain(window, width, height, factory); }
bool GraphicsEngine::createSynchronizationWithGPUObject() { return m_backend->createSynchronizationWithGPUObject(); }
void GraphicsEngine::createBackBufferViews() { m_backend->createBackBufferViews(); }
void GraphicsEngine::createDepthStencilBuffer() { m_backend->createDepthStencilBuffer(); }
