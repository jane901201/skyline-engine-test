#include <catch2/catch_test_macros.hpp>

#include "graphics_engine.h"
#include "render_pass/renderer.h"
#include "root_signature.h"
#include "shader.h"
#include "ui/imgui_system.h"

#include <directx/d3d12sdklayers.h>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <vector>

TEST_CASE("RHI pipeline adapters render the sky cloud path across resize", "[gpu][rhi]")
{
    // 使用隱藏視窗驗證實際引擎路徑，不開啟互動式範例視窗。
    std::unique_ptr<std::remove_pointer_t<HWND>, decltype(&DestroyWindow)> window(
        CreateWindowExW(0, L"STATIC", L"RHI integration test", WS_OVERLAPPEDWINDOW,
                        0, 0, 64, 64, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr), &DestroyWindow);
    REQUIRE(window != nullptr);

    Microsoft::WRL::ComPtr<ID3D12Debug> debug;
    REQUIRE(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))));
    debug->EnableDebugLayer();

    GraphicsEngine engine;
    REQUIRE_THROWS_AS(engine.getRhiDevice(), std::logic_error);
    REQUIRE_THROWS_AS(engine.getBackBufferLayout(), std::logic_error);
    REQUIRE(engine.init(window.get(), 64, 64));
    const auto initialLayout = engine.getBackBufferLayout();
    REQUIRE(initialLayout.colorCount == 1);
    REQUIRE(initialLayout.colorFormats[0] == Skyline::RHI::Format::Rgba8Unorm);
    REQUIRE(initialLayout.depthFormat == Skyline::RHI::Format::D32Float);
    REQUIRE(initialLayout.samples.count == 1);

    Microsoft::WRL::ComPtr<ID3D12InfoQueue> messages;
    REQUIRE(SUCCEEDED(engine.getD3DDevice()->QueryInterface(IID_PPV_ARGS(&messages))));

    RootSignature layout;
    REQUIRE(layout.init(D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                         D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP));
    Shader vs, ps, missing;
    vs.loadVS(RHI_TEST_SHADER_PATH, "VSMain");
    ps.loadPS(RHI_TEST_SHADER_PATH, "PSMain");
    PipelineState pipeline;
    auto desc = Skyline::RHI::GraphicsPipelineDesc::fullscreen();
    desc.attachments = initialLayout;
    desc.vertexInput = {{"POSITION", 0, Skyline::RHI::Format::Rgb32Float}};
    REQUIRE_THROWS_AS(pipeline.init(desc, engine, layout, missing, ps), std::invalid_argument);
    REQUIRE_THROWS_AS(pipeline.init(desc, engine, layout, ps, vs), std::invalid_argument);
    REQUIRE_NOTHROW(pipeline.init(desc, engine, layout, vs, ps));
    const auto* original = pipeline.get();
    REQUIRE_THROWS_AS(pipeline.init(desc, engine, layout, ps, vs), std::invalid_argument);
    REQUIRE(pipeline.get() == original);

    auto& device = engine.getRhiDevice();
    auto rhiLayout = device.createBindingLayout({});
    auto rhiVs = device.createShader({RHI_TEST_SHADER_PATH, "VSMain", Skyline::RHI::ShaderStage::Vertex});
    auto rhiPs = device.createShader({RHI_TEST_SHADER_PATH, "PSMain", Skyline::RHI::ShaderStage::Pixel});
    auto rhiDesc = Skyline::RHI::GraphicsPipelineDesc::fullscreen();
    rhiDesc.vertexInput = {{"POSITION", 0, Skyline::RHI::Format::Rgb32Float}};
    rhiDesc.vertexShader = rhiVs.get();
    rhiDesc.pixelShader = rhiPs.get();
    rhiDesc.bindingLayout = rhiLayout.get();
    rhiDesc.attachments = initialLayout;
    auto ownedPipeline = device.createGraphicsPipeline(rhiDesc);
    // 驗證一般範例使用的中立管線命令轉接，隨後由 Renderer 設定場景管線。
    engine.beginRender();
    REQUIRE_THROWS_AS(engine.getRenderContext().setComputePipeline(*ownedPipeline), std::invalid_argument);
    engine.getRenderContext().setGraphicsPipeline(*ownedPipeline);
    engine.endRender();

    Renderer renderer(engine);
    auto settings = renderer.getCloudSettings();
    settings.cloudCount = 64;
    settings.rayMarchSteps = 8;
    settings.lightMarchSteps = 2;
    renderer.setCloudSettings(settings);
    REQUIRE_NOTHROW(renderer.init());
    Skyline::UI::ImGuiSystem ui;
    REQUIRE_NOTHROW(ui.initialize(window.get(), engine));
    for (int frame = 0; frame < 3; ++frame)
    {
        engine.beginRender();
        renderer.render();
        ui.beginFrame();
        ui.beginWindow("RHI integration");
        float color[3]{1, 0, 0};
        ui.colorSliders("Color", color);
        ui.endWindow();
        ui.render();
        engine.endRender();
    }
    engine.resize(96, 64);
    const auto resizedLayout = engine.getBackBufferLayout();
    REQUIRE(resizedLayout.colorFormats == initialLayout.colorFormats);
    REQUIRE(resizedLayout.depthFormat == initialLayout.depthFormat);
    engine.beginRender();
    renderer.render();
    engine.endRender();
    ui.shutdown();

    // 已完成提交與等待後，檢查初始化及影格命令是否產生原生錯誤。
    for (UINT64 index = 0; index < messages->GetNumStoredMessagesAllowedByRetrievalFilter(); ++index)
    {
        SIZE_T size = 0;
        REQUIRE(SUCCEEDED(messages->GetMessage(index, nullptr, &size)));
        std::vector<std::byte> storage(size);
        auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
        REQUIRE(SUCCEEDED(messages->GetMessage(index, message, &size)));
        INFO(message->pDescription);
        REQUIRE(message->Severity != D3D12_MESSAGE_SEVERITY_ERROR);
        REQUIRE(message->Severity != D3D12_MESSAGE_SEVERITY_CORRUPTION);
    }
}
