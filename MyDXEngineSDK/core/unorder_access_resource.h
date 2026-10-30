#pragma once

#include "shader_resource.h"

class IUnorderAccessResource : public IShaderResource
{
  public:
    ~IUnorderAccessResource() override = default;
    virtual void RegisterUnorderAccessView(D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle, int bufferNo) = 0;
};
