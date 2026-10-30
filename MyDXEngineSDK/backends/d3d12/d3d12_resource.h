#pragma once

#include "rhi/device.h"
#include <directx/d3d12.h>
#include <wrl/client.h>

namespace Skyline::D3D12
{
class Pipeline;
class DescriptorView
{
public:
    virtual ~DescriptorView() = default;
    virtual RHI::BindingType type() const noexcept = 0;
    virtual void writeDescriptor(ID3D12Device& device, D3D12_CPU_DESCRIPTOR_HANDLE handle) const = 0;
};
class Buffer final : public RHI::Buffer, public std::enable_shared_from_this<Buffer>
{
public:
    Buffer(const RHI::Device& owner, ID3D12Device& device, RHI::BufferDesc desc);
    const RHI::BufferDesc& description() const noexcept override { return m_desc; }
    void write(std::uint64_t offset, std::span<const std::byte> data) override;
    void read(std::uint64_t offset, std::span<std::byte> data) const override;
    ID3D12Resource* native() const noexcept { return m_resource.Get(); }
    mutable RHI::ResourceState state;
private:
    RHI::BufferDesc m_desc;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_resource;
};

class BufferView final : public RHI::BufferView, public DescriptorView
{
public:
    BufferView(const RHI::Device& owner, std::shared_ptr<Buffer> buffer, RHI::BufferViewDesc desc);
    const RHI::BufferViewDesc& description() const noexcept override { return m_desc; }
    ID3D12Resource* resource() const noexcept { return m_buffer->native(); }
    RHI::BindingType type() const noexcept override { return m_desc.type; }
    void writeDescriptor(ID3D12Device& device, D3D12_CPU_DESCRIPTOR_HANDLE handle) const override;
    D3D12_GPU_VIRTUAL_ADDRESS address() const noexcept { return resource()->GetGPUVirtualAddress() + m_desc.offset; }
private:
    std::shared_ptr<Buffer> m_buffer;
    RHI::BufferViewDesc m_desc;
};

class Texture final : public RHI::Texture, public std::enable_shared_from_this<Texture>
{
public:
    Texture(const RHI::Device& owner, ID3D12Device& device, RHI::TextureDesc desc);
    const RHI::TextureDesc& description() const noexcept override { return m_desc; }
    ID3D12Resource* native() const noexcept { return m_resource.Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE attachment() const;
    mutable RHI::ResourceState state;
private:
    RHI::TextureDesc m_desc;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_resource;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_attachment;
};
class TextureView final : public RHI::TextureView, public DescriptorView
{
public:
    TextureView(const RHI::Device& owner, std::shared_ptr<Texture> texture, RHI::TextureViewDesc desc);
    const RHI::TextureViewDesc& description() const noexcept override { return m_desc; }
    RHI::BindingType type() const noexcept override { return m_desc.type; }
    void writeDescriptor(ID3D12Device& device, D3D12_CPU_DESCRIPTOR_HANDLE handle) const override;
private:
    std::shared_ptr<Texture> m_texture;
    RHI::TextureViewDesc m_desc;
};

class BindingSet final : public RHI::BindingSet
{
public:
    BindingSet(const RHI::Device& owner, ID3D12Device& device, const RHI::BindingSetDesc& desc);
    void apply(ID3D12GraphicsCommandList& commands, const Pipeline& pipeline) const;
    void apply(ID3D12GraphicsCommandList& commands, ID3D12RootSignature* layout, RHI::PipelineKind kind) const;
    void retain(std::vector<Microsoft::WRL::ComPtr<IUnknown>>& natives,
                std::vector<std::shared_ptr<const RHI::DeviceObject>>& objects) const;
private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_layout;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_heap;
    std::vector<std::shared_ptr<RHI::DeviceObject>> m_views;
    std::vector<D3D12_GPU_VIRTUAL_ADDRESS> m_constants;
};

D3D12_RESOURCE_STATES resourceState(RHI::ResourceState state);
void validateBufferState(const RHI::BufferDesc& desc, RHI::ResourceState state);
void validateTextureState(const RHI::TextureDesc& desc, RHI::ResourceState state);
std::uint32_t pixelSize(RHI::Format format);
}
