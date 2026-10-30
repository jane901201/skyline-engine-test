#include <catch2/catch_test_macros.hpp>
#include "backends/d3d12/d3d12_device.h"
#include "backends/d3d12/d3d12_interop.h"
#include <wrl/client.h>
#include <stdexcept>

TEST_CASE("RHI factories validate device identity stages and required inputs", "[gpu][rhi]")
{
    using namespace Skyline;
    Microsoft::WRL::ComPtr<ID3D12Device> native;
    REQUIRE(SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&native))));
    auto device = D3D12::createDevice(*native.Get());
    auto other = D3D12::createDevice(*native.Get());
    REQUIRE(device->capabilities().shaderModelMajor == 6);
    REQUIRE(device->supportsAttachment(RHI::Format::Rgba8Unorm));
    REQUIRE(device->supportsAttachment(RHI::Format::D32Float));
    REQUIRE_FALSE(device->supportsAttachment(RHI::Format::Unknown));
    REQUIRE_FALSE(device->supportsAttachment(RHI::Format::Rgba8Unorm, {0, 0}));
    REQUIRE_THROWS_AS(device->createShader({}), std::invalid_argument);
    REQUIRE_THROWS_AS(device->createShader({RHI_TEST_SHADER_PATH, "VSMain", RHI::ShaderStage::Unknown}), std::invalid_argument);
    REQUIRE_THROWS_AS(device->createShader({RHI_TEST_SHADER_PATH, "MissingEntry", RHI::ShaderStage::Vertex}), std::runtime_error);

    auto layout = device->createBindingLayout({});
    auto foreignLayout = other->createBindingLayout({});
    auto vs = device->createShader({RHI_TEST_SHADER_PATH, "VSMain", RHI::ShaderStage::Vertex});
    auto ps = device->createShader({RHI_TEST_SHADER_PATH, "PSMain", RHI::ShaderStage::Pixel});
    auto cs = device->createShader({RHI_TEST_SHADER_PATH, "CSMain", RHI::ShaderStage::Compute});
    auto foreignVs = other->createShader({RHI_TEST_SHADER_PATH, "VSMain", RHI::ShaderStage::Vertex});
    auto desc = RHI::GraphicsPipelineDesc::fullscreen();
    desc.vertexInput = {{"POSITION", 0, RHI::Format::Rgb32Float}};
    desc.attachments.colorCount = 1;
    desc.attachments.colorFormats[0] = RHI::Format::Rgba8Unorm;
    REQUIRE_THROWS_AS(device->createGraphicsPipeline(desc), std::invalid_argument);
    desc.bindingLayout = layout.get();
    desc.vertexShader = vs.get();
    desc.pixelShader = ps.get();
    auto pipeline = device->createGraphicsPipeline(desc);
    REQUIRE(pipeline->kind() == RHI::PipelineKind::Graphics);
    REQUIRE(&pipeline->device() == device.get());
    desc.vertexShader = ps.get();
    REQUIRE_THROWS_AS(device->createGraphicsPipeline(desc), std::invalid_argument);
    desc.vertexShader = foreignVs.get();
    REQUIRE_THROWS_AS(device->createGraphicsPipeline(desc), std::invalid_argument);
    desc.vertexShader = vs.get();
    desc.bindingLayout = foreignLayout.get();
    REQUIRE_THROWS_AS(device->createGraphicsPipeline(desc), std::invalid_argument);

    RHI::ComputePipelineDesc compute;
    compute.bindingLayout = layout.get();
    compute.computeShader = ps.get();
    REQUIRE_THROWS_AS(device->createComputePipeline(compute), std::invalid_argument);
    compute.computeShader = cs.get();
    auto computePipeline = device->createComputePipeline(compute);
    REQUIRE(computePipeline->kind() == RHI::PipelineKind::Compute);
    REQUIRE_THROWS_AS(other->createComputePipeline(compute), std::invalid_argument);

    // 工廠只在建立時借用輸入；管線保留執行所需的原生配置。
    auto* retainedLayout = D3D12::nativePipelineLayout(*pipeline);
    layout.reset();
    vs.reset();
    ps.reset();
    cs.reset();
    REQUIRE(D3D12::nativePipeline(*pipeline) != nullptr);
    REQUIRE(D3D12::nativePipelineLayout(*pipeline) == retainedLayout);
    REQUIRE(D3D12::nativePipelineLayout(*computePipeline) == retainedLayout);
}
