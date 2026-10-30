#pragma once

#include "rhi/device.h"
#include "rhi/command_context.h"
#include <utility>

namespace Skyline::Examples
{
inline std::unique_ptr<RHI::Pipeline> graphics(RHI::Device& device, const RHI::BindingLayout& layout,
    RHI::AttachmentLayout attachments, const char* path, std::vector<RHI::VertexAttribute> input = {},
    bool alpha = false, bool depth = false)
{
    auto vs = device.createShader({path, "VSMain", RHI::ShaderStage::Vertex});
    auto ps = device.createShader({path, "PSMain", RHI::ShaderStage::Pixel});
    auto desc = RHI::GraphicsPipelineDesc::fullscreen();
    desc.debugName = path; desc.vertexShader = vs.get(); desc.pixelShader = ps.get(); desc.bindingLayout = &layout;
    desc.attachments = attachments; desc.vertexInput = std::move(input); desc.depth.test = depth; desc.depth.write = depth;
    if (alpha)
    {
        desc.blend[0] = RHI::BlendDesc::alpha();
        desc.blend[0].destinationAlpha = RHI::BlendFactor::Zero;
    }
    return device.createGraphicsPipeline(desc);
}
inline std::shared_ptr<RHI::Buffer> constants(RHI::Device& device, std::span<const std::byte> data)
{
    auto buffer = device.createBuffer({data.size(), RHI::BufferUsage::Constant, RHI::MemoryType::Upload, RHI::ResourceState::GenericRead});
    buffer->write(0, data);
    return buffer;
}
inline std::unique_ptr<RHI::BindingSet> bindConstants(RHI::Device& device, const RHI::BindingLayout& layout,
    std::shared_ptr<RHI::Buffer> buffer, std::uint32_t shaderRegister = 0)
{
    auto view = device.createBufferView(buffer, {RHI::BindingType::ConstantBuffer, 0, buffer->description().size});
    return device.createBindingSet({&layout, {{shaderRegister, 0, std::move(view)}}});
}
inline std::pair<std::shared_ptr<RHI::Buffer>, std::shared_ptr<RHI::Buffer>> geometry(RHI::Device& device,
    std::span<const std::byte> vertices, std::span<const std::byte> indices)
{
    auto vb = device.createBuffer({vertices.size(), RHI::BufferUsage::Vertex | RHI::BufferUsage::CopyDestination});
    auto ib = device.createBuffer({indices.size(), RHI::BufferUsage::Index | RHI::BufferUsage::CopyDestination});
    auto commands = device.createCommandContext();
    commands->begin();
    commands->uploadBuffer(*vb, 0, vertices, RHI::ResourceState::Vertex);
    commands->uploadBuffer(*ib, 0, indices, RHI::ResourceState::Index);
    commands->submitAndWait();
    return {std::move(vb), std::move(ib)};
}
}
