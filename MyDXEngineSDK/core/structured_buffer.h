#pragma once
#include "skyline_api.h"

#include "shader_resource.h"

class SKYLINE_API StructuredBuffer final : public IShaderResource
{
  public:
    ~StructuredBuffer() override;

    void init(int sizeOfElement, int numElement, void* initData = nullptr);
    void registerShaderResourceView(D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle, int bufferNo) override;
    void update(void* data);
    bool isInited() const
    {
        return m_isInited;
    }
    ID3D12Resource* getD3DResource();

  private:
    ID3D12Resource* m_buffersOnGPU[2] = {nullptr, nullptr};
    void* m_buffersOnCPU[2] = {nullptr, nullptr};
    int m_numElement = 0;
    int m_sizeOfElement = 0;
    bool m_isInited = false;
};
