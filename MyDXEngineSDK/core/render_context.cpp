#include "render_context.h"
#include "descriptor_heap.h"

// 使用自訂 DescriptorHeap 包裝類別設定單一 Descriptor Heap
void RenderContext::setDescriptorHeap(DescriptorHeap& descriptorHeap)
{
    // 取得底層 ID3D12DescriptorHeap* 並交給另一個 overload 處理
    setDescriptorHeap(descriptorHeap.get());
}

// 一次設定多個 Descriptor Heap
void RenderContext::setDescriptorHeaps(int numDescriptorHeap, const DescriptorHeap* descriptorHeaps[])
{
    // 確認 Descriptor Heap 數量在允許範圍內
    if (numDescriptorHeap <= 0 || numDescriptorHeap > MAX_DESCRIPTOR_HEAP)
    {
        throw std::invalid_argument("RenderContext: Descriptor heap count is out of range.");
    }

    // 確認有傳入有效的 DescriptorHeap 指標陣列
    if (descriptorHeaps == nullptr)
    {
        throw std::invalid_argument("RenderContext: Descriptor heap array is required.");
    }

    // 將每個 DescriptorHeap 包裝物件轉成底層 ID3D12DescriptorHeap* 保存
    for (int index = 0; index < numDescriptorHeap; ++index)
    {
        // 確認陣列中的 DescriptorHeap 指標不為 nullptr
        if (descriptorHeaps[index] == nullptr)
        {
            throw std::invalid_argument("RenderContext: Descriptor heap entries must not be null.");
        }

        // 取得底層 ID3D12DescriptorHeap* 並存入內部陣列
        m_descriptorHeaps[index] = descriptorHeaps[index]->get();
    }

    // 將指定數量的 Descriptor Heap 綁定到目前的 Command List
    m_commandList->SetDescriptorHeaps(
        // 將 Heap 數量轉成 Direct3D 12 使用的 UINT
        static_cast<UINT>(numDescriptorHeap),

        // 傳入 ID3D12DescriptorHeap* 陣列
        m_descriptorHeaps);
}
