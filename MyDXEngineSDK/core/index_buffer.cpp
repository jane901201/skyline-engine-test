#include "index_buffer.h"
#include "graphics_engine.h"
#include <limits>
#include <stdexcept>

    // 建立指定 Index 數量與 Index 大小的 Upload Heap Index Buffer
    void IndexBuffer::init(int count, int stride)
{
    // 確認 Index 數量大於 0 且單一 Index 大小只能是 2 或 4 Bytes
    if (count <= 0 || (stride != 2 && stride != 4))
        throw std::invalid_argument("IndexBuffer: Count must be positive and stride must be 2 or 4 bytes.");

    // 確認 count * stride 不會超出 int 可表示的範圍
    if (count > std::numeric_limits<int>::max() / stride)
        throw std::overflow_error("IndexBuffer: Buffer size exceeds the supported range.");

    // 確認 GraphicsEngine 與 Direct3D 12 Device 已完成初始化
    if (g_graphicsEngine == nullptr || g_graphicsEngine->getD3DDevice() == nullptr)
        throw std::runtime_error("IndexBuffer: Graphics device is not initialized.");

    // 保存 Index 的數量
    m_count = count;

    // 保存每個 Index 使用的 Byte 數
    m_strideInBytes = stride;

    // 計算整個 Index Buffer 的總 Byte 數
    m_sizeInBytes = count * stride;

    // 取得 Direct3D 12 Device
    auto d3dDevice = g_graphicsEngine->getD3DDevice();

    // 建立 Upload Heap 的 Heap Properties
    auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    // 建立指定大小的 Buffer Resource 描述
    auto rDesc = CD3DX12_RESOURCE_DESC::Buffer(m_sizeInBytes);

    // 建立實際的 Direct3D 12 Index Buffer Resource
    const HRESULT result =
        d3dDevice->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &rDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                           nullptr, IID_PPV_ARGS(&m_indexBuffer));

    // 如果 Index Buffer Resource 建立失敗就丟出例外
    if (FAILED(result))
        throw std::runtime_error("IndexBuffer: Failed to create the Direct3D resource.");

    // 設定 Index Buffer 在 GPU 記憶體中的起始位址
    m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();

    // 根據 Index 大小設定成 16 位元或 32 位元格式
    m_indexBufferView.Format = stride == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

    // 設定 Index Buffer 的總 Byte 數
    m_indexBufferView.SizeInBytes = m_sizeInBytes;
}

// 將 16 位元 Index 資料複製到 Index Buffer
void IndexBuffer::copy(const std::uint16_t* srcIndices)
{
    // 確認 Index Buffer 已初始化且格式為 16 位元
    if (m_indexBuffer == nullptr || m_strideInBytes != 2)
        throw std::logic_error("IndexBuffer: A 16-bit buffer must be initialized before copy.");

    // 確認來源 Index 資料不是空指標
    if (srcIndices == nullptr)
        throw std::invalid_argument("IndexBuffer: Source indices must not be null.");

    // 保存 Map 後取得的 16 位元 CPU 可寫入記憶體位址
    std::uint16_t* pData = nullptr;

    // 表示 CPU 不會讀取這塊 Resource 的任何範圍
    const D3D12_RANGE readRange{0, 0};

    // 將 Index Buffer 映射成 CPU 可以存取的記憶體
    const HRESULT result = m_indexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pData));

    // 如果 Map 失敗就丟出例外
    if (FAILED(result))
        throw std::runtime_error("IndexBuffer: Failed to map the Direct3D resource.");

    // 將所有 16 位元 Index 資料逐筆複製到 Buffer
    for (int i = 0; i < m_count; i++)
    {
        pData[i] = srcIndices[i];
    }

    // 結束 CPU 對 Index Buffer 的映射
    m_indexBuffer->Unmap(0, nullptr);
}

// 將 32 位元 Index 資料複製到 Index Buffer
void IndexBuffer::copy(const uint32_t* srcIndices)
{
    // 確認 Index Buffer 已初始化且格式為 32 位元
    if (m_indexBuffer == nullptr || m_strideInBytes != 4)
        throw std::logic_error("IndexBuffer: A 32-bit buffer must be initialized before copy.");

    // 確認來源 Index 資料不是空指標
    if (srcIndices == nullptr)
        throw std::invalid_argument("IndexBuffer: Source indices must not be null.");

    // 保存 Map 後取得的 32 位元 CPU 可寫入記憶體位址
    uint32_t* pData = nullptr;

    // 表示 CPU 不會讀取這塊 Resource 的任何範圍
    const D3D12_RANGE readRange{0, 0};

    // 將 Index Buffer 映射成 CPU 可以存取的記憶體
    const HRESULT result = m_indexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pData));

    // 如果 Map 失敗就丟出例外
    if (FAILED(result))
        throw std::runtime_error("IndexBuffer: Failed to map the Direct3D resource.");

    // 將所有 32 位元 Index 資料逐筆複製到 Buffer
    for (int i = 0; i < m_count; i++)
    {
        pData[i] = srcIndices[i];
    }

    // 結束 CPU 對 Index Buffer 的映射
    m_indexBuffer->Unmap(0, nullptr);
}
