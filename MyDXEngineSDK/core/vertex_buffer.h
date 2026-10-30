#pragma once
#include "skyline_api.h"
#include <directx/d3d12.h>
#include <wrl/client.h>

class SKYLINE_API VertexBuffer
{
public:
    void init(int count, int stride);
    void copy(const void* srcVertices);

    [[nodiscard]]
    const D3D12_VERTEX_BUFFER_VIEW& getView() const 
    {
        return m_vertexBufferView;
    }

    [[nodiscard]]
    UINT getStrideInBytes() const
    {
        return m_vertexBufferView.StrideInBytes;
    }

    [[nodiscard]]
    UINT getSizeInBytes() const {
        return m_vertexBufferView.SizeInBytes;
    }

    [[nodiscard]]
    ID3D12Resource* getID3DResourceAddress() const {
        return m_vertexBuffer.Get();
    }

  private:
    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    int m_sizeInBytes = 0;
};


