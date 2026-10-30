#include "structured_buffer.h"

#include <cstring>
#include <directx/d3dx12_core.h>
#include <stdexcept>

#include "graphics_engine.h"

StructuredBuffer::~StructuredBuffer()
{
    const D3D12_RANGE readRange{0, 0};
    for (int index = 0; index < 2; ++index)
    {
        if (m_buffersOnGPU[index] != nullptr)
        {
            m_buffersOnGPU[index]->Unmap(0, &readRange);
            m_buffersOnGPU[index]->Release();
        }
    }
}

void StructuredBuffer::init(int sizeOfElement, int numElement, void* initData)
{
    if (m_isInited)
        throw std::logic_error("StructuredBuffer: The buffer is already initialized.");
    if (g_graphicsEngine == nullptr || g_graphicsEngine->getD3DDevice() == nullptr)
        throw std::runtime_error("StructuredBuffer: GraphicsEngine must be initialized first.");
    if (sizeOfElement <= 0 || numElement <= 0)
        throw std::invalid_argument("StructuredBuffer: Element size and count must be positive.");

    m_sizeOfElement = sizeOfElement;
    m_numElement = numElement;
    const UINT64 byteSize = static_cast<UINT64>(sizeOfElement) * numElement;
    const D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    const D3D12_RESOURCE_DESC description = CD3DX12_RESOURCE_DESC::Buffer(byteSize);

    for (int index = 0; index < 2; ++index)
    {
        if (FAILED(g_graphicsEngine->getD3DDevice()->CreateCommittedResource(
                &heapProperties, D3D12_HEAP_FLAG_NONE, &description, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                IID_PPV_ARGS(&m_buffersOnGPU[index]))))
        {
            throw std::runtime_error("StructuredBuffer: Failed to create an upload buffer.");
        }
        const D3D12_RANGE readRange{0, 0};
        if (FAILED(m_buffersOnGPU[index]->Map(0, &readRange, &m_buffersOnCPU[index])))
            throw std::runtime_error("StructuredBuffer: Failed to map an upload buffer.");
        if (initData != nullptr)
            std::memcpy(m_buffersOnCPU[index], initData, static_cast<size_t>(byteSize));
    }
    m_isInited = true;
}

void StructuredBuffer::registerShaderResourceView(D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle, int bufferNo)
{
    if (!m_isInited)
        throw std::logic_error("StructuredBuffer: init must be called before registering an SRV.");
    if (bufferNo < 0 || bufferNo >= 2)
        throw std::out_of_range("StructuredBuffer: Buffer number is out of range.");

    D3D12_SHADER_RESOURCE_VIEW_DESC description{};
    description.Format = DXGI_FORMAT_UNKNOWN;
    description.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    description.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    description.Buffer.NumElements = static_cast<UINT>(m_numElement);
    description.Buffer.StructureByteStride = static_cast<UINT>(m_sizeOfElement);
    g_graphicsEngine->getD3DDevice()->CreateShaderResourceView(m_buffersOnGPU[bufferNo], &description,
                                                               descriptorHandle);
}

void StructuredBuffer::update(void* data)
{
    if (!m_isInited)
        throw std::logic_error("StructuredBuffer: init must be called before update.");
    if (data == nullptr)
        throw std::invalid_argument("StructuredBuffer: Update data is required.");
    const UINT frameIndex = g_graphicsEngine->getBackBufferIndex();
    std::memcpy(m_buffersOnCPU[frameIndex], data, static_cast<size_t>(m_sizeOfElement) * m_numElement);
}

ID3D12Resource* StructuredBuffer::getD3DResource()
{
    if (!m_isInited)
        throw std::logic_error("StructuredBuffer: init must be called before getD3DResource.");
    return m_buffersOnGPU[g_graphicsEngine->getBackBufferIndex()];
}
