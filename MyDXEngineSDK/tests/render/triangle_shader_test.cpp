#include <catch2/catch_test_macros.hpp>

#include <Windows.h>
#include <d3dcompiler.h>
#include <directx/dxgiformat.h>
#include <directx/d3d12.h>
#include <directx/d3dx12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <type_traits>

#include "shader.h"
#include "backends/d3d12/d3d12_pipeline.h"
#include "backends/d3d12/d3d12_device.h"
#include "backends/d3d12/d3d12_interop.h"

namespace
{
    using Microsoft::WRL::ComPtr;

    constexpr UINT targetWidth = 8;
    constexpr UINT targetHeight = 8;
    constexpr DXGI_FORMAT targetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

    struct Vertex
    {
        float x;
        float y;
        float z;
    };

    using Pixel = std::array<std::uint8_t, 4>;

    struct RenderedPixels
    {
        Pixel center;
        Pixel corner;
    };

    void checkHr(HRESULT result, const char* message)
    {
        if (FAILED(result))
            throw std::runtime_error(message);
    }

    D3D12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE type)
    {
        D3D12_HEAP_PROPERTIES properties{};
        properties.Type = type;
        properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        properties.CreationNodeMask = 1;
        properties.VisibleNodeMask = 1;
        return properties;
    }

    D3D12_RESOURCE_DESC bufferDescription(UINT64 size)
    {
        D3D12_RESOURCE_DESC description{};
        description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        description.Width = size;
        description.Height = 1;
        description.DepthOrArraySize = 1;
        description.MipLevels = 1;
        description.SampleDesc.Count = 1;
        description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        return description;
    }

    [[deprecated("Unused after the RHI ownership refactor. Use device.createBindingLayout().")]]
    ComPtr<ID3D12RootSignature> createRootSignature(ID3D12Device* device)
    {
        D3D12_ROOT_SIGNATURE_DESC description{};
        description.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> serialized;
        ComPtr<ID3DBlob> errors;
        checkHr(D3D12SerializeRootSignature(&description, D3D_ROOT_SIGNATURE_VERSION_1,
                                            &serialized, &errors),
                "Triangle shader test: Failed to serialize root signature.");

        ComPtr<ID3D12RootSignature> rootSignature;
        checkHr(device->CreateRootSignature(0, serialized->GetBufferPointer(),
                                            serialized->GetBufferSize(),
                                            IID_PPV_ARGS(&rootSignature)),
                "Triangle shader test: Failed to create root signature.");
        return rootSignature;
    }

    [[deprecated("Retained as the pre-RHI reference. Use the overload accepting GraphicsPipelineDesc.")]]
    ComPtr<ID3D12PipelineState> createPipeline(ID3D12Device* device,
                                               ID3D12RootSignature* rootSignature)
    {
        Shader vertexShader;
        Shader pixelShader;
        vertexShader.loadVS(TRIANGLE_SHADER_PATH, "VSMain");
        pixelShader.loadPS(TRIANGLE_SHADER_PATH, "PSMain");

        constexpr D3D12_INPUT_ELEMENT_DESC inputElements[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC description{};
        description.InputLayout = {inputElements, 1};
        description.pRootSignature = rootSignature;
        description.VS = {vertexShader.getCompiledBlob()->GetBufferPointer(),
                          vertexShader.getCompiledBlob()->GetBufferSize()};
        description.PS = {pixelShader.getCompiledBlob()->GetBufferPointer(),
                          pixelShader.getCompiledBlob()->GetBufferSize()};
        description.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        description.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        description.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        description.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        description.DepthStencilState.DepthEnable = FALSE;
        description.DepthStencilState.StencilEnable = FALSE;
        description.SampleMask = UINT_MAX;
        description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        description.NumRenderTargets = 1;
        description.RTVFormats[0] = targetFormat;
        description.SampleDesc.Count = 1;

        ComPtr<ID3D12PipelineState> pipeline;
        checkHr(device->CreateGraphicsPipelineState(&description, IID_PPV_ARGS(&pipeline)),
                "Triangle shader test: Failed to create graphics pipeline.");
        return pipeline;
    }

    [[deprecated("Replaced by RHI factories. Use createPipeline(RHI::Device, desc).")]]
    ComPtr<ID3D12PipelineState> createPipeline(ID3D12Device* device, ID3D12RootSignature* rootSignature,
                                              const Skyline::RHI::GraphicsPipelineDesc& desc)
    {
        Shader vertexShader;
        Shader pixelShader;
        const char* path = desc.blend[0].enabled ? RHI_TEST_SHADER_PATH : TRIANGLE_SHADER_PATH;
        vertexShader.loadVS(path, "VSMain");
        pixelShader.loadPS(path, "PSMain");
        return Skyline::D3D12::createGraphicsPipeline(*device, rootSignature,
            {vertexShader.getCompiledBlob()->GetBufferPointer(), vertexShader.getCompiledBlob()->GetBufferSize()},
            {pixelShader.getCompiledBlob()->GetBufferPointer(), pixelShader.getCompiledBlob()->GetBufferSize()}, desc);
    }

    std::unique_ptr<Skyline::RHI::Pipeline> createPipeline(Skyline::RHI::Device& device,
                                                         Skyline::RHI::GraphicsPipelineDesc desc)
    {
        const char* path = desc.blend[0].enabled ? RHI_TEST_SHADER_PATH : TRIANGLE_SHADER_PATH;
        auto vs = device.createShader({path, "VSMain", Skyline::RHI::ShaderStage::Vertex});
        auto ps = device.createShader({path, "PSMain", Skyline::RHI::ShaderStage::Pixel});
        auto layout = device.createBindingLayout({});
        desc.vertexShader = vs.get();
        desc.pixelShader = ps.get();
        desc.bindingLayout = layout.get();
        // 返回後 Shader 與配置物件會解構，GPU 讀回亦驗證管線保留必要資源。
        return device.createGraphicsPipeline(desc);
    }

    RenderedPixels renderTriangle(bool alphaBlend = false, bool replaceAlpha = false)
    {
        ComPtr<ID3D12Device> device;
        checkHr(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)),
                "Triangle shader test: Failed to create D3D12 device.");

        D3D12_COMMAND_QUEUE_DESC queueDescription{};
        queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        ComPtr<ID3D12CommandQueue> queue;
        checkHr(device->CreateCommandQueue(&queueDescription, IID_PPV_ARGS(&queue)),
                "Triangle shader test: Failed to create command queue.");

        ComPtr<ID3D12CommandAllocator> allocator;
        checkHr(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                               IID_PPV_ARGS(&allocator)),
                "Triangle shader test: Failed to create command allocator.");

        auto rhiDevice = Skyline::D3D12::createDevice(*device.Get());
        auto pipelineDesc = Skyline::RHI::GraphicsPipelineDesc::fullscreen();
        pipelineDesc.debugName = "RHI triangle GPU test";
        pipelineDesc.vertexInput = {{"POSITION", 0, Skyline::RHI::Format::Rgb32Float}};
        pipelineDesc.attachments.colorCount = 1;
        pipelineDesc.attachments.colorFormats[0] = Skyline::RHI::Format::Rgba8Unorm;
        if (alphaBlend)
            pipelineDesc.blend[0] = Skyline::RHI::BlendDesc::alpha();
        if (replaceAlpha)
            pipelineDesc.blend[0].destinationAlpha = Skyline::RHI::BlendFactor::Zero;
        auto pipeline = createPipeline(*rhiDevice, pipelineDesc);

        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDescription{};
        rtvHeapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDescription.NumDescriptors = 1;
        ComPtr<ID3D12DescriptorHeap> rtvHeap;
        checkHr(device->CreateDescriptorHeap(&rtvHeapDescription, IID_PPV_ARGS(&rtvHeap)),
                "Triangle shader test: Failed to create RTV heap.");

        D3D12_RESOURCE_DESC targetDescription{};
        targetDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        targetDescription.Width = targetWidth;
        targetDescription.Height = targetHeight;
        targetDescription.DepthOrArraySize = 1;
        targetDescription.MipLevels = 1;
        targetDescription.Format = targetFormat;
        targetDescription.SampleDesc.Count = 1;
        targetDescription.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        targetDescription.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE clearValue{};
        clearValue.Format = targetFormat;
        const auto defaultHeap = heapProperties(D3D12_HEAP_TYPE_DEFAULT);
        ComPtr<ID3D12Resource> target;
        checkHr(device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE,
                                                &targetDescription, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                &clearValue, IID_PPV_ARGS(&target)),
                "Triangle shader test: Failed to create render target.");
        const auto rtv = rtvHeap->GetCPUDescriptorHandleForHeapStart();
        device->CreateRenderTargetView(target.Get(), nullptr, rtv);

        constexpr Vertex vertices[] = {
            {-0.5f, -0.5f, 0.0f},
            {0.0f, 0.5f, 0.0f},
            {0.5f, -0.5f, 0.0f},
        };
        const auto uploadHeap = heapProperties(D3D12_HEAP_TYPE_UPLOAD);
        const auto vertexDescription = bufferDescription(sizeof(vertices));
        ComPtr<ID3D12Resource> vertexBuffer;
        checkHr(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE,
                                                &vertexDescription, D3D12_RESOURCE_STATE_GENERIC_READ,
                                                nullptr, IID_PPV_ARGS(&vertexBuffer)),
                "Triangle shader test: Failed to create vertex buffer.");
        void* vertexData = nullptr;
        D3D12_RANGE noRead{0, 0};
        checkHr(vertexBuffer->Map(0, &noRead, &vertexData),
                "Triangle shader test: Failed to map vertex buffer.");
        std::memcpy(vertexData, vertices, sizeof(vertices));
        vertexBuffer->Unmap(0, nullptr);
        D3D12_VERTEX_BUFFER_VIEW vertexView{};
        vertexView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
        vertexView.SizeInBytes = sizeof(vertices);
        vertexView.StrideInBytes = sizeof(Vertex);

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT64 readbackSize = 0;
        device->GetCopyableFootprints(&targetDescription, 0, 1, 0, &footprint,
                                      nullptr, nullptr, &readbackSize);
        const auto readbackHeap = heapProperties(D3D12_HEAP_TYPE_READBACK);
        const auto readbackDescription = bufferDescription(readbackSize);
        ComPtr<ID3D12Resource> readback;
        checkHr(device->CreateCommittedResource(&readbackHeap, D3D12_HEAP_FLAG_NONE,
                                                &readbackDescription, D3D12_RESOURCE_STATE_COPY_DEST,
                                                nullptr, IID_PPV_ARGS(&readback)),
                "Triangle shader test: Failed to create readback buffer.");

        ComPtr<ID3D12GraphicsCommandList> commands;
        checkHr(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(),
                                          Skyline::D3D12::nativePipeline(*pipeline), IID_PPV_ARGS(&commands)),
                "Triangle shader test: Failed to create command list.");

        const D3D12_VIEWPORT viewport{0.0f, 0.0f, float(targetWidth), float(targetHeight), 0.0f, 1.0f};
        const D3D12_RECT scissor{0, 0, LONG(targetWidth), LONG(targetHeight)};
        const float black[] = {0.0f, 0.0f, 0.0f, alphaBlend ? 0.25f : 1.0f};
        commands->RSSetViewports(1, &viewport);
        commands->RSSetScissorRects(1, &scissor);
        commands->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        commands->ClearRenderTargetView(rtv, black, 0, nullptr);
        commands->SetGraphicsRootSignature(Skyline::D3D12::nativePipelineLayout(*pipeline));
        commands->SetPipelineState(Skyline::D3D12::nativePipeline(*pipeline));
        commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commands->IASetVertexBuffers(0, 1, &vertexView);
        commands->DrawInstanced(3, 1, 0, 0);

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = target.Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        commands->ResourceBarrier(1, &barrier);

        D3D12_TEXTURE_COPY_LOCATION destination{};
        destination.pResource = readback.Get();
        destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        destination.PlacedFootprint = footprint;
        D3D12_TEXTURE_COPY_LOCATION source{};
        source.pResource = target.Get();
        source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        source.SubresourceIndex = 0;
        commands->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
        checkHr(commands->Close(), "Triangle shader test: Failed to close command list.");

        ID3D12CommandList* commandLists[] = {commands.Get()};
        queue->ExecuteCommandLists(1, commandLists);
        ComPtr<ID3D12Fence> fence;
        checkHr(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)),
                "Triangle shader test: Failed to create fence.");
        checkHr(queue->Signal(fence.Get(), 1), "Triangle shader test: Failed to signal fence.");
        std::unique_ptr<std::remove_pointer_t<HANDLE>, decltype(&CloseHandle)> event(
            CreateEventW(nullptr, FALSE, FALSE, nullptr), &CloseHandle);
        if (!event)
            throw std::runtime_error("Triangle shader test: Failed to create fence event.");
        checkHr(fence->SetEventOnCompletion(1, event.get()),
                "Triangle shader test: Failed to wait for GPU completion.");
        if (WaitForSingleObject(event.get(), INFINITE) != WAIT_OBJECT_0)
            throw std::runtime_error("Triangle shader test: GPU wait failed.");

        D3D12_RANGE readRange{0, static_cast<SIZE_T>(readbackSize)};
        void* mapped = nullptr;
        checkHr(readback->Map(0, &readRange, &mapped),
                "Triangle shader test: Failed to map readback buffer.");
        const auto* bytes = static_cast<const std::uint8_t*>(mapped) + footprint.Offset;
        const auto pixelAt = [bytes, &footprint](UINT x, UINT y) {
            Pixel pixel{};
            std::memcpy(pixel.data(), bytes + y * footprint.Footprint.RowPitch + x * pixel.size(),
                        pixel.size());
            return pixel;
        };
        RenderedPixels result{pixelAt(4, 4), pixelAt(0, 0)};
        D3D12_RANGE noWrite{0, 0};
        readback->Unmap(0, &noWrite);
        return result;
    }
}

TEST_CASE("triangle shader renders an opaque red triangle", "[gpu][triangle]")
{
    const auto pixels = renderTriangle();
    REQUIRE(pixels.center == Pixel{255, 0, 0, 255});
    REQUIRE(pixels.corner == Pixel{0, 0, 0, 255});
}

TEST_CASE("RHI cloud blend accumulates translucent alpha on the GPU", "[gpu][rhi]")
{
    const auto pixels = renderTriangle(true);
    REQUIRE(pixels.center[0] >= 127);
    REQUIRE(pixels.center[0] <= 128);
    REQUIRE(pixels.center[1] == 0);
    REQUIRE(pixels.center[2] == 0);
    REQUIRE(pixels.center[3] >= 159);
    REQUIRE(pixels.center[3] <= 160);
    REQUIRE(pixels.corner == Pixel{0, 0, 0, 64});
}

TEST_CASE("RHI sun blend replaces translucent alpha on the GPU", "[gpu][rhi]")
{
    const auto pixels = renderTriangle(true, true);
    REQUIRE(pixels.center[0] >= 127);
    REQUIRE(pixels.center[0] <= 128);
    REQUIRE(pixels.center[1] == 0);
    REQUIRE(pixels.center[2] == 0);
    REQUIRE(pixels.center[3] >= 127);
    REQUIRE(pixels.center[3] <= 128);
    REQUIRE(pixels.corner == Pixel{0, 0, 0, 64});
}
