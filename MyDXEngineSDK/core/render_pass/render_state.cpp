#include "render_state.h"

#include <stdexcept>

#include "constant_buffer.h"
#include "render_context.h"
#include "shader_resource.h"
#include "unorder_access_resource.h"

// 初始化 RenderState 使用的 Descriptor Heap 與相關狀態
void RenderState::init(ID3D12Device* device)
{
    // 避免重複初始化
    if (m_initialized)
        // 已初始化時拋出邏輯錯誤
        throw std::logic_error("RenderState: Render state is already initialized.");

    // 確認傳入的 D3D12 Device 有效
    if (device == nullptr)
        // Device 無效時拋出參數錯誤
        throw std::invalid_argument("RenderState: A graphics device is required.");

    // 保存 D3D12 Device 供後續建立 Descriptor 使用
    m_device = device;

    // 取得 CBV、SRV、UAV Descriptor 在 Heap 中的間隔大小
    m_descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // 建立並零初始化 Descriptor Heap 設定
    D3D12_DESCRIPTOR_HEAP_DESC description{};

    // 指定 Heap 用來存放 CBV、SRV 與 UAV
    description.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

    // 設定 Heap 內總 Descriptor 數量
    description.NumDescriptors = DescriptorCount;

    // 設定 Heap 可由 Shader 存取
    description.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    // 為每個 Frame 建立獨立的 Descriptor Heap
    for (UINT frameIndex = 0; frameIndex < FrameCount; ++frameIndex)
    {
        // 建立目前 Frame 使用的 Shader-visible Descriptor Heap
        if (FAILED(device->CreateDescriptorHeap(&description,
                                                IID_PPV_ARGS(m_descriptorHeaps[frameIndex].ReleaseAndGetAddressOf()))))
            // Descriptor Heap 建立失敗時拋出執行期錯誤
            throw std::runtime_error("RenderState: Failed to create a shader-visible descriptor heap.");
    }

    // 標記 RenderState 已完成初始化
    m_initialized = true;

    // 將所有資源 Descriptor 初始化成 Null Descriptor
    clearStructuredBuffers();
}

// 將 Structured Buffer 設定到指定 SRV slot
void RenderState::setStructuredBuffer(UINT slot, IShaderResource& buffer)
{
    // 確認 RenderState 已完成初始化
    requireInitialized();

    // 確認 SRV slot 沒有超出允許範圍
    if (slot >= ShaderResourceCount)
        // slot 超出範圍時拋出錯誤
        throw std::out_of_range("RenderState: Shader-resource slot is out of range.");

    // 為每個 Frame 註冊對應的 SRV
    for (UINT frameIndex = 0; frameIndex < FrameCount; ++frameIndex)
        // 將 Buffer 的 SRV 寫入目前 Frame 對應的 Descriptor
        buffer.registerShaderResourceView(
            // 取得指定 SRV slot 的 CPU Descriptor Handle
            getCpuHandle(frameIndex, ShaderResourceOffset + slot),
            // 將 Frame index 傳給資源以取得對應 Frame 的內容
            static_cast<int>(frameIndex));
}

// 同時將 Structured Buffer 設定成 SRV 與 UAV
void RenderState::setRWStructuredBuffer(UINT shaderResourceSlot, UINT unorderedAccessSlot,
                                        IUnorderAccessResource& buffer)
{
    // 將 Buffer 註冊到指定的 SRV slot
    setStructuredBuffer(shaderResourceSlot, buffer);

    // 確認 UAV slot 沒有超出允許範圍
    if (unorderedAccessSlot >= UnorderedAccessCount)
        // slot 超出範圍時拋出錯誤
        throw std::out_of_range("RenderState: Unordered-access slot is out of range.");

    // 為每個 Frame 註冊對應的 UAV
    for (UINT frameIndex = 0; frameIndex < FrameCount; ++frameIndex)
        // 將 Buffer 的 UAV 寫入目前 Frame 對應的 Descriptor
        buffer.RegisterUnorderAccessView(
            // 取得指定 UAV slot 的 CPU Descriptor Handle
            getCpuHandle(frameIndex, UnorderedAccessOffset + unorderedAccessSlot),
            // 將 Frame index 傳給資源以取得對應 Frame 的內容
            static_cast<int>(frameIndex));
}

// 清除指定 SRV slot
void RenderState::clearStructuredBuffer(UINT slot)
{
    // 確認 RenderState 已完成初始化
    requireInitialized();

    // 確認 SRV slot 沒有超出允許範圍
    if (slot >= ShaderResourceCount)
        // slot 超出範圍時拋出錯誤
        throw std::out_of_range("RenderState: Shader-resource slot is out of range.");

    // 清除每個 Frame 中指定的 SRV slot
    for (UINT frameIndex = 0; frameIndex < FrameCount; ++frameIndex)
        // 寫入 Null SRV
        writeNullShaderResource(frameIndex, slot);
}

// 清除指定 Buffer 對應的 SRV 與 UAV slot
void RenderState::clearRWStructuredBuffer(UINT shaderResourceSlot, UINT unorderedAccessSlot)
{
    // 清除指定的 SRV slot
    clearStructuredBuffer(shaderResourceSlot);

    // 確認 UAV slot 沒有超出允許範圍
    if (unorderedAccessSlot >= UnorderedAccessCount)
        // slot 超出範圍時拋出錯誤
        throw std::out_of_range("RenderState: Unordered-access slot is out of range.");

    // 清除每個 Frame 中指定的 UAV slot
    for (UINT frameIndex = 0; frameIndex < FrameCount; ++frameIndex)
        // 寫入 Null UAV
        writeNullUnorderedAccess(frameIndex, unorderedAccessSlot);
}

// 清除所有 Frame 中的 CBV、SRV 與 UAV Descriptor
void RenderState::clearStructuredBuffers()
{
    // 確認 RenderState 已完成初始化
    requireInitialized();

    // 逐一處理所有 Frame
    for (UINT frameIndex = 0; frameIndex < FrameCount; ++frameIndex)
    {
        // 清除目前 Frame 的所有 Constant Buffer slot
        for (UINT slot = 0; slot < ConstantBufferCount; ++slot)
            // 寫入 Null CBV
            writeNullConstantBuffer(frameIndex, slot);

        // 清除目前 Frame 的所有 Shader Resource slot
        for (UINT slot = 0; slot < ShaderResourceCount; ++slot)
            // 寫入 Null SRV
            writeNullShaderResource(frameIndex, slot);

        // 清除目前 Frame 的所有 Unordered Access slot
        for (UINT slot = 0; slot < UnorderedAccessCount; ++slot)
            // 寫入 Null UAV
            writeNullUnorderedAccess(frameIndex, slot);
    }
}

// 將 Constant Buffer 綁定到指定 Shader Stage 的 Root Parameter
void RenderState::setConstantBuffer(RenderContext& renderContext, ShaderStage stage, UINT slot,
                                    ConstantBuffer& buffer) const
{
    // 確認 RenderState 已完成初始化
    requireInitialized();

    // 確認 Constant Buffer slot 沒有超出允許範圍
    if (slot >= ConstantBufferCount)
        // slot 超出範圍時拋出錯誤
        throw std::out_of_range("RenderState: Constant-buffer slot is out of range.");

    // 取得 Constant Buffer 的 GPU Virtual Address
    const D3D12_GPU_VIRTUAL_ADDRESS address = buffer.getGPUVirtualAddress();

    // 判斷 Constant Buffer 要綁定到 Graphics 或 Compute Pipeline
    if (stage == ShaderStage::Graphics)
        // 設定 Graphics Root CBV
        renderContext.setGraphicsRootConstantBufferView(slot, address);
    else
        // 設定 Compute Root CBV
        renderContext.setComputeRootConstantBufferView(slot, address);
}

// 套用 Graphics Pipeline 所需的 Descriptor Heap 與 Descriptor Table
void RenderState::applyGraphics(RenderContext& renderContext, UINT frameIndex) const
{
    // 確認 RenderState 已完成初始化
    requireInitialized();

    // 確認 Frame index 沒有超出範圍
    if (frameIndex >= FrameCount)
        // Frame index 超出範圍時拋出錯誤
        throw std::out_of_range("RenderState: Frame index is out of range.");

    // 將目前 Frame 的 Descriptor Heap 綁定到 Command List
    renderContext.setDescriptorHeap(m_descriptorHeaps[frameIndex].Get());

    // 將目前 Frame 的 Descriptor Table 綁定到 Graphics Root Signature
    renderContext.setGraphicsRootDescriptorTable(ResourceTableRootParameter, getGpuTableStart(frameIndex));
}

// 套用 Compute Pipeline 所需的 Descriptor Heap 與 Descriptor Table
void RenderState::applyCompute(RenderContext& renderContext, UINT frameIndex) const
{
    // 確認 RenderState 已完成初始化
    requireInitialized();

    // 確認 Frame index 沒有超出範圍
    if (frameIndex >= FrameCount)
        // Frame index 超出範圍時拋出錯誤
        throw std::out_of_range("RenderState: Frame index is out of range.");

    // 將目前 Frame 的 Descriptor Heap 綁定到 Command List
    renderContext.setDescriptorHeap(m_descriptorHeaps[frameIndex].Get());

    // 將目前 Frame 的 Descriptor Table 綁定到 Compute Root Signature
    renderContext.setComputeRootDescriptorTable(ResourceTableRootParameter, getGpuTableStart(frameIndex));
}

// 取得指定 Descriptor 的 CPU Handle
D3D12_CPU_DESCRIPTOR_HANDLE RenderState::getCpuHandle(UINT frameIndex, UINT descriptorIndex) const
{
    // 取得目前 Frame Descriptor Heap 起始位置的 CPU Handle
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_descriptorHeaps[frameIndex]->GetCPUDescriptorHandleForHeapStart();

    // 根據 Descriptor index 與 Descriptor 大小計算目標位置
    handle.ptr += static_cast<SIZE_T>(descriptorIndex) * m_descriptorSize;

    // 回傳計算完成的 CPU Descriptor Handle
    return handle;
}

// 取得指定 Frame Descriptor Heap 的 GPU 起始 Handle
D3D12_GPU_DESCRIPTOR_HANDLE RenderState::getGpuTableStart(UINT frameIndex) const
{
    // 回傳 Descriptor Heap 起始位置的 GPU Handle
    return m_descriptorHeaps[frameIndex]->GetGPUDescriptorHandleForHeapStart();
}

// 在指定 Constant Buffer slot 建立 Null CBV
void RenderState::writeNullConstantBuffer(UINT frameIndex, UINT slot)
{
    // 使用空 Resource 建立 Null Constant Buffer View
    m_device->CreateConstantBufferView(nullptr, getCpuHandle(frameIndex, slot));
}

// 在指定 Shader Resource slot 建立 Null SRV
void RenderState::writeNullShaderResource(UINT frameIndex, UINT slot)
{
    // 建立並零初始化 SRV 設定
    D3D12_SHADER_RESOURCE_VIEW_DESC description{};

    // Structured Buffer 不使用特定像素格式
    description.Format = DXGI_FORMAT_UNKNOWN;

    // 將 SRV 類型設定成 Buffer
    description.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;

    // 使用 D3D12 預設的 Shader Component Mapping
    description.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    // Null SRV 使用一個虛擬元素描述 Buffer
    description.Buffer.NumElements = 1;

    // 將虛擬元素大小設定成 32-bit
    description.Buffer.StructureByteStride = sizeof(std::uint32_t);

    // 使用空 Resource 建立 Null Shader Resource View
    m_device->CreateShaderResourceView(nullptr, &description,
                                       // 將 SRV 寫入指定 Frame 與 slot 的 Descriptor
                                       getCpuHandle(frameIndex, ShaderResourceOffset + slot));
}

// 在指定 Unordered Access slot 建立 Null UAV
void RenderState::writeNullUnorderedAccess(UINT frameIndex, UINT slot)
{
    // 建立並零初始化 UAV 設定
    D3D12_UNORDERED_ACCESS_VIEW_DESC description{};

    // Structured Buffer 不使用特定像素格式
    description.Format = DXGI_FORMAT_UNKNOWN;

    // 將 UAV 類型設定成 Buffer
    description.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

    // Null UAV 使用一個虛擬元素描述 Buffer
    description.Buffer.NumElements = 1;

    // 將虛擬元素大小設定成 32-bit
    description.Buffer.StructureByteStride = sizeof(std::uint32_t);

    // 使用空 Resource 與空 Counter Resource 建立 Null UAV
    m_device->CreateUnorderedAccessView(nullptr, nullptr, &description,
                                        // 將 UAV 寫入指定 Frame 與 slot 的 Descriptor
                                        getCpuHandle(frameIndex, UnorderedAccessOffset + slot));
}

// 確認 RenderState 已完成初始化
void RenderState::requireInitialized() const
{
    // 尚未初始化時禁止使用 RenderState
    if (!m_initialized)
        // 拋出錯誤提醒必須先呼叫 init
        throw std::logic_error("RenderState: init must be called before use.");
}