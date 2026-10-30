#pragma once
#include "skyline_api.h"

#include <array>
#include <cstdint>
#include <directx/d3d12.h>
#include <wrl/client.h>

class ConstantBuffer;
class IShaderResource;
class IUnorderAccessResource;
class RenderContext;

enum class ShaderStage
{
    Graphics,
    Compute
};

class SKYLINE_API RenderState
{
  public:
    static constexpr UINT FrameCount = 2;
    static constexpr UINT ConstantBufferCount = 4;
    static constexpr UINT ShaderResourceCount = 32;
    static constexpr UINT UnorderedAccessCount = 8;
    static constexpr UINT ResourceTableRootParameter = 4;

    void init(ID3D12Device* device);

    void setStructuredBuffer(UINT slot, IShaderResource& buffer);
    void setRWStructuredBuffer(
        UINT shaderResourceSlot,
        UINT unorderedAccessSlot,
        IUnorderAccessResource& buffer);
    void clearStructuredBuffer(UINT slot);
    void clearRWStructuredBuffer(UINT shaderResourceSlot, UINT unorderedAccessSlot);
    void clearStructuredBuffers();

    void setConstantBuffer(
        RenderContext& renderContext,
        ShaderStage stage,
        UINT slot,
        ConstantBuffer& buffer) const;
    void applyGraphics(RenderContext& renderContext, UINT frameIndex) const;
    void applyCompute(RenderContext& renderContext, UINT frameIndex) const;

  private:
    static constexpr UINT ShaderResourceOffset = ConstantBufferCount;
    static constexpr UINT UnorderedAccessOffset = ShaderResourceOffset + ShaderResourceCount;
    static constexpr UINT DescriptorCount = UnorderedAccessOffset + UnorderedAccessCount;

    D3D12_CPU_DESCRIPTOR_HANDLE getCpuHandle(UINT frameIndex, UINT descriptorIndex) const;
    D3D12_GPU_DESCRIPTOR_HANDLE getGpuTableStart(UINT frameIndex) const;
    void writeNullConstantBuffer(UINT frameIndex, UINT slot);
    void writeNullShaderResource(UINT frameIndex, UINT slot);
    void writeNullUnorderedAccess(UINT frameIndex, UINT slot);
    void requireInitialized() const;

    ID3D12Device* m_device = nullptr;
    std::array<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>, FrameCount> m_descriptorHeaps;
    UINT m_descriptorSize = 0;
    bool m_initialized = false;
};
