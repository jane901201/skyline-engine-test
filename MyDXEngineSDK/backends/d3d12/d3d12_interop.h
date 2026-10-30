#pragma once

#include "rhi/device.h"
#include "rhi/frame.h"
#include <directx/d3d12.h>

namespace Skyline::D3D12
{
SKYLINE_API ID3D12Device* nativeFrameDevice(const RHI::Frame& frame);
SKYLINE_API ID3D12CommandQueue* nativeFrameQueue(const RHI::Frame& frame);
SKYLINE_API ID3D12GraphicsCommandList10* nativeFrameCommands(const RHI::Frame& frame);
SKYLINE_API ID3D12PipelineState* nativePipeline(const RHI::Pipeline& pipeline);
SKYLINE_API ID3D12RootSignature* nativePipelineLayout(const RHI::Pipeline& pipeline);
SKYLINE_API ID3D12RootSignature* nativeBindingLayout(const RHI::BindingLayout& layout);
SKYLINE_API ID3D12Resource* nativeBuffer(const RHI::Buffer& buffer);
SKYLINE_API void applyBindingSet(ID3D12GraphicsCommandList& commands, const RHI::BindingSet& set, const RHI::Pipeline& pipeline);
}
