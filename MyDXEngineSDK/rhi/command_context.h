#pragma once

#include "resource.h"
#include "pipeline.h"
#include <array>

namespace Skyline::RHI
{
enum class IndexType { Uint16, Uint32 };
struct Viewport { float x = 0, y = 0, width = 0, height = 0, minDepth = 0, maxDepth = 1; };
struct Scissor { std::int32_t left = 0, top = 0, right = 0, bottom = 0; };

class SKYLINE_API CommandContext : public DeviceObject
{
public:
    ~CommandContext() override;
    virtual void begin() = 0;
    // 提交後等待完成才回收暫存資源；初期維持同步提交的既有行為。
    virtual void submitAndWait() = 0;
    virtual void setPipeline(const Pipeline& pipeline) = 0;
    virtual void setBindingSet(const BindingSet& set) = 0;
    virtual void setVertexBuffer(const Buffer& buffer, std::uint32_t stride, std::uint64_t offset = 0) = 0;
    virtual void setIndexBuffer(const Buffer& buffer, IndexType type, std::uint64_t offset = 0) = 0;
    virtual void setPrimitiveTopology(PrimitiveType primitive) = 0;
    virtual void setViewport(const Viewport& viewport, const Scissor& scissor) = 0;
    virtual void setRenderTarget(const Texture& color, const Texture* depth = nullptr) = 0;
    virtual void clearColor(const Texture& color, const std::array<float, 4>& value) = 0;
    virtual void clearDepth(const Texture& depth, float value = 1) = 0;
    virtual void transition(const Buffer& buffer, ResourceState after) = 0;
    virtual void transition(const Texture& texture, ResourceState after) = 0;
    virtual void storageBarrier(const Buffer& buffer) = 0;
    virtual void storageBarrier(const Texture& texture) = 0;
    virtual void uploadBuffer(const Buffer& buffer, std::uint64_t offset, std::span<const std::byte> data, ResourceState after) = 0;
    virtual void uploadTexture(const Texture& texture, std::span<const std::byte> data, std::uint32_t rowPitch, ResourceState after = ResourceState::ShaderRead) = 0;
    virtual void copyBuffer(const Buffer& destination, std::uint64_t destinationOffset,
        const Buffer& source, std::uint64_t sourceOffset, std::uint64_t size) = 0;
    // 僅限獨立 Context：提交目前命令並等待完成，回傳緊密排列的像素。
    virtual std::vector<std::byte> readTexture(const Texture& texture) = 0;
    virtual void draw(std::uint32_t vertexCount, std::uint32_t instanceCount = 1, std::uint32_t firstVertex = 0) = 0;
    virtual void drawIndexed(std::uint32_t indexCount, std::uint32_t instanceCount = 1, std::uint32_t firstIndex = 0, std::int32_t vertexOffset = 0) = 0;
    virtual void dispatch(std::uint32_t x, std::uint32_t y = 1, std::uint32_t z = 1) = 0;
protected:
    explicit CommandContext(const Device& device) noexcept : DeviceObject(device) {}
};
}
