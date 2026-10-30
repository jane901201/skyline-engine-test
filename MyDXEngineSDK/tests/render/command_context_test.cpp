#include <catch2/catch_test_macros.hpp>
#include "backends/d3d12/d3d12_device.h"
#include "rhi/command_context.h"
#include "mesh.h"
#include "backends/d3d12/d3d12_frame.h"
#include <type_traits>
#include <directx/d3d12sdklayers.h>
#include <wrl/client.h>
#include <array>
#include <algorithm>
#include <stdexcept>
#include <limits>

namespace
{
struct Fixture
{
    Microsoft::WRL::ComPtr<ID3D12Device> native;
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> messages;
    std::unique_ptr<Skyline::RHI::Device> device;
    Fixture()
    {
        Microsoft::WRL::ComPtr<ID3D12Debug> debug;
        REQUIRE(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))));
        debug->EnableDebugLayer();
        REQUIRE(SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&native))));
        REQUIRE(SUCCEEDED(native.As(&messages)));
        device = Skyline::D3D12::createDevice(*native.Get());
    }
    void checkMessages()
    {
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
};
}

TEST_CASE("RHI texture sampling indexed drawing updates and descriptor replacement render correct pixels", "[gpu][rhi]")
{
    using namespace Skyline::RHI;
    Fixture fixture;
    auto& device = *fixture.device;
    auto commands = device.createCommandContext();
    auto source = device.createTexture({3, 2, Format::Rgba8Unorm, TextureUsage::Sampled | TextureUsage::CopyDestination});
    auto replacement = device.createTexture({3, 2, Format::Rgba8Unorm, TextureUsage::Sampled | TextureUsage::CopyDestination});
    auto target = device.createTexture({8, 8, Format::Rgba8Unorm, TextureUsage::RenderTarget | TextureUsage::CopySource, ResourceState::RenderTarget});
    const std::array<float, 8> vertices{-1, -1, -1, 1, 1, 1, 1, -1};
    const std::array<std::uint16_t, 6> indices{0, 1, 2, 0, 2, 3};
    auto vb = device.createBuffer({sizeof(vertices), BufferUsage::Vertex | BufferUsage::CopyDestination});
    auto ib = device.createBuffer({sizeof(indices), BufferUsage::Index | BufferUsage::CopyDestination});
    BindingLayoutDesc definition;
    definition.resources = {{BindingType::SampledResource, 0, 1}};
    definition.samplers = {{0, 0, ShaderVisibility::Pixel, SamplerFilter::Point, AddressMode::Clamp, AddressMode::Clamp, AddressMode::Clamp}};
    auto layout = device.createBindingLayout(definition);
    auto vs = device.createShader({RHI_TEXTURE_SHADER_PATH, "VSMain", ShaderStage::Vertex});
    auto ps = device.createShader({RHI_TEXTURE_SHADER_PATH, "PSMain", ShaderStage::Pixel});
    auto desc = GraphicsPipelineDesc::fullscreen();
    desc.vertexShader = vs.get(); desc.pixelShader = ps.get(); desc.bindingLayout = layout.get();
    desc.vertexInput = {{"POSITION", 0, Format::Rg32Float}};
    desc.attachments.colorCount = 1; desc.attachments.colorFormats[0] = Format::Rgba8Unorm;
    auto pipeline = device.createGraphicsPipeline(desc);
    auto view = device.createTextureView(source);
    BindingSetDesc bindingsDesc;
    bindingsDesc.layout = layout.get(); bindingsDesc.textures = {{0, 0, view}};
    auto set = device.createBindingSet(bindingsDesc);
    view.reset(); vs.reset(); ps.reset();
    commands->begin();
    commands->uploadBuffer(*vb, 0, std::as_bytes(std::span{vertices}), ResourceState::Vertex);
    commands->uploadBuffer(*ib, 0, std::as_bytes(std::span{indices}), ResourceState::Index);
    commands->submitAndWait();
    for (std::uint8_t iteration = 0; iteration < 3; ++iteration)
    {
        // 非 256 位元組的輸入 Row Pitch，驗證後端自行處理原生對齊。
        const std::array<std::uint8_t, 4> expected{static_cast<std::uint8_t>(30 + iteration * 70), 60, 120, 255};
        std::array<std::uint8_t, 32> image{};
        for (std::size_t y = 0; y < 2; ++y)
            for (std::size_t x = 0; x < 3; ++x)
                std::copy(expected.begin(), expected.end(), image.begin() + y * 16 + x * 4);
        commands->begin();
        auto& selected = iteration == 2 ? replacement : source;
        commands->uploadTexture(*selected, std::as_bytes(std::span{image}), 16);
        if (iteration == 2)
        {
            bindingsDesc.textures = {{0, 0, device.createTextureView(replacement)}};
            auto newSet = device.createBindingSet(bindingsDesc);
            set = std::move(newSet);
        }
        commands->setRenderTarget(*target);
        commands->setViewport({0, 0, 8, 8}, {0, 0, 8, 8});
        commands->clearColor(*target, {0, 0, 0, 1});
        commands->setPipeline(*pipeline);
        commands->setBindingSet(*set);
        commands->setVertexBuffer(*vb, 2 * sizeof(float));
        commands->setIndexBuffer(*ib, IndexType::Uint16);
        REQUIRE_THROWS_AS(commands->drawIndexed(7), std::out_of_range);
        commands->drawIndexed(6);
        // 記錄後即可釋放集合；Context 保留本次提交使用的 Heap、View 與資源。
        if (iteration == 2) set.reset();
        const auto pixels = commands->readTexture(*target);
        REQUIRE(pixels.size() == 8 * 8 * 4);
        for (std::size_t pixel = 0; pixel < 64; ++pixel)
            for (std::size_t component = 0; component < 4; ++component)
                REQUIRE(std::to_integer<std::uint8_t>(pixels[pixel * 4 + component]) == expected[component]);
    }
    fixture.checkMessages();
}

TEST_CASE("RHI mesh upload renders indexed geometry and preserves it after failed reload", "[gpu][rhi]")
{
    using namespace Skyline::RHI;
    Fixture fixture;
    auto& device = *fixture.device;
    Mesh mesh;
    REQUIRE_THROWS_AS(mesh.initFromFbxFile("", device), std::invalid_argument);
    mesh.initFromFbxFile(RHI_MESH_PATH, device);
    REQUIRE_THROWS_AS(mesh.initFromFbxFile("missing_mesh.fbx", device), std::runtime_error);
    auto target = device.createTexture({16, 16, Format::Rgba8Unorm,
        TextureUsage::RenderTarget | TextureUsage::CopySource, ResourceState::RenderTarget});
    auto depth = device.createTexture({16, 16, Format::D32Float, TextureUsage::DepthStencil, ResourceState::DepthWrite});
    auto layout = device.createBindingLayout({});
    auto vs = device.createShader({RHI_TEST_SHADER_PATH, "VSMain", ShaderStage::Vertex});
    auto ps = device.createShader({RHI_TEST_SHADER_PATH, "PSMain", ShaderStage::Pixel});
    auto desc = GraphicsPipelineDesc::fullscreen();
    desc.vertexShader = vs.get(); desc.pixelShader = ps.get(); desc.bindingLayout = layout.get();
    desc.vertexInput = {{"POSITION", 0, Format::Rgb32Float}};
    desc.attachments.colorCount = 1; desc.attachments.colorFormats[0] = Format::Rgba8Unorm;
    desc.attachments.depthFormat = Format::D32Float; desc.depth.test = true; desc.depth.write = true;
    auto pipeline = device.createGraphicsPipeline(desc);
    auto commands = device.createCommandContext();
    commands->begin();
    commands->setRenderTarget(*target, depth.get());
    commands->setViewport({0, 0, 16, 16}, {0, 0, 16, 16});
    commands->clearColor(*target, {0, 0, 0, 1});
    commands->clearDepth(*depth);
    commands->setPipeline(*pipeline);
    mesh.draw(*commands);
    pipeline.reset();
    const auto pixels = commands->readTexture(*target);
    const auto center = (8 * 16 + 8) * 4;
    REQUIRE(pixels[center] == std::byte{255});
    REQUIRE(pixels[center + 1] == std::byte{0});
    REQUIRE(pixels[center + 2] == std::byte{0});
    REQUIRE(pixels[0] == std::byte{0});
    fixture.checkMessages();
}

TEST_CASE("RHI storage textures dispatch and read back across partial thread groups", "[gpu][rhi]")
{
    using namespace Skyline::RHI;
    Fixture fixture;
    auto& device = *fixture.device;
    auto texture = device.createTexture({9, 7, Format::Rgba8Unorm,
        TextureUsage::Storage | TextureUsage::CopySource, ResourceState::Storage});
    BindingLayoutDesc definition;
    definition.resources = {{BindingType::StorageResource, 0, 1}};
    auto layout = device.createBindingLayout(definition);
    auto shader = device.createShader({RHI_TEXTURE_SHADER_PATH, "FillMain", ShaderStage::Compute});
    auto pipeline = device.createComputePipeline({"Texture fill", shader.get(), layout.get()});
    BindingSetDesc desc;
    desc.layout = layout.get(); desc.textures = {{0, 0, device.createTextureView(texture, {BindingType::StorageResource})}};
    auto set = device.createBindingSet(desc);
    auto commands = device.createCommandContext();
    commands->begin(); commands->setPipeline(*pipeline); commands->setBindingSet(*set);
    commands->dispatch(2, 1, 1); commands->storageBarrier(*texture);
    const auto pixels = commands->readTexture(*texture);
    const std::array<std::byte, 4> expected{std::byte{32}, std::byte{64}, std::byte{128}, std::byte{255}};
    REQUIRE(pixels.size() == 9 * 7 * 4);
    for (std::size_t index = 0; index < pixels.size(); ++index)
        REQUIRE(pixels[index] == expected[index % 4]);
    fixture.checkMessages();
}

TEST_CASE("RHI migrated example shaders match their graphics and compute layouts", "[gpu][rhi]")
{
    using namespace Skyline::RHI;
    Fixture fixture;
    auto& device = *fixture.device;
    const std::array<const char*, 8> files{"sky_gradient.hlsl", "sun.hlsl", "fbx.hlsl", "light.hlsl",
        "imgui_triangle.hlsl", "fbm.hlsl", "pixel_shader.hlsl", "particle_render.hlsl"};
    for (const auto* file : files)
    {
        INFO(file);
        const std::string name = file;
        const auto path = std::string(RHI_EXAMPLE_SHADER_DIR) + name;
        BindingLayoutDesc definition;
        if (name != "sky_gradient.hlsl") definition.constantBuffers = {{0}};
        if (name == "particle_render.hlsl") definition.resources = {{BindingType::SampledResource, 0, 1}};
        auto layout = device.createBindingLayout(definition);
        auto vs = device.createShader({path, "VSMain", ShaderStage::Vertex});
        auto ps = device.createShader({path, "PSMain", ShaderStage::Pixel});
        auto desc = GraphicsPipelineDesc::fullscreen();
        desc.vertexShader = vs.get(); desc.pixelShader = ps.get(); desc.bindingLayout = layout.get();
        desc.attachments.colorCount = 1; desc.attachments.colorFormats[0] = Format::Rgba8Unorm;
        desc.attachments.depthFormat = Format::D32Float;
        if (name == "fbx.hlsl" || name == "light.hlsl")
            desc.vertexInput = {{"POSITION", 0, Format::Rgb32Float}, {"NORMAL", 0, Format::Rgb32Float, 0, 12},
                {"TEXCOORD", 0, Format::Rg32Float, 0, 24}};
        if (name == "imgui_triangle.hlsl") desc.vertexInput = {{"POSITION", 0, Format::Rgb32Float}};
        if (name == "light.hlsl") { desc.depth.test = true; desc.depth.write = true; }
        if (name == "sun.hlsl" || name == "particle_render.hlsl") desc.blend[0] = BlendDesc::alpha();
        REQUIRE_NOTHROW(device.createGraphicsPipeline(desc));
    }
    BindingLayoutDesc definition;
    definition.constantBuffers = {{0}};
    definition.resources = {{BindingType::StorageResource, 0, 1}};
    auto layout = device.createBindingLayout(definition);
    auto shader = device.createShader({std::string(RHI_EXAMPLE_SHADER_DIR) + "particle_compute.hlsl", "CSMain", ShaderStage::Compute});
    REQUIRE_NOTHROW(device.createComputePipeline({"Particle simulation", shader.get(), layout.get()}));
    fixture.checkMessages();
}

TEST_CASE("RHI frame lifecycle validates recording resize minimize restore and teardown", "[gpu][rhi]")
{
    using namespace Skyline::RHI;
    std::unique_ptr<std::remove_pointer_t<HWND>, decltype(&DestroyWindow)> window(
        CreateWindowExW(0, L"STATIC", L"RHI frame test", WS_OVERLAPPEDWINDOW,
            0, 0, 64, 64, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr), &DestroyWindow);
    REQUIRE(window != nullptr);
    REQUIRE_THROWS_AS(Skyline::D3D12::createFrame({nullptr, 64, 64}), std::invalid_argument);
    REQUIRE_THROWS_AS(Skyline::D3D12::createFrame({window.get(), 0, 64}), std::invalid_argument);
    auto frame = Skyline::D3D12::createFrame({window.get(), 64, 64});
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> messages;
    REQUIRE(SUCCEEDED(Skyline::D3D12::nativeFrameDevice(*frame)->QueryInterface(IID_PPV_ARGS(&messages))));
    REQUIRE(&frame->commands().device() == &frame->device());
    REQUIRE_THROWS_AS(frame->end(), std::logic_error);
    REQUIRE_THROWS_AS(frame->commands().begin(), std::logic_error);
    REQUIRE_THROWS_AS(frame->commands().draw(3), std::logic_error);
    auto layout = frame->device().createBindingLayout({});
    auto vs = frame->device().createShader({RHI_TEXTURE_SHADER_PATH, "FullscreenMain", ShaderStage::Vertex});
    auto ps = frame->device().createShader({RHI_TEST_SHADER_PATH, "PSMain", ShaderStage::Pixel});
    auto desc = GraphicsPipelineDesc::fullscreen();
    desc.vertexShader = vs.get(); desc.pixelShader = ps.get(); desc.bindingLayout = layout.get(); desc.attachments = frame->attachments();
    auto pipeline = frame->device().createGraphicsPipeline(desc);
    for (int iteration = 0; iteration < 3; ++iteration)
    {
        frame->begin();
        REQUIRE_THROWS_AS(frame->begin(), std::logic_error);
        REQUIRE_THROWS_AS(frame->resize(96, 80), std::logic_error);
        REQUIRE_THROWS_AS(frame->commands().submitAndWait(), std::logic_error);
        frame->commands().setPipeline(*pipeline);
        frame->commands().draw(3);
        frame->end();
    }
    frame->resize(0, 0);
    REQUIRE(frame->width() == 64);
    frame->resize(96, 80);
    REQUIRE(frame->width() == 96); REQUIRE(frame->height() == 80);
    frame->begin(); frame->commands().setPipeline(*pipeline); frame->commands().draw(3); frame->end();
    frame->waitIdle();
    pipeline.reset(); ps.reset(); vs.reset(); layout.reset(); frame.reset();
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

TEST_CASE("RHI GPU buffer updates preserve untouched ranges and track state across submissions", "[gpu][rhi]")
{
    using namespace Skyline::RHI;
    Fixture fixture;
    auto& device = *fixture.device;
    auto commands = device.createCommandContext();
    auto gpu = device.createBuffer({16, BufferUsage::CopyDestination | BufferUsage::CopySource});
    auto readback = device.createBuffer({16, BufferUsage::CopyDestination, MemoryType::Readback, ResourceState::CopyDestination});
    std::array<std::uint32_t, 4> values{1, 2, 3, 4};
    commands->begin();
    commands->uploadBuffer(*gpu, 0, std::as_bytes(std::span{values}), ResourceState::CopySource);
    commands->copyBuffer(*readback, 0, *gpu, 0, 16);
    commands->submitAndWait();
    std::array<std::uint32_t, 4> actual{};
    readback->read(0, std::as_writable_bytes(std::span{actual}));
    REQUIRE(actual == values);
    const std::array<std::uint32_t, 1> update{99};
    commands->begin();
    commands->uploadBuffer(*gpu, 4, std::as_bytes(std::span{update}), ResourceState::CopySource);
    commands->copyBuffer(*readback, 0, *gpu, 0, 16);
    commands->submitAndWait();
    readback->read(0, std::as_writable_bytes(std::span{actual}));
    REQUIRE(actual == std::array<std::uint32_t, 4>{1, 99, 3, 4});
    fixture.checkMessages();
}

TEST_CASE("RHI commands reject invalid recording ownership and resource operations", "[gpu][rhi]")
{
    using namespace Skyline::RHI;
    Fixture fixture;
    auto& device = *fixture.device;
    auto commands = device.createCommandContext();
    REQUIRE_THROWS_AS(commands->dispatch(1), std::logic_error);
    REQUIRE_THROWS_AS(commands->submitAndWait(), std::logic_error);
    commands->begin();
    REQUIRE_THROWS_AS(commands->begin(), std::logic_error);
    REQUIRE_THROWS_AS(commands->dispatch(1), std::logic_error);
    REQUIRE_THROWS_AS(commands->draw(3), std::logic_error);
    REQUIRE_THROWS_AS(commands->setViewport({0, 0, 0, 1}, {0, 0, 1, 1}), std::invalid_argument);
    REQUIRE_THROWS_AS(commands->setPrimitiveTopology(static_cast<PrimitiveType>(999)), std::invalid_argument);
    auto other = Skyline::D3D12::createDevice(*fixture.native.Get());
    auto foreign = other->createBuffer({16, BufferUsage::CopyDestination});
    REQUIRE_THROWS_AS(commands->transition(*foreign, ResourceState::CopyDestination), std::invalid_argument);
    auto buffer = device.createBuffer({16, BufferUsage::CopySource | BufferUsage::CopyDestination});
    REQUIRE_THROWS_AS(commands->setVertexBuffer(*buffer, 4), std::invalid_argument);
    REQUIRE_THROWS_AS(commands->setIndexBuffer(*buffer, IndexType::Uint16), std::invalid_argument);
    REQUIRE_THROWS_AS(commands->transition(*buffer, ResourceState::Storage), std::invalid_argument);
    REQUIRE_THROWS_AS(commands->storageBarrier(*buffer), std::logic_error);
    std::array<std::byte, 4> data{};
    REQUIRE_THROWS_AS(commands->uploadBuffer(*buffer, 13, data, ResourceState::CopySource), std::out_of_range);
    auto texture = device.createTexture({2, 2});
    REQUIRE_THROWS_AS(commands->uploadTexture(*texture, data, 8), std::invalid_argument);
    REQUIRE_THROWS_AS(commands->transition(*texture, ResourceState::RenderTarget), std::invalid_argument);
    commands->submitAndWait();
    fixture.checkMessages();
}

TEST_CASE("RHI texture factories validate dimensions formats views and ownership", "[gpu][rhi]")
{
    using namespace Skyline::RHI;
    Fixture fixture;
    auto& device = *fixture.device;
    REQUIRE_THROWS_AS(device.createTexture({}), std::invalid_argument);
    REQUIRE_THROWS_AS(device.createTexture({16385, 1}), std::invalid_argument);
    REQUIRE_THROWS_AS(device.createTexture({2, 2, Format::Unknown}), std::invalid_argument);
    REQUIRE_THROWS_AS(device.createTexture({2, 2, Format::D32Float}), std::invalid_argument);
    REQUIRE_THROWS_AS(device.createTexture({2, 2, Format::Rgba8Unorm, static_cast<TextureUsage>(128)}), std::invalid_argument);
    auto texture = device.createTexture({2, 2});
    REQUIRE_NOTHROW(device.createTextureView(texture));
    REQUIRE_THROWS_AS(device.createTextureView(texture, {BindingType::StorageResource}), std::invalid_argument);
    REQUIRE_THROWS_AS(device.createTextureView(texture, {BindingType::ConstantBuffer}), std::invalid_argument);
    auto other = Skyline::D3D12::createDevice(*fixture.native.Get());
    REQUIRE_THROWS_AS(other->createTextureView(texture), std::invalid_argument);
    auto storage = device.createTexture({2, 2, Format::Rgba8Unorm, TextureUsage::Storage, ResourceState::Storage});
    REQUIRE_NOTHROW(device.createTextureView(storage, {BindingType::StorageResource}));
    auto depth = device.createTexture({2, 2, Format::D32Float, TextureUsage::DepthStencil, ResourceState::DepthWrite});
    REQUIRE_THROWS_AS(device.createTextureView(depth), std::invalid_argument);
    fixture.checkMessages();
}
