#include "constant_buffer.h"

#include <cstring>
#include <directx/d3dx12_core.h>
#include <stdexcept>
#include <utility>

#include "graphics_engine.h"

ConstantBuffer::~ConstantBuffer()
{
    const D3D12_RANGE readRange{0, 0};
    for (auto& buffer : m_constantBuffer)
    {
        if (buffer != nullptr)
        {
            buffer->Unmap(0, &readRange);
            buffer->Release();
        }
    }
}

ConstantBuffer& ConstantBuffer::operator=(ConstantBuffer&& other) noexcept
{
    if (this == &other)
        return *this;

    const D3D12_RANGE readRange{0, 0};
    for (auto& buffer : m_constantBuffer)
    {
        if (buffer != nullptr)
        {
            buffer->Unmap(0, &readRange);
            buffer->Release();
        }
    }

    for (int index = 0; index < 2; ++index)
    {
        m_constantBuffer[index] = std::exchange(other.m_constantBuffer[index], nullptr);
        m_constBufferCPU[index] = std::exchange(other.m_constBufferCPU[index], nullptr);
    }
    m_size = std::exchange(other.m_size, 0);
    m_allocSize = std::exchange(other.m_allocSize, 0);
    m_isValid = std::exchange(other.m_isValid, false);
    return *this;
}

void ConstantBuffer::init(int size, void* sourceData)
{
    if (m_isValid)
        throw std::logic_error("ConstantBuffer: The buffer is already initialized.");
    if (size <= 0)
        throw std::invalid_argument("ConstantBuffer: Size must be positive.");
    if (g_graphicsEngine == nullptr || g_graphicsEngine->getD3DDevice() == nullptr)
        throw std::runtime_error("ConstantBuffer: GraphicsEngine must be initialized first.");

    m_size = size;
    // D3D12 的 CBV 大小必須對齊至 256 位元組。
    m_allocSize = (size + 255) & ~255;
    const D3D12_HEAP_PROPERTIES heapProperties =
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    const D3D12_RESOURCE_DESC description =
        CD3DX12_RESOURCE_DESC::Buffer(static_cast<UINT64>(m_allocSize));

    for (int index = 0; index < 2; ++index)
    {
        if (FAILED(g_graphicsEngine->getD3DDevice()->CreateCommittedResource(
                &heapProperties,
                D3D12_HEAP_FLAG_NONE,
                &description,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&m_constantBuffer[index]))))
        {
            throw std::runtime_error("ConstantBuffer: Failed to create an upload buffer.");
        }

        const D3D12_RANGE readRange{0, 0};
        if (FAILED(m_constantBuffer[index]->Map(
                0,
                &readRange,
                &m_constBufferCPU[index])))
        {
            throw std::runtime_error("ConstantBuffer: Failed to map an upload buffer.");
        }
        if (sourceData != nullptr)
            std::memcpy(m_constBufferCPU[index], sourceData, static_cast<std::size_t>(m_size));
    }
    m_isValid = true;
}

void ConstantBuffer::registerConstantBufferView(
    D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle,
    int bufferNo)
{
    if (!m_isValid)
        throw std::logic_error("ConstantBuffer: init must be called before registering a CBV.");
    if (bufferNo < 0 || bufferNo >= 2)
        throw std::out_of_range("ConstantBuffer: Buffer number is out of range.");

    D3D12_CONSTANT_BUFFER_VIEW_DESC description{};
    description.BufferLocation = m_constantBuffer[bufferNo]->GetGPUVirtualAddress();
    description.SizeInBytes = static_cast<UINT>(m_allocSize);
    g_graphicsEngine->getD3DDevice()->CreateConstantBufferView(
        &description,
        descriptorHandle);
}

void ConstantBuffer::registerConstantBufferView(
    D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle)
{
    registerConstantBufferView(
        descriptorHandle,
        static_cast<int>(g_graphicsEngine->getBackBufferIndex()));
}

void ConstantBuffer::copyToVRAM(void* data)
{
    if (!m_isValid)
        throw std::logic_error("ConstantBuffer: init must be called before copying data.");
    if (data == nullptr)
        throw std::invalid_argument("ConstantBuffer: Source data is required.");
    std::memcpy(
        m_constBufferCPU[g_graphicsEngine->getBackBufferIndex()],
        data,
        static_cast<std::size_t>(m_size));
}

D3D12_GPU_VIRTUAL_ADDRESS ConstantBuffer::getGPUVirtualAddress()
{
    if (!m_isValid)
        throw std::logic_error("ConstantBuffer: init must be called before requesting an address.");
    return m_constantBuffer[g_graphicsEngine->getBackBufferIndex()]->GetGPUVirtualAddress();
}
