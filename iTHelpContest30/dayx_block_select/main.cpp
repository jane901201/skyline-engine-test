#include "backends/d3d12/d3d12_interop.h"
#include "../rhi_example.h"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "graphics_engine.h"
#include "math/vector.h"
#include "mesh.h"
#include "my_engine.h"
#include "skyline_debugger.h"
#include "system.h"

namespace
{
// 定義傳給後處理 Shader 的常數，必須與 block_post.hlsl 的 PostConstants 完全一致
struct PostConstants
{
    // 儲存目前選取的色塊 key（色塊編號 + 1），0 表示沒有選取
    std::uint32_t selectedKey;

    // 對齊到 16 bytes
    std::uint32_t reserved[3];

    // 儲存選取色塊的疊色
    Vector4 highlightColor;

    // 儲存選取色塊的外框顏色
    Vector4 outlineColor;
};

static_assert(sizeof(PostConstants) == 48, "PostConstants must match the HLSL cbuffer layout.");
static_assert(sizeof(Vector4) == 16, "Block colors must match the HLSL float4 structured-buffer layout.");

// 設定水平方向的色塊數量
constexpr std::uint32_t ColumnCount = 10;

// 設定垂直方向的色塊數量
constexpr std::uint32_t RowCount = 10;

// 計算畫面上的色塊總數
constexpr std::uint32_t BlockCount = ColumnCount * RowCount;

// 建立所有色塊的顏色，沿用 DayX_ColorBlock 的顏色表與分散公式
std::array<Vector4, BlockCount> createBlockColors()
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

    // 建立並零初始化所有色塊顏色
    std::array<Vector4, BlockCount> colors{};

    // 逐列建立色塊顏色
    for (std::uint32_t row = 0; row < RowCount; ++row)
    {
        // 逐欄建立目前列中的色塊顏色
        for (std::uint32_t column = 0; column < ColumnCount; ++column)
        {
            // 將二維列欄座標轉換成一維色塊編號
            const std::uint32_t blockIndex = row * ColumnCount + column;

            // 依照色塊位置產生分散的顏色表 index
            colors[blockIndex] = palette[(blockIndex * 7 + row * 3) % palette.size()];
        }
    }

    // 回傳建立完成的色塊顏色
    return colors;
}

// 以 addVertex / addTriangle 組成單一 Mesh：每個色塊 4 個頂點、2 個連續三角形，
// 因此 SV_PrimitiveID / 2 即為色塊編號
void buildGridMesh(Mesh& mesh)
{
    // 根據欄數計算每個色塊在 NDC 中的寬度
    constexpr float blockWidth = 2.0f / static_cast<float>(ColumnCount);

    // 根據列數計算每個色塊在 NDC 中的高度
    constexpr float blockHeight = 2.0f / static_cast<float>(RowCount);

    // 逐列建立色塊
    for (std::uint32_t row = 0; row < RowCount; ++row)
    {
        // 逐欄建立目前列中的色塊
        for (std::uint32_t column = 0; column < ColumnCount; ++column)
        {
            // 計算色塊左右上下邊界在 NDC 中的位置
            const float left = -1.0f + blockWidth * static_cast<float>(column);
            const float right = left + blockWidth;
            const float top = 1.0f - blockHeight * static_cast<float>(row);
            const float bottom = top - blockHeight;

            // 依序加入左下、左上、右上、右下四個頂點，addVertex 回傳各自的 index
            const std::uint32_t bottomLeft = mesh.addVertex({{left, bottom, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}});
            const std::uint32_t topLeft = mesh.addVertex({{left, top, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}});
            const std::uint32_t topRight = mesh.addVertex({{right, top, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}});
            const std::uint32_t bottomRight =
                mesh.addVertex({{right, bottom, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}});

            // 兩個連續三角形組成一個色塊，順序決定了 SV_PrimitiveID
            mesh.addTriangle(bottomLeft, topLeft, topRight);
            mesh.addTriangle(bottomLeft, topRight, bottomRight);
        }
    }
}

// 保存隨視窗大小變動的離屏資源
struct OffscreenTargets
{
    // 場景 Pass 的輸出顏色
    std::shared_ptr<Skyline::RHI::Texture> sceneColor;

    // ID 輔助 Pass 的輸出（編碼後的色塊編號）
    std::shared_ptr<Skyline::RHI::Texture> blockIds;

    // 後處理綁定組，同時參照上面兩張紋理
    std::unique_ptr<Skyline::RHI::BindingSet> postBindings;

    // 記錄建立時的尺寸，用來偵測視窗縮放
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

// 依照目前視窗尺寸建立離屏紋理與後處理綁定組
OffscreenTargets createOffscreenTargets(Skyline::RHI::Device& device, const Skyline::RHI::BindingLayout& postLayout,
                                        const std::shared_ptr<Skyline::RHI::BufferView>& postConstantView,
                                        Skyline::RHI::Format sceneFormat, std::uint32_t width, std::uint32_t height)
{
    using namespace Skyline::RHI;

    // 場景顏色需要同時作為 Render Target 與後處理的取樣來源
    TextureDesc sceneDesc{width, height, sceneFormat, TextureUsage::RenderTarget | TextureUsage::Sampled,
                          ResourceState::RenderTarget, "Block scene color"};

    // ID 紋理額外加上 CopySource，讓滑鼠點擊時可以讀回
    TextureDesc idDesc{width,
                       height,
                       Format::Rgba8Unorm,
                       TextureUsage::RenderTarget | TextureUsage::Sampled | TextureUsage::CopySource,
                       ResourceState::RenderTarget,
                       "Block id"};

    OffscreenTargets targets;
    targets.width = width;
    targets.height = height;
    targets.sceneColor = device.createTexture(sceneDesc);
    targets.blockIds = device.createTexture(idDesc);

    // t0 = 場景顏色、t1 = 色塊 ID、b0 = 後處理常數
    BindingSetDesc bindings;
    bindings.layout = &postLayout;
    bindings.debugName = "Block post bindings";
    bindings.buffers = {{0, 0, postConstantView}};
    bindings.textures = {{0, 0, device.createTextureView(targets.sceneColor)},
                         {1, 0, device.createTextureView(targets.blockIds)}};
    targets.postBindings = device.createBindingSet(bindings);

    return targets;
}

// 從已讀回的 ID 像素中取出指定位置的色塊 key
std::uint32_t decodeBlockKey(const std::vector<std::byte>& pixels, std::uint32_t width, int x, int y)
{
    // Rgba8Unorm 每個像素 4 個 byte，順序為 R、G、B、A
    const std::size_t offset = (static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)) * 4;

    // 把 R、G、B 三個 byte 組回 key
    return std::to_integer<std::uint32_t>(pixels[offset]) | (std::to_integer<std::uint32_t>(pixels[offset + 1]) << 8) |
           (std::to_integer<std::uint32_t>(pixels[offset + 2]) << 16);
}
} // namespace

// Windows GUI 應用程式的進入點
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    // 捕捉初始化與執行期間可能發生的例外
    try
    {
        using namespace Skyline::RHI;

        // 初始化 Skyline Debugger 並在建立 D3D12 Device 前啟用 Debug Layer
        SkylineDebugger::Initialize("DayX_BlockSelect");

        // 建立應用程式視窗
        initWindow(hInstance, hPrevInstance, lpCmdLine, nCmdShow, TEXT("Block Select"));

        // 確認視窗已成功建立
        if (g_hWnd == nullptr)
            throw std::runtime_error("DayX_BlockSelect: Failed to create the application window.");

        // 建立並初始化 Graphics Engine
        GraphicsEngine graphicsEngine;
        graphicsEngine.init(g_hWnd, FRAME_BUFFER_W, FRAME_BUFFER_H);

        // 將建立完成的 D3D12 Device 交給 Skyline Debugger 設定
        SkylineDebugger::ConfigureD3D12(Skyline::D3D12::nativeFrameDevice(graphicsEngine.getRhiFrame()));

        auto& device = graphicsEngine.getRhiDevice();
        auto& frame = graphicsEngine.getRhiFrame();

        // 取得 Back Buffer 的配置，場景 Pass 沿用它的顏色格式
        const AttachmentLayout backBufferLayout = graphicsEngine.getBackBufferLayout();

        // 場景 Pass 輸出到與 Back Buffer 相同格式的離屏紋理，不使用深度
        AttachmentLayout sceneLayout{};
        sceneLayout.colorCount = 1;
        sceneLayout.colorFormats[0] = backBufferLayout.colorFormats[0];

        // ID 輔助 Pass 固定輸出 Rgba8Unorm，確保編碼值能精確還原
        AttachmentLayout idLayout{};
        idLayout.colorCount = 1;
        idLayout.colorFormats[0] = Format::Rgba8Unorm;

        // 場景 Pass 只需要 t0 的色塊顏色表
        BindingLayoutDesc sceneLayoutDesc;
        sceneLayoutDesc.debugName = "Block scene layout";
        sceneLayoutDesc.resources = {{BindingType::SampledResource, 0, 1}};
        auto sceneBindingLayout = device.createBindingLayout(sceneLayoutDesc);

        // ID 輔助 Pass 不需要任何綁定資源
        BindingLayoutDesc idLayoutDesc;
        idLayoutDesc.debugName = "Block id layout";
        auto idBindingLayout = device.createBindingLayout(idLayoutDesc);

        // 後處理 Pass 使用 b0 常數與 t0、t1 兩張紋理，使用 Load 取樣所以不需要 Sampler
        BindingLayoutDesc postLayoutDesc;
        postLayoutDesc.debugName = "Block post layout";
        postLayoutDesc.constantBuffers = {{0, 0, ShaderVisibility::Pixel}};
        postLayoutDesc.resources = {{BindingType::SampledResource, 0, 2}};
        auto postBindingLayout = device.createBindingLayout(postLayoutDesc);

        // 三個 Pass 的 Mesh 只讀取 POSITION，stride 由 Mesh::draw 以 sizeof(MeshVertex) 設定
        const std::vector<VertexAttribute> positionInput = {{"POSITION", 0, Format::Rgb32Float, 0, 0}};
        auto scenePipeline = Skyline::Examples::graphics(device, *sceneBindingLayout, sceneLayout,
                                                         "assets/shaders/block_scene.hlsl", positionInput);
        auto idPipeline = Skyline::Examples::graphics(device, *idBindingLayout, idLayout,
                                                      "assets/shaders/block_id.hlsl", positionInput);
        auto postPipeline = Skyline::Examples::graphics(device, *postBindingLayout, backBufferLayout,
                                                        "assets/shaders/block_post.hlsl");

        // 以 addVertex / addTriangle 組成單一 Mesh，再一次上傳，所有色塊共用同一組 Vertex / Index Buffer
        Mesh blockMesh;
        buildGridMesh(blockMesh);
        blockMesh.upload(device);

        // 建立存放所有色塊顏色的 Structured Buffer 與綁定組
        const auto blockColors = createBlockColors();
        const auto blockColorBytes = std::as_bytes(std::span{blockColors});
        auto blockColorBuffer = device.createBuffer({blockColorBytes.size(), BufferUsage::Sampled, MemoryType::Upload,
                                                     ResourceState::GenericRead, "Block colors"});
        blockColorBuffer->write(0, blockColorBytes);
        auto blockColorView = device.createBufferView(
            blockColorBuffer, {BindingType::SampledResource, 0, blockColorBytes.size(), sizeof(Vector4)});
        auto sceneBindings = device.createBindingSet({sceneBindingLayout.get(), {{0, 0, blockColorView}}});

        // 建立後處理常數，初始為未選取
        PostConstants postConstants{};
        postConstants.highlightColor = Vector4{1.0f, 1.0f, 1.0f, 0.25f};
        postConstants.outlineColor = Vector4{1.0f, 1.0f, 1.0f, 1.0f};
        auto postConstantBuffer =
            Skyline::Examples::constants(device, std::as_bytes(std::span{&postConstants, std::size_t{1}}));
        auto postConstantView =
            device.createBufferView(postConstantBuffer, {BindingType::ConstantBuffer, 0, sizeof(PostConstants)});

        // 離屏資源會在視窗縮放時重新建立
        OffscreenTargets targets;

        // 持續處理視窗訊息直到應用程式結束
        while (dispatchWindowMessage())
        {
            // 取得目前交換鏈尺寸
            const std::uint32_t width = frame.width();
            const std::uint32_t height = frame.height();

            // 視窗縮放後重新建立離屏紋理；上一個 Frame 已等待 GPU 完成，可以安全釋放舊資源
            if (targets.width != width || targets.height != height)
            {
                targets = createOffscreenTargets(device, *postBindingLayout, postConstantView,
                                                 sceneLayout.colorFormats[0], width, height);
            }

            // 上一個 Frame 已完成，可以安全更新常數
            postConstantBuffer->write(0, std::as_bytes(std::span{&postConstants, std::size_t{1}}));

            // 開始目前 Frame 的繪製流程
            frame.begin();
            auto& commands = frame.commands();

            // 離屏資源在上一個 Frame 結尾是 ShaderRead 狀態，先轉回 RenderTarget
            commands.transition(*targets.sceneColor, ResourceState::RenderTarget);
            commands.transition(*targets.blockIds, ResourceState::RenderTarget);

            // 離屏紋理與 Back Buffer 同尺寸，使用同一組 Viewport 與 Scissor
            const Viewport viewport{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)};
            const Scissor scissor{0, 0, static_cast<std::int32_t>(width), static_cast<std::int32_t>(height)};

            // 輔助 Pass：把每個像素屬於哪個色塊寫進 ID 紋理，背景為 0
            commands.setRenderTarget(*targets.blockIds);
            commands.setViewport(viewport, scissor);
            commands.clearColor(*targets.blockIds, {0.0f, 0.0f, 0.0f, 0.0f});
            commands.setPipeline(*idPipeline);
            blockMesh.draw(commands);

            // 場景 Pass：用同一個 Mesh 畫出所有色塊的顏色
            commands.setRenderTarget(*targets.sceneColor);
            commands.setViewport(viewport, scissor);
            commands.clearColor(*targets.sceneColor, {0.0f, 0.0f, 0.0f, 0.0f});
            commands.setPipeline(*scenePipeline);
            commands.setBindingSet(*sceneBindings);
            blockMesh.draw(commands);

            // 兩張離屏紋理轉成可取樣狀態，供後處理讀取
            commands.transition(*targets.sceneColor, ResourceState::ShaderRead);
            commands.transition(*targets.blockIds, ResourceState::ShaderRead);

            // 後處理 Pass：回到 Back Buffer，以全螢幕三角形合成最終畫面
            commands.setBackBufferTarget();
            commands.setViewport(viewport, scissor);
            commands.setPipeline(*postPipeline);
            commands.setBindingSet(*targets.postBindings);
            commands.draw(3);

            // 結束目前 Frame 並提交繪圖命令
            frame.end();

            // 輸出 D3D12 Debug Layer 產生的訊息
            SkylineDebugger::LogD3D12Messages(Skyline::D3D12::nativeFrameDevice(frame));

            // 處理滑鼠點擊：ID 紋理仍保留剛剛畫面上的內容，直接讀回點擊位置的像素
            int clickX = 0;
            int clickY = 0;
            if (consumeMouseClick(clickX, clickY) && clickX >= 0 && clickY >= 0 &&
                clickX < static_cast<int>(width) && clickY < static_cast<int>(height))
            {
                // readTexture 只能在獨立的 CommandContext 使用，並會同步等待 GPU 完成
                auto readback = device.createCommandContext();
                readback->begin();
                const auto pixels = readback->readTexture(*targets.blockIds);

                // 解碼點擊位置的 key，再次點擊同一個色塊則取消選取
                const std::uint32_t clickedKey = decodeBlockKey(pixels, width, clickX, clickY);
                postConstants.selectedKey = (clickedKey == postConstants.selectedKey) ? 0 : clickedKey;

                // 在視窗標題顯示目前選取的色塊位置
                std::wstring title = L"Block Select";
                if (postConstants.selectedKey != 0)
                {
                    const std::uint32_t blockIndex = postConstants.selectedKey - 1;
                    title += L" - selected (row " + std::to_wstring(blockIndex / ColumnCount) + L", column " +
                             std::to_wstring(blockIndex % ColumnCount) + L")";
                }
                SetWindowTextW(g_hWnd, title.c_str());
            }
        }

        SkylineDebugger::Shutdown();

        return EXIT_SUCCESS;
    }
    catch (const std::exception& exception)
    {
        SkylineDebugger::ShowFatalError("DayX_BlockSelect initialization failed", exception);
        SkylineDebugger::Shutdown();

        return EXIT_FAILURE;
    }
    catch (...)
    {
        SkylineDebugger::ShowFatalError("DayX_BlockSelect initialization failed", "An unknown fatal error occurred.");
        SkylineDebugger::Shutdown();

        return EXIT_FAILURE;
    }
}
