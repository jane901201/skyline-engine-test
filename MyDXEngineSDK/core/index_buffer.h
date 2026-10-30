#pragma once
#include "skyline_api.h"
#include <cstdint>
#include <directx/d3d12.h>
#include <wrl/client.h>

class SKYLINE_API IndexBuffer
{
public:
    void init(int count, int stride);

    void copy(const std::uint16_t* srcIndices);

    void copy(const std::uint32_t* srcIndices);

    [[nodiscard]]
    const D3D12_INDEX_BUFFER_VIEW& getView() const
    {
        return m_indexBufferView;
    }

    [[nodiscard]]
    UINT getCount() const
    {
        return static_cast<UINT>(m_count);
    }
private:
    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer = nullptr;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView{};
    int m_count = 0;
    int m_strideInBytes = 0;
    int m_sizeInBytes = 0;
};
