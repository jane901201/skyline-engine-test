#pragma once

// 範例共用：把目前的 back buffer 存成 BMP，用來在沒有人看畫面時檢查繪製結果，
// 以及解析 --screenshot=路徑 命令列參數。只供範例使用，所以全部寫在標頭。

#include <Windows.h>
#include <wrl/client.h>

#include <cstdint>
#include <cstring>
#include <cwchar>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "backends/d3d12/d3d12_interop.h"
#include "graphics_engine.h"

/// 解析 --screenshot=路徑，沒有指定時回傳空字串（路徑不可含空白）
inline std::wstring parseScreenshotPath(const wchar_t* commandLine)
{
    const wchar_t* found = commandLine != nullptr ? std::wcsstr(commandLine, L"--screenshot=") : nullptr;
    if (found == nullptr)
        return {};
    std::wstring path(found + 13);
    const std::size_t end = path.find(L' ');
    return end == std::wstring::npos ? path : path.substr(0, end);
}

/// <summary>
/// 把目前的 back buffer 存成 BMP。
/// </summary>
/// <remarks>
/// 流程：record() 在 endRender 之前記錄複製命令，save() 在影格結束、GPU 完成之後寫檔。
/// </remarks>
class FrameCapture
{
  public:
    void record(GraphicsEngine& graphicsEngine)
    {
        auto& frame = graphicsEngine.getRhiFrame();
        ID3D12Device* device = Skyline::D3D12::nativeFrameDevice(frame);
        ID3D12GraphicsCommandList* commands = Skyline::D3D12::nativeFrameCommands(frame);
        ID3D12Resource* backBuffer = graphicsEngine.getCurrentBackBuffer();

        const D3D12_RESOURCE_DESC description = backBuffer->GetDesc();
        m_width = static_cast<UINT>(description.Width);
        m_height = description.Height;

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT64 totalBytes = 0;
        device->GetCopyableFootprints(&description, 0, 1, 0, &footprint, nullptr, nullptr, &totalBytes);
        m_rowPitch = footprint.Footprint.RowPitch;

        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_READBACK;
        D3D12_RESOURCE_DESC bufferDesc{};
        bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Width = totalBytes;
        bufferDesc.Height = 1;
        bufferDesc.DepthOrArraySize = 1;
        bufferDesc.MipLevels = 1;
        bufferDesc.SampleDesc.Count = 1;
        bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &bufferDesc,
                                                   D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                   IID_PPV_ARGS(&m_readback))))
            throw std::runtime_error("FrameCapture: Failed to create the readback buffer.");

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = backBuffer;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        commands->ResourceBarrier(1, &barrier);

        D3D12_TEXTURE_COPY_LOCATION destination{};
        destination.pResource = m_readback.Get();
        destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        destination.PlacedFootprint = footprint;
        D3D12_TEXTURE_COPY_LOCATION source{};
        source.pResource = backBuffer;
        source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        source.SubresourceIndex = 0;
        commands->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);

        // 還原狀態，endRender 才能照原樣把它轉成 PRESENT
        std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
        commands->ResourceBarrier(1, &barrier);
    }

    /// 影格結束、GPU 完成之後呼叫。back buffer 是 R8G8B8A8，BMP 要 B8G8R8，所以交換 R 與 B
    void save(const std::wstring& path) const
    {
        void* mapped = nullptr;
        if (FAILED(m_readback->Map(0, nullptr, &mapped)))
            throw std::runtime_error("FrameCapture: Failed to map the readback buffer.");

        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(m_width) * m_height * 4);
        for (UINT y = 0; y < m_height; ++y)
        {
            const auto* row = static_cast<const std::uint8_t*>(mapped) + static_cast<std::size_t>(y) * m_rowPitch;
            std::uint8_t* out = pixels.data() + static_cast<std::size_t>(y) * m_width * 4;
            for (UINT x = 0; x < m_width; ++x)
            {
                out[x * 4 + 0] = row[x * 4 + 2];
                out[x * 4 + 1] = row[x * 4 + 1];
                out[x * 4 + 2] = row[x * 4 + 0];
                out[x * 4 + 3] = 255;
            }
        }
        m_readback->Unmap(0, nullptr);

        const std::uint32_t imageBytes = static_cast<std::uint32_t>(pixels.size());
        const std::uint32_t fileBytes = 54 + imageBytes;
        std::uint8_t header[54]{'B', 'M'};
        const auto put32 = [&](int offset, std::uint32_t value) { std::memcpy(header + offset, &value, 4); };
        put32(2, fileBytes);
        put32(10, 54);
        put32(14, 40);
        put32(18, m_width);
        put32(22, static_cast<std::uint32_t>(-static_cast<std::int32_t>(m_height))); // 負值：由上到下
        header[26] = 1;
        header[28] = 32;
        put32(34, imageBytes);

        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(header), sizeof(header));
        file.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
        if (!file)
            throw std::runtime_error("FrameCapture: Failed to write the screenshot file.");
    }

  private:
    Microsoft::WRL::ComPtr<ID3D12Resource> m_readback;
    UINT m_width = 0;
    UINT m_height = 0;
    UINT64 m_rowPitch = 0;
};
