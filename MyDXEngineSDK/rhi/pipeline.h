#pragma once

#include "types.h"
#include "object.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Skyline::RHI
{
class Shader;
class BindingLayout;
inline constexpr std::size_t MaxColorAttachments = 8;

struct AttachmentLayout
{
    std::array<Format, MaxColorAttachments> colorFormats{};
    std::uint32_t colorCount = 0;
    Format depthFormat = Format::Unknown;
    SampleDesc samples;
};

struct VertexAttribute
{
    std::string semantic;
    std::uint32_t semanticIndex = 0;
    Format format = Format::Unknown;
    std::uint32_t slot = 0;
    std::uint32_t offset = 0;
    InputRate rate = InputRate::Vertex;
    std::uint32_t instanceStepRate = 0;
};

struct BlendDesc
{
    bool enabled = false;
    BlendFactor sourceColor = BlendFactor::One;
    BlendFactor destinationColor = BlendFactor::Zero;
    BlendOp colorOp = BlendOp::Add;
    BlendFactor sourceAlpha = BlendFactor::One;
    BlendFactor destinationAlpha = BlendFactor::Zero;
    BlendOp alphaOp = BlendOp::Add;
    std::uint8_t writeMask = 0x0f;

    static BlendDesc alpha()
    {
        BlendDesc desc;
        desc.enabled = true;
        desc.sourceColor = BlendFactor::SourceAlpha;
        desc.destinationColor = BlendFactor::InverseSourceAlpha;
        desc.destinationAlpha = BlendFactor::InverseSourceAlpha;
        return desc;
    }
};

struct RasterizerDesc
{
    FillMode fill = FillMode::Solid;
    CullMode cull = CullMode::Back;
    bool frontCounterClockwise = false;
    bool depthClip = true;
};

struct DepthDesc
{
    bool test = true;
    bool write = true;
    CompareOp compare = CompareOp::Less;
};

// 工廠建立期間借用 Shader 與綁定配置，後端保留執行所需的原生資源。
struct GraphicsPipelineDesc
{
    std::string debugName = "Graphics pipeline";
    const Shader* vertexShader = nullptr;
    const Shader* pixelShader = nullptr;
    const BindingLayout* bindingLayout = nullptr;
    std::vector<VertexAttribute> vertexInput;
    PrimitiveType primitive = PrimitiveType::Triangle;
    RasterizerDesc rasterizer;
    DepthDesc depth;
    std::array<BlendDesc, MaxColorAttachments> blend;
    bool independentBlend = false;
    bool alphaToCoverage = false;
    std::uint32_t sampleMask = 0xffffffffu;
    AttachmentLayout attachments;

    static GraphicsPipelineDesc fullscreen()
    {
        GraphicsPipelineDesc desc;
        desc.rasterizer.cull = CullMode::None;
        desc.depth.test = false;
        desc.depth.write = false;
        return desc;
    }
};

struct ComputePipelineDesc
{
    std::string debugName = "Compute pipeline";
    const Shader* computeShader = nullptr;
    const BindingLayout* bindingLayout = nullptr;
};

enum class PipelineKind { Graphics, Compute };

class SKYLINE_API Pipeline : public DeviceObject
{
public:
    ~Pipeline() override;
    [[nodiscard]] virtual PipelineKind kind() const noexcept = 0;

protected:
    explicit Pipeline(const Device& device) noexcept : DeviceObject(device) {}
};
}
