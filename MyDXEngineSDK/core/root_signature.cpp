#include "root_signature.h"

#include <array>                 
#include <d3dcompiler.h>         
#include <directx/d3dx12_core.h>
#include <stdexcept>
#include <string>
#include <vector>

#include "graphics_engine.h"
#include "shader.h"

    namespace
{
    // 場景固定使用四個 Root CBV，分別對應 b0～b3
    constexpr UINT sceneConstantBufferCount = 4;

    // 將 Root Signature 序列化失敗時的錯誤資訊整理成 std::string
    std::string makeSerializationError(ID3DBlob * errorBlob)
    {
        std::string message = "RootSignature: Failed to serialize the root signature.";

        // 如果有錯誤 Blob，就把 Direct3D 回傳的詳細錯誤文字附加到訊息中
        if (errorBlob != nullptr)
        {
            message += "\n";

            // 將 errorBlob 中的文字資料加入 message
            message.append(static_cast<const char*>(errorBlob->GetBufferPointer()), errorBlob->GetBufferSize());
        }

        return message;
    }
}

// 使用簡單的 Sampler 設定建立 Static Sampler，再交給完整版本的 init()
bool RootSignature::init(D3D12_FILTER samplerFilter, D3D12_TEXTURE_ADDRESS_MODE textureAddressModeU,
                         D3D12_TEXTURE_ADDRESS_MODE textureAddressModeV, D3D12_TEXTURE_ADDRESS_MODE textureAddressModeW,
                         UINT maxCbvDescriptor, UINT maxSrvDescriptor, UINT maxUavDescriptor)
{
    // 建立並將所有欄位初始化為 0 的 Static Sampler 描述
    D3D12_STATIC_SAMPLER_DESC samplerDesc{};

    // 設定 Texture 的取樣過濾方式
    samplerDesc.Filter = samplerFilter;

    // 設定 Texture 在 U、V、W 座標超出 0～1 時的處理方式
    samplerDesc.AddressU = textureAddressModeU;
    samplerDesc.AddressV = textureAddressModeV;
    samplerDesc.AddressW = textureAddressModeW;

    // 設定 Anisotropic Filtering 的最大倍率
    samplerDesc.MaxAnisotropy = 1;

    // 設定 Comparison Sampler 的比較方式為永遠通過
    samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;

    // 設定 BORDER Address Mode 使用的邊界顏色為不透明黑色
    samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;

    // 允許使用到最大的 Mipmap Level
    samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

    // 限制此 Static Sampler 只能由 Pixel Shader 存取
    samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // 將建立好的單一 Static Sampler 傳給完整版本的 init()
    return init(&samplerDesc, 1, maxCbvDescriptor, maxSrvDescriptor, maxUavDescriptor);
}

// 建立包含四個 Root CBV 與一個可選 Descriptor Table 的完整 Root Signature
bool RootSignature::init(D3D12_STATIC_SAMPLER_DESC* samplerDescArray, int numSampler, UINT maxCbvDescriptor,
                         UINT maxSrvDescriptor, UINT maxUavDescriptor, UINT offsetInDescriptorsFromTableStartCB,
                         UINT offsetInDescriptorsFromTableStartSRV, UINT offsetInDescriptorsFromTableStartUAV)
{
    // 確認 GraphicsEngine 與 Direct3D 12 Device 已完成初始化
    if (g_graphicsEngine == nullptr || g_graphicsEngine->getD3DDevice() == nullptr)
    {
        throw std::runtime_error("RootSignature: Graphics device is not initialized.");
    }

    // Static Sampler 數量不可為負數
    if (numSampler < 0)
    {
        throw std::invalid_argument("RootSignature: Sampler count must not be negative.");
    }

    // 當 Static Sampler 數量大於 0 時必須提供有效的 Sampler 陣列
    if (numSampler > 0 && samplerDescArray == nullptr)
    {
        throw std::invalid_argument("RootSignature: A sampler array is required when sampler count is non-zero.");
    }

    // 因為 b0～b3 固定作為 Root CBV，所以 CBV 最大數量不可少於 4
    if (maxCbvDescriptor < sceneConstantBufferCount)
    {
        throw std::invalid_argument("RootSignature: At least four constant-buffer slots are required.");
    }

    // 建立四個 Root CBV 加上一個 Descriptor Table 用的 Root Parameter 陣列
    std::array<CD3DX12_ROOT_PARAMETER, sceneConstantBufferCount + 1> rootParameters{};

    // 建立對應 b0～b3 的四個 Root CBV
    for (UINT registerIndex = 0; registerIndex < sceneConstantBufferCount; ++registerIndex)
    {
        rootParameters[registerIndex].InitAsConstantBufferView(
            // 使用目前索引作為 CBV 的 Shader Register
            registerIndex,

            // 使用 register space0
            0,

            // 允許所有 Shader Stage 存取此 Root CBV
            D3D12_SHADER_VISIBILITY_ALL);
    }

    // 保存 Descriptor Table 中需要使用的 CBV、SRV 與 UAV Descriptor Range
    std::vector<CD3DX12_DESCRIPTOR_RANGE> descriptorRanges;

    // 預先保留三個元素空間，避免新增 Range 時重複配置記憶體
    descriptorRanges.reserve(3);

    // 計算扣除 b0～b3 後還需要放進 Descriptor Table 的 CBV 數量
    const UINT additionalCbvDescriptorCount = maxCbvDescriptor - sceneConstantBufferCount;

    // 如果還有 b4 之後的 CBV，就建立對應的 Descriptor Range
    if (additionalCbvDescriptorCount > 0)
    {
        // 在 Descriptor Range 陣列尾端建立一個新的 Range
        descriptorRanges.emplace_back();

        // 將新的 Descriptor Range 設定成 CBV Range
        descriptorRanges.back().Init(
            // 指定此 Descriptor Range 的類型為 CBV
            D3D12_DESCRIPTOR_RANGE_TYPE_CBV,

            // 設定此 Range 包含的 CBV Descriptor 數量
            additionalCbvDescriptorCount,

            // 因為 b0～b3 已被使用，所以此 Range 從 b4 開始
            sceneConstantBufferCount,

            // 使用 register space0
            0,

            // 設定此 Range 在 Descriptor Table 中的起始偏移
            offsetInDescriptorsFromTableStartCB);
    }

    // 如果需要 SRV，就建立一個 SRV Descriptor Range
    if (maxSrvDescriptor > 0)
    {
        // 在 Descriptor Range 陣列尾端建立一個新的 Range
        descriptorRanges.emplace_back();

        // 將新的 Descriptor Range 設定成 SRV Range
        descriptorRanges.back().Init(
            // 指定此 Descriptor Range 的類型為 SRV
            D3D12_DESCRIPTOR_RANGE_TYPE_SRV,

            // 設定可以使用的 SRV Descriptor 數量
            maxSrvDescriptor,

            // SRV 從 t0 開始
            0,

            // 使用 register space0
            0,

            // 設定此 Range 在 Descriptor Table 中的起始偏移
            offsetInDescriptorsFromTableStartSRV);
    }

    // 如果需要 UAV，就建立一個 UAV Descriptor Range
    if (maxUavDescriptor > 0)
    {
        // 在 Descriptor Range 陣列尾端建立一個新的 Range
        descriptorRanges.emplace_back();

        // 將新的 Descriptor Range 設定成 UAV Range
        descriptorRanges.back().Init(
            // 指定此 Descriptor Range 的類型為 UAV
            D3D12_DESCRIPTOR_RANGE_TYPE_UAV,

            // 設定可以使用的 UAV Descriptor 數量
            maxUavDescriptor,

            // UAV 從 u0 開始
            0,

            // 使用 register space0
            0,

            // 設定此 Range 在 Descriptor Table 中的起始偏移
            offsetInDescriptorsFromTableStartUAV);
    }

    // Root Signature 一開始固定有四個 Root CBV
    UINT rootParameterCount = sceneConstantBufferCount;

    // 如果存在任何 Descriptor Range，就將它們組成同一個 Descriptor Table
    if (!descriptorRanges.empty())
    {
        rootParameters[sceneConstantBufferCount].InitAsDescriptorTable(
            // 設定 Descriptor Table 中包含多少個 Descriptor Range
            static_cast<UINT>(descriptorRanges.size()),

            // 傳入 Descriptor Range 陣列的起始位置
            descriptorRanges.data(),

            // 允許所有 Shader Stage 存取此 Descriptor Table
            D3D12_SHADER_VISIBILITY_ALL);

        // 因為新增了一個 Descriptor Table，所以 Root Parameter 數量加一
        ++rootParameterCount;
    }

    // 建立 Root Signature 的完整描述結構
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};

    // 設定 Root Signature 中實際使用的 Root Parameter 數量
    rootSignatureDesc.NumParameters = rootParameterCount;

    // 指向 Root Parameter 陣列的起始位置
    rootSignatureDesc.pParameters = rootParameters.data();

    // 設定 Root Signature 中的 Static Sampler 數量
    rootSignatureDesc.NumStaticSamplers = static_cast<UINT>(numSampler);

    // 指向 Static Sampler 描述陣列
    rootSignatureDesc.pStaticSamplers = samplerDescArray;

    // 允許 Input Assembler 使用 Input Layout
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // 保存序列化完成後的 Root Signature 二進位資料
    Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSignature;

    // 保存 Root Signature 序列化失敗時的詳細錯誤資訊
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    // 將 D3D12_ROOT_SIGNATURE_DESC 序列化成 CreateRootSignature() 可使用的 Binary Blob
    const HRESULT serializeResult = D3D12SerializeRootSignature(
        // 傳入要序列化的 Root Signature 描述
        &rootSignatureDesc,

        // 指定使用 Root Signature 1.0
        D3D_ROOT_SIGNATURE_VERSION_1,

        // 接收序列化成功後的 Root Signature Binary Blob
        serializedRootSignature.GetAddressOf(),

        // 接收序列化失敗時的錯誤資訊
        errorBlob.GetAddressOf());

    // 如果 Root Signature 序列化失敗就丟出包含詳細資訊的例外
    if (FAILED(serializeResult))
    {
        throw std::runtime_error(makeSerializationError(errorBlob.Get()));
    }

    // 使用序列化後的 Binary Blob 建立真正的 ID3D12RootSignature 物件
    const HRESULT createResult = g_graphicsEngine->getD3DDevice()->CreateRootSignature(
        // 使用一般單 GPU 的 NodeMask 0
        0,

        // 傳入序列化後 Root Signature Binary 的起始位置
        serializedRootSignature->GetBufferPointer(),

        // 傳入序列化後 Root Signature Binary 的資料大小
        serializedRootSignature->GetBufferSize(),

        // 釋放舊物件並取得可接收新 COM 指標的位址
        IID_PPV_ARGS(m_rootSignature.ReleaseAndGetAddressOf()));

    // 如果建立 ID3D12RootSignature 失敗就丟出例外
    if (FAILED(createResult))
    {
        throw std::runtime_error("RootSignature: Failed to create the root signature.");
    }

    return true;
}

// 從已編譯 Shader Bytecode 中取出 Embedded Root Signature 並建立 Root Signature
bool RootSignature::init(Shader& shader)
{
    // 確認 GraphicsEngine 與 Direct3D 12 Device 已完成初始化
    if (g_graphicsEngine == nullptr || g_graphicsEngine->getD3DDevice() == nullptr)
    {
        throw std::runtime_error("RootSignature: Graphics device is not initialized.");
    }

    // 確認 Shader 已經成功編譯並具有可讀取的 Bytecode
    if (shader.getCompiledBlob() == nullptr)
    {
        throw std::invalid_argument("RootSignature: A compiled shader is required.");
    }

    // 保存從 Shader Bytecode 中抽取出的 Root Signature Binary
    Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSignature;

    // 從 Shader Bytecode 中抽取 Embedded Root Signature
    const HRESULT extractResult = D3DGetBlobPart(
        // 傳入 Shader Binary 的起始位置
        shader.getCompiledBlob()->GetBufferPointer(),

        // 傳入 Shader Binary 的資料大小
        shader.getCompiledBlob()->GetBufferSize(),

        // 指定要抽取 Shader 中的 Root Signature 部分
        D3D_BLOB_ROOT_SIGNATURE,

        // 不使用額外的抽取 Flags
        0,

        // 接收抽取出的 Root Signature Binary Blob
        serializedRootSignature.GetAddressOf());

    // 如果 Shader 中沒有 Embedded Root Signature 或抽取失敗就丟出例外
    if (FAILED(extractResult))
    {
        throw std::runtime_error("RootSignature: The shader does not contain an embedded root signature.");
    }

    // 使用 Shader 中抽取出的 Root Signature Binary 建立 ID3D12RootSignature
    const HRESULT createResult = g_graphicsEngine->getD3DDevice()->CreateRootSignature(
        // 使用一般單 GPU 的 NodeMask 0
        0,

        // 傳入 Root Signature Binary 的起始位置
        serializedRootSignature->GetBufferPointer(),

        // 傳入 Root Signature Binary 的資料大小
        serializedRootSignature->GetBufferSize(),

        // 釋放舊物件並將新建立的 COM 指標寫入 m_rootSignature
        IID_PPV_ARGS(m_rootSignature.ReleaseAndGetAddressOf()));

    // 如果建立 Embedded Root Signature 失敗就丟出例外
    if (FAILED(createResult))
    {
        throw std::runtime_error("RootSignature: Failed to create the embedded root signature.");
    }

    return true;
}
