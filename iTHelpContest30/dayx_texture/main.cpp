#include "../rhi_example.h"
#include "graphics_engine.h"
#include "skyline_debugger.h"
#include "system.h"
#include <array>
#include <cstdlib>
#include <exception>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, LPWSTR commandLine, int show)
{
    try
    {
        SkylineDebugger::Initialize("DayX_Texture");
        initWindow(instance, previous, commandLine, show, TEXT("RHI Texture"));
        GraphicsEngine engine;
        engine.init(g_hWnd, FRAME_BUFFER_W, FRAME_BUFFER_H);
        using namespace Skyline::RHI;
        auto& device = engine.getRhiDevice();
        BindingLayoutDesc definition;
        definition.resources = {{BindingType::SampledResource, 0, 1}};
        definition.samplers = {{0, 0, ShaderVisibility::Pixel, SamplerFilter::Point,
            AddressMode::Clamp, AddressMode::Clamp, AddressMode::Clamp}};
        auto layout = device.createBindingLayout(definition);
        auto pipeline = Skyline::Examples::graphics(device, *layout, engine.getBackBufferLayout(),
            "assets/shaders/texture.hlsl", {{"POSITION", 0, Format::Rg32Float}});
        const std::array<float, 8> vertices{-0.8f, -0.8f, -0.8f, 0.8f, 0.8f, 0.8f, 0.8f, -0.8f};
        const std::array<std::uint16_t, 6> indices{0, 1, 2, 0, 2, 3};
        auto [vb, ib] = Skyline::Examples::geometry(device, std::as_bytes(std::span{vertices}), std::as_bytes(std::span{indices}));
        auto texture = device.createTexture({2, 2});
        const std::array<std::uint8_t, 16> pixels{255, 40, 40, 255, 40, 255, 40, 255,
            40, 40, 255, 255, 255, 255, 40, 255};
        auto upload = device.createCommandContext();
        upload->begin();
        upload->uploadTexture(*texture, std::as_bytes(std::span{pixels}), 8);
        upload->submitAndWait();
        BindingSetDesc bindings;
        bindings.layout = layout.get(); bindings.textures = {{0, 0, device.createTextureView(texture)}};
        auto set = device.createBindingSet(bindings);
        auto& frame = engine.getRhiFrame();
        while (dispatchWindowMessage())
        {
            frame.begin();
            auto& commands = frame.commands();
            commands.setPipeline(*pipeline);
            commands.setBindingSet(*set);
            commands.setVertexBuffer(*vb, 2 * sizeof(float));
            commands.setIndexBuffer(*ib, IndexType::Uint16);
            commands.drawIndexed(6);
            frame.end();
        }
        SkylineDebugger::Shutdown();
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        SkylineDebugger::ShowFatalError("DayX_Texture failed", error);
        SkylineDebugger::Shutdown();
        return EXIT_FAILURE;
    }
}
