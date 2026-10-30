#include "backends/d3d12/d3d12_interop.h"
#include "imgui_system.h"

#include <stdexcept>

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

#include "graphics_engine.h"
#include "system.h"

    // 宣告 ImGui Win32 backend 提供的 Windows 訊息處理函式
    extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND window, UINT message, WPARAM wParam,
                                                                 LPARAM lParam);

namespace Skyline::UI
{
// 解構時自動釋放 ImGui 使用的資源
ImGuiSystem::~ImGuiSystem() noexcept
{
    // 關閉 ImGui 系統並清理資源
    shutdown();
}

// 初始化 ImGui 的 Win32 與 DirectX 12 backend
void ImGuiSystem::initialize(HWND window, GraphicsEngine& graphicsEngine)
{
    // 防止重複初始化
    if (m_initialized)
        // 系統已初始化時拋出邏輯錯誤
        throw std::logic_error("ImGuiSystem: The system is already initialized.");

    // 確認傳入有效的視窗 Handle
    if (window == nullptr)
        // 視窗無效時拋出參數錯誤
        throw std::invalid_argument("ImGuiSystem: A valid window is required.");

    // 取得 GraphicsEngine 使用的 D3D12 device
    ID3D12Device* device = Skyline::D3D12::nativeFrameDevice(graphicsEngine.getRhiFrame());

    // 取得提交 GPU 指令使用的 command queue
    ID3D12CommandQueue* commandQueue = Skyline::D3D12::nativeFrameQueue(graphicsEngine.getRhiFrame());

    // 確認 D3D12 device 已完成初始化
    if (device == nullptr)
        // device 無效時拋出參數錯誤
        throw std::invalid_argument("ImGuiSystem: An initialized D3D12 device is required.");

    // 確認 command queue 已完成初始化
    if (commandQueue == nullptr)
        // command queue 無效時拋出參數錯誤
        throw std::invalid_argument("ImGuiSystem: An initialized D3D12 command queue is required.");

    // 建立 descriptor heap 的設定資料並初始化為零
    D3D12_DESCRIPTOR_HEAP_DESC heapDescription{};

    // 設定為可存放 CBV、SRV 與 UAV 的 descriptor heap
    heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

    // 設定 descriptor heap 可容納的 descriptor 數量
    heapDescription.NumDescriptors = DescriptorCount;

    // 允許 shader 存取這個 descriptor heap
    heapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    // 建立 ImGui 使用的 descriptor heap
    if (FAILED(device->CreateDescriptorHeap(&heapDescription, IID_PPV_ARGS(&m_descriptorHeap))))
        // 建立失敗時拋出執行階段錯誤
        throw std::runtime_error("ImGuiSystem: Failed to create the descriptor heap.");

    // 檢查目前 ImGui header 與 library 的版本是否相容
    IMGUI_CHECKVERSION();

    // 建立 ImGui context
    ImGui::CreateContext();

    // 套用 ImGui 預設深色主題
    ImGui::StyleColorsDark();

    // 初始化 ImGui 的 Win32 backend
    if (!ImGui_ImplWin32_Init(window))
    {
        // Win32 backend 初始化失敗時銷毀已建立的 ImGui context
        ImGui::DestroyContext();

        // 回報 Win32 backend 初始化失敗
        throw std::runtime_error("ImGuiSystem: Failed to initialize the Win32 backend.");
    }

    // 取得 CBV、SRV、UAV descriptor 在 heap 中的位元組間距
    m_descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // 建立 DirectX 12 backend 的初始化資訊並初始化為零
    ImGui_ImplDX12_InitInfo initializationInformation{};

    // 指定 ImGui 使用的 D3D12 device
    initializationInformation.Device = device;

    // 指定 ImGui 使用的 command queue
    initializationInformation.CommandQueue = commandQueue;

    // 設定同時處理中的 frame 數量
    initializationInformation.NumFramesInFlight = GraphicsEngine::FRAME_BUFFER_COUNT;

    // 設定 Render Target 的像素格式
    initializationInformation.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

    // 設定 Depth Stencil Buffer 的像素格式
    initializationInformation.DSVFormat = DXGI_FORMAT_D32_FLOAT;

    // 提供 ImGui 配置 SRV 時使用的 descriptor heap
    initializationInformation.SrvDescriptorHeap = m_descriptorHeap.Get();

    // 將目前 ImGuiSystem 實例傳給 descriptor callback 使用
    initializationInformation.UserData = this;

    // 指定 descriptor 配置 callback
    initializationInformation.SrvDescriptorAllocFn = allocateDescriptor;

    // 指定 descriptor 釋放 callback
    initializationInformation.SrvDescriptorFreeFn = freeDescriptor;

    // 初始化 ImGui 的 DirectX 12 backend
    if (!ImGui_ImplDX12_Init(&initializationInformation))
    {
        // DirectX 12 backend 初始化失敗時先關閉 Win32 backend
        ImGui_ImplWin32_Shutdown();

        // 銷毀 ImGui context
        ImGui::DestroyContext();

        // 回報 DirectX 12 backend 初始化失敗
        throw std::runtime_error("ImGuiSystem: Failed to initialize the DirectX 12 backend.");
    }

    // 保存 GraphicsEngine 的位址供後續 render 與 shutdown 使用
    m_graphicsEngine = &graphicsEngine;

    // 將 ImGui 的 Win32 訊息處理函式註冊到系統
    setWindowMessageHandler(ImGui_ImplWin32_WndProcHandler);

    // 標記 ImGui 系統已成功初始化
    m_initialized = true;
}

// 開始建立新的 ImGui frame
void ImGuiSystem::beginFrame()
{
    // 更新 DirectX 12 backend 的新 frame 狀態
    ImGui_ImplDX12_NewFrame();

    // 更新 Win32 backend 的新 frame 狀態
    ImGui_ImplWin32_NewFrame();

    // 開始新的 ImGui frame
    ImGui::NewFrame();
}

// 開始建立指定標題的 ImGui 視窗
bool ImGuiSystem::beginWindow(const char* title)
{
    // 建立視窗並回傳目前是否應繼續提交視窗內容
    return ImGui::Begin(title);
}

// 結束目前的 ImGui 視窗
void ImGuiSystem::endWindow()
{
    // 結束由 ImGui::Begin 建立的視窗
    ImGui::End();
}

// 建立控制 RGB 三個色彩分量的 slider
bool ImGuiSystem::colorSliders(const char* label, float color[3])
{
    // 將 label 推入 ImGui ID stack 避免不同元件發生 ID 衝突
    ImGui::PushID(label);

    // 顯示這組色彩控制項的文字標籤
    ImGui::TextUnformatted(label);

    // 建立紅色分量的 slider 並記錄數值是否被修改
    bool changed = ImGui::SliderFloat("Red", &color[0], 0.0f, 1.0f);

    // 建立綠色分量的 slider 並保留先前任一 slider 的修改狀態
    changed = ImGui::SliderFloat("Green", &color[1], 0.0f, 1.0f) || changed;

    // 建立藍色分量的 slider 並整合整組控制項的修改狀態
    changed = ImGui::SliderFloat("Blue", &color[2], 0.0f, 1.0f) || changed;

    // 移除先前推入 ID stack 的 label
    ImGui::PopID();

    // 回傳任一色彩分量是否被修改
    return changed;
}

// 將目前 ImGui frame 轉換並提交成 DirectX 12 繪製指令
void ImGuiSystem::render()
{
    // 完成目前 ImGui frame 並產生 draw data
    ImGui::Render();

    // 建立包含 ImGui descriptor heap 的陣列供 command list 綁定
    ID3D12DescriptorHeap* heaps[] = {m_descriptorHeap.Get()};

    // 取得目前用來記錄 GPU 繪圖命令的 command list
    ID3D12GraphicsCommandList* commandList = Skyline::D3D12::nativeFrameCommands(m_graphicsEngine->getRhiFrame());

    // 將 ImGui 使用的 shader-visible descriptor heap 綁定到 command list
    commandList->SetDescriptorHeaps(1, heaps);

    // 將 ImGui draw data 轉換成 DirectX 12 繪製命令
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
}

// 關閉 ImGui 系統並釋放相關資源
void ImGuiSystem::shutdown() noexcept
{
    // 尚未初始化時不需要執行任何清理
    if (!m_initialized)
        return;

    // 移除 ImGui 的 Windows 訊息處理函式
    setWindowMessageHandler(nullptr);

    // 等待目前繪圖工作完成以避免資源仍被 GPU 使用
    m_graphicsEngine->waitDraw();

    // 關閉 ImGui DirectX 12 backend
    ImGui_ImplDX12_Shutdown();

    // 關閉 ImGui Win32 backend
    ImGui_ImplWin32_Shutdown();

    // 銷毀 ImGui context
    ImGui::DestroyContext();

    // 釋放 descriptor heap
    m_descriptorHeap.Reset();

    // 將所有 descriptor 使用狀態重設為未使用
    m_descriptorUsage.fill(false);

    // 清除 descriptor 的位元組間距
    m_descriptorSize = 0;

    // 清除保存的 GraphicsEngine 指標
    m_graphicsEngine = nullptr;

    // 標記 ImGui 系統目前未初始化
    m_initialized = false;
}

// 從 ImGui 專用 descriptor heap 中配置一組 CPU 與 GPU descriptor handle
void ImGuiSystem::allocateDescriptor(ImGui_ImplDX12_InitInfo* information, D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle,
                                     D3D12_GPU_DESCRIPTOR_HANDLE* gpuHandle)
{
    // 從 UserData 取回這個 callback 所屬的 ImGuiSystem 實例
    auto& system = *static_cast<ImGuiSystem*>(information->UserData);

    // 逐一尋找尚未被使用的 descriptor
    for (UINT index = 0; index < DescriptorCount; ++index)
    {
        // 已被使用的 descriptor 直接跳過
        if (system.m_descriptorUsage[index])
            continue;

        // 將目前 descriptor 標記為已配置
        system.m_descriptorUsage[index] = true;

        // 取得 descriptor heap 起點的 CPU handle
        *cpuHandle = system.m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();

        // 取得 descriptor heap 起點的 GPU handle
        *gpuHandle = system.m_descriptorHeap->GetGPUDescriptorHandleForHeapStart();

        // 將 CPU handle 移動到目前 descriptor 的位置
        cpuHandle->ptr += static_cast<SIZE_T>(index) * system.m_descriptorSize;

        // 將 GPU handle 移動到目前 descriptor 的位置
        gpuHandle->ptr += static_cast<UINT64>(index) * system.m_descriptorSize;

        // descriptor 配置完成後離開函式
        return;
    }

    // descriptor heap 已滿時在 Debug 模式觸發 assertion
    IM_ASSERT(false && "ImGuiSystem: The descriptor heap is full.");

    // 配置失敗時將 CPU handle 清空
    *cpuHandle = {};

    // 配置失敗時將 GPU handle 清空
    *gpuHandle = {};
}

// 將先前配置的 descriptor 標記為可再次使用
void ImGuiSystem::freeDescriptor(ImGui_ImplDX12_InitInfo* information, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
                                 D3D12_GPU_DESCRIPTOR_HANDLE)
{
    // 從 UserData 取回這個 callback 所屬的 ImGuiSystem 實例
    auto& system = *static_cast<ImGuiSystem*>(information->UserData);

    // 取得 descriptor heap 起點的 CPU handle 位址
    const SIZE_T heapStart = system.m_descriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr;

    // 計算欲釋放 descriptor 與 heap 起點之間的位元組距離
    const SIZE_T offset = cpuHandle.ptr - heapStart;

    // 根據 offset 與 descriptor 大小換算出 descriptor index
    const UINT index = static_cast<UINT>(offset / system.m_descriptorSize);

    // 確認計算出的 index 位於 descriptor heap 範圍內
    IM_ASSERT(index < DescriptorCount);

    // 將 descriptor 標記為未使用以供後續重新配置
    system.m_descriptorUsage[index] = false;
}
}
