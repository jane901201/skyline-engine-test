#include <catch2/catch_test_macros.hpp>
#include "backends/d3d12/d3d12_device.h"
#include "backends/d3d12/d3d12_interop.h"
#include <wrl/client.h>
#include <array>
#include <limits>
#include <stdexcept>

namespace
{
std::unique_ptr<Skyline::RHI::Device> createDevice()
{
    Microsoft::WRL::ComPtr<ID3D12Device> native;
    REQUIRE(SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&native))));
    return Skyline::D3D12::createDevice(*native.Get());
}
}

TEST_CASE("RHI buffers reject invalid memory usage and state combinations", "[gpu][rhi]")
{
    using namespace Skyline::RHI;
    auto device = createDevice();
    BufferDesc desc{64, BufferUsage::Sampled, MemoryType::Gpu, ResourceState::ShaderRead};
    SECTION("zero size") { desc.size = 0; }
    SECTION("size overflow") { desc.size = (std::numeric_limits<std::uint64_t>::max)(); }
    SECTION("no usage") { desc.usage = BufferUsage::None; }
    SECTION("unknown usage") { desc.usage = static_cast<BufferUsage>(128); }
    SECTION("unknown memory") { desc.memory = static_cast<MemoryType>(999); }
    SECTION("unknown state") { desc.initialState = static_cast<ResourceState>(999); }
    SECTION("upload state") { desc.memory = MemoryType::Upload; }
    SECTION("storage requires usage") { desc.initialState = ResourceState::Storage; }
    SECTION("storage rejects CPU memory")
    {
        desc.memory = MemoryType::Upload;
        desc.usage = BufferUsage::Storage;
        desc.initialState = ResourceState::GenericRead;
    }
    SECTION("readback rejects shader usage")
    {
        desc.memory = MemoryType::Readback;
        desc.initialState = ResourceState::CopyDestination;
        desc.usage = BufferUsage::CopyDestination | BufferUsage::Sampled;
    }
    REQUIRE_THROWS_AS(device->createBuffer(desc), std::invalid_argument);
}

TEST_CASE("RHI buffer access checks logical size and memory direction", "[gpu][rhi]")
{
    using namespace Skyline::RHI;
    auto device = createDevice();
    auto upload = device->createBuffer({144, BufferUsage::Constant, MemoryType::Upload, ResourceState::GenericRead});
    REQUIRE(upload->description().size == 144);
    REQUIRE(Skyline::D3D12::nativeBuffer(*upload)->GetDesc().Width == 256);
    std::array<std::byte, 4> data{};
    REQUIRE_NOTHROW(upload->write(140, data));
    REQUIRE_NOTHROW(upload->write(144, {}));
    REQUIRE_THROWS_AS(upload->write(141, data), std::out_of_range);
    REQUIRE_THROWS_AS(upload->write(256, {}), std::out_of_range);
    REQUIRE_THROWS_AS(upload->read(0, data), std::logic_error);
    auto gpu = device->createBuffer({64, BufferUsage::Storage, MemoryType::Gpu, ResourceState::Storage});
    REQUIRE_THROWS_AS(gpu->write(0, data), std::logic_error);
    REQUIRE_THROWS_AS(gpu->read(0, data), std::logic_error);
    auto readback = device->createBuffer({64, BufferUsage::CopyDestination, MemoryType::Readback, ResourceState::CopyDestination});
    REQUIRE_THROWS_AS(readback->write(0, data), std::logic_error);
    REQUIRE_THROWS_AS(readback->read(61, data), std::out_of_range);
}

TEST_CASE("RHI buffer views validate ownership alignment stride and usage", "[gpu][rhi]")
{
    using namespace Skyline::RHI;
    auto device = createDevice();
    auto other = createDevice();
    auto gpu = device->createBuffer({256, BufferUsage::Sampled | BufferUsage::Storage, MemoryType::Gpu, ResourceState::Storage});
    BufferViewDesc desc{BindingType::SampledResource, 16, 64, 16};
    REQUIRE_NOTHROW(device->createBufferView(gpu, desc));
    REQUIRE_THROWS_AS(other->createBufferView(gpu, desc), std::invalid_argument);
    REQUIRE_THROWS_AS(device->createBufferView(nullptr, desc), std::invalid_argument);
    SECTION("zero stride") { desc.stride = 0; }
    SECTION("unaligned stride") { desc.stride = 3; }
    SECTION("oversized stride") { desc.stride = 2052; }
    SECTION("partial element size") { desc.size = 63; }
    SECTION("unaligned offset") { desc.offset = 4; }
    SECTION("empty view") { desc.size = 0; }
    SECTION("invalid type") { desc.type = static_cast<BindingType>(999); }
    SECTION("constant usage absent") { desc = {BindingType::ConstantBuffer, 0, 64}; }
    REQUIRE_THROWS_AS(device->createBufferView(gpu, desc), std::invalid_argument);
    REQUIRE_THROWS_AS(device->createBufferView(gpu, {BindingType::SampledResource, 256, 16, 16}), std::out_of_range);
}

TEST_CASE("RHI constant views support padded tails and reject misaligned subranges", "[gpu][rhi]")
{
    using namespace Skyline::RHI;
    auto device = createDevice();
    auto buffer = device->createBuffer({400, BufferUsage::Constant, MemoryType::Upload, ResourceState::GenericRead});
    REQUIRE_NOTHROW(device->createBufferView(buffer, {BindingType::ConstantBuffer, 256, 144}));
    REQUIRE_THROWS_AS(device->createBufferView(buffer, {BindingType::ConstantBuffer, 4, 144}), std::invalid_argument);
    REQUIRE_THROWS_AS(device->createBufferView(buffer, {BindingType::ConstantBuffer, 0, 144, 4}), std::invalid_argument);
    auto large = device->createBuffer({65537, BufferUsage::Constant, MemoryType::Upload, ResourceState::GenericRead});
    REQUIRE_NOTHROW(device->createBufferView(large, {BindingType::ConstantBuffer, 0, 65536}));
    REQUIRE_THROWS_AS(device->createBufferView(large, {BindingType::ConstantBuffer, 0, 65537}), std::invalid_argument);
}

TEST_CASE("RHI binding sets require complete unique compatible bindings", "[gpu][rhi]")
{
    using namespace Skyline::RHI;
    auto device = createDevice();
    auto other = createDevice();
    BindingLayoutDesc layoutDesc;
    layoutDesc.constantBuffers = {{2, 3}};
    layoutDesc.resources = {{BindingType::SampledResource, 4, 2, 5}};
    auto layout = device->createBindingLayout(layoutDesc);
    auto constants = device->createBuffer({16, BufferUsage::Constant, MemoryType::Upload, ResourceState::GenericRead});
    auto buffer = device->createBuffer({64, BufferUsage::Sampled | BufferUsage::Storage, MemoryType::Gpu, ResourceState::ShaderRead});
    auto cbv = device->createBufferView(constants, {BindingType::ConstantBuffer, 0, 16});
    auto srv = device->createBufferView(buffer, {BindingType::SampledResource, 0, 64, 4});
    BindingSetDesc desc{layout.get(), {{2, 3, cbv}, {4, 5, srv}, {5, 5, srv}}};
    REQUIRE_NOTHROW(device->createBindingSet(desc));
    REQUIRE_THROWS_AS(other->createBindingSet(desc), std::invalid_argument);
    SECTION("missing layout") { desc.layout = nullptr; }
    SECTION("missing array element") { desc.buffers.pop_back(); }
    SECTION("duplicate binding") { desc.buffers.push_back(desc.buffers[0]); }
    SECTION("wrong space") { desc.buffers[0].registerSpace = 0; }
    SECTION("extra binding") { desc.buffers.push_back({8, 5, srv}); }
    SECTION("null view") { desc.buffers[0].view.reset(); }
    SECTION("wrong view type")
    {
        desc.buffers[1].view = device->createBufferView(buffer, {BindingType::StorageResource, 0, 64, 4});
    }
    SECTION("foreign view")
    {
        auto foreign = other->createBuffer({16, BufferUsage::Constant, MemoryType::Upload, ResourceState::GenericRead});
        desc.buffers[0].view = other->createBufferView(foreign, {BindingType::ConstantBuffer, 0, 16});
    }
    REQUIRE_THROWS_AS(device->createBindingSet(desc), std::invalid_argument);
}

TEST_CASE("RHI binding sets retain views and buffers until the set is released", "[gpu][rhi]")
{
    using namespace Skyline::RHI;
    auto device = createDevice();
    BindingLayoutDesc definition;
    definition.constantBuffers = {{0}};
    auto layout = device->createBindingLayout(definition);
    auto buffer = device->createBuffer({16, BufferUsage::Constant, MemoryType::Upload, ResourceState::GenericRead});
    auto view = device->createBufferView(buffer, {BindingType::ConstantBuffer, 0, 16});
    std::weak_ptr<Buffer> bufferLifetime = buffer;
    std::weak_ptr<BufferView> viewLifetime = view;
    auto set = device->createBindingSet({layout.get(), {{0, 0, view}}});
    layout.reset();
    buffer.reset();
    view.reset();
    REQUIRE_FALSE(bufferLifetime.expired());
    REQUIRE_FALSE(viewLifetime.expired());
    set.reset();
    REQUIRE(bufferLifetime.expired());
    REQUIRE(viewLifetime.expired());
}
