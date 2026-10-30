#include "rw_structured_buffer.h"

#include <directx/d3dx12_core.h>
#include <stdexcept>

#include "graphics_engine.h"

RWStructuredBuffer::~RWStructuredBuffer()
{
    for (auto& buffer : m_buffersOnGPU)
    {
        if (buffer != nullptr)
            buffer->Release();
    }
}

void RWStructuredBuffer::Init(int sizeOfElement, int numElement, void* initData)
{
    if (m_isInited)
        throw std::logic_error("RWStructuredBuffer: The buffer is already initialized.");
    if (g_graphicsEngine == nullptr || g_graphicsEngine->getD3DDevice() == nullptr)
        throw std::runtime_error("RWStructuredBuffer: GraphicsEngine must be initialized first.");
    if (sizeOfElement <= 0 || numElement <= 0)
        throw std::invalid_argument("RWStructuredBuffer: Element size and count must be positive.");
    if (initData != nullptr)
        throw std::invalid_argument(
            "RWStructuredBuffer: CPU initialization is unavailable for a GPU-writable buffer; initialize it with a compute shader.");

    m_sizeOfElement = sizeOfElement;
    m_numElement = numElement;

    const UINT64 byteSize = static_cast<UINT64>(m_sizeOfElement) * static_cast<UINT64>(m_numElement);
    const D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    const D3D12_RESOURCE_DESC description = CD3DX12_RESOURCE_DESC::Buffer(
        byteSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    for (auto& buffer : m_buffersOnGPU)
    {
        if (FAILED(g_graphicsEngine->getD3DDevice()->CreateCommittedResource(
                &heapProperties, D3D12_HEAP_FLAG_NONE, &description,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&buffer))))
        {
            throw std::runtime_error("RWStructuredBuffer: Failed to create a GPU-writable buffer.");
        }
    }

    m_isInited = true;
}

void RWStructuredBuffer::Init(const VertexBuffer&, bool)
{
    throw std::logic_error("RWStructuredBuffer: Vertex-buffer aliasing is not supported.");
}

void RWStructuredBuffer::Init(const IndexBuffer&, bool)
{
    throw std::logic_error("RWStructuredBuffer: Index-buffer aliasing is not supported.");
}

void* RWStructuredBuffer::GetResourceOnCPU()
{
    if (!m_isInited)
        throw std::logic_error("RWStructuredBuffer: Init must be called before requesting CPU data.");
    return nullptr;
}

void RWStructuredBuffer::RegisterUnorderAccessView(D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle, int bufferNo)
{
    if (!m_isInited)
        throw std::logic_error("RWStructuredBuffer: Init must be called before registering a UAV.");
    if (bufferNo < 0 || bufferNo >= 2)
        throw std::out_of_range("RWStructuredBuffer: Buffer number is out of range.");

    D3D12_UNORDERED_ACCESS_VIEW_DESC description{};
    description.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    description.Format = DXGI_FORMAT_UNKNOWN;
    description.Buffer.NumElements = static_cast<UINT>(m_numElement);
    description.Buffer.StructureByteStride = static_cast<UINT>(m_sizeOfElement);
    g_graphicsEngine->getD3DDevice()->CreateUnorderedAccessView(
        m_buffersOnGPU[bufferNo], nullptr, &description, descriptorHandle);
}

void RWStructuredBuffer::registerShaderResourceView(D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle, int bufferNo)
{
    if (!m_isInited)
        throw std::logic_error("RWStructuredBuffer: Init must be called before registering an SRV.");
    if (bufferNo < 0 || bufferNo >= 2)
        throw std::out_of_range("RWStructuredBuffer: Buffer number is out of range.");

    D3D12_SHADER_RESOURCE_VIEW_DESC description{};
    description.Format = DXGI_FORMAT_UNKNOWN;
    description.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    description.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    description.Buffer.NumElements = static_cast<UINT>(m_numElement);
    description.Buffer.StructureByteStride = static_cast<UINT>(m_sizeOfElement);
    g_graphicsEngine->getD3DDevice()->CreateShaderResourceView(
        m_buffersOnGPU[bufferNo], &description, descriptorHandle);
}

ID3D12Resource* RWStructuredBuffer::getD3DResource()
{
    if (!m_isInited)
        throw std::logic_error("RWStructuredBuffer: Init must be called before requesting the GPU resource.");
    return m_buffersOnGPU[g_graphicsEngine->getBackBufferIndex()];
}

ID3D12Resource* RWStructuredBuffer::GetD3DResoruce()
{
    return getD3DResource();
}
