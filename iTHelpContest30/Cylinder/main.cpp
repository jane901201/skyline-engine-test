#include <cstdlib>
#include <directx/d3dx12_core.h>
#include <exception>
#include <stdexcept>

#include "graphics_engine.h"
#include "my_engine.h"
#include "system.h"

namespace
{
struct SimpleVertex
{
    float x, y, z;
};

void initRootSignature(RootSignature& rs)
{
    rs.init(D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP);
}

D3D12_GRAPHICS_PIPELINE_STATE_DESC createTrianglePipelineStateDescription(ID3D12RootSignature* rootSignature,
                                                                          ID3DBlob* vertexShader, ID3DBlob* pixelShader)
{
    if (rootSignature == nullptr)
        throw std::invalid_argument("TrianglePipeline: Root signature is required.");
    if (vertexShader == nullptr)
        throw std::invalid_argument("TrianglePipeline: Vertex shader is required.");
    if (pixelShader == nullptr)
        throw std::invalid_argument("TrianglePipeline: Pixel shader is required.");

    static constexpr D3D12_INPUT_ELEMENT_DESC inputElementDescriptions[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC description{};
    description.InputLayout = {inputElementDescriptions, _countof(inputElementDescriptions)};
    description.pRootSignature = rootSignature;
    description.VS = CD3DX12_SHADER_BYTECODE(vertexShader);
    description.PS = CD3DX12_SHADER_BYTECODE(pixelShader);
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
    description.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    description.SampleDesc.Count = 1;
    return description;
}

void initPipelineState(PipelineState& pipelineState, RootSignature& rs, Shader& vs, Shader& ps)
{
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc =
        createTrianglePipelineStateDescription(rs.get(), vs.getCompiledBlob(), ps.getCompiledBlob());
    pipelineState.init(psoDesc);
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    try
    {
        // 基礎圖形初始化
        initWindow(hInstance, hPrevInstance, lpCmdLine, nCmdShow, TEXT("Game"));
        if (g_hWnd == nullptr)
            throw std::runtime_error("Cylinder: Failed to create the application window.");

        GraphicsEngine graphicsEngine;
        graphicsEngine.init(g_hWnd, FRAME_BUFFER_W, FRAME_BUFFER_H);

        // 1. RootSignature 初始化
        RootSignature rootSignature;
        initRootSignature(rootSignature);

        Shader vs, ps;
        vs.loadVS("cylinder.hlsl", "VSMain");
        ps.loadPS("cylinder.hlsl", "PSMain");

        PipelineState pipelineState;
        initPipelineState(pipelineState, rootSignature, vs, ps);

        // 三角形的頂點初始化
        SimpleVertex vertices[] = {
            {-0.5f, -0.5f, 0.0f},
            {0.0f, 0.5f, 0.0f},
            {0.5f, -0.5f, 0.0f},
        };

        VertexBuffer triangleVB;
        triangleVB.init(_countof(vertices), sizeof(vertices[0]));
        triangleVB.copy(vertices);

        uint16_t indices[] = {0, 1, 2};

        IndexBuffer triangleIB;
        triangleIB.init(_countof(indices), sizeof(indices[0]));
        triangleIB.copy(indices);

        RenderContext& renderContext = graphicsEngine.getRenderContext();
        renderContext.SetCommandList(graphicsEngine.getCommandList());

        while (dispatchWindowMessage())
        {
            graphicsEngine.beginRender();

            renderContext.setRootSignature(rootSignature);
            renderContext.setPipelineState(pipelineState);
            renderContext.setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            renderContext.setVertexBuffer(triangleVB);
            renderContext.setIndexBuffer(triangleIB);
            renderContext.drawIndexed(3);

            graphicsEngine.endRender();
        }

        return EXIT_SUCCESS;
    }
    catch (const std::exception& exception)
    {
        MessageBoxA(nullptr, exception.what(), "Cylinder initialization failed", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }
    catch (...)
    {
        MessageBoxA(nullptr, "An unknown fatal error occurred.", "DayX_Triangle initialization failed",
                    MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }
}