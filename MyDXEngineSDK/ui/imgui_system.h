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

    /// <summary>
    /// 目前的 ImGuiContext（以 void* 回傳，避免標頭依賴 imgui.h）。
    /// </summary>
    /// <remarks>
    /// ImGui 以靜態函式庫連結進 SDK 的 DLL，使用端若自己也連結 imgui 並直接呼叫 ImGui::，
    /// 會用到自己那份全域狀態而沒有 Context。使用端要在 initialize 之後呼叫
    /// ImGui::SetCurrentContext(static_cast<ImGuiContext*>(context()))，之後才能直接使用 ImGui API。
    /// </remarks>
    [[nodiscard]] void* context() const noexcept;

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
