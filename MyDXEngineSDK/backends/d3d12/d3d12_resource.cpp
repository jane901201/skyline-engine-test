#include "d3d12_resource.h"
#include "d3d12_objects.h"
#include "d3d12_pipeline.h"
#include <directx/d3dx12_core.h>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace Skyline::D3D12
{
namespace
{
void requireRange(std::uint64_t capacity, std::uint64_t offset, std::uint64_t size)
{
    if (offset > capacity || size > capacity - offset)
        throw std::out_of_range("Buffer: Range exceeds its logical size.");
}
}

D3D12_RESOURCE_STATES resourceState(RHI::ResourceState state)
{
    switch (state)
    {
    case RHI::ResourceState::Common: return D3D12_RESOURCE_STATE_COMMON;
    case RHI::ResourceState::ShaderRead: return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    case RHI::ResourceState::Storage: return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    case RHI::ResourceState::CopySource: return D3D12_RESOURCE_STATE_COPY_SOURCE;
    case RHI::ResourceState::CopyDestination: return D3D12_RESOURCE_STATE_COPY_DEST;
    case RHI::ResourceState::GenericRead: return D3D12_RESOURCE_STATE_GENERIC_READ;
    case RHI::ResourceState::Vertex: return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    case RHI::ResourceState::Index: return D3D12_RESOURCE_STATE_INDEX_BUFFER;
    case RHI::ResourceState::RenderTarget: return D3D12_RESOURCE_STATE_RENDER_TARGET;
    case RHI::ResourceState::DepthWrite: return D3D12_RESOURCE_STATE_DEPTH_WRITE;
    }
    throw std::invalid_argument("Buffer: Unsupported resource state.");
}

void validateBufferState(const RHI::BufferDesc& desc, RHI::ResourceState state)
{
    resourceState(state);
    using namespace RHI;
    if ((desc.memory == MemoryType::Upload && state != ResourceState::GenericRead) ||
        (desc.memory == MemoryType::Readback && state != ResourceState::CopyDestination))
        throw std::invalid_argument(desc.debugName + ": CPU-visible memory has a fixed resource state.");
    const auto requireUsage = [&desc](BufferUsage usage) {
        if (!hasUsage(desc.usage, usage))
            throw std::invalid_argument(desc.debugName + ": State requires a matching buffer usage.");
    };
    switch (state)
    {
    case ResourceState::Storage: requireUsage(BufferUsage::Storage); break;
    case ResourceState::ShaderRead: requireUsage(BufferUsage::Sampled); break;
    case ResourceState::CopySource: requireUsage(BufferUsage::CopySource); break;
    case ResourceState::CopyDestination: requireUsage(BufferUsage::CopyDestination); break;
    case ResourceState::Vertex: requireUsage(BufferUsage::Vertex); break;
    case ResourceState::Index: requireUsage(BufferUsage::Index); break;
    case ResourceState::RenderTarget:
    case ResourceState::DepthWrite: throw std::invalid_argument(desc.debugName + ": Texture-only state used for a buffer.");
    default: break;
    }
}

Buffer::Buffer(const RHI::Device& owner, ID3D12Device& device, RHI::BufferDesc desc)
    : RHI::Buffer(owner), state(desc.initialState), m_desc(std::move(desc))
{
    using namespace RHI;
    if (m_desc.size == 0 || m_desc.size > (std::numeric_limits<SIZE_T>::max)() - 255 ||
        m_desc.usage == BufferUsage::None || (static_cast<std::uint32_t>(m_desc.usage) & ~127u) != 0)
        throw std::invalid_argument(m_desc.debugName + ": A finite non-zero size and supported usage are required.");
    D3D12_HEAP_TYPE memory;
    switch (m_desc.memory)
    {
    case MemoryType::Gpu: memory = D3D12_HEAP_TYPE_DEFAULT; break;
    case MemoryType::Upload: memory = D3D12_HEAP_TYPE_UPLOAD; break;
    case MemoryType::Readback: memory = D3D12_HEAP_TYPE_READBACK; break;
    default: throw std::invalid_argument(m_desc.debugName + ": Unsupported memory type.");
    }
    if (hasUsage(m_desc.usage, BufferUsage::Storage) && m_desc.memory != MemoryType::Gpu)
        throw std::invalid_argument(m_desc.debugName + ": Storage buffers require GPU memory.");
    if (m_desc.memory == MemoryType::Readback && m_desc.usage != BufferUsage::CopyDestination)
        throw std::invalid_argument(m_desc.debugName + ": Readback buffers support copy destinations only.");
    validateBufferState(m_desc, m_desc.initialState);
    const auto size = hasUsage(m_desc.usage, BufferUsage::Constant) ? (m_desc.size + 255) & ~std::uint64_t(255) : m_desc.size;
    const auto flags = hasUsage(m_desc.usage, BufferUsage::Storage) ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
    const auto native = CD3DX12_RESOURCE_DESC::Buffer(size, flags);
    const auto heap = CD3DX12_HEAP_PROPERTIES(memory);
    if (FAILED(device.CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &native,
        resourceState(m_desc.initialState), nullptr, IID_PPV_ARGS(&m_resource))))
        throw std::runtime_error(m_desc.debugName + ": Failed to create buffer.");
}

void Buffer::write(std::uint64_t offset, std::span<const std::byte> data)
{
    if (m_desc.memory != RHI::MemoryType::Upload)
        throw std::logic_error(m_desc.debugName + ": CPU writes require upload memory.");
    requireRange(m_desc.size, offset, data.size());
    if (data.empty()) return;
    void* mapped = nullptr;
    const D3D12_RANGE noRead{0, 0};
    if (FAILED(m_resource->Map(0, &noRead, &mapped)))
        throw std::runtime_error(m_desc.debugName + ": Failed to map upload buffer.");
    std::memcpy(static_cast<std::byte*>(mapped) + offset, data.data(), data.size());
    const D3D12_RANGE written{static_cast<SIZE_T>(offset), static_cast<SIZE_T>(offset + data.size())};
    m_resource->Unmap(0, &written);
}

void Buffer::read(std::uint64_t offset, std::span<std::byte> data) const
{
    if (m_desc.memory != RHI::MemoryType::Readback)
        throw std::logic_error(m_desc.debugName + ": CPU reads require readback memory.");
    requireRange(m_desc.size, offset, data.size());
    if (data.empty()) return;
    void* mapped = nullptr;
    const D3D12_RANGE range{static_cast<SIZE_T>(offset), static_cast<SIZE_T>(offset + data.size())};
    if (FAILED(m_resource->Map(0, &range, &mapped)))
        throw std::runtime_error(m_desc.debugName + ": Failed to map readback buffer.");
    std::memcpy(data.data(), static_cast<std::byte*>(mapped) + offset, data.size());
    const D3D12_RANGE noWrite{0, 0};
    m_resource->Unmap(0, &noWrite);
}

BufferView::BufferView(const RHI::Device& owner, std::shared_ptr<Buffer> buffer, RHI::BufferViewDesc desc)
    : RHI::BufferView(owner), m_buffer(std::move(buffer)), m_desc(desc)
{
    using namespace RHI;
    requireRange(m_buffer->description().size, desc.offset, desc.size);
    if (desc.size == 0)
        throw std::invalid_argument("Buffer view: Size must be non-zero.");
    BufferUsage usage;
    switch (desc.type)
    {
    case BindingType::ConstantBuffer:
        usage = BufferUsage::Constant;
        if (desc.stride != 0 || desc.offset % 256 != 0 || desc.size > 65536)
            throw std::invalid_argument("Buffer view: Constant views require 256-byte aligned offsets and at most 64 KiB.");
        // CBV 大小向上對齊；不可使子範圍的補齊跨越實際配置。
        if (((desc.size + 255) & ~std::uint64_t(255)) > m_buffer->native()->GetDesc().Width - desc.offset)
            throw std::invalid_argument("Buffer view: Aligned constant range exceeds the allocation.");
        break;
    case BindingType::SampledResource: usage = BufferUsage::Sampled; break;
    case BindingType::StorageResource: usage = BufferUsage::Storage; break;
    default: throw std::invalid_argument("Buffer view: Unsupported binding type.");
    }
    if (!hasUsage(m_buffer->description().usage, usage))
        throw std::invalid_argument("Buffer view: Resource usage does not permit this view.");
    if (desc.type != BindingType::ConstantBuffer &&
        (desc.stride == 0 || desc.stride > 2048 || desc.stride % 4 != 0 ||
         desc.offset % desc.stride != 0 || desc.size % desc.stride != 0 ||
         desc.size / desc.stride > (std::numeric_limits<UINT>::max)()))
        throw std::invalid_argument("Buffer view: Structured ranges require a valid stride and whole elements.");
}

void BufferView::writeDescriptor(ID3D12Device& device, D3D12_CPU_DESCRIPTOR_HANDLE handle) const
{
    switch (m_desc.type)
    {
    case RHI::BindingType::ConstantBuffer:
    {
        const D3D12_CONSTANT_BUFFER_VIEW_DESC desc{address(), static_cast<UINT>((m_desc.size + 255) & ~std::uint64_t(255))};
        device.CreateConstantBufferView(&desc, handle);
        break;
    }
    case RHI::BindingType::SampledResource:
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
        desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        desc.Buffer.FirstElement = m_desc.offset / m_desc.stride;
        desc.Buffer.NumElements = static_cast<UINT>(m_desc.size / m_desc.stride);
        desc.Buffer.StructureByteStride = m_desc.stride;
        device.CreateShaderResourceView(resource(), &desc, handle);
        break;
    }
    case RHI::BindingType::StorageResource:
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC desc{};
        desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        desc.Buffer.FirstElement = m_desc.offset / m_desc.stride;
        desc.Buffer.NumElements = static_cast<UINT>(m_desc.size / m_desc.stride);
        desc.Buffer.StructureByteStride = m_desc.stride;
        device.CreateUnorderedAccessView(resource(), nullptr, &desc, handle);
        break;
    }
    }
}

BindingSet::BindingSet(const RHI::Device& owner, ID3D12Device& device, const RHI::BindingSetDesc& desc)
    : RHI::BindingSet(owner)
{
    const auto* layout = dynamic_cast<const BindingLayout*>(desc.layout);
    if (layout == nullptr || &layout->device() != &owner)
        throw std::invalid_argument(desc.debugName + ": A layout from this device is required.");
    const auto& definition = layout->description();
    m_layout = layout->native();
    struct Entry { UINT shaderRegister, space; };
    std::vector<Entry> entries;
    const auto add = [&](const auto& binding) {
        auto view = std::static_pointer_cast<RHI::DeviceObject>(binding.view);
        if (!view || &view->device() != &owner || dynamic_cast<const DescriptorView*>(view.get()) == nullptr)
            throw std::invalid_argument(desc.debugName + ": Views must belong to this backend and device.");
        m_views.push_back(std::move(view));
        entries.push_back({binding.shaderRegister, binding.registerSpace});
    };
    for (const auto& binding : desc.buffers) add(binding);
    for (const auto& binding : desc.textures) add(binding);
    std::vector<bool> used(entries.size());
    const auto find = [&](RHI::BindingType type, UINT shaderRegister, UINT space) -> const DescriptorView&
    {
        const DescriptorView* found = nullptr;
        for (std::size_t index = 0; index < entries.size(); ++index)
            if (entries[index].shaderRegister == shaderRegister && entries[index].space == space &&
                dynamic_cast<const DescriptorView&>(*m_views[index]).type() == type)
            {
                if (found) throw std::invalid_argument(desc.debugName + ": Duplicate binding.");
                found = &dynamic_cast<const DescriptorView&>(*m_views[index]);
                used[index] = true;
            }
        if (!found) throw std::invalid_argument(desc.debugName + ": A required binding is missing.");
        return *found;
    };
    for (const auto& slot : definition.constantBuffers)
        m_constants.push_back(dynamic_cast<const BufferView&>(find(RHI::BindingType::ConstantBuffer, slot.shaderRegister, slot.registerSpace)).address());
    UINT count = 0;
    for (const auto& range : definition.resources) count += range.count;
    if (count != 0)
    {
        const D3D12_DESCRIPTOR_HEAP_DESC heapDesc{D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, count,
            D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 0};
        if (FAILED(device.CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_heap))))
            throw std::runtime_error(desc.debugName + ": Failed to create binding heap.");
        auto handle = m_heap->GetCPUDescriptorHandleForHeapStart();
        const auto increment = device.GetDescriptorHandleIncrementSize(heapDesc.Type);
        for (const auto& range : definition.resources)
            for (UINT index = 0; index < range.count; ++index)
            {
                find(range.type, range.shaderRegister + index, range.registerSpace).writeDescriptor(device, handle);
                handle.ptr += increment;
            }
    }
    for (bool entry : used)
        if (!entry) throw std::invalid_argument(desc.debugName + ": A binding is outside the layout.");
}

void BindingSet::apply(ID3D12GraphicsCommandList& commands, const Pipeline& pipeline) const
{
    if (&pipeline.device() != &device() || pipeline.layout() != m_layout.Get())
        throw std::invalid_argument("Binding set: Pipeline and binding layout must match.");
    apply(commands, pipeline.layout(), pipeline.kind());
}

void BindingSet::retain(std::vector<Microsoft::WRL::ComPtr<IUnknown>>& natives,
                        std::vector<std::shared_ptr<const RHI::DeviceObject>>& objects) const
{
    natives.emplace_back(m_layout.Get());
    if (m_heap) natives.emplace_back(m_heap.Get());
    for (const auto& view : m_views) objects.push_back(view);
}

void BindingSet::apply(ID3D12GraphicsCommandList& commands, ID3D12RootSignature* layout, RHI::PipelineKind kind) const
{
    if (layout != m_layout.Get())
        throw std::invalid_argument("Binding set: Pipeline and binding layout must match.");
    const bool compute = kind == RHI::PipelineKind::Compute;
    // 綁定組完整設定配置與所有插槽；建立後不修改 Heap，避免改寫使用中的 Descriptor。
    if (compute) commands.SetComputeRootSignature(m_layout.Get());
    else commands.SetGraphicsRootSignature(m_layout.Get());
    for (UINT index = 0; index < m_constants.size(); ++index)
    {
        if (compute) commands.SetComputeRootConstantBufferView(index, m_constants[index]);
        else commands.SetGraphicsRootConstantBufferView(index, m_constants[index]);
    }
    if (m_heap)
    {
        ID3D12DescriptorHeap* heaps[] = {m_heap.Get()};
        commands.SetDescriptorHeaps(1, heaps);
        const auto index = static_cast<UINT>(m_constants.size());
        if (compute) commands.SetComputeRootDescriptorTable(index, m_heap->GetGPUDescriptorHandleForHeapStart());
        else commands.SetGraphicsRootDescriptorTable(index, m_heap->GetGPUDescriptorHandleForHeapStart());
    }
}
}
