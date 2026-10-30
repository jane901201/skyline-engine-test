#pragma once
#include "skyline_api.h"

#include <d3d12.h>

#ifdef SKYLINE_ENABLE_TRACY
#include <tracy/TracyD3D12.hpp>
#endif

class SKYLINE_API TracyProfiler
{
public:
    TracyProfiler() = default;
    ~TracyProfiler() noexcept;

    TracyProfiler(const TracyProfiler&) = delete;
    TracyProfiler& operator=(const TracyProfiler&) = delete;

    void init(ID3D12Device* device, ID3D12CommandQueue* queue);
    void beginFrame();
    void collect();

#ifdef SKYLINE_ENABLE_TRACY
    TracyD3D12Ctx gpuContext() const noexcept { return m_gpuContext; }
#endif

private:
#ifdef SKYLINE_ENABLE_TRACY
    TracyD3D12Ctx m_gpuContext = nullptr;
#endif
};
