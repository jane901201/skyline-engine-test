#include <catch2/catch_test_macros.hpp>
#include "backends/d3d12/d3d12_device.h"
#include "backends/d3d12/d3d12_interop.h"
#include "rhi/command_context.h"
#include <directx/d3dx12_core.h>
#include <directx/d3dx12_barriers.h>
#include <directx/d3d12sdklayers.h>
#include <wrl/client.h>
#include <array>
#include <cstring>
#include <memory>
#include <type_traits>

TEST_CASE("RHI compute dispatch writes every element across partial thread groups", "[gpu][rhi]")
{
    using namespace Skyline;
    using Microsoft::WRL::ComPtr;
    ComPtr<ID3D12Debug> debug;
    REQUIRE(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))));
    debug->EnableDebugLayer();
    ComPtr<ID3D12Device> native;
    REQUIRE(SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&native))));
    ComPtr<ID3D12InfoQueue> messages;
    REQUIRE(SUCCEEDED(native.As(&messages)));
    auto device = D3D12::createDevice(*native.Get());
    bool sampleResults = false;
    SECTION("storage writes") {}
    SECTION("storage writes followed by sampled reads") { sampleResults = true; }
    RHI::BindingLayoutDesc layoutDesc;
    layoutDesc.allowVertexInput = false;
    layoutDesc.constantBuffers = {{0, 0, RHI::ShaderVisibility::Compute}};
    layoutDesc.resources = {{RHI::BindingType::SampledResource, 0, 1}, {RHI::BindingType::StorageResource, 0, 1}};
    auto layout = device->createBindingLayout(layoutDesc);
    auto shader = device->createShader({RHI_COMPUTE_SHADER_PATH, "CSMain", RHI::ShaderStage::Compute});
    RHI::ComputePipelineDesc desc;
    desc.bindingLayout = layout.get();
    desc.computeShader = shader.get();
    auto pipeline = device->createComputePipeline(desc);
    auto readShader = device->createShader({RHI_COMPUTE_SHADER_PATH, "ReadMain", RHI::ShaderStage::Compute});
    desc.computeShader = readShader.get();
    auto readPipeline = device->createComputePipeline(desc);
    shader.reset();
    readShader.reset();

    constexpr UINT count = 130;
    constexpr UINT byteSize = count * sizeof(UINT);
    using namespace RHI;
    // 非零 View 偏移可偵測後端錯誤地忽略 FirstElement 的情況。
    constexpr UINT viewOffset = 4 * sizeof(UINT);
    auto output = device->createBuffer({byteSize + viewOffset, BufferUsage::Storage | BufferUsage::Sampled | BufferUsage::CopySource,
        MemoryType::Gpu, ResourceState::Storage});
    auto destination = device->createBuffer({byteSize, BufferUsage::Storage | BufferUsage::CopySource,
        MemoryType::Gpu, ResourceState::Storage});
    auto readback = device->createBuffer({byteSize, BufferUsage::CopyDestination,
        MemoryType::Readback, ResourceState::CopyDestination});
    auto constants = device->createBuffer({8, BufferUsage::Constant,
        MemoryType::Upload, ResourceState::GenericRead});
    const UINT parameters[] = {count, 17};
    constants->write(0, std::as_bytes(std::span{parameters}));
    auto cbv = device->createBufferView(constants, {BindingType::ConstantBuffer, 0, sizeof(parameters)});
    auto srv = device->createBufferView(output, {BindingType::SampledResource, viewOffset, byteSize, sizeof(UINT)});
    auto uav = device->createBufferView(output, {BindingType::StorageResource, viewOffset, byteSize, sizeof(UINT)});
    auto destinationView = device->createBufferView(destination, {BindingType::StorageResource, 0, byteSize, sizeof(UINT)});
    auto bindings = device->createBindingSet({layout.get(), {{0, 0, cbv}, {0, 0, srv}, {0, 0, uav}}});
    auto readBindings = device->createBindingSet({layout.get(), {{0, 0, cbv}, {0, 0, srv}, {0, 0, destinationView}}});
    layout.reset();
    cbv.reset();
    srv.reset();
    uav.reset();
    destinationView.reset();
    constants.reset();

    auto recording = device->createCommandContext();
    auto& context = *recording;
    context.begin();
    auto unrelatedLayout = device->createBindingLayout({});
    auto unrelatedShader = device->createShader({RHI_TEST_SHADER_PATH, "CSMain", ShaderStage::Compute});
    ComputePipelineDesc unrelatedDesc;
    unrelatedDesc.bindingLayout = unrelatedLayout.get();
    unrelatedDesc.computeShader = unrelatedShader.get();
    auto unrelatedPipeline = device->createComputePipeline(unrelatedDesc);
    context.setPipeline(*unrelatedPipeline);
    REQUIRE_THROWS_AS(context.setBindingSet(*bindings), std::invalid_argument);
    REQUIRE_THROWS_AS(context.transition(*readback, ResourceState::CopySource), std::invalid_argument);
    REQUIRE_THROWS_AS(context.storageBarrier(*readback), std::logic_error);
    context.setPipeline(*pipeline);
    context.setBindingSet(*bindings);
    context.dispatch((count + 63) / 64, 1, 1);
    if (sampleResults)
    {
        context.transition(*output, ResourceState::ShaderRead);
        context.setPipeline(*readPipeline);
        context.setBindingSet(*readBindings);
        context.dispatch((count + 63) / 64, 1, 1);
        context.transition(*destination, ResourceState::CopySource);
        context.copyBuffer(*readback, 0, *destination, 0, byteSize);
    }
    else
    {
        context.transition(*output, ResourceState::CopySource);
        context.copyBuffer(*readback, 0, *output, viewOffset, byteSize);
    }
    context.submitAndWait();
    std::array<UINT, count> result{};
    readback->read(0, std::as_writable_bytes(std::span{result}));
    for (UINT index = 0; index < count; ++index)
    {
        INFO(index);
        const UINT written = index * 3 + parameters[1];
        REQUIRE(result[index] == (sampleResults ? written * 5 + parameters[1] : written));
    }
    for (UINT64 index = 0; index < messages->GetNumStoredMessagesAllowedByRetrievalFilter(); ++index)
    {
        SIZE_T size = 0;
        REQUIRE(SUCCEEDED(messages->GetMessage(index, nullptr, &size)));
        auto storage = std::make_unique<std::byte[]>(size);
        auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.get());
        REQUIRE(SUCCEEDED(messages->GetMessage(index, message, &size)));
        INFO(message->pDescription);
        REQUIRE(message->Severity != D3D12_MESSAGE_SEVERITY_ERROR);
        REQUIRE(message->Severity != D3D12_MESSAGE_SEVERITY_CORRUPTION);
    }
}
