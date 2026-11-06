// 編輯器：用 ImGui 排出 Viewport、Outliner、Details、Content Browser 與 Output Log。
//
// 操作：
//   Viewport  按住右鍵拖曳轉動視角，W/A/S/D 前後左右，Q/E 下上，Shift 加速；左鍵點擊選取；F 聚焦選取的物件
//   Outliner  點選、拖曳改變階層、右鍵選單、Del 刪除、上方可搜尋與新增
//   Details   編輯名稱、Position / Rotation / Scale、材質；World Settings 分頁調整太陽與攝影機
//   Content   Import 匯入 FBX / OBJ / glTF；雙擊或拖進 Viewport 放進場景
//
// 命令列：--frames=N 跑 N 個影格後自動離開；--screenshot=路徑.bmp 存最後一個影格；
//         --demo-import 啟動時把 Content 裡的 Sphere.fbx 放進場景；--select=名稱 選取實體；
//         --folder=子資料夾 切換 Content Browser；--tab=log / --tab=world 切換分頁（這幾個是自動化測試用）。

#include "backends/d3d12/d3d12_interop.h"
#include "backends/win32/win32_input_device.h"

#include <Windows.h>

#include <cstdint>
#include <cstdlib>
#include <cwchar>
#include <exception>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>

#include "../frame_capture.h"
#include "editor_actions.h"
#include "editor_panels.h"
#include "editor_state.h"
#include "graphics_engine.h"
#include "gameplay/game_world.h"
#include "render_pass/scene_renderer.h"
#include "skyline_debugger.h"
#include "system.h"
#include "ui/imgui_system.h"

using namespace Skyline;
namespace fs = std::filesystem;

namespace
{
std::uint64_t parseFrameLimit(const wchar_t* commandLine)
{
    const wchar_t* found = commandLine != nullptr ? std::wcsstr(commandLine, L"--frames=") : nullptr;
    return found != nullptr ? std::wcstoull(found + 9, nullptr, 10) : 0;
}

bool hasOption(const wchar_t* commandLine, const wchar_t* option)
{
    return commandLine != nullptr && std::wcsstr(commandLine, option) != nullptr;
}

// 取出 --名稱=值 的值（只支援 ASCII、不含空白），沒有這個參數時回傳空字串
std::string optionValue(const wchar_t* commandLine, const wchar_t* option)
{
    const wchar_t* found = commandLine != nullptr ? std::wcsstr(commandLine, option) : nullptr;
    if (found == nullptr)
        return {};
    std::wstring value(found + std::wcslen(option));
    value = value.substr(0, value.find(L' '));
    std::string ascii;
    for (const wchar_t c : value)
        ascii.push_back(static_cast<char>(c));
    return ascii;
}

fs::path executableDirectory()
{
    wchar_t buffer[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0 || length == MAX_PATH)
        throw std::runtime_error("DayX_Editor: Failed to determine the executable directory.");
    return fs::path(buffer).parent_path();
}

// 第一次執行時把範例模型複製到 Content/Meshes，讓 Content Browser 一開始就有東西可以看
void seedContent(const fs::path& contentRoot, const fs::path& assetsFbx, EditorLog& log)
{
    const fs::path meshes = contentRoot / "Meshes";
    std::error_code error;
    if (fs::exists(meshes, error) || !fs::is_directory(assetsFbx, error))
        return;
    fs::create_directories(meshes, error);
    for (const auto& entry : fs::directory_iterator(assetsFbx, error))
        if (entry.is_regular_file() && entry.path().extension() == ".fbx")
            fs::copy_file(entry.path(), meshes / entry.path().filename(), fs::copy_options::skip_existing, error);
    log.Info("Copied the sample models into '" + Utf8(meshes) + "'.");
}

// ImGui 預設字型沒有中文。載入 Windows 內建的微軟正黑體；1.92 的字型是依需要才產生字形，不需要指定字元範圍
void loadFont(EditorLog& log)
{
    wchar_t windows[MAX_PATH] = {};
    GetWindowsDirectoryW(windows, MAX_PATH);
    const fs::path font = fs::path(windows) / "Fonts" / "msjh.ttc";
    std::error_code error;
    if (!fs::exists(font, error))
    {
        log.Warning("The font 'msjh.ttc' was not found. Chinese text may not display.");
        return;
    }
    ImGui::GetIO().Fonts->AddFontFromFileTTF(Utf8(font).c_str(), 17.0f);
}

void buildDefaultScene(EditorState& state)
{
    SceneRenderer& renderer = *state.renderer;
    Scene& scene = state.world.GetScene();

    state.assets.NameMesh(renderer.CubeMesh(), "Cube");
    state.assets.NameMesh(renderer.SphereMesh(), "Sphere");
    state.assets.NameMesh(renderer.PlaneMesh(), "Plane");

    const auto add = [&](PrimitiveKind kind, const char* name, EntityId parent, const Vector3& position,
                         const Vector3& scale, const Vector3& color) {
        const EntityId entity = CreatePrimitiveEntity(state, kind, parent);
        scene.SetName(entity, name);
        Transform transform = scene.LocalTransform(entity);
        transform.position = position;
        transform.scale = scale;
        scene.SetLocalTransform(entity, transform);
        renderer.MaterialAt(scene.GetRenderable(entity)->material).baseColor = color;
        return entity;
    };

    add(PrimitiveKind::Plane, "Ground", nullEntity, {0, 0, 0}, {40, 1, 40}, {0.35f, 0.42f, 0.36f});
    const EntityId group = state.world.CreateEntity("Props");
    add(PrimitiveKind::Cube, "Crate", group, {-3, 0.5f, 2}, {1, 1, 1}, {0.80f, 0.50f, 0.20f});
    add(PrimitiveKind::Cube, "Wall", group, {4, 1.0f, 4}, {6, 2, 0.5f}, {0.60f, 0.60f, 0.65f});
    add(PrimitiveKind::Sphere, "Ball", nullEntity, {1, 0.5f, -1}, {1, 1, 1}, {0.20f, 0.55f, 0.90f});
    state.selected = nullEntity;
}
} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR commandLine, int)
{
    try
    {
        SkylineDebugger::Initialize("DayX_Editor");
        const std::uint64_t frameLimit = parseFrameLimit(commandLine);
        const std::wstring screenshotPath = parseScreenshotPath(commandLine);
        const bool demoImport = hasOption(commandLine, L"--demo-import");
        FrameCapture capture;

        initWindow(hInstance, hPrevInstance, commandLine, SW_SHOWMAXIMIZED, TEXT("DayX Editor"));
        if (g_hWnd == nullptr)
            throw std::runtime_error("DayX_Editor: Failed to create the application window.");

        GraphicsEngine graphicsEngine;
        graphicsEngine.init(g_hWnd, FRAME_BUFFER_W, FRAME_BUFFER_H);
        {
            // 視窗在建立 GraphicsEngine 之前就已經最大化，那時的 WM_SIZE 沒有人接收，
            // 所以交換鏈還是預設大小，這裡補上實際的 Client 區大小
            RECT client{};
            GetClientRect(g_hWnd, &client);
            graphicsEngine.resize(static_cast<UINT>(client.right - client.left),
                                  static_cast<UINT>(client.bottom - client.top));
        }
        SkylineDebugger::ConfigureD3D12(Skyline::D3D12::nativeFrameDevice(graphicsEngine.getRhiFrame()));

        // 解構順序與宣告相反：EditorState（場景）最先、GraphicsEngine 最後
        Skyline::UI::ImGuiSystem imgui;
        imgui.initialize(g_hWnd, graphicsEngine);
        // ImGui 以靜態函式庫連結在 SDK 的 DLL 裡，這裡要共用它的 Context 才能直接呼叫 ImGui API
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(imgui.context()));
        ImGui::GetIO().IniFilename = nullptr; // 版面由程式排，不需要 imgui.ini

        SceneRenderer renderer;
        renderer.init(PassInitContext{graphicsEngine.getRhiDevice(), graphicsEngine.getRhiFrame().attachments(),
                                      graphicsEngine.getFrameBufferWidth(), graphicsEngine.GetFrameBufferHeight()});

        const std::unique_ptr<Input::Device> input = Win32::createInputDevice(g_hWnd);

        const fs::path exeDirectory = executableDirectory();
        EditorState state(renderer, exeDirectory / "Content");
        state.input = &input->state();
        loadFont(state.log);
        seedContent(state.contentRoot, exeDirectory / "assets" / "fbx", state.log);
        buildDefaultScene(state);
        UpdateSunDirection(state);

        state.camera.transform = Transform::LookAt(Vector3(7, 6, -11), Vector3(0, 0.5f, 0));
        state.camera.farZ = 2000.0f;
        state.fly.SyncFrom(state.camera);
        state.log.Info("Editor ready. Hold the right mouse button in the Viewport and use W/A/S/D to fly.");

        if (demoImport)
        {
            const fs::path model = state.contentRoot / "Meshes" / "Sphere.fbx";
            InstantiateAsset(state, model, Vector3(-1.5f, 1.0f, -3.0f));
            state.selected = nullEntity;
        }
        // 自動化測試用：--select=名稱 選取第一個同名的實體；--folder=子資料夾 切換 Content Browser
        const std::string selectName = optionValue(commandLine, L"--select=");
        if (!selectName.empty())
        {
            const Scene& scene = state.world.GetScene();
            scene.ForEach([&](EntityId id) {
                if (state.selected.IsNull() && scene.Name(id) == selectName)
                    state.selected = id;
            });
        }
        const std::string tabName = optionValue(commandLine, L"--tab=");
        state.forceLogTab = tabName == "log";
        state.forceWorldTab = tabName == "world";
        const std::string folderName = optionValue(commandLine, L"--folder=");
        if (!folderName.empty())
            state.currentFolder = state.contentRoot / folderName;

        // ----- 主迴圈 -----
        LARGE_INTEGER frequency{};
        LARGE_INTEGER previous{};
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&previous);
        std::uint64_t frameCount = 0;

        while (dispatchWindowMessage())
        {
            LARGE_INTEGER now{};
            QueryPerformanceCounter(&now);
            state.deltaTime = static_cast<float>(now.QuadPart - previous.QuadPart) / static_cast<float>(frequency.QuadPart);
            previous = now;

            if (graphicsEngine.getFrameBufferWidth() == 0 || graphicsEngine.GetFrameBufferHeight() == 0)
                continue; // 視窗被縮到最小

            input->update();
            if (state.quitRequested)
                break;

            imgui.beginFrame();
            DrawEditorUi(state);

            renderer.SetScene(&state.world.GetScene());
            renderer.SetCamera(&state.camera);
            renderer.SetHighlight(state.selected);
            renderer.SetViewportRect(state.viewport.x, state.viewport.y, state.viewport.width, state.viewport.height);

            graphicsEngine.beginRender();
            auto& frame = graphicsEngine.getRhiFrame();
            renderer.execute(PassFrameContext{frame.commands(), frame.frameIndex(), frame.width(), frame.height()});
            state.drawCount = renderer.LastDrawCount();
            imgui.render();

            const bool captureThisFrame = !screenshotPath.empty() && frameLimit != 0 && frameCount + 1 >= frameLimit;
            if (captureThisFrame)
                capture.record(graphicsEngine);
            graphicsEngine.endRender();
            if (captureThisFrame)
            {
                graphicsEngine.waitDraw();
                capture.save(screenshotPath);
            }
            SkylineDebugger::LogD3D12Messages(Skyline::D3D12::nativeFrameDevice(graphicsEngine.getRhiFrame()));

            if (frameLimit != 0 && ++frameCount >= frameLimit)
                break;
        }

        graphicsEngine.waitDraw();
        SkylineDebugger::Shutdown();
        return EXIT_SUCCESS;
    }
    catch (const std::exception& exception)
    {
        SkylineDebugger::ShowFatalError("DayX_Editor failed", exception);
        SkylineDebugger::Shutdown();
        return EXIT_FAILURE;
    }
    catch (...)
    {
        SkylineDebugger::ShowFatalError("DayX_Editor failed", "An unknown fatal error occurred.");
        SkylineDebugger::Shutdown();
        return EXIT_FAILURE;
    }
}
