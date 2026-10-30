#pragma once

#include "binding.h"
#include "types.h"
#include <cstddef>
#include <memory>
#include <span>

namespace Skyline::RHI
{
enum class MemoryType { Gpu, Upload, Readback };
enum class ResourceState { Common, ShaderRead, Storage, CopySource, CopyDestination, GenericRead, Vertex, Index, RenderTarget, DepthWrite };
enum class BufferUsage : std::uint32_t
{
    None = 0, Constant = 1, Sampled = 2, Storage = 4, Vertex = 8, Index = 16,
    CopySource = 32, CopyDestination = 64
};
constexpr BufferUsage operator|(BufferUsage left, BufferUsage right) noexcept
{
    return static_cast<BufferUsage>(static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
}
constexpr bool hasUsage(BufferUsage value, BufferUsage flag) noexcept
{
    return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(flag)) != 0;
}

struct BufferDesc
{
    std::uint64_t size = 0;
    BufferUsage usage = BufferUsage::None;
    MemoryType memory = MemoryType::Gpu;
    ResourceState initialState = ResourceState::Common;
    std::string debugName = "Buffer";
};

class SKYLINE_API Buffer : public DeviceObject
{
public:
    ~Buffer() override;
    [[nodiscard]] virtual const BufferDesc& description() const noexcept = 0;
    // 呼叫端必須在寫入前確認 GPU 已完成上一輪使用；讀回前亦須等待提交完成。
    virtual void write(std::uint64_t offset, std::span<const std::byte> data) = 0;
    virtual void read(std::uint64_t offset, std::span<std::byte> data) const = 0;
protected:
    explicit Buffer(const Device& device) noexcept : DeviceObject(device) {}
};

struct BufferViewDesc
{
    BindingType type = BindingType::SampledResource;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
    std::uint32_t stride = 0;
};

class SKYLINE_API BufferView : public DeviceObject
{
public:
    ~BufferView() override;
    [[nodiscard]] virtual const BufferViewDesc& description() const noexcept = 0;
protected:
    explicit BufferView(const Device& device) noexcept : DeviceObject(device) {}
};

enum class TextureUsage : std::uint32_t { Sampled = 1, Storage = 2, RenderTarget = 4, DepthStencil = 8, CopySource = 16, CopyDestination = 32 };
constexpr TextureUsage operator|(TextureUsage left, TextureUsage right) noexcept
{
    return static_cast<TextureUsage>(static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
}
constexpr bool hasUsage(TextureUsage value, TextureUsage flag) noexcept
{
    return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(flag)) != 0;
}
struct TextureDesc
{
    std::uint32_t width = 0, height = 0;
    Format format = Format::Rgba8Unorm;
    TextureUsage usage = TextureUsage::Sampled | TextureUsage::CopyDestination;
    ResourceState initialState = ResourceState::CopyDestination;
    std::string debugName = "Texture";
};
class SKYLINE_API Texture : public DeviceObject
{
public:
    ~Texture() override;
    [[nodiscard]] virtual const TextureDesc& description() const noexcept = 0;
protected:
    explicit Texture(const Device& device) noexcept : DeviceObject(device) {}
};
struct TextureViewDesc { BindingType type = BindingType::SampledResource; };
class SKYLINE_API TextureView : public DeviceObject
{
public:
    ~TextureView() override;
    [[nodiscard]] virtual const TextureViewDesc& description() const noexcept = 0;
protected:
    explicit TextureView(const Device& device) noexcept : DeviceObject(device) {}
};
struct TextureBinding
{
    std::uint32_t shaderRegister = 0, registerSpace = 0;
    std::shared_ptr<TextureView> view;
};

struct BufferBinding
{
    std::uint32_t shaderRegister = 0;
    std::uint32_t registerSpace = 0;
    std::shared_ptr<BufferView> view;
};

struct BindingSetDesc
{
    const BindingLayout* layout = nullptr;
    std::vector<BufferBinding> buffers;
    std::string debugName = "Binding set";
    std::vector<TextureBinding> textures;
};

class SKYLINE_API BindingSet : public DeviceObject
{
public:
    ~BindingSet() override;
protected:
    explicit BindingSet(const Device& device) noexcept : DeviceObject(device) {}
};
}
