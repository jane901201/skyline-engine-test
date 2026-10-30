#include "backends/d3d12/d3d12_interop.h"
#include "../rhi_example.h"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <directx/d3dx12_core.h>
#include <exception>
#include <stdexcept>

#include "graphics_engine.h"
#include "math/vector.h"
#include "my_engine.h"
#include "render_state.h"
#include "skyline_debugger.h"
#include "system.h"

namespace
{
// 定義單一頂點只包含二維位置資料
struct SimpleVertex
{
    // 儲存頂點在區域座標中的二維位置
    Vector2 position;
};

// 定義傳給 Shader 的單一色塊資料
struct ColorBlockData
{
    // 儲存色塊中心位置
    Vector2 center;

    // 儲存色塊寬高的一半
    Vector2 halfSize;

    // 儲存色塊的 RGBA 顏色
    Vector4 color;
};

static_assert(sizeof(ColorBlockData) == sizeof(float) * 8,
              "ColorBlockData must match the HLSL structured-buffer layout.");

// 設定水平方向的色塊數量
constexpr std::uint32_t ColumnCount = 10;

// 設定垂直方向的色塊數量
constexpr std::uint32_t RowCount = 10;

// 計算畫面上的色塊總數
constexpr std::uint32_t BlockCount = ColumnCount * RowCount;

// 建立所有色塊的位置、大小與顏色資料
std::array<ColorBlockData, BlockCount> createColorBlocks()
{
    // 定義色塊使用的顏色表
    static const std::array<Vector4, 20> palette = {
        Vector4{1.00f, 0.05f, 0.03f, 1.0f}, Vector4{0.05f, 0.08f, 1.00f, 1.0f}, Vector4{1.00f, 0.88f, 0.03f, 1.0f},
        Vector4{0.12f, 0.92f, 0.10f, 1.0f}, Vector4{0.95f, 0.20f, 0.68f, 1.0f}, Vector4{0.02f, 0.85f, 0.88f, 1.0f},
        Vector4{1.00f, 0.43f, 0.02f, 1.0f}, Vector4{0.45f, 0.05f, 0.62f, 1.0f}, Vector4{0.98f, 0.67f, 0.82f, 1.0f},
        Vector4{0.02f, 0.45f, 0.13f, 1.0f}, Vector4{0.33f, 0.27f, 0.92f, 1.0f}, Vector4{0.22f, 0.90f, 0.67f, 1.0f},
        Vector4{1.00f, 0.30f, 0.32f, 1.0f}, Vector4{0.04f, 0.09f, 0.48f, 1.0f}, Vector4{0.98f, 0.96f, 0.39f, 1.0f},
        Vector4{0.83f, 0.02f, 0.59f, 1.0f}, Vector4{0.02f, 0.59f, 0.56f, 1.0f}, Vector4{1.00f, 0.62f, 0.42f, 1.0f},
        Vector4{0.57f, 0.05f, 0.98f, 1.0f}, Vector4{0.54f, 0.63f, 0.12f, 1.0f},
    };

    // 根據欄數計算每個色塊在 NDC 中的寬度
    constexpr float blockWidth = 2.0f / static_cast<float>(ColumnCount);

    // 根據列數計算每個色塊在 NDC 中的高度
    constexpr float blockHeight = 2.0f / static_cast<float>(RowCount);

    // 計算每個色塊從中心到邊界的距離
    const Vector2 halfSize{blockWidth * 0.5f, blockHeight * 0.5f};

    // 建立並零初始化所有色塊資料
    std::array<ColorBlockData, BlockCount> blocks{};

    // 逐列建立色塊
    for (std::uint32_t row = 0; row < RowCount; ++row)
    {
        // 逐欄建立目前列中的色塊
        for (std::uint32_t column = 0; column < ColumnCount; ++column)
        {
            // 將二維列欄座標轉換成一維陣列 index
            const std::uint32_t blockIndex = row * ColumnCount + column;

            // 依照色塊位置產生分散的顏色表 index
            const std::uint32_t colorIndex = (blockIndex * 7 + row * 3) % palette.size();

            // 計算目前色塊在 NDC 中的中心位置
            const Vector2 center{-1.0f + blockWidth * (static_cast<float>(column) + 0.5f),
                                 1.0f - blockHeight * (static_cast<float>(row) + 0.5f)};

            // 寫入目前色塊的位置、大小與顏色
            blocks[blockIndex] = {center, halfSize, palette[colorIndex]};
        }
    }

    // 回傳建立完成的色塊資料
    return blocks;
}

// 初始化 Root Signature
[[deprecated("Replaced by RHI factories. Use Skyline::Examples::graphics().")]]
void initRootSignature(RootSignature& rootSignature)
{
    // 使用線性取樣與 Wrap addressing mode 建立 Root Signature
    rootSignature.init(D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                       D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
}

// 建立繪製色塊使用的 Graphics Pipeline State 設定
[[deprecated("Replaced by RHI factories. Use Skyline::Examples::graphics().")]]
D3D12_GRAPHICS_PIPELINE_STATE_DESC createColorBlockPipelineStateDescription(ID3D12RootSignature* rootSignature,
                                                                            ID3DBlob* vertexShader,
                                                                            ID3DBlob* pixelShader)
{
    if (rootSignature == nullptr)
        throw std::invalid_argument("ColorBlockPipeline: Root signature is required.");

    if (vertexShader == nullptr)
        throw std::invalid_argument("ColorBlockPipeline: Vertex shader is required.");

    if (pixelShader == nullptr)
        throw std::invalid_argument("ColorBlockPipeline: Pixel shader is required.");

    // 定義 Vertex Buffer 中 POSITION attribute 的格式
    static constexpr D3D12_INPUT_ELEMENT_DESC inputElementDescriptions[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    // 建立並零初始化 Graphics Pipeline State 設定
    D3D12_GRAPHICS_PIPELINE_STATE_DESC description{};

    // 設定 Vertex Shader 的 Input Layout
    description.InputLayout = {inputElementDescriptions, _countof(inputElementDescriptions)};

    // 指定 Pipeline 使用的 Root Signature
    description.pRootSignature = rootSignature;

    // 設定 Vertex Shader bytecode
    description.VS = CD3DX12_SHADER_BYTECODE(vertexShader);

    // 設定 Pixel Shader bytecode
    description.PS = CD3DX12_SHADER_BYTECODE(pixelShader);

    // 使用 D3D12 預設 Rasterizer State
    description.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

    // 關閉背面剔除以繪製所有三角形方向
    description.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    // 使用 D3D12 預設 Blend State
    description.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

    // 使用 D3D12 預設 Depth Stencil State
    description.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

    // 關閉 Depth Test
    description.DepthStencilState.DepthEnable = FALSE;

    // 禁止寫入 Depth Buffer
    description.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    // 關閉 Stencil Test
    description.DepthStencilState.StencilEnable = FALSE;

    // 啟用所有 sample
    description.SampleMask = UINT_MAX;

    // 指定 Pipeline 接收 Triangle primitive
    description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    // 設定使用一個 Render Target
    description.NumRenderTargets = 1;

    // 設定 Render Target 的像素格式
    description.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

    // 設定 Depth Stencil Buffer 的像素格式
    description.DSVFormat = DXGI_FORMAT_D32_FLOAT;

    // 關閉 MSAA 並使用單一 sample
    description.SampleDesc.Count = 1;

    // 回傳完成的 Pipeline State 設定
    return description;
}

// 建立色塊繪製使用的 Pipeline State
[[deprecated("Replaced by RHI factories. Use Skyline::Examples::graphics().")]]
void initPipelineState(PipelineState& pipelineState, RootSignature& rootSignature, Shader& vertexShader,
                       Shader& pixelShader)
{
    // 根據 Root Signature 與 Shader 建立 Pipeline State 描述
    const auto description = createColorBlockPipelineStateDescription(
        rootSignature.get(), vertexShader.getCompiledBlob(), pixelShader.getCompiledBlob());

    // 使用描述資料建立 Pipeline State Object
    pipelineState.init(description);
}
} // namespace

// Windows GUI 應用程式的進入點
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    // 捕捉初始化與執行期間可能發生的例外
    try
    {
        // 初始化 Skyline Debugger 並在建立 D3D12 Device 前啟用 Debug Layer
        SkylineDebugger::Initialize("DayX_ColorBlock");

        // 建立應用程式視窗
        initWindow(hInstance, hPrevInstance, lpCmdLine, nCmdShow, TEXT("Color Block"));

        // 確認視窗已成功建立
        if (g_hWnd == nullptr)
            throw std::runtime_error("DayX_ColorBlock: Failed to create the application window.");

        // 建立 Graphics Engine
        GraphicsEngine graphicsEngine;

        // 初始化 D3D12 Device、Swap Chain 與相關圖形資源
        graphicsEngine.init(g_hWnd, FRAME_BUFFER_W, FRAME_BUFFER_H);

        // 將建立完成的 D3D12 Device 交給 Skyline Debugger 設定
        SkylineDebugger::ConfigureD3D12(Skyline::D3D12::nativeFrameDevice(graphicsEngine.getRhiFrame()));

        // 建立 Root Signature
        auto& device = graphicsEngine.getRhiDevice();
        Skyline::RHI::BindingLayoutDesc definition;
        definition.resources = {{Skyline::RHI::BindingType::SampledResource, 0, 1}};
        auto rootSignature = device.createBindingLayout(definition);
        auto pipelineState = Skyline::Examples::graphics(device, *rootSignature, graphicsEngine.getBackBufferLayout(),
            "assets/shaders/color_block.hlsl", {{"POSITION", 0, Skyline::RHI::Format::Rg32Float}});
        // 定義所有色塊共用的單位四邊形頂點
        const SimpleVertex vertices[] = {{{-1.0f, -1.0f}}, {{-1.0f, 1.0f}}, {{1.0f, 1.0f}}, {{1.0f, -1.0f}}};

        // 建立四邊形的 Vertex Buffer


        // 定義組成四邊形兩個三角形的 index
        const std::uint16_t indices[] = {0, 1, 2, 0, 2, 3};

        // 建立四邊形的 Index Buffer
        auto [blockVertexBuffer, blockIndexBuffer] = Skyline::Examples::geometry(device, std::as_bytes(std::span{vertices}), std::as_bytes(std::span{indices}));

        // 產生所有色塊的 instance 資料
        auto blocks = createColorBlocks();

        // 建立存放所有色塊資料的 Structured Buffer
        const auto blockData = std::as_bytes(std::span{blocks});
        auto colorBlockBuffer = device.createBuffer({blockData.size(), Skyline::RHI::BufferUsage::Sampled,
            Skyline::RHI::MemoryType::Upload, Skyline::RHI::ResourceState::GenericRead});
        colorBlockBuffer->write(0, blockData);
        auto view = device.createBufferView(colorBlockBuffer, {Skyline::RHI::BindingType::SampledResource, 0, blockData.size(), sizeof(ColorBlockData)});
        auto bindings = device.createBindingSet({rootSignature.get(), {{0, 0, view}}});


        // 取得 Graphics Engine 使用的 RenderContext
        auto& renderContext = graphicsEngine.getRhiCommandContext();

        // 將目前的 Command List 設定到 RenderContext


        // 持續處理視窗訊息直到應用程式結束
        while (dispatchWindowMessage())
        {
            // 開始目前 Frame 的繪製流程
            graphicsEngine.beginRender();

            // 綁定目前繪圖使用的 Root Signature


            // 綁定色塊繪製使用的 Pipeline State
            renderContext.setPipeline(*pipelineState);

            // 綁定目前 Back Buffer 對應的 Descriptor Heap 與資源表
            renderContext.setBindingSet(*bindings);

            // 指定輸入的 index 以 Triangle List 方式組成三角形


            // 綁定四邊形的 Vertex Buffer
            renderContext.setVertexBuffer(*blockVertexBuffer, sizeof(vertices[0]));

            // 綁定四邊形的 Index Buffer
            renderContext.setIndexBuffer(*blockIndexBuffer, Skyline::RHI::IndexType::Uint16);

            // 使用同一組四邊形 geometry 執行 BlockCount 次 instanced drawing
            renderContext.drawIndexed(_countof(indices), BlockCount);

            // 結束目前 Frame 並提交繪圖命令
            graphicsEngine.endRender();

            // 輸出 D3D12 Debug Layer 產生的訊息
            SkylineDebugger::LogD3D12Messages(Skyline::D3D12::nativeFrameDevice(graphicsEngine.getRhiFrame()));
        }

        SkylineDebugger::Shutdown();

        return EXIT_SUCCESS;
    }
    catch (const std::exception& exception)
    {
        SkylineDebugger::ShowFatalError("DayX_ColorBlock initialization failed", exception);
        SkylineDebugger::Shutdown();

        return EXIT_FAILURE;
    }
    catch (...)
    {
        SkylineDebugger::ShowFatalError("DayX_ColorBlock initialization failed", "An unknown fatal error occurred.");
        SkylineDebugger::Shutdown();

        return EXIT_FAILURE;
    }
}
