#pragma once
#include "skyline_api.h"

#include <directx/d3d12.h>
#include <wrl/client.h>

class Shader;

// 封裝 Direct3D 12 Root Signature，用來定義 Shader 可存取的 GPU 資源與綁定方式
class SKYLINE_API RootSignature
{
  public:
    // 使用單一 Static Sampler 設定與 CBV、SRV、UAV 數量初始化 Root Signature
    bool init(D3D12_FILTER samplerFilter, D3D12_TEXTURE_ADDRESS_MODE textureAddressModeU,
              D3D12_TEXTURE_ADDRESS_MODE textureAddressModeV, D3D12_TEXTURE_ADDRESS_MODE textureAddressModeW,
              UINT maxCbvDescriptor = 8, UINT maxSrvDescriptor = 32, UINT maxUavDescriptor = 8);

    // 使用外部 Static Sampler 陣列與 Descriptor Range 設定初始化 Root Signature
    bool init(D3D12_STATIC_SAMPLER_DESC* samplerDescArray, int numSampler, UINT maxCbvDescriptor = 8,
              UINT maxSrvDescriptor = 32, UINT maxUavDescriptor = 8,
              UINT offsetInDescriptorsFromTableStartCB = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND,
              UINT offsetInDescriptorsFromTableStartSRV = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND,
              UINT offsetInDescriptorsFromTableStartUAV = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);

    // 根據 Shader 物件內提供的資訊初始化 Root Signature
    bool init(Shader& shader);

    // 取得底層的 ID3D12RootSignature 原始指標
    [[nodiscard]]
    ID3D12RootSignature* get() const
    {
        return m_rootSignature.Get();
    }

  private:
    // 保存 Direct3D 12 的 Root Signature COM 物件
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
};
