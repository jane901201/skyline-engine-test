#include "d3d12_interop.h"
#include "d3d12_objects.h"
#include "d3d12_resource.h"
#include "d3d12_frame.h"
#include <directx/d3dx12_barriers.h>
#include "core/render_context.h"

#include <stdexcept>

namespace Skyline::D3D12
{
namespace
{
template<class Native, class Object>
const Native& backendObject(const Object& object)
{
    const auto* native = dynamic_cast<const Native*>(&object);
    if (native == nullptr)
        throw std::invalid_argument("D3D12 interop: Incompatible RHI backend object.");
    return *native;
}
}

ID3D12PipelineState* nativePipeline(const RHI::Pipeline& pipeline) { return backendObject<Pipeline>(pipeline).native(); }
ID3D12CommandQueue* nativeFrameQueue(const RHI::Frame& frame) { return backendObject<FrameBackend>(frame).getCommandQueue(); }
ID3D12GraphicsCommandList10* nativeFrameCommands(const RHI::Frame& frame) { return backendObject<FrameBackend>(frame).getCommandList(); }
ID3D12RootSignature* nativePipelineLayout(const RHI::Pipeline& pipeline) { return backendObject<Pipeline>(pipeline).layout(); }
ID3D12RootSignature* nativeBindingLayout(const RHI::BindingLayout& layout) { return backendObject<BindingLayout>(layout).native(); }
ID3D12Resource* nativeBuffer(const RHI::Buffer& buffer) { return backendObject<Buffer>(buffer).native(); }
void applyBindingSet(ID3D12GraphicsCommandList& commands, const RHI::BindingSet& set, const RHI::Pipeline& pipeline)
{
    backendObject<BindingSet>(set).apply(commands, backendObject<Pipeline>(pipeline));
}
}

void RenderContext::setGraphicsPipeline(const Skyline::RHI::Pipeline& pipeline)
{
    if (pipeline.kind() != Skyline::RHI::PipelineKind::Graphics)
        throw std::invalid_argument("RenderContext: A graphics pipeline is required.");
    m_commandList->SetGraphicsRootSignature(Skyline::D3D12::nativePipelineLayout(pipeline));
    m_commandList->SetPipelineState(Skyline::D3D12::nativePipeline(pipeline));
}

void RenderContext::setComputePipeline(const Skyline::RHI::Pipeline& pipeline)
{
    if (pipeline.kind() != Skyline::RHI::PipelineKind::Compute)
        throw std::invalid_argument("RenderContext: A compute pipeline is required.");
    m_commandList->SetComputeRootSignature(Skyline::D3D12::nativePipelineLayout(pipeline));
    m_commandList->SetPipelineState(Skyline::D3D12::nativePipeline(pipeline));
}

void RenderContext::setRootSignature(const Skyline::RHI::BindingLayout& layout)
{
    m_commandList->SetGraphicsRootSignature(Skyline::D3D12::nativeBindingLayout(layout));
}

void RenderContext::setBindingSet(const Skyline::RHI::BindingSet& set, const Skyline::RHI::Pipeline& pipeline)
{
    Skyline::D3D12::applyBindingSet(*m_commandList, set, pipeline);
}

void RenderContext::transitionBuffer(const Skyline::RHI::Buffer& buffer,
    Skyline::RHI::ResourceState before, Skyline::RHI::ResourceState after)
{
    Skyline::D3D12::validateBufferState(buffer.description(), before);
    Skyline::D3D12::validateBufferState(buffer.description(), after);
    if (before == after) return;
    const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(Skyline::D3D12::nativeBuffer(buffer),
        Skyline::D3D12::resourceState(before), Skyline::D3D12::resourceState(after));
    m_commandList->ResourceBarrier(1, &barrier);
}

void RenderContext::storageBarrier(const Skyline::RHI::Buffer& buffer)
{
    Skyline::D3D12::validateBufferState(buffer.description(), Skyline::RHI::ResourceState::Storage);
    const auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(Skyline::D3D12::nativeBuffer(buffer));
    m_commandList->ResourceBarrier(1, &barrier);
}
