#include <directx/d3d12.h>

#pragma once

// 定義可註冊 Shader Resource View 的資源介面
class IShaderResource
{
  public:
    // 提供 virtual destructor，確保透過介面指標刪除物件時能正確呼叫衍生類別 destructor
    virtual ~IShaderResource() = default;

    // 將資源的 SRV 註冊到指定的 CPU Descriptor Handle
    virtual void registerShaderResourceView(D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle, int bufferNo) = 0;
};