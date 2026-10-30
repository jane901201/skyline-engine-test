#include <cstring>              
#include <directx/d3dx12_core.h> 
#include <limits>                
#include <stdexcept>             

#include "graphics_engine.h"
#include "vertex_buffer.h"

    // 建立指定頂點數量與單一頂點大小的 Upload Heap Vertex Buffer
    void VertexBuffer::init(int count, int stride)
{
    // 確認頂點數量與單一頂點大小都大於 0
    if (count <= 0 || stride <= 0)
        throw std::invalid_argument("VertexBuffer: Count and stride must be positive.");

    // 確認 count * stride 不會超出 int 可表示的範圍
    if (count > std::numeric_limits<int>::max() / stride)
        throw std::overflow_error("VertexBuffer: Buffer size exceeds the supported range.");

    // 確認 GraphicsEngine 與 Direct3D 12 Device 已完成初始化
    if (g_graphicsEngine == nullptr || g_graphicsEngine->getD3DDevice() == nullptr)
        throw std::runtime_error("VertexBuffer: Graphics device is not initialized.");

    // 計算 Vertex Buffer 需要的總 Byte 數
    m_sizeInBytes = count * stride;

    // 建立 Upload Heap 的 Heap Properties
    const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    // 建立指定大小的 Buffer Resource 描述
    const auto resourceDescription = CD3DX12_RESOURCE_DESC::Buffer(m_sizeInBytes);

    // 建立實際的 Direct3D 12 Vertex Buffer Resource
    const HRESULT result = g_graphicsEngine->getD3DDevice()->CreateCommittedResource(
        &heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDescription, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&m_vertexBuffer));

    // 如果 Vertex Buffer Resource 建立失敗就丟出例外
    if (FAILED(result))
        throw std::runtime_error("VertexBuffer: Failed to create the Direct3D resource.");

    // 設定 Vertex Buffer 在 GPU 記憶體中的起始位址
    m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();

    // 設定 Vertex Buffer 的總 Byte 數
    m_vertexBufferView.SizeInBytes = static_cast<UINT>(m_sizeInBytes);

    // 設定每一個 Vertex 的 Byte 大小
    m_vertexBufferView.StrideInBytes = static_cast<UINT>(stride);
}

// 將 CPU 端的 Vertex 資料複製到 Vertex Buffer
void VertexBuffer::copy(const void* srcVertices)
{
    // 確認 Vertex Buffer 已經先完成初始化
    if (m_vertexBuffer == nullptr)
        throw std::logic_error("VertexBuffer: init must be called before copy.");

    // 確認來源 Vertex 資料不是空指標
    if (srcVertices == nullptr)
        throw std::invalid_argument("VertexBuffer: Source vertices must not be null.");

    // 保存 Map 後取得的 CPU 可寫入記憶體位址
    void* destination = nullptr;

    // 表示 CPU 不會讀取這塊 GPU Resource 的任何範圍
    const D3D12_RANGE readRange{0, 0};

    // 將 Vertex Buffer 映射成 CPU 可以存取的記憶體
    const HRESULT result = m_vertexBuffer->Map(0, &readRange, &destination);

    // 如果 Map 失敗就丟出例外
    if (FAILED(result))
        throw std::runtime_error("VertexBuffer: Failed to map the Direct3D resource.");

    // 將 CPU 端 Vertex 資料複製到 Vertex Buffer
    std::memcpy(destination, srcVertices, static_cast<std::size_t>(m_sizeInBytes));

    // 結束 CPU 對 Vertex Buffer 的映射
    m_vertexBuffer->Unmap(0, nullptr);
}
