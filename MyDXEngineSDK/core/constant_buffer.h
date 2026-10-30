#pragma once
#include "skyline_api.h"
#include <directx/d3d12.h>
#include <utility>

class SKYLINE_API ConstantBuffer
{
public:
    ConstantBuffer() = default;
    ~ConstantBuffer();
    ConstantBuffer(const ConstantBuffer&) = delete;
    ConstantBuffer& operator=(const ConstantBuffer&) = delete;

    ConstantBuffer(ConstantBuffer&& r)
    {
        m_constantBuffer[0] = r.m_constantBuffer[0];
        m_constantBuffer[1] = r.m_constantBuffer[1];
        m_constBufferCPU[0] = r.m_constBufferCPU[0];
        m_constBufferCPU[1] = r.m_constBufferCPU[1];
        m_size = r.m_size;
        m_allocSize = r.m_allocSize;
        m_isValid = r.m_isValid;

        r.m_constantBuffer[0] = nullptr;
        r.m_constantBuffer[1] = nullptr;
        r.m_constBufferCPU[0] = nullptr;
        r.m_constBufferCPU[1] = nullptr;
        r.m_size = 0;
        r.m_allocSize = 0;
        r.m_isValid = false;
    }
    ConstantBuffer& operator=(ConstantBuffer&& r) noexcept;

    void init(int size, void* srcData = nullptr);

    bool isValid() const
    {
        return m_isValid;
    }

    void copyToVRAM(void* data);
    template<class T>
    void copyToVRAM(T& data)
    {
        copyToVRAM(&data);
    }

    void registerConstantBufferView(D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle);
    void registerConstantBufferView(D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle, int bufferNo);

    D3D12_GPU_VIRTUAL_ADDRESS getGPUVirtualAddress();


private:
    ID3D12Resource* m_constantBuffer[2] = { nullptr };
    void* m_constBufferCPU[2] = { nullptr };
    int m_size = 0;
    int m_allocSize = 0;
    bool m_isValid = false;
};


