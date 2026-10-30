#include "backends/d3d12/d3d12_interop.h"
#include <cstdlib>
#include <exception>

#include "graphics_engine.h"
#include "my_engine.h"
#include "render_pass/renderer.h"
#include "skyline_debugger.h"
#include "system.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, LPWSTR commandLine, int showCommand)
{
    try
    {
        SkylineDebugger::Initialize("DayX_MVP");

        initWindow(instance, previous, commandLine, showCommand, TEXT("DayX MVP"));

        GraphicsEngine graphicsEngine;
        graphicsEngine.init(g_hWnd, FRAME_BUFFER_W, FRAME_BUFFER_H);

        SkylineDebugger::ConfigureD3D12(Skyline::D3D12::nativeFrameDevice(graphicsEngine.getRhiFrame()));

        Renderer renderer(graphicsEngine);
        renderer.init();

        while (dispatchWindowMessage())
        {
            graphicsEngine.beginRender();
            renderer.render();
            graphicsEngine.endRender();

            SkylineDebugger::LogD3D12Messages(Skyline::D3D12::nativeFrameDevice(graphicsEngine.getRhiFrame()));
        }

        SkylineDebugger::Shutdown();
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        SkylineDebugger::ShowFatalError("DayX_MVP initialization failed", error);
        SkylineDebugger::Shutdown();
        return EXIT_FAILURE;
    }
    catch (...)
    {
        SkylineDebugger::ShowFatalError("DayX_MVP initialization failed", "An unknown fatal error occurred.");
        SkylineDebugger::Shutdown();
        return EXIT_FAILURE;
    }
}
