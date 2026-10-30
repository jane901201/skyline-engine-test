#include "d3d12_command_context.h"
#include "d3d12_objects.h"
#include <directx/d3dx12_core.h>
#include <directx/d3dx12_barriers.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace Skyline::D3D12
{
namespace
{
void check(HRESULT result, const char* operation)
{
    if (FAILED(result)) throw std::runtime_error(std::string("CommandContext: ") + operation);
}
void range(UINT64 capacity, UINT64 offset, UINT64 size)
{
    if (offset > capacity || size > capacity - offset)
        throw std::out_of_range("CommandContext: Buffer range exceeds logical size.");
}
}

CommandContext::CommandContext(const RHI::Device& owner, ID3D12Device& device, ID3D12GraphicsCommandList10* external)
    : RHI::CommandContext(owner), m_device(&device), m_external(external != nullptr)
{
    if (external) { m_commands = external; return; }
    D3D12_COMMAND_QUEUE_DESC queue{};
    check(device.CreateCommandQueue(&queue, IID_PPV_ARGS(&m_queue)), "Failed to create queue.");
    check(device.CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_allocator)), "Failed to create allocator.");
    check(device.CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_allocator.Get(), nullptr, IID_PPV_ARGS(&m_commands)), "Failed to create command list.");
    check(m_commands->Close(), "Failed to close initial command list.");
    check(device.CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)), "Failed to create fence.");
    m_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!m_event) throw std::runtime_error("CommandContext: Failed to create fence event.");
}

CommandContext::~CommandContext()
{
    if (m_inFlight) { try { wait(); } catch (...) {} }
    if (m_event) CloseHandle(m_event);
}

void CommandContext::resetState()
{
    m_natives.clear(); m_objects.clear(); m_states.clear(); m_layout.Reset();
    m_hasPipeline = false; m_bound = false; m_vertices = 0; m_indices = 0;
    m_needsBindings = false; m_needsVertices = false; m_targets = {};
    m_recording = true;
}

void CommandContext::begin()
{
    if (m_external) throw std::logic_error("CommandContext: Frame commands are begun by Frame::begin().");
    if (m_recording || m_inFlight) throw std::logic_error("CommandContext: Finish the previous recording/submission first.");
    check(m_allocator->Reset(), "Failed to reset allocator.");
    check(m_commands->Reset(m_allocator.Get(), nullptr), "Failed to reset commands.");
    resetState();
}

void CommandContext::beginExternal(const RHI::AttachmentLayout& attachments)
{
    if (m_recording) throw std::logic_error("CommandContext: A frame is already recording.");
    resetState();
    m_targets = attachments;
}

void CommandContext::completeExternal()
{
    for (const auto& entry : m_states) entry.second.commit();
    m_recording = false;
    m_natives.clear(); m_objects.clear(); m_states.clear();
}

void CommandContext::wait()
{
    if (m_fence->GetCompletedValue() < m_fenceValue)
    {
        check(m_fence->SetEventOnCompletion(m_fenceValue, m_event), "Failed to set fence event.");
        for (;;)
        {
            const auto status = WaitForSingleObject(m_event, 1000);
            if (status == WAIT_OBJECT_0) break;
            if (status != WAIT_TIMEOUT) throw std::runtime_error("CommandContext: GPU wait failed.");
            check(m_device->GetDeviceRemovedReason(), "Device was removed during GPU wait.");
        }
    }
    m_inFlight = false;
}

void CommandContext::submitAndWait()
{
    requireRecording();
    if (m_external) throw std::logic_error("CommandContext: Frame commands are submitted by Frame::end().");
    check(m_commands->Close(), "Failed to close commands.");
    ID3D12CommandList* lists[] = {m_commands.Get()};
    m_queue->ExecuteCommandLists(1, lists);
    m_inFlight = true;
    check(m_queue->Signal(m_fence.Get(), ++m_fenceValue), "Failed to signal fence.");
    wait();
    completeExternal();
}

void CommandContext::requireRecording() const
{
    if (!m_recording) throw std::logic_error("CommandContext: begin() must precede command recording.");
}

void CommandContext::setPipeline(const RHI::Pipeline& value)
{
    requireRecording();
    const auto& pipeline = object<Pipeline>(value);
    m_kind = pipeline.kind(); m_layout = pipeline.layout(); m_hasPipeline = true; m_bound = false;
    m_needsBindings = pipeline.needsBindings; m_needsVertices = pipeline.needsVertices; m_expected = pipeline.attachments;
    m_commands->SetPipelineState(pipeline.native());
    if (m_kind == RHI::PipelineKind::Graphics)
    {
        m_commands->SetGraphicsRootSignature(pipeline.layout());
        setPrimitiveTopology(pipeline.primitive);
    }
    else m_commands->SetComputeRootSignature(pipeline.layout());
    m_natives.emplace_back(pipeline.native()); m_natives.emplace_back(pipeline.layout());
}

void CommandContext::setBindingSet(const RHI::BindingSet& value)
{
    requireRecording();
    if (!m_hasPipeline) throw std::logic_error("CommandContext: Select a pipeline before binding resources.");
    const auto& set = object<BindingSet>(value);
    set.apply(*m_commands.Get(), m_layout.Get(), m_kind);
    set.retain(m_natives, m_objects);
    m_bound = true;
}

template<class Resource> void CommandContext::change(const Resource& resource, RHI::ResourceState after)
{
    const auto before = current(resource);
    auto retained = resource.shared_from_this();
    m_objects.push_back(retained);
    if (before != after)
    {
        const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.native(), resourceState(before), resourceState(after));
        m_commands->ResourceBarrier(1, &barrier);
    }
    m_states[&resource] = {after, [retained, after] { retained->state = after; }};
}

void CommandContext::transition(const RHI::Buffer& value, RHI::ResourceState after)
{
    requireRecording();
    const auto& resource = object<Buffer>(value);
    validateBufferState(resource.description(), after);
    change(resource, after);
}
void CommandContext::transition(const RHI::Texture& value, RHI::ResourceState after)
{
    requireRecording();
    const auto& resource = object<Texture>(value);
    validateTextureState(resource.description(), after);
    change(resource, after);
}

void CommandContext::storageBarrier(const RHI::Buffer& value)
{
    requireRecording(); const auto& resource = object<Buffer>(value);
    if (current(resource) != RHI::ResourceState::Storage) throw std::logic_error("CommandContext: Storage barrier requires Storage state.");
    const auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(resource.native());
    m_commands->ResourceBarrier(1, &barrier); m_objects.push_back(resource.shared_from_this());
}
void CommandContext::storageBarrier(const RHI::Texture& value)
{
    requireRecording(); const auto& resource = object<Texture>(value);
    if (current(resource) != RHI::ResourceState::Storage) throw std::logic_error("CommandContext: Storage barrier requires Storage state.");
    const auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(resource.native());
    m_commands->ResourceBarrier(1, &barrier); m_objects.push_back(resource.shared_from_this());
}

void CommandContext::setVertexBuffer(const RHI::Buffer& value, UINT stride, UINT64 offset)
{
    requireRecording(); const auto& buffer = object<Buffer>(value);
    if (!hasUsage(buffer.description().usage, RHI::BufferUsage::Vertex) || stride == 0 || stride > 2048)
        throw std::invalid_argument("CommandContext: Vertex usage and a valid stride are required.");
    range(buffer.description().size, offset, 0);
    const auto size = buffer.description().size - offset;
    if (size == 0 || size > (std::numeric_limits<UINT>::max)() || size % stride != 0)
        throw std::invalid_argument("CommandContext: Vertex range must contain complete vertices.");
    if (current(buffer) != RHI::ResourceState::Vertex && current(buffer) != RHI::ResourceState::GenericRead)
        throw std::logic_error("CommandContext: Transition the buffer to Vertex state first.");
    const D3D12_VERTEX_BUFFER_VIEW view{buffer.native()->GetGPUVirtualAddress() + offset, static_cast<UINT>(size), stride};
    m_commands->IASetVertexBuffers(0, 1, &view); m_vertices = size / stride;
    m_objects.push_back(buffer.shared_from_this());
}
void CommandContext::setIndexBuffer(const RHI::Buffer& value, RHI::IndexType type, UINT64 offset)
{
    requireRecording(); const auto& buffer = object<Buffer>(value);
    const UINT stride = type == RHI::IndexType::Uint16 ? 2 : 4;
    if ((type != RHI::IndexType::Uint16 && type != RHI::IndexType::Uint32) || !hasUsage(buffer.description().usage, RHI::BufferUsage::Index) || offset % stride != 0)
        throw std::invalid_argument("CommandContext: Index usage, format, and aligned offset are required.");
    range(buffer.description().size, offset, 0);
    const auto size = buffer.description().size - offset;
    if (size == 0 || size > (std::numeric_limits<UINT>::max)() || size % stride != 0)
        throw std::invalid_argument("CommandContext: Index range must contain complete indices.");
    if (current(buffer) != RHI::ResourceState::Index && current(buffer) != RHI::ResourceState::GenericRead)
        throw std::logic_error("CommandContext: Transition the buffer to Index state first.");
    const D3D12_INDEX_BUFFER_VIEW view{buffer.native()->GetGPUVirtualAddress() + offset, static_cast<UINT>(size),
        type == RHI::IndexType::Uint16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT};
    m_commands->IASetIndexBuffer(&view); m_indices = size / stride;
    m_objects.push_back(buffer.shared_from_this());
}
void CommandContext::setPrimitiveTopology(RHI::PrimitiveType primitive)
{
    requireRecording();
    D3D12_PRIMITIVE_TOPOLOGY topology;
    switch (primitive)
    {
    case RHI::PrimitiveType::Point: topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST; break;
    case RHI::PrimitiveType::Line: topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST; break;
    case RHI::PrimitiveType::Triangle: topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
    default: throw std::invalid_argument("CommandContext: Unsupported primitive type.");
    }
    m_commands->IASetPrimitiveTopology(topology);
}
void CommandContext::setViewport(const RHI::Viewport& view, const RHI::Scissor& scissor)
{
    requireRecording();
    if (!std::isfinite(view.x) || !std::isfinite(view.y) || !std::isfinite(view.width) || !std::isfinite(view.height) ||
        !std::isfinite(view.minDepth) || !std::isfinite(view.maxDepth) || view.width <= 0 || view.height <= 0 ||
        view.minDepth < 0 || view.maxDepth > 1 || view.minDepth > view.maxDepth ||
        scissor.left < 0 || scissor.top < 0 || scissor.right < scissor.left || scissor.bottom < scissor.top)
        throw std::invalid_argument("CommandContext: Invalid viewport or scissor.");
    const D3D12_VIEWPORT native{view.x, view.y, view.width, view.height, view.minDepth, view.maxDepth};
    const D3D12_RECT rect{scissor.left, scissor.top, scissor.right, scissor.bottom};
    m_commands->RSSetViewports(1, &native); m_commands->RSSetScissorRects(1, &rect);
}

void CommandContext::setRenderTarget(const RHI::Texture& value, const RHI::Texture* depthValue)
{
    requireRecording(); const auto& color = object<Texture>(value);
    if (current(color) != RHI::ResourceState::RenderTarget) throw std::logic_error("CommandContext: Color attachment requires RenderTarget state.");
    const auto rtv = color.attachment();
    D3D12_CPU_DESCRIPTOR_HANDLE dsv{};
    const Texture* depth = nullptr;
    if (depthValue)
    {
        depth = &object<Texture>(*depthValue);
        if (current(*depth) != RHI::ResourceState::DepthWrite || depth->description().width != color.description().width ||
            depth->description().height != color.description().height)
            throw std::invalid_argument("CommandContext: Depth attachment must match dimensions and DepthWrite state.");
        dsv = depth->attachment(); m_objects.push_back(depth->shared_from_this());
    }
    m_commands->OMSetRenderTargets(1, &rtv, FALSE, depth ? &dsv : nullptr);
    m_targets = {}; m_targets.colorCount = 1; m_targets.colorFormats[0] = color.description().format;
    if (depth) m_targets.depthFormat = depth->description().format;
    m_objects.push_back(color.shared_from_this());
}
void CommandContext::clearColor(const RHI::Texture& value, const std::array<float, 4>& color)
{
    requireRecording(); const auto& texture = object<Texture>(value);
    if (current(texture) != RHI::ResourceState::RenderTarget) throw std::logic_error("CommandContext: Clear requires RenderTarget state.");
    m_commands->ClearRenderTargetView(texture.attachment(), color.data(), 0, nullptr);
    m_objects.push_back(texture.shared_from_this());
}
void CommandContext::clearDepth(const RHI::Texture& value, float depth)
{
    requireRecording(); const auto& texture = object<Texture>(value);
    if (!std::isfinite(depth) || depth < 0 || depth > 1 || current(texture) != RHI::ResourceState::DepthWrite)
        throw std::invalid_argument("CommandContext: Clear requires DepthWrite state and depth in [0,1].");
    m_commands->ClearDepthStencilView(texture.attachment(), D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
    m_objects.push_back(texture.shared_from_this());
}

Microsoft::WRL::ComPtr<ID3D12Resource> CommandContext::staging(UINT64 size, D3D12_HEAP_TYPE type)
{
    const auto heap = CD3DX12_HEAP_PROPERTIES(type);
    const auto desc = CD3DX12_RESOURCE_DESC::Buffer(size);
    Microsoft::WRL::ComPtr<ID3D12Resource> result;
    check(m_device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
        type == D3D12_HEAP_TYPE_UPLOAD ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr, IID_PPV_ARGS(&result)), "Failed to create staging resource.");
    m_natives.emplace_back(result.Get());
    return result;
}
void CommandContext::uploadBuffer(const RHI::Buffer& value, UINT64 offset, std::span<const std::byte> data, RHI::ResourceState after)
{
    requireRecording(); const auto& buffer = object<Buffer>(value);
    range(buffer.description().size, offset, data.size()); validateBufferState(buffer.description(), after);
    if (buffer.description().memory != RHI::MemoryType::Gpu || !hasUsage(buffer.description().usage, RHI::BufferUsage::CopyDestination))
        throw std::invalid_argument("CommandContext: Upload destination requires GPU memory and CopyDestination usage.");
    if (data.empty()) return;
    auto upload = staging(data.size(), D3D12_HEAP_TYPE_UPLOAD);
    void* mapped = nullptr; const D3D12_RANGE noRead{0, 0};
    check(upload->Map(0, &noRead, &mapped), "Failed to map upload.");
    std::memcpy(mapped, data.data(), data.size()); upload->Unmap(0, nullptr);
    transition(value, RHI::ResourceState::CopyDestination);
    m_commands->CopyBufferRegion(buffer.native(), offset, upload.Get(), 0, data.size());
    transition(value, after);
}
void CommandContext::uploadTexture(const RHI::Texture& value, std::span<const std::byte> data, UINT rowPitch, RHI::ResourceState after)
{
    requireRecording(); const auto& texture = object<Texture>(value);
    const auto& desc = texture.description(); const UINT tight = desc.width * pixelSize(desc.format);
    if (rowPitch < tight || data.size() != std::uint64_t(rowPitch) * desc.height || !hasUsage(desc.usage, RHI::TextureUsage::CopyDestination))
        throw std::invalid_argument("CommandContext: Texture upload needs complete rows and CopyDestination usage.");
    validateTextureState(desc, after);
    const auto native = texture.native()->GetDesc(); D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{}; UINT64 bytes = 0;
    m_device->GetCopyableFootprints(&native, 0, 1, 0, &footprint, nullptr, nullptr, &bytes);
    auto upload = staging(bytes, D3D12_HEAP_TYPE_UPLOAD);
    void* mapped = nullptr; const D3D12_RANGE noRead{0, 0};
    check(upload->Map(0, &noRead, &mapped), "Failed to map texture upload.");
    for (UINT y = 0; y < desc.height; ++y)
        std::memcpy(static_cast<std::byte*>(mapped) + footprint.Offset + y * footprint.Footprint.RowPitch, data.data() + std::size_t(y) * rowPitch, tight);
    upload->Unmap(0, nullptr);
    transition(value, RHI::ResourceState::CopyDestination);
    const CD3DX12_TEXTURE_COPY_LOCATION source(upload.Get(), footprint), destination(texture.native(), 0);
    m_commands->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
    transition(value, after);
}
void CommandContext::copyBuffer(const RHI::Buffer& destinationValue, UINT64 destinationOffset, const RHI::Buffer& sourceValue, UINT64 sourceOffset, UINT64 size)
{
    requireRecording(); const auto& destination = object<Buffer>(destinationValue); const auto& source = object<Buffer>(sourceValue);
    range(destination.description().size, destinationOffset, size); range(source.description().size, sourceOffset, size);
    if (&source == &destination) throw std::invalid_argument("CommandContext: In-place buffer copies are not supported.");
    if (!hasUsage(source.description().usage, RHI::BufferUsage::CopySource) || !hasUsage(destination.description().usage, RHI::BufferUsage::CopyDestination) ||
        (current(source) != RHI::ResourceState::CopySource && current(source) != RHI::ResourceState::GenericRead) ||
        current(destination) != RHI::ResourceState::CopyDestination)
        throw std::logic_error("CommandContext: Copy usage and states must match source and destination.");
    m_objects.push_back(source.shared_from_this()); m_objects.push_back(destination.shared_from_this());
    if (size) m_commands->CopyBufferRegion(destination.native(), destinationOffset, source.native(), sourceOffset, size);
}
std::vector<std::byte> CommandContext::readTexture(const RHI::Texture& value)
{
    requireRecording();
    if (m_external) throw std::logic_error("CommandContext: Synchronous texture readback requires a standalone context.");
    const auto& texture = object<Texture>(value); const auto& desc = texture.description();
    if (!hasUsage(desc.usage, RHI::TextureUsage::CopySource)) throw std::invalid_argument("CommandContext: Texture readback requires CopySource usage.");
    const UINT tight = desc.width * pixelSize(desc.format); const auto previous = current(texture);
    const auto native = texture.native()->GetDesc(); D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{}; UINT64 bytes = 0;
    m_device->GetCopyableFootprints(&native, 0, 1, 0, &footprint, nullptr, nullptr, &bytes);
    auto readback = staging(bytes, D3D12_HEAP_TYPE_READBACK);
    transition(value, RHI::ResourceState::CopySource);
    const CD3DX12_TEXTURE_COPY_LOCATION source(texture.native(), 0), destination(readback.Get(), footprint);
    m_commands->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
    transition(value, previous);
    submitAndWait();
    void* mapped = nullptr; const D3D12_RANGE range{0, static_cast<SIZE_T>(bytes)};
    check(readback->Map(0, &range, &mapped), "Failed to map texture readback.");
    std::vector<std::byte> result(std::size_t(tight) * desc.height);
    for (UINT y = 0; y < desc.height; ++y)
        std::memcpy(result.data() + std::size_t(y) * tight, static_cast<std::byte*>(mapped) + footprint.Offset + y * footprint.Footprint.RowPitch, tight);
    const D3D12_RANGE noWrite{0, 0}; readback->Unmap(0, &noWrite);
    return result;
}

void CommandContext::requireGraphics() const
{
    requireRecording();
    if (!m_hasPipeline || m_kind != RHI::PipelineKind::Graphics) throw std::logic_error("CommandContext: Draw requires a graphics pipeline.");
    if (m_needsBindings && !m_bound) throw std::logic_error("CommandContext: Required bindings are missing.");
    if (m_needsVertices && m_vertices == 0) throw std::logic_error("CommandContext: Required vertex buffer is missing.");
    if (m_targets.colorCount != m_expected.colorCount || m_targets.depthFormat != m_expected.depthFormat ||
        m_targets.samples.count != m_expected.samples.count || m_targets.samples.quality != m_expected.samples.quality)
        throw std::logic_error("CommandContext: Attachment layout does not match the pipeline.");
    for (UINT index = 0; index < m_targets.colorCount; ++index)
        if (m_targets.colorFormats[index] != m_expected.colorFormats[index])
            throw std::logic_error("CommandContext: Color attachment format does not match the pipeline.");
}
void CommandContext::draw(UINT count, UINT instances, UINT first)
{
    requireGraphics();
    if (m_needsVertices && (std::uint64_t(first) + count > m_vertices)) throw std::out_of_range("CommandContext: Draw exceeds vertex range.");
#ifdef SKYLINE_ENABLE_TRACY
    if (m_profiler)
    {
        TracyD3D12ZoneS(m_profiler, m_commands.Get(), "RHI draw", 4);
        m_commands->DrawInstanced(count, instances, first, 0);
        return;
    }
#endif
    m_commands->DrawInstanced(count, instances, first, 0);
}
void CommandContext::drawIndexed(UINT count, UINT instances, UINT first, INT offset)
{
    requireGraphics();
    if (m_indices == 0 || std::uint64_t(first) + count > m_indices) throw std::out_of_range("CommandContext: Draw exceeds index range.");
#ifdef SKYLINE_ENABLE_TRACY
    if (m_profiler)
    {
        TracyD3D12ZoneS(m_profiler, m_commands.Get(), "RHI indexed draw", 4);
        m_commands->DrawIndexedInstanced(count, instances, first, offset, 0);
        return;
    }
#endif
    m_commands->DrawIndexedInstanced(count, instances, first, offset, 0);
}
void CommandContext::dispatch(UINT x, UINT y, UINT z)
{
    requireRecording();
    if (!m_hasPipeline || m_kind != RHI::PipelineKind::Compute || (m_needsBindings && !m_bound))
        throw std::logic_error("CommandContext: Dispatch requires a compute pipeline and its bindings.");
    if (x > 65535 || y > 65535 || z > 65535) throw std::invalid_argument("CommandContext: Dispatch group count exceeds device limits.");
#ifdef SKYLINE_ENABLE_TRACY
    if (m_profiler)
    {
        TracyD3D12ZoneS(m_profiler, m_commands.Get(), "RHI dispatch", 4);
        m_commands->Dispatch(x, y, z);
        return;
    }
#endif
    m_commands->Dispatch(x, y, z);
}
}
