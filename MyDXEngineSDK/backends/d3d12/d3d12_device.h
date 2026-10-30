#pragma once

#include "rhi/device.h"
#include <directx/d3d12.h>

namespace Skyline::D3D12
{
// 暫時共用 GraphicsEngine 建立的裝置；RHI 後端持有自己的 COM 參考。
SKYLINE_API std::unique_ptr<RHI::Device> createDevice(ID3D12Device& device);
}
