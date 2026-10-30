#pragma once
#include "skyline_api.h"

#include <Windows.h>
#include <array>
#include <d3d12.h>
#include <wrl/client.h>

class GraphicsEngine;
struct ImGui_ImplDX12_InitInfo;

namespace Skyline::UI
{
class SKYLINE_API ImGuiSystem
{
public:
    ImGuiSystem() = default;
    ~ImGuiSystem() noexcept;

    ImGuiSystem(const ImGuiSystem&) = delete;
    ImGuiSystem& operator=(const ImGuiSystem&) = delete;
    ImGuiSystem(ImGuiSystem&&) = delete;
    ImGuiSystem& operator=(ImGuiSystem&&) = delete;

    void initialize(HWND window, GraphicsEngine& graphicsEngine);
    void beginFrame();
    void render();
    void shutdown() noexcept;
    bool beginWindow(const char* title);
    void endWindow();
    bool colorSliders(const char* label, float color[3]);

  private:
    static constexpr UINT DescriptorCount = 32;

    static void allocateDescriptor(
        ImGui_ImplDX12_InitInfo* information,
        D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE* gpuHandle);
    static void freeDescriptor(
        ImGui_ImplDX12_InitInfo* information,
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle);

    GraphicsEngine* m_graphicsEngine = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;
    std::array<bool, DescriptorCount> m_descriptorUsage{};
    UINT m_descriptorSize = 0;
    bool m_initialized = false;
};
}
