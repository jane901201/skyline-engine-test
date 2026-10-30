#pragma once

#include <DirectXMath.h>

#include "constant_buffer.h"

struct MaterialConstants
{
    DirectX::XMFLOAT4 baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
    DirectX::XMFLOAT4 emissiveAndStrength = {0.0f, 0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT4 surface = {0.5f, 0.0f, 1.0f, 0.0f};
};

static_assert(sizeof(MaterialConstants) % 16 == 0);

class Material
{
  public:
    void init(const MaterialConstants& constants = {});
    void setConstants(const MaterialConstants& constants);
    D3D12_GPU_VIRTUAL_ADDRESS getConstantsAddress() const;

  private:
    ConstantBuffer m_constantBuffer;
    MaterialConstants m_constants{};
};
