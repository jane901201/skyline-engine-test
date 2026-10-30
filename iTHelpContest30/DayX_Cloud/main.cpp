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
        SkylineDebugger::Initialize("DayX_Cloud");

        initWindow(instance, previous, commandLine, showCommand, TEXT("DayX_Cloud"));

        GraphicsEngine graphicsEngine;
        graphicsEngine.init(g_hWnd, FRAME_BUFFER_W, FRAME_BUFFER_H);

        SkylineDebugger::ConfigureD3D12(Skyline::D3D12::nativeFrameDevice(graphicsEngine.getRhiFrame()));

        Renderer renderer(graphicsEngine);
        //設定雲相關的參數
        CloudSettings cloudSettings;
        cloudSettings.cloudCount = 128;
        cloudSettings.bottomHeight = 6.0f;
        cloudSettings.topHeight = 16.0f;
        cloudSettings.cellSize = 4.5f;
        cloudSettings.densityScale = 0.18f;
        cloudSettings.speed = 0.7f;
        cloudSettings.rayMarchSteps = 48;
        cloudSettings.extinction = 0.18f;
        cloudSettings.color = Vector3(1.0f, 0.98f, 0.94f);
        cloudSettings.coverage = 0.48f;
        cloudSettings.densityMultiplier = 1.4f;
        cloudSettings.baseNoiseScale = 0.015f;
        cloudSettings.detailNoiseScale = 0.08f;
        cloudSettings.detailErosion = 0.22f;
        cloudSettings.bottomFadeDistance = 3.0f;
        cloudSettings.topFadeDistance = 5.0f;
        cloudSettings.lightAbsorption = 0.35f;
        cloudSettings.noiseIterations = 4;
        cloudSettings.lightMarchSteps = 3;
        cloudSettings.lightStepSize = 2.5f;
        renderer.setCloudSettings(cloudSettings);
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
        SkylineDebugger::ShowFatalError("DayX_Cloud initialization failed", error);
        SkylineDebugger::Shutdown();
        return EXIT_FAILURE;
    }
    catch (...)
    {
        SkylineDebugger::ShowFatalError("DayX_Cloud initialization failed", "An unknown fatal error occurred.");
        SkylineDebugger::Shutdown();
        return EXIT_FAILURE;
    }
}
