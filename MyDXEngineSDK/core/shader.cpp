#include "shader.h"
#include <Windows.h>
#include <dxcapi.h>

#include "backends/d3d12/d3d12_shader.h"
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

    namespace
{
    // 設定 Vertex Shader 使用的 Shader Model 版本
    constexpr const wchar_t* g_vsShaderModelName = L"vs_6_4";

    // 設定 Pixel Shader 使用的 Shader Model 版本
    constexpr const wchar_t* g_psShaderModelName = L"ps_6_4";

    // 設定 Compute Shader 使用的 Shader Model 版本
    constexpr const wchar_t* g_csShaderModelName = L"cs_6_4";

    // 將輸入的 Shader 路徑轉換成可實際存取的完整檔案路徑
    [[deprecated("Unused after the RHI shader refactor. Use Skyline::D3D12::compileShader().")]]
    std::filesystem::path resolveShaderPath(const char* filePath)
    {
        // 確認 Shader 檔案路徑有效
        if (filePath == nullptr || filePath[0] == '\0')
        {
            throw std::invalid_argument("Shader: A shader file path is required.");
        }

        // 計算 UTF-8 路徑轉成寬字元後需要的字元數量
        const int requiredCharacterCount = MultiByteToWideChar(CP_UTF8, 0, filePath, -1, nullptr, 0);

        // 如果路徑字串轉換需求計算失敗就丟出例外
        if (requiredCharacterCount == 0)
        {
            throw std::runtime_error("Shader: Failed to convert shader path.");
        }

        // 建立足夠大小的寬字元字串
        std::wstring wideFilePath(static_cast<std::size_t>(requiredCharacterCount), L'\0');

        // 將 UTF-8 Shader 路徑轉換成 UTF-16 寬字元字串
        if (MultiByteToWideChar(CP_UTF8, 0, filePath, -1, wideFilePath.data(), requiredCharacterCount) == 0)
        {
            throw std::runtime_error("Shader: Failed to convert shader path.");
        }

        // 移除轉換後字串尾端多出的 Null 字元
        wideFilePath.pop_back();

        // 將寬字元字串轉成 filesystem 路徑物件
        std::filesystem::path shaderPath(wideFilePath);

        // 如果已經是絕對路徑就直接回傳
        if (shaderPath.is_absolute())
        {
            return shaderPath;
        }

        // 建立用來保存執行檔路徑的字元陣列
        wchar_t executablePath[MAX_PATH]{};

        // 取得目前執行檔的完整路徑
        const DWORD executablePathLength = GetModuleFileNameW(nullptr, executablePath, _countof(executablePath));

        // 如果無法取得有效的執行檔路徑就丟出例外
        if (executablePathLength == 0 || executablePathLength == _countof(executablePath))
        {
            throw std::runtime_error("Shader: Failed to determine "
                                     "the executable directory.");
        }

        // 將相對 Shader 路徑接到執行檔所在資料夾後回傳
        return std::filesystem::path(executablePath).parent_path() / shaderPath;
    }

    // 將 UTF-8 字串轉換成 UTF-16 寬字元字串
    [[deprecated("Unused after the RHI shader refactor. Use Skyline::D3D12::compileShader().")]]
    std::wstring convertUtf8ToWide(const char* text)
    {
        // 確認輸入文字不是空指標
        if (text == nullptr)
        {
            throw std::invalid_argument("Shader: Text must not be null.");
        }

        // 計算 UTF-8 字串轉換成寬字元後需要的字元數量
        const int requiredCharacterCount = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);

        // 如果字串轉換需求計算失敗就丟出例外
        if (requiredCharacterCount == 0)
        {
            throw std::runtime_error("Shader: UTF-8 conversion failed.");
        }

        // 建立足夠大小的寬字元字串
        std::wstring wideText(static_cast<std::size_t>(requiredCharacterCount), L'\0');

        // 將 UTF-8 字串實際轉換成 UTF-16
        if (MultiByteToWideChar(CP_UTF8, 0, text, -1, wideText.data(), requiredCharacterCount) == 0)
        {
            throw std::runtime_error("Shader: UTF-8 conversion failed.");
        }

        // 移除轉換後字串尾端多出的 Null 字元
        wideText.pop_back();

        return wideText;
    }

    // 如果 HRESULT 代表失敗就丟出指定的 runtime_error
    [[deprecated("Unused after the RHI shader refactor. Use Skyline::D3D12::compileShader().")]]
    void throwIfFailed(HRESULT hr, const char* message)
    {
        // 判斷 Direct3D 或 DXC API 呼叫是否失敗
        if (FAILED(hr))
        {
            throw std::runtime_error(message);
        }
    }
}

// 從 HLSL 檔案讀取原始碼並使用 DXC 編譯成指定 Shader Model 的 DXIL
void Shader::load(const char* filePath, const char* entryFuncName, const wchar_t* shaderModel)
{
    if (shaderModel == nullptr)
        throw std::invalid_argument("Shader: A shader model is required.");
    Skyline::RHI::ShaderStage stage;
    if (std::wcscmp(shaderModel, g_vsShaderModelName) == 0)
        stage = Skyline::RHI::ShaderStage::Vertex;
    else if (std::wcscmp(shaderModel, g_psShaderModelName) == 0)
        stage = Skyline::RHI::ShaderStage::Pixel;
    else if (std::wcscmp(shaderModel, g_csShaderModelName) == 0)
        stage = Skyline::RHI::ShaderStage::Compute;
    else
        throw std::invalid_argument("Shader: Unsupported shader model.");
    // 舊介面委派到後端，DXC 編譯只維護一份實作。
    m_blob = Skyline::D3D12::compileShader({filePath ? filePath : "", entryFuncName ? entryFuncName : "", stage});
    m_isInited = true;
}

// 使用 Vertex Shader 的 Shader Model 編譯指定 HLSL Entry Function
void Shader::loadVS(const char* filePath, const char* entryFuncName)
{
    load(filePath, entryFuncName, g_vsShaderModelName);
    m_stage = Skyline::RHI::ShaderStage::Vertex;
}

// 使用 Pixel Shader 的 Shader Model 編譯指定 HLSL Entry Function
void Shader::loadPS(const char* filePath, const char* entryFuncName)
{
    load(filePath, entryFuncName, g_psShaderModelName);
    m_stage = Skyline::RHI::ShaderStage::Pixel;
}

// 使用 Compute Shader 的 Shader Model 編譯指定 HLSL Entry Function
void Shader::loadCS(const char* filePath, const char* entryFuncName)
{
    load(filePath, entryFuncName, g_csShaderModelName);
    m_stage = Skyline::RHI::ShaderStage::Compute;
}
