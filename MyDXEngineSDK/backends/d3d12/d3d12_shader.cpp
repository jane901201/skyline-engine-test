#include "d3d12_shader.h"
#include <Windows.h>
#include <dxcapi.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace Skyline::D3D12
{
namespace
{
    // 設定 Vertex Shader 使用的 Shader Model 版本
    constexpr const wchar_t* g_vsShaderModelName = L"vs_6_4";

    // 設定 Pixel Shader 使用的 Shader Model 版本
    constexpr const wchar_t* g_psShaderModelName = L"ps_6_4";

    // 設定 Compute Shader 使用的 Shader Model 版本
    constexpr const wchar_t* g_csShaderModelName = L"cs_6_4";

    // 將輸入的 Shader 路徑轉換成可實際存取的完整檔案路徑
    std::filesystem::path resolveShaderPath(const char* filePath)
    {
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
    std::wstring convertUtf8ToWide(const char* text)
    {
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
Microsoft::WRL::ComPtr<ID3DBlob> compileShader(const RHI::ShaderDesc& desc)
{
    if (desc.filePath.empty() || desc.entryPoint.empty() ||
        desc.filePath.find('\0') != std::string::npos || desc.entryPoint.find('\0') != std::string::npos)
        throw std::invalid_argument(desc.debugName + ": Shader path and entry point must be non-empty and contain no null characters.");
    const char* filePath = desc.filePath.c_str();
    const char* entryFuncName = desc.entryPoint.c_str();
    const wchar_t* shaderModel = nullptr;
    switch (desc.stage)
    {
    case RHI::ShaderStage::Vertex: shaderModel = g_vsShaderModelName; break;
    case RHI::ShaderStage::Pixel: shaderModel = g_psShaderModelName; break;
    case RHI::ShaderStage::Compute: shaderModel = g_csShaderModelName; break;
    default: throw std::invalid_argument(desc.debugName + ": Unsupported shader stage.");
    }
    // 取得 Shader 檔案的完整路徑
    const std::filesystem::path shaderPath = resolveShaderPath(filePath);

    // 將 Entry Function 名稱轉換成 DXC 使用的寬字元格式
    const std::wstring entryPoint = convertUtf8ToWide(entryFuncName);

    // 保存 DXC 工具介面
    Microsoft::WRL::ComPtr<IDxcUtils> utils;

    // 保存 DXC Shader Compiler 介面
    Microsoft::WRL::ComPtr<IDxcCompiler3> compiler;

    // 保存 DXC 預設 Include Handler
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler;

    // 建立 DXC Utils 物件
    throwIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils)), "Shader: Failed to create IDxcUtils.");

    // 建立 DXC Compiler 物件
    throwIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)),
                  "Shader: Failed to create IDxcCompiler3.");

    // 建立處理 HLSL include 的預設 Include Handler
    throwIfFailed(utils->CreateDefaultIncludeHandler(&includeHandler), "Shader: Failed to create include handler.");

    // 以 Binary 模式開啟 HLSL Shader 檔案
    std::ifstream sourceFile(shaderPath, std::ios::binary);

    // 如果 HLSL 檔案開啟失敗就丟出例外
    if (!sourceFile)
        throw std::runtime_error("Shader: Failed to open HLSL file.");

    // 將整個 HLSL 檔案內容讀入 char 陣列
    const std::vector<char> sourceBytes{std::istreambuf_iterator<char>{sourceFile}, std::istreambuf_iterator<char>{}};

    // 如果讀取 HLSL 檔案過程發生錯誤就丟出例外
    if (sourceFile.bad())
        throw std::runtime_error("Shader: Failed while reading HLSL file.");

    // 如果 HLSL 檔案沒有內容就丟出例外
    if (sourceBytes.empty())
        throw std::runtime_error("Shader: HLSL file is empty.");

    // 建立提供給 DXC 的 Shader Source Buffer
    DxcBuffer sourceBuffer{};

    // 指向 HLSL 原始碼資料
    sourceBuffer.Ptr = sourceBytes.data();

    // 設定 HLSL 原始碼資料大小
    sourceBuffer.Size = sourceBytes.size();

    // 指定 HLSL 原始碼使用 UTF-8 編碼
    sourceBuffer.Encoding = DXC_CP_UTF8;

    // 建立傳給 DXC Compiler 的編譯參數
    std::vector<LPCWSTR> arguments{shaderPath.c_str(),

                                   L"-E",
                                   entryPoint.c_str(),

                                   L"-T",
                                   shaderModel,

                                   L"-HV",
                                   L"2021",

                                   L"-Zpr"};

#ifdef _DEBUG
    // Debug 模式加入 Shader Debug 資訊
    arguments.push_back(L"-Zi");

    // 將 Debug 資訊直接嵌入編譯後的 Shader
    arguments.push_back(L"-Qembed_debug");

    // Debug 模式停用 Shader 最佳化
    arguments.push_back(L"-Od");
#else
    // Release 模式啟用最高等級 Shader 最佳化
    arguments.push_back(L"-O3");
#endif

    // 保存 DXC 編譯結果
    Microsoft::WRL::ComPtr<IDxcResult> result;

    // 使用 DXC 編譯 HLSL Shader
    throwIfFailed(compiler->Compile(&sourceBuffer, arguments.data(), static_cast<UINT32>(arguments.size()),
                                    includeHandler.Get(), IID_PPV_ARGS(&result)),
                  "Shader: DXC compiler invocation failed.");

    // 保存 DXC 編譯器輸出的錯誤與警告訊息
    Microsoft::WRL::ComPtr<IDxcBlobUtf8> errorBlob;

    // 從編譯結果中取得錯誤與警告訊息
    throwIfFailed(result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errorBlob), nullptr),
                  "Shader: Failed to retrieve compiler messages.");

    // 如果有編譯訊息就輸出到 Visual Studio Debug Output
    if (errorBlob != nullptr && errorBlob->GetStringLength() > 0)
    {
        OutputDebugStringA(errorBlob->GetStringPointer());
    }

    // 保存實際的 Shader 編譯結果狀態
    HRESULT compileStatus = S_OK;

    // 取得 Shader 是否成功編譯的 HRESULT
    throwIfFailed(result->GetStatus(&compileStatus), "Shader: Failed to retrieve compile status.");

    // 如果 Shader 編譯失敗就整理錯誤訊息並丟出例外
    if (FAILED(compileStatus))
    {
        std::string message = "Shader: Failed to compile ";

        // 將發生編譯錯誤的 Shader 路徑加入訊息
        message += shaderPath.string();

        // 如果 DXC 有詳細錯誤資訊就加入例外訊息
        if (errorBlob != nullptr && errorBlob->GetStringLength() > 0)
        {
            message += "\n";
            message += errorBlob->GetStringPointer();
        }

        throw std::runtime_error(message);
    }

    // 保存成功編譯後產生的 DXIL Blob
    Microsoft::WRL::ComPtr<IDxcBlob> compiledBlob;

    // 從 DXC 編譯結果中取得 DXIL Object
    throwIfFailed(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&compiledBlob), nullptr),
                  "Shader: Failed to retrieve DXIL.");

    // 保留編譯後位元碼的所有權，原生介面留在後端。
    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    throwIfFailed(compiledBlob.As(&blob), "Shader: Failed to expose DXIL as ID3DBlob.");

    return blob;
}
}
