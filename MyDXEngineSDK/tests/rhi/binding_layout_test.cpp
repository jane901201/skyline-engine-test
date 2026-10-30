#include <catch2/catch_test_macros.hpp>
#include "backends/d3d12/d3d12_binding.h"
#include <limits>
#include <stdexcept>

TEST_CASE("RHI binding validation accepts empty scene and separate register spaces", "[rhi]")
{
    using namespace Skyline;
    REQUIRE_NOTHROW(D3D12::validateBindingLayout({}));
    REQUIRE_NOTHROW(D3D12::validateBindingLayout(RHI::BindingLayoutDesc::scene()));
    RHI::BindingLayoutDesc desc;
    desc.constantBuffers = {{0, 0}, {0, 1}};
    desc.resources = {{RHI::BindingType::SampledResource, 0, 2, 0}, {RHI::BindingType::SampledResource, 0, 2, 1}};
    REQUIRE_NOTHROW(D3D12::validateBindingLayout(desc));
}

TEST_CASE("RHI binding validation rejects ambiguous and overflowing ranges", "[rhi]")
{
    using namespace Skyline;
    auto desc = RHI::BindingLayoutDesc::scene();
    SECTION("duplicate constants") { desc.constantBuffers.push_back({0}); }
    SECTION("overlapping root and table constants") { desc.resources[0].shaderRegister = 3; }
    SECTION("overlapping resource ranges") { desc.resources.push_back({RHI::BindingType::SampledResource, 4, 1}); }
    SECTION("zero count") { desc.resources[1].count = 0; }
    SECTION("register overflow") { desc.resources[1].shaderRegister = (std::numeric_limits<std::uint32_t>::max)(); }
    SECTION("unbounded range") { desc.resources[1].count = (std::numeric_limits<std::uint32_t>::max)(); }
    SECTION("duplicate samplers") { desc.samplers.push_back({}); }
    SECTION("invalid resource type") { desc.resources[0].type = static_cast<RHI::BindingType>(999); }
    SECTION("invalid visibility") { desc.constantBuffers[0].visibility = static_cast<RHI::ShaderVisibility>(999); }
    SECTION("invalid sampler filter") { desc.samplers[0].filter = static_cast<RHI::SamplerFilter>(999); }
    REQUIRE_THROWS_AS(D3D12::validateBindingLayout(desc), std::invalid_argument);
}

TEST_CASE("RHI binding budget counts unique root constants and the resource table", "[rhi]")
{
    using namespace Skyline;
    RHI::BindingLayoutDesc desc;
    SECTION("without a table")
    {
        for (std::uint32_t index = 0; index < 32; ++index)
            desc.constantBuffers.push_back({index});
    }
    SECTION("with a table")
    {
        for (std::uint32_t index = 0; index < 31; ++index)
            desc.constantBuffers.push_back({index});
        desc.resources = {{RHI::BindingType::SampledResource, 0, 1}};
    }
    REQUIRE_NOTHROW(D3D12::validateBindingLayout(desc));
    REQUIRE_NOTHROW(D3D12::serializeBindingLayout(desc));
    desc.constantBuffers.push_back({static_cast<std::uint32_t>(desc.constantBuffers.size())});
    REQUIRE_THROWS_AS(D3D12::validateBindingLayout(desc), std::invalid_argument);
    REQUIRE_THROWS_AS(D3D12::serializeBindingLayout(desc), std::invalid_argument);
}
