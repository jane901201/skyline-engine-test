#pragma once

#include "rhi/shader.h"
#include <directx/d3dcommon.h>
#include <wrl/client.h>

namespace Skyline::D3D12
{
SKYLINE_API Microsoft::WRL::ComPtr<ID3DBlob> compileShader(const RHI::ShaderDesc& desc);
}
