#pragma once

#include "rhi/device.h"
#include <array>

namespace Skyline
{
// 目前引擎每影格等待 GPU 完成；保留兩份資源以符合既有交換鏈索引。
struct RhiPassResources
{
    static constexpr std::size_t frameCount = 2;
    std::array<std::shared_ptr<RHI::Buffer>, frameCount> constants;
    std::array<std::unique_ptr<RHI::BindingSet>, frameCount> bindings;

    void init(RHI::Device& device, const RHI::BindingLayout& layout, std::uint64_t size)
    {
        for (std::size_t frame = 0; frame < frameCount; ++frame)
        {
            constants[frame] = device.createBuffer({size, RHI::BufferUsage::Constant,
                RHI::MemoryType::Upload, RHI::ResourceState::GenericRead});
            auto view = device.createBufferView(constants[frame], {RHI::BindingType::ConstantBuffer, 0, size});
            bindings[frame] = device.createBindingSet({&layout, {{0, 0, std::move(view)}}});
        }
    }
};
}
