#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>

#include "backends/d3d12/d3d12_pipeline.h"

#include <memory>
#include <stdexcept>
#include <string>

namespace
{
using namespace Skyline;

RHI::GraphicsPipelineDesc fullscreen()
{
    auto desc = RHI::GraphicsPipelineDesc::fullscreen();
    desc.debugName = "Test pipeline";
    desc.attachments.colorCount = 1;
    desc.attachments.colorFormats[0] = RHI::Format::Rgba8Unorm;
    return desc;
}
}

TEST_CASE("RHI fullscreen translation matches depth-disabled render target state", "[rhi]")
{
    D3D12::GraphicsPipelineDescription translated(fullscreen());
    const auto native = translated.get();
    REQUIRE(native.InputLayout.NumElements == 0);
    REQUIRE(native.InputLayout.pInputElementDescs == nullptr);
    REQUIRE(native.RasterizerState.CullMode == D3D12_CULL_MODE_NONE);
    REQUIRE_FALSE(native.DepthStencilState.DepthEnable);
    REQUIRE(native.DepthStencilState.DepthWriteMask == D3D12_DEPTH_WRITE_MASK_ZERO);
    REQUIRE_FALSE(native.DepthStencilState.StencilEnable);
    REQUIRE_FALSE(native.BlendState.RenderTarget[0].BlendEnable);
    REQUIRE(native.RTVFormats[0] == DXGI_FORMAT_R8G8B8A8_UNORM);
    REQUIRE(native.DSVFormat == DXGI_FORMAT_UNKNOWN);
    REQUIRE(native.SampleDesc.Count == 1);
}

TEST_CASE("RHI preserves cloud alpha accumulation and sun alpha replacement", "[rhi]")
{
    auto desc = fullscreen();
    desc.blend[0] = RHI::BlendDesc::alpha();
    D3D12::GraphicsPipelineDescription cloud(desc);
    const auto cloudBlend = cloud.get().BlendState.RenderTarget[0];
    REQUIRE(cloudBlend.BlendEnable);
    REQUIRE(cloudBlend.SrcBlend == D3D12_BLEND_SRC_ALPHA);
    REQUIRE(cloudBlend.DestBlend == D3D12_BLEND_INV_SRC_ALPHA);
    REQUIRE(cloudBlend.SrcBlendAlpha == D3D12_BLEND_ONE);
    REQUIRE(cloudBlend.DestBlendAlpha == D3D12_BLEND_INV_SRC_ALPHA);

    desc.blend[0].destinationAlpha = RHI::BlendFactor::Zero;
    D3D12::GraphicsPipelineDescription sun(desc);
    REQUIRE(sun.get().BlendState.RenderTarget[0].DestBlendAlpha == D3D12_BLEND_ZERO);
}

TEST_CASE("RHI backend owns vertex semantics after the source description is destroyed", "[rhi]")
{
    auto translated = [] {
        auto desc = fullscreen();
        desc.vertexInput = {
            {"POSITION", 0, RHI::Format::Rgb32Float},
            {"INSTANCE_TRANSFORM", 2, RHI::Format::Rgba32Float, 1, 16, RHI::InputRate::Instance, 1}
        };
        return std::make_unique<D3D12::GraphicsPipelineDescription>(desc);
    }();
    const auto native = translated->get();
    REQUIRE(native.InputLayout.NumElements == 2);
    const auto* attributes = native.InputLayout.pInputElementDescs;
    REQUIRE(std::string(attributes[0].SemanticName) == "POSITION");
    REQUIRE(attributes[0].Format == DXGI_FORMAT_R32G32B32_FLOAT);
    REQUIRE(std::string(attributes[1].SemanticName) == "INSTANCE_TRANSFORM");
    REQUIRE(attributes[1].SemanticIndex == 2);
    REQUIRE(attributes[1].InputSlot == 1);
    REQUIRE(attributes[1].AlignedByteOffset == 16);
    REQUIRE(attributes[1].InputSlotClass == D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA);
    REQUIRE(attributes[1].InstanceDataStepRate == 1);
}

TEST_CASE("RHI preserves depth rasterizer MRT and sample overrides", "[rhi]")
{
    auto desc = fullscreen();
    desc.attachments.colorCount = 2;
    desc.attachments.colorFormats[1] = RHI::Format::Rgba16Float;
    desc.attachments.depthFormat = RHI::Format::D32Float;
    desc.attachments.samples = {4, 0};
    desc.depth = {true, false, RHI::CompareOp::GreaterEqual};
    desc.rasterizer = {RHI::FillMode::Wireframe, RHI::CullMode::Front, true, false};
    desc.independentBlend = true;
    desc.blend[1] = RHI::BlendDesc::alpha();
    desc.blend[1].writeMask = 0x07;
    D3D12::GraphicsPipelineDescription translated(desc);
    const auto native = translated.get();
    REQUIRE(native.NumRenderTargets == 2);
    REQUIRE(native.RTVFormats[1] == DXGI_FORMAT_R16G16B16A16_FLOAT);
    REQUIRE(native.DSVFormat == DXGI_FORMAT_D32_FLOAT);
    REQUIRE(native.SampleDesc.Count == 4);
    REQUIRE(native.DepthStencilState.DepthEnable);
    REQUIRE(native.DepthStencilState.DepthWriteMask == D3D12_DEPTH_WRITE_MASK_ZERO);
    REQUIRE(native.DepthStencilState.DepthFunc == D3D12_COMPARISON_FUNC_GREATER_EQUAL);
    REQUIRE(native.RasterizerState.FillMode == D3D12_FILL_MODE_WIREFRAME);
    REQUIRE(native.RasterizerState.CullMode == D3D12_CULL_MODE_FRONT);
    REQUIRE(native.RasterizerState.FrontCounterClockwise);
    REQUIRE_FALSE(native.RasterizerState.DepthClipEnable);
    REQUIRE(native.BlendState.IndependentBlendEnable);
    REQUIRE(native.BlendState.RenderTarget[1].RenderTargetWriteMask == 0x07);
}

TEST_CASE("RHI rejects invalid attachment depth and input combinations at creation", "[rhi]")
{
    auto desc = fullscreen();
    SECTION("missing attachments") { desc.attachments.colorCount = 0; }
    SECTION("attachment count overflow") { desc.attachments.colorCount = 9; }
    SECTION("depth format used as color") { desc.attachments.colorFormats[0] = RHI::Format::D32Float; }
    SECTION("color format used as depth") { desc.attachments.depthFormat = RHI::Format::Rgba8Unorm; }
    SECTION("depth without depth attachment") { desc.depth.test = true; }
    SECTION("depth writes without testing") { desc.attachments.depthFormat = RHI::Format::D32Float; desc.depth.write = true; }
    SECTION("zero samples") { desc.attachments.samples.count = 0; }
    SECTION("single sample quality") { desc.attachments.samples.quality = 1; }
    SECTION("unknown vertex format") { desc.vertexInput = {{"POSITION"}}; }
    SECTION("empty semantic") { desc.vertexInput = {{"", 0, RHI::Format::Rgb32Float}}; }
    SECTION("invalid input slot") { desc.vertexInput = {{"POSITION", 0, RHI::Format::Rgb32Float, 32}}; }
    SECTION("instance step on vertex input") { desc.vertexInput = {{"POSITION", 0, RHI::Format::Rgb32Float, 0, 0, RHI::InputRate::Vertex, 1}}; }
    SECTION("invalid write mask") { desc.blend[0].writeMask = 0xff; }
    REQUIRE_THROWS_MATCHES(D3D12::GraphicsPipelineDescription(desc), std::invalid_argument,
                          Catch::Matchers::MessageMatches(Catch::Matchers::ContainsSubstring("Test pipeline")));
}

TEST_CASE("D3D12 format conversion rejects unknown backend values", "[rhi]")
{
    REQUIRE_THROWS_AS(D3D12::toNativeFormat(static_cast<RHI::Format>(999)), std::invalid_argument);
    REQUIRE_THROWS_AS(D3D12::fromNativeFormat(DXGI_FORMAT_R8_TYPELESS), std::invalid_argument);
}
