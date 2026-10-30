#pragma once
#include "skyline_api.h"

#include "unorder_access_resource.h"

class VertexBuffer;
class IndexBuffer;

class SKYLINE_API RWStructuredBuffer : public IUnorderAccessResource
{
  public:
    RWStructuredBuffer() = default;

    ~RWStructuredBuffer() override;

    RWStructuredBuffer(const RWStructuredBuffer&) = delete;
    RWStructuredBuffer& operator=(const RWStructuredBuffer&) = delete;

    void Init(int sizeOfElement, int numElement, void* initData);

    [[deprecated("Vertex-buffer aliasing is unsupported after the GPU-only UAV refactor. Use Init(sizeOfElement, numElement, nullptr) instead.")]]
    void Init(const VertexBuffer& vb, bool isUpdateByCPU);

    [[deprecated("Index-buffer aliasing is unsupported after the GPU-only UAV refactor. Use Init(sizeOfElement, numElement, nullptr) instead.")]]
    void Init(const IndexBuffer& ib, bool isUpdateByCPU);

    void RegisterUnorderAccessView(D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle, int bufferNo) override;

    void registerShaderResourceView(D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle, int bufferNo) override;

    bool IsInited() const
    {
        return m_isInited;
    }

    [[deprecated("CPU mapping is unavailable after the GPU-only UAV refactor.")]]
    void* GetResourceOnCPU();

    ID3D12Resource* getD3DResource();

    [[deprecated("Misspelled legacy API. Use getD3DResource() instead.")]]
    ID3D12Resource* GetD3DResoruce();

  private:
    ID3D12Resource* m_buffersOnGPU[2] = {nullptr};
    void* m_buffersOnCPU[2] = {nullptr};
    int m_numElement = 0;                
    int m_sizeOfElement = 0;             
    bool m_isInited = false;
};
