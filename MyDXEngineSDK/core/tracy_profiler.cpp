#include "tracy_profiler.h"

#include <stdexcept>

TracyProfiler::~TracyProfiler() noexcept
{
#ifdef SKYLINE_ENABLE_TRACY
    if (m_gpuContext != nullptr)
        TracyD3D12Destroy(m_gpuContext);
#endif
}

void TracyProfiler::init(ID3D12Device* device, ID3D12CommandQueue* queue)
{
    if (device == nullptr || queue == nullptr)
        throw std::invalid_argument("TracyProfiler: A D3D12 device and command queue are required.");

#ifdef SKYLINE_ENABLE_TRACY
    if (m_gpuContext != nullptr)
        throw std::logic_error("TracyProfiler: Already initialized.");

    m_gpuContext = TracyD3D12Context(device, queue);
    if (m_gpuContext == nullptr)
        throw std::runtime_error("TracyProfiler: Failed to create the D3D12 GPU context.");
#endif
}

void TracyProfiler::beginFrame()
{
#ifdef SKYLINE_ENABLE_TRACY
    TracyD3D12NewFrame(m_gpuContext);
#endif
}

void TracyProfiler::collect()
{
#ifdef SKYLINE_ENABLE_TRACY
    TracyD3D12Collect(m_gpuContext);
#endif
}
