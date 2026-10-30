#pragma once

#include "object.h"
#include <cstdint>
#include <string>
#include <vector>

namespace Skyline::RHI
{
enum class ShaderVisibility { All, Vertex, Pixel, Compute };
enum class BindingType { ConstantBuffer, SampledResource, StorageResource };
enum class SamplerFilter { Point, Linear };
enum class AddressMode { Wrap, Clamp, Mirror };

struct ConstantBufferSlot
{
    std::uint32_t shaderRegister = 0;
    std::uint32_t registerSpace = 0;
    ShaderVisibility visibility = ShaderVisibility::All;
};

struct ResourceRange
{
    BindingType type = BindingType::SampledResource;
    std::uint32_t shaderRegister = 0;
    std::uint32_t count = 1;
    std::uint32_t registerSpace = 0;
};

struct StaticSamplerDesc
{
    std::uint32_t shaderRegister = 0;
    std::uint32_t registerSpace = 0;
    ShaderVisibility visibility = ShaderVisibility::Pixel;
    SamplerFilter filter = SamplerFilter::Linear;
    AddressMode addressU = AddressMode::Wrap;
    AddressMode addressV = AddressMode::Wrap;
    AddressMode addressW = AddressMode::Wrap;
};

struct BindingLayoutDesc
{
    std::string debugName = "Binding layout";
    std::vector<ConstantBufferSlot> constantBuffers;
    std::vector<ResourceRange> resources;
    std::vector<StaticSamplerDesc> samplers;
    bool allowVertexInput = true;

    // 過渡期間維持既有場景的 b0～b3 與資源表排列，供 RenderState 使用。
    static BindingLayoutDesc scene()
    {
        BindingLayoutDesc desc;
        desc.debugName = "Scene binding layout";
        desc.constantBuffers = {{0}, {1}, {2}, {3}};
        desc.resources = {
            {BindingType::ConstantBuffer, 4, 4},
            {BindingType::SampledResource, 0, 32},
            {BindingType::StorageResource, 0, 8}
        };
        desc.samplers = {{}};
        return desc;
    }
};

class SKYLINE_API BindingLayout : public DeviceObject
{
public:
    ~BindingLayout() override;
    [[nodiscard]] virtual const BindingLayoutDesc& description() const noexcept = 0;

protected:
    explicit BindingLayout(const Device& device) noexcept : DeviceObject(device) {}
};
}
