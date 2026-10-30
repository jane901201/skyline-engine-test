#pragma once

#include "rhi/command_context.h"
#include "d3d12_resource.h"
#include <functional>
#include <unordered_map>
#include <stdexcept>
#ifdef SKYLINE_ENABLE_TRACY
#include <tracy/TracyD3D12.hpp>
#endif

namespace Skyline::D3D12
{
class CommandContext final : public RHI::CommandContext
{
public:
    CommandContext(const RHI::Device& owner, ID3D12Device& device, ID3D12GraphicsCommandList10* external = nullptr);
    ~CommandContext() override;
    void begin() override;
    void submitAndWait() override;
    void setPipeline(const RHI::Pipeline& pipeline) override;
    void setBindingSet(const RHI::BindingSet& set) override;
    void setVertexBuffer(const RHI::Buffer& buffer, UINT stride, UINT64 offset) override;
    void setIndexBuffer(const RHI::Buffer& buffer, RHI::IndexType type, UINT64 offset) override;
    void setPrimitiveTopology(RHI::PrimitiveType primitive) override;
    void setViewport(const RHI::Viewport& viewport, const RHI::Scissor& scissor) override;
    void setRenderTarget(const RHI::Texture& color, const RHI::Texture* depth) override;
    void clearColor(const RHI::Texture& color, const std::array<float, 4>& value) override;
    void clearDepth(const RHI::Texture& depth, float value) override;
    void transition(const RHI::Buffer& buffer, RHI::ResourceState after) override;
    void transition(const RHI::Texture& texture, RHI::ResourceState after) override;
    void storageBarrier(const RHI::Buffer& buffer) override;
    void storageBarrier(const RHI::Texture& texture) override;
    void uploadBuffer(const RHI::Buffer& buffer, UINT64 offset, std::span<const std::byte> data, RHI::ResourceState after) override;
    void uploadTexture(const RHI::Texture& texture, std::span<const std::byte> data, UINT rowPitch, RHI::ResourceState after) override;
    void copyBuffer(const RHI::Buffer& destination, UINT64 destinationOffset, const RHI::Buffer& source, UINT64 sourceOffset, UINT64 size) override;
    std::vector<std::byte> readTexture(const RHI::Texture& texture) override;
    void draw(UINT count, UINT instances, UINT first) override;
    void drawIndexed(UINT count, UINT instances, UINT first, INT offset) override;
    void dispatch(UINT x, UINT y, UINT z) override;
    void beginExternal(const RHI::AttachmentLayout& attachments);
    void completeExternal();
#ifdef SKYLINE_ENABLE_TRACY
    void setProfiler(TracyD3D12Ctx context) noexcept { m_profiler = context; }
#endif
    ID3D12GraphicsCommandList10* native() const noexcept { return m_commands.Get(); }
private:
    void resetState();
    void requireRecording() const;
    void requireGraphics() const;
    void wait();
    Microsoft::WRL::ComPtr<ID3D12Resource> staging(UINT64 size, D3D12_HEAP_TYPE heap);
    template<class Backend, class Public> const Backend& object(const Public& value) const
    {
        const auto* result = dynamic_cast<const Backend*>(&value);
        if (!result || &value.device() != &device())
            throw std::invalid_argument("CommandContext: Object must belong to this backend and device.");
        return *result;
    }
    template<class Resource> RHI::ResourceState current(const Resource& resource) const
    {
        const auto found = m_states.find(&resource);
        return found == m_states.end() ? resource.state : found->second.state;
    }
    template<class Resource> void change(const Resource& resource, RHI::ResourceState after);
    struct Pending { RHI::ResourceState state; std::function<void()> commit; };
    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList10> m_commands;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_allocator;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_queue;
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    HANDLE m_event = nullptr;
#ifdef SKYLINE_ENABLE_TRACY
    TracyD3D12Ctx m_profiler = nullptr;
#endif
    UINT64 m_fenceValue = 0;
    bool m_external = false, m_recording = false, m_inFlight = false, m_hasPipeline = false, m_bound = false;
    bool m_needsBindings = false, m_needsVertices = false;
    UINT64 m_vertices = 0, m_indices = 0;
    RHI::PipelineKind m_kind = RHI::PipelineKind::Graphics;
    RHI::AttachmentLayout m_targets{}, m_expected{};
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_layout;
    std::vector<Microsoft::WRL::ComPtr<IUnknown>> m_natives;
    std::vector<std::shared_ptr<const RHI::DeviceObject>> m_objects;
    std::unordered_map<const void*, Pending> m_states;
};
}
