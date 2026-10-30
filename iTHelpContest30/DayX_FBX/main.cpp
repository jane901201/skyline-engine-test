#include "../rhi_example.h"
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <stdexcept>
#include <string>

#include <DirectXMath.h>
#include <directx/d3dx12_core.h>
#include <wrl/client.h>

#include "graphics_engine.h"
#include "mesh.h"
#include "my_engine.h"
#include "pipeline_state.h"
#include "render_context.h"
#include "shader.h"
#include "system.h"

namespace
{
using Microsoft::WRL::ComPtr;

struct TransformConstants
{
    DirectX::XMFLOAT4X4 worldViewProjection;
};

// 取得執行檔所在目錄，方便從相對路徑尋找 FBX 資源
std::filesystem::path getExecutableDirectory()
{
    wchar_t executablePath[MAX_PATH]{};
    const DWORD pathLength = GetModuleFileNameW(nullptr, executablePath, _countof(executablePath));
    if (pathLength == 0 || pathLength == _countof(executablePath))
        throw std::runtime_error("DayX_ReadFBX: Failed to determine the executable directory.");

    return std::filesystem::path(executablePath).parent_path();
}

[[deprecated("Replaced by RHI ownership. Use RHI Device factories and Skyline::Examples helpers.")]]
ComPtr<ID3D12RootSignature> createRootSignature(ID3D12Device* device)
{
    if (device == nullptr)
        throw std::invalid_argument("DayX_ReadFBX: A graphics device is required.");

    D3D12_ROOT_PARAMETER transformParameter{};
    transformParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    transformParameter.Descriptor.ShaderRegister = 0;
    transformParameter.Descriptor.RegisterSpace = 0;
    transformParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_ROOT_SIGNATURE_DESC description{};
    description.NumParameters = 1;
    description.pParameters = &transformParameter;
    description.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> serializedRootSignature;
    ComPtr<ID3DBlob> errorBlob;
    const HRESULT serializeResult = D3D12SerializeRootSignature(
        &description, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSignature.GetAddressOf(), errorBlob.GetAddressOf());
    if (FAILED(serializeResult))
    {
        std::string message = "DayX_ReadFBX: Failed to serialize the root signature.";
        if (errorBlob != nullptr)
        {
            message += "\n";
            message.append(static_cast<const char*>(errorBlob->GetBufferPointer()), errorBlob->GetBufferSize());
        }
        throw std::runtime_error(message);
    }

    ComPtr<ID3D12RootSignature> rootSignature;
    if (FAILED(device->CreateRootSignature(0, serializedRootSignature->GetBufferPointer(),
                                           serializedRootSignature->GetBufferSize(),
                                           IID_PPV_ARGS(rootSignature.GetAddressOf()))))
    {
        throw std::runtime_error("DayX_ReadFBX: Failed to create the root signature.");
    }
    return rootSignature;
}

[[deprecated("Replaced by RHI ownership. Use RHI Device factories and Skyline::Examples helpers.")]]
PipelineState createPipelineState(ID3D12RootSignature* rootSignature, Shader& vertexShader, Shader& pixelShader)
{
    if (rootSignature == nullptr)
        throw std::invalid_argument("DayX_ReadFBX: A root signature is required.");

    // 定義 FBX 頂點資料中的 Position、Normal 與 UV Layout
    static constexpr D3D12_INPUT_ELEMENT_DESC inputElementDescriptions[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC description{};
    description.InputLayout = {inputElementDescriptions, _countof(inputElementDescriptions)};
    description.pRootSignature = rootSignature;
    description.VS = CD3DX12_SHADER_BYTECODE(vertexShader.getCompiledBlob());
    description.PS = CD3DX12_SHADER_BYTECODE(pixelShader.getCompiledBlob());
    description.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    description.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    description.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    description.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    description.DepthStencilState.DepthEnable = FALSE;
    description.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    description.DepthStencilState.StencilEnable = FALSE;
    description.SampleMask = UINT_MAX;
    description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    description.NumRenderTargets = 1;
    description.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.SampleDesc.Count = 1;

    PipelineState pipelineState;
    pipelineState.init(description);
    return pipelineState;
}

[[deprecated("Replaced by RHI ownership. Use RHI Device factories and Skyline::Examples helpers.")]]
ComPtr<ID3D12Resource> createTransformBuffer(ID3D12Device* device, const TransformConstants& constants)
{
    if (device == nullptr)
        throw std::invalid_argument("DayX_ReadFBX: A graphics device is required.");

    constexpr UINT64 allocationSize = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
    const D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    const D3D12_RESOURCE_DESC resourceDescription = CD3DX12_RESOURCE_DESC::Buffer(allocationSize);

    ComPtr<ID3D12Resource> transformBuffer;
    if (FAILED(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDescription,
                                               D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                               IID_PPV_ARGS(transformBuffer.GetAddressOf()))))
    {
        throw std::runtime_error("DayX_ReadFBX: Failed to create the transform constant buffer.");
    }

    void* mappedData = nullptr;
    const D3D12_RANGE readRange{0, 0};
    if (FAILED(transformBuffer->Map(0, &readRange, &mappedData)))
        throw std::runtime_error("DayX_ReadFBX: Failed to map the transform constant buffer.");

    std::memcpy(mappedData, &constants, sizeof(constants));
    transformBuffer->Unmap(0, nullptr);
    return transformBuffer;
}

TransformConstants createTransformConstants()
{
    using namespace DirectX;

    const XMMATRIX world = XMMatrixIdentity();
    const XMVECTOR eyePosition = XMVectorSet(0.0f, 2.25f, -7.5f, 1.0f);
    const XMVECTOR focusPosition = XMVectorZero();
    const XMVECTOR upDirection = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    const XMMATRIX view = XMMatrixLookAtLH(eyePosition, focusPosition, upDirection);
    const XMMATRIX projection =
        XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f),
                                 static_cast<float>(FRAME_BUFFER_W) / static_cast<float>(FRAME_BUFFER_H), 0.1f, 100.0f);

    TransformConstants constants{};
    XMStoreFloat4x4(&constants.worldViewProjection, world * view * projection);
    return constants;
}
} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    try
    {
        initWindow(hInstance, hPrevInstance, lpCmdLine, nCmdShow, TEXT("DayX Read FBX"));
        if (g_hWnd == nullptr)
            throw std::runtime_error("DayX_ReadFBX: Failed to create the application window.");

        GraphicsEngine graphicsEngine;
        graphicsEngine.init(g_hWnd, FRAME_BUFFER_W, FRAME_BUFFER_H);

        auto& device = graphicsEngine.getRhiDevice();
        Skyline::RHI::BindingLayoutDesc definition;
        definition.constantBuffers = {{0}};
        auto rootSignature = device.createBindingLayout(definition);
        auto pipelineState = Skyline::Examples::graphics(device, *rootSignature, graphicsEngine.getBackBufferLayout(),
            "assets/shaders/fbx.hlsl", {{"POSITION", 0, Skyline::RHI::Format::Rgb32Float},
            {"NORMAL", 0, Skyline::RHI::Format::Rgb32Float, 0, 12}, {"TEXCOORD", 0, Skyline::RHI::Format::Rg32Float, 0, 24}});
        const TransformConstants transformConstants = createTransformConstants();
        auto transformBuffer = Skyline::Examples::constants(device, std::as_bytes(std::span{&transformConstants, 1}));
        auto bindings = Skyline::Examples::bindConstants(device, *rootSignature, transformBuffer);

        // 建立用來保存 FBX 模型資料的 Mesh
        Mesh cube;

        // 組合 Cube.fbx 的完整檔案路徑
        const std::filesystem::path cubePath = getExecutableDirectory() / "assets" / "fbx" / "Cube.fbx";

        // 讀取 FBX 並建立對應的 Vertex Buffer 與 Index Buffer
        cube.initFromFbxFile(cubePath.string(), device);

        auto& renderContext = graphicsEngine.getRhiCommandContext();
        while (dispatchWindowMessage())
        {
            graphicsEngine.beginRender();


            // 套用能處理 FBX 頂點格式的 Pipeline State
            renderContext.setPipeline(*pipelineState);

            // FBX 載入時已三角化，因此以 Triangle List 方式繪製

            renderContext.setBindingSet(*bindings);

            // 繪製從 Cube.fbx 載入的所有 Mesh
            cube.draw(renderContext);

            graphicsEngine.endRender();
        }

        return EXIT_SUCCESS;
    }
    catch (const std::exception& exception)
    {
        MessageBoxA(nullptr, exception.what(), "DayX_ReadFBX initialization failed", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }
    catch (...)
    {
        MessageBoxA(nullptr, "An unknown fatal error occurred.", "DayX_ReadFBX initialization failed",
                    MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }
}
