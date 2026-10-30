#include "d3d12_pipeline.h"

#include <directx/d3dx12_core.h>
#include <stdexcept>

namespace Skyline::D3D12
{
DXGI_FORMAT toNativeFormat(RHI::Format format)
{
    switch (format)
    {
    case RHI::Format::Unknown: return DXGI_FORMAT_UNKNOWN;
    case RHI::Format::Rgba8Unorm: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case RHI::Format::Bgra8Unorm: return DXGI_FORMAT_B8G8R8A8_UNORM;
    case RHI::Format::Rgba16Float: return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case RHI::Format::R32Float: return DXGI_FORMAT_R32_FLOAT;
    case RHI::Format::Rg32Float: return DXGI_FORMAT_R32G32_FLOAT;
    case RHI::Format::Rgb32Float: return DXGI_FORMAT_R32G32B32_FLOAT;
    case RHI::Format::Rgba32Float: return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case RHI::Format::D32Float: return DXGI_FORMAT_D32_FLOAT;
    case RHI::Format::D24UnormS8Uint: return DXGI_FORMAT_D24_UNORM_S8_UINT;
    }
    throw std::invalid_argument("D3D12 pipeline: Unsupported RHI format.");
}

RHI::Format fromNativeFormat(DXGI_FORMAT format)
{
    switch (format)
    {
    case DXGI_FORMAT_UNKNOWN: return RHI::Format::Unknown;
    case DXGI_FORMAT_R8G8B8A8_UNORM: return RHI::Format::Rgba8Unorm;
    case DXGI_FORMAT_B8G8R8A8_UNORM: return RHI::Format::Bgra8Unorm;
    case DXGI_FORMAT_R16G16B16A16_FLOAT: return RHI::Format::Rgba16Float;
    case DXGI_FORMAT_R32_FLOAT: return RHI::Format::R32Float;
    case DXGI_FORMAT_R32G32_FLOAT: return RHI::Format::Rg32Float;
    case DXGI_FORMAT_R32G32B32_FLOAT: return RHI::Format::Rgb32Float;
    case DXGI_FORMAT_R32G32B32A32_FLOAT: return RHI::Format::Rgba32Float;
    case DXGI_FORMAT_D32_FLOAT: return RHI::Format::D32Float;
    case DXGI_FORMAT_D24_UNORM_S8_UINT: return RHI::Format::D24UnormS8Uint;
    default: throw std::invalid_argument("D3D12 pipeline: Unsupported native format.");
    }
}

namespace
{
bool isDepth(RHI::Format format)
{
    return format == RHI::Format::D32Float || format == RHI::Format::D24UnormS8Uint;
}

D3D12_BLEND blendFactor(RHI::BlendFactor factor)
{
    switch (factor)
    {
    case RHI::BlendFactor::Zero: return D3D12_BLEND_ZERO;
    case RHI::BlendFactor::One: return D3D12_BLEND_ONE;
    case RHI::BlendFactor::SourceAlpha: return D3D12_BLEND_SRC_ALPHA;
    case RHI::BlendFactor::InverseSourceAlpha: return D3D12_BLEND_INV_SRC_ALPHA;
    case RHI::BlendFactor::DestinationAlpha: return D3D12_BLEND_DEST_ALPHA;
    case RHI::BlendFactor::InverseDestinationAlpha: return D3D12_BLEND_INV_DEST_ALPHA;
    }
    throw std::invalid_argument("D3D12 pipeline: Unsupported blend factor.");
}

D3D12_BLEND_OP blendOp(RHI::BlendOp op)
{
    switch (op)
    {
    case RHI::BlendOp::Add: return D3D12_BLEND_OP_ADD;
    case RHI::BlendOp::Subtract: return D3D12_BLEND_OP_SUBTRACT;
    case RHI::BlendOp::ReverseSubtract: return D3D12_BLEND_OP_REV_SUBTRACT;
    case RHI::BlendOp::Minimum: return D3D12_BLEND_OP_MIN;
    case RHI::BlendOp::Maximum: return D3D12_BLEND_OP_MAX;
    }
    throw std::invalid_argument("D3D12 pipeline: Unsupported blend operation.");
}

D3D12_COMPARISON_FUNC compareOp(RHI::CompareOp op)
{
    switch (op)
    {
    case RHI::CompareOp::Never: return D3D12_COMPARISON_FUNC_NEVER;
    case RHI::CompareOp::Less: return D3D12_COMPARISON_FUNC_LESS;
    case RHI::CompareOp::Equal: return D3D12_COMPARISON_FUNC_EQUAL;
    case RHI::CompareOp::LessEqual: return D3D12_COMPARISON_FUNC_LESS_EQUAL;
    case RHI::CompareOp::Greater: return D3D12_COMPARISON_FUNC_GREATER;
    case RHI::CompareOp::NotEqual: return D3D12_COMPARISON_FUNC_NOT_EQUAL;
    case RHI::CompareOp::GreaterEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    case RHI::CompareOp::Always: return D3D12_COMPARISON_FUNC_ALWAYS;
    }
    throw std::invalid_argument("D3D12 pipeline: Unsupported depth comparison.");
}
}

GraphicsPipelineDescription::GraphicsPipelineDescription(const RHI::GraphicsPipelineDesc& desc)
{
    const auto fail = [&desc](const char* message) {
        throw std::invalid_argument(desc.debugName + ": " + message);
    };
    if (desc.attachments.colorCount > RHI::MaxColorAttachments)
        fail("Too many color attachments.");
    if (desc.attachments.colorCount == 0 && desc.attachments.depthFormat == RHI::Format::Unknown)
        fail("At least one rendering attachment is required.");
    if (desc.attachments.samples.count == 0)
        fail("Sample count must be greater than zero.");
    if (desc.attachments.samples.count == 1 && desc.attachments.samples.quality != 0)
        fail("Single-sample attachments require quality zero.");
    if (desc.attachments.depthFormat != RHI::Format::Unknown && !isDepth(desc.attachments.depthFormat))
        fail("Depth attachment requires a depth format.");
    if ((desc.depth.test || desc.depth.write) && desc.attachments.depthFormat == RHI::Format::Unknown)
        fail("Depth testing or writing requires a depth attachment.");
    if (desc.depth.write && !desc.depth.test)
        fail("Depth writing requires enabled depth testing.");
    if (desc.vertexInput.size() > D3D12_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT)
        fail("Too many vertex attributes.");

    m_description.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    switch (desc.rasterizer.fill)
    {
    case RHI::FillMode::Solid: m_description.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; break;
    case RHI::FillMode::Wireframe: m_description.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME; break;
    default: fail("Unsupported fill mode.");
    }
    switch (desc.rasterizer.cull)
    {
    case RHI::CullMode::None: m_description.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; break;
    case RHI::CullMode::Front: m_description.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT; break;
    case RHI::CullMode::Back: m_description.RasterizerState.CullMode = D3D12_CULL_MODE_BACK; break;
    default: fail("Unsupported cull mode.");
    }
    m_description.RasterizerState.FrontCounterClockwise = desc.rasterizer.frontCounterClockwise;
    m_description.RasterizerState.DepthClipEnable = desc.rasterizer.depthClip;
    m_description.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    m_description.BlendState.IndependentBlendEnable = desc.independentBlend;
    m_description.BlendState.AlphaToCoverageEnable = desc.alphaToCoverage;
    for (std::size_t index = 0; index < RHI::MaxColorAttachments; ++index)
    {
        const auto& blend = desc.blend[index];
        if ((blend.writeMask & 0xf0) != 0)
            fail("Color write mask contains unsupported bits.");
        auto& native = m_description.BlendState.RenderTarget[index];
        native.BlendEnable = blend.enabled;
        native.SrcBlend = blendFactor(blend.sourceColor);
        native.DestBlend = blendFactor(blend.destinationColor);
        native.BlendOp = blendOp(blend.colorOp);
        native.SrcBlendAlpha = blendFactor(blend.sourceAlpha);
        native.DestBlendAlpha = blendFactor(blend.destinationAlpha);
        native.BlendOpAlpha = blendOp(blend.alphaOp);
        native.RenderTargetWriteMask = blend.writeMask;
    }
    m_description.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    m_description.DepthStencilState.DepthEnable = desc.depth.test;
    m_description.DepthStencilState.DepthWriteMask = desc.depth.write ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    m_description.DepthStencilState.DepthFunc = compareOp(desc.depth.compare);
    m_description.DepthStencilState.StencilEnable = FALSE;
    m_description.SampleMask = desc.sampleMask;
    switch (desc.primitive)
    {
    case RHI::PrimitiveType::Point: m_description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT; break;
    case RHI::PrimitiveType::Line: m_description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE; break;
    case RHI::PrimitiveType::Triangle: m_description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; break;
    default: fail("Unsupported primitive type.");
    }
    m_description.NumRenderTargets = desc.attachments.colorCount;
    for (std::uint32_t index = 0; index < desc.attachments.colorCount; ++index)
    {
        const auto format = desc.attachments.colorFormats[index];
        if (format == RHI::Format::Unknown || isDepth(format))
            fail("Color attachment requires a color format.");
        m_description.RTVFormats[index] = toNativeFormat(format);
    }
    m_description.DSVFormat = toNativeFormat(desc.attachments.depthFormat);
    m_description.SampleDesc = {desc.attachments.samples.count, desc.attachments.samples.quality};

    // 先完成名稱配置，再保存 c_str，避免容器擴充使指標失效。
    m_semantics.reserve(desc.vertexInput.size());
    for (const auto& attribute : desc.vertexInput)
        m_semantics.push_back(attribute.semantic);
    m_input.reserve(desc.vertexInput.size());
    for (std::size_t index = 0; index < desc.vertexInput.size(); ++index)
    {
        const auto& attribute = desc.vertexInput[index];
        if (attribute.semantic.empty() || attribute.semantic.find('\0') != std::string::npos)
            fail("Vertex semantic must be non-empty and contain no null characters.");
        if (attribute.format == RHI::Format::Unknown || isDepth(attribute.format))
            fail("Vertex attribute requires a vertex-compatible format.");
        if (attribute.slot >= D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT)
            fail("Vertex input slot is out of range.");
        D3D12_INPUT_CLASSIFICATION classification;
        switch (attribute.rate)
        {
        case RHI::InputRate::Vertex:
            if (attribute.instanceStepRate != 0)
                fail("Per-vertex attributes require instance step rate zero.");
            classification = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            break;
        case RHI::InputRate::Instance:
            classification = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
            break;
        default: fail("Unsupported input rate."); return;
        }
        m_input.push_back({m_semantics[index].c_str(), attribute.semanticIndex,
            toNativeFormat(attribute.format), attribute.slot, attribute.offset,
            classification, attribute.instanceStepRate});
    }
}

D3D12_GRAPHICS_PIPELINE_STATE_DESC GraphicsPipelineDescription::get() const
{
    auto desc = m_description;
    desc.InputLayout = {m_input.empty() ? nullptr : m_input.data(), static_cast<UINT>(m_input.size())};
    return desc;
}

Microsoft::WRL::ComPtr<ID3D12PipelineState> createGraphicsPipeline(
    ID3D12Device& device, ID3D12RootSignature* layout, D3D12_SHADER_BYTECODE vertexShader,
    D3D12_SHADER_BYTECODE pixelShader, const RHI::GraphicsPipelineDesc& desc)
{
    if (layout == nullptr || vertexShader.pShaderBytecode == nullptr || vertexShader.BytecodeLength == 0 ||
        pixelShader.pShaderBytecode == nullptr || pixelShader.BytecodeLength == 0)
        throw std::invalid_argument(desc.debugName + ": A binding layout and compiled graphics shaders are required.");
    GraphicsPipelineDescription translated(desc);
    auto native = translated.get();
    native.pRootSignature = layout;
    native.VS = vertexShader;
    native.PS = pixelShader;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline;
    const auto result = device.CreateGraphicsPipelineState(&native, IID_PPV_ARGS(&pipeline));
    if (FAILED(result))
        throw std::runtime_error(desc.debugName + ": Failed to create graphics pipeline; HRESULT " + std::to_string(result));
    return pipeline;
}

Microsoft::WRL::ComPtr<ID3D12PipelineState> createComputePipeline(
    ID3D12Device& device, ID3D12RootSignature* layout, D3D12_SHADER_BYTECODE computeShader,
    const RHI::ComputePipelineDesc& desc)
{
    if (layout == nullptr || computeShader.pShaderBytecode == nullptr || computeShader.BytecodeLength == 0)
        throw std::invalid_argument(desc.debugName + ": A binding layout and compiled compute shader are required.");
    D3D12_COMPUTE_PIPELINE_STATE_DESC native{};
    native.pRootSignature = layout;
    native.CS = computeShader;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline;
    const auto result = device.CreateComputePipelineState(&native, IID_PPV_ARGS(&pipeline));
    if (FAILED(result))
        throw std::runtime_error(desc.debugName + ": Failed to create compute pipeline; HRESULT " + std::to_string(result));
    return pipeline;
}
}
