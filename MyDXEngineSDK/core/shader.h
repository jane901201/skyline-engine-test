#pragma once
#include "skyline_api.h"
#include "rhi/types.h"
#include <directx/d3dcommon.h>
#include <wrl/client.h>


class SKYLINE_API Shader
{
public:
    void loadPS(const char* filePath, const char* entryFuncName);
    void loadVS(const char* filePath, const char* entryFuncName);
    void loadCS(const char* filePath, const char* entryFuncName);

    [[nodiscard]] Skyline::RHI::ShaderStage getStage() const noexcept { return m_stage; }

    [[nodiscard]]
    ID3DBlob* getCompiledBlob() const
    {
        return m_blob.Get();
    }


private:
    void load(const char* filePath, const char* entryFuncName,
        const wchar_t* shaderModel);

    Microsoft::WRL::ComPtr<ID3DBlob> m_blob; //編譯完成的 shader data
    bool m_isInited = false;
    Skyline::RHI::ShaderStage m_stage = Skyline::RHI::ShaderStage::Unknown;
};
