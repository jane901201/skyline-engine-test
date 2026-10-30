#pragma once

#include "rhi/binding.h"
#include <directx/d3d12.h>
#include <d3dcommon.h>
#include <wrl/client.h>

namespace Skyline::D3D12
{
SKYLINE_API void validateBindingLayout(const RHI::BindingLayoutDesc& desc);
SKYLINE_API Microsoft::WRL::ComPtr<ID3DBlob> serializeBindingLayout(const RHI::BindingLayoutDesc& desc);
SKYLINE_API Microsoft::WRL::ComPtr<ID3D12RootSignature> createBindingLayout(
    ID3D12Device& device, const RHI::BindingLayoutDesc& desc);
}
