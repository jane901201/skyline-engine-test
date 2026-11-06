// 場景視窗範例：在真正的 3D 視窗裡，把前面做的 Scene、Camera、物理、動畫與 GameWorld 行為放在一起。
//
// 操作：按住滑鼠右鍵移動滑鼠轉動視角，W/A/S/D 前後左右，Q/E 下上，Shift 加速；
//       Space 從攝影機前方射出一顆球；Esc 離開。滑鼠指到的物件會變亮黃色，名稱顯示在視窗標題。
// 命令列：--frames=N 跑 N 個影格後自動離開（用於自動化測試）；
//         --screenshot=路徑.bmp 搭配 --frames，把最後一個影格存成 BMP（路徑不可含空白）。
//
// 場景內容：
//   - 左後方：太陽 -> 地球軸心 -> 地球 -> 月球軸心 -> 月球（Spinner 行為旋轉軸心，階層矩陣讓子節點繞行）
//   - 前方：平台來回滑動、箱子上下浮動並自轉（AnimatorBehaviour 播放 AnimationClip，Spinner 讓箱子自轉）。
//           平台與箱子只有動畫，沒有碰撞
//   - 地面、三根柱子與滑落的球：Jolt 物理（RigidBodyBehaviour 把剛體位置寫回實體）

#include "backends/d3d12/d3d12_interop.h"
#include "backends/jolt/jolt_physics_world.h"
#include "backends/win32/win32_input_device.h"

#include <wrl/client.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <deque>
#include <exception>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "gameplay/animator_behaviour.h"
#include "gameplay/fly_camera_behaviour.h"
#include "gameplay/rigid_body_behaviour.h"
#include "../frame_capture.h"
#include "graphics_engine.h"
#include "render_pass/scene_renderer.h"
#include "skyline_debugger.h"
#include "system.h"

using namespace Skyline;

namespace
{
constexpr std::size_t kMaxBalls = 80;

// 每秒繞 Y 軸轉固定的角度，只改旋轉，不動位置與縮放
class Spinner : public Behaviour
{
  public:
    explicit Spinner(float radiansPerSecond) : m_speed(radiansPerSecond) {}

    void Update(float deltaTime) override
    {
        m_angle += m_speed * deltaTime;
        Transform transform = LocalTransform();
        DirectX::XMStoreFloat4(&transform.rotation.vec,
                               DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(0, 1, 0, 0), m_angle));
        SetLocalTransform(transform);
    }

  private:
    float m_speed;
    float m_angle = 0.0f;
};

// 視窗大小，由主迴圈每個影格更新，供需要畫面座標的行為使用
struct Viewport
{
    float width = 1.0f;
    float height = 1.0f;
};

// 找出滑鼠指到的物件
class HoverPicker : public Behaviour
{
  public:
    HoverPicker(const Camera& camera, const Viewport& viewport) : m_camera(&camera), m_viewport(&viewport) {}

    EntityId hovered = nullEntity;

    void Awake() override
    {
        SetExecutionOrder(10); // 等攝影機在這個影格更新完再取射線
    }

    void Update(float) override
    {
        const Vector2& mouse = InputState().mousePosition();
        const Ray ray = m_camera->ScreenPointToRay(mouse.x, mouse.y, m_viewport->width, m_viewport->height);
        const auto hit = GetScene().Raycast(ray);
        hovered = hit ? hit->entity : nullEntity;
    }

  private:
    const Camera* m_camera;
    const Viewport* m_viewport;
};

// 建立落下的球；數量超過上限時銷毀最舊的；按 Space 從攝影機前方射出一顆
class BallSpawner : public Behaviour
{
  public:
    BallSpawner(Physics::World& physics, SceneRenderer& renderer, const Camera& camera)
        : m_physics(&physics), m_renderer(&renderer), m_camera(&camera)
    {
        const Vector3 colors[] = {{0.90f, 0.25f, 0.20f}, {0.20f, 0.60f, 0.90f}, {0.95f, 0.75f, 0.20f},
                                  {0.30f, 0.80f, 0.40f}, {0.75f, 0.40f, 0.85f}, {0.95f, 0.95f, 0.95f}};
        for (const Vector3& color : colors)
        {
            SurfaceMaterial material;
            material.baseColor = color;
            material.roughness = 0.35f;
            m_materials.push_back(renderer.AddMaterial(material));
        }
    }

    void SpawnBall(const Vector3& position, const Vector3& velocity)
    {
        constexpr float radius = 0.4f;

        const EntityId entity = World().CreateEntity("ball");
        Transform transform;
        transform.position = position;
        transform.scale = Vector3(radius * 2.0f, radius * 2.0f, radius * 2.0f); // 內建球的直徑是 1
        GetScene().SetLocalTransform(entity, transform);

        Renderable renderable;
        renderable.mesh = m_renderer->SphereMesh();
        renderable.material = m_materials[m_next++ % m_materials.size()];
        renderable.localBounds = AABB::FromCenterExtents(Vector3(0, 0, 0), Vector3(0.5f, 0.5f, 0.5f));
        GetScene().SetRenderable(entity, renderable);

        Physics::BodyDesc body;
        body.shape = Physics::ShapeDesc::MakeSphere(radius);
        body.position = position;
        body.restitution = 0.55f;
        body.friction = 0.4f;
        const RigidBodyBehaviour& rigidBody = World().AddBehaviour<RigidBodyBehaviour>(entity, *m_physics, body);
        m_physics->setLinearVelocity(rigidBody.Body(), velocity);

        m_balls.push_back(entity);
        if (m_balls.size() > kMaxBalls)
        {
            World().DestroyEntity(m_balls.front());
            m_balls.pop_front();
        }
    }

    void Update(float) override
    {
        if (!InputState().wasPressed(Input::Key::Space))
            return;
        const Vector3 forward = m_camera->transform.Forward();
        const Vector3 origin = m_camera->transform.position;
        SpawnBall(Vector3(origin.x + forward.x * 2.0f, origin.y + forward.y * 2.0f, origin.z + forward.z * 2.0f),
                  Vector3(forward.x * 14.0f, forward.y * 14.0f, forward.z * 14.0f));
    }

  private:
    Physics::World* m_physics;
    SceneRenderer* m_renderer;
    const Camera* m_camera;
    std::vector<std::uint32_t> m_materials;
    std::size_t m_next = 0;
    std::deque<EntityId> m_balls;
};

Renderable MakeRenderable(std::uint32_t mesh, std::uint32_t material, const Vector3& halfExtents)
{
    Renderable renderable;
    renderable.mesh = mesh;
    renderable.material = material;
    renderable.localBounds = AABB::FromCenterExtents(Vector3(0, 0, 0), halfExtents);
    return renderable;
}

Transform MakeTransform(const Vector3& position, const Vector3& scale = Vector3(1, 1, 1))
{
    Transform transform;
    transform.position = position;
    transform.scale = scale;
    return transform;
}

// 解析 --frames=N，沒有指定時回傳 0（代表不限制）
std::uint64_t parseFrameLimit(const wchar_t* commandLine)
{
    const wchar_t* found = commandLine != nullptr ? std::wcsstr(commandLine, L"--frames=") : nullptr;
    return found != nullptr ? std::wcstoull(found + 9, nullptr, 10) : 0;
}


void updateTitle(const std::string& hoveredName, std::size_t entityCount, std::uint32_t drawCount, double fps)
{
    wchar_t title[256];
    const std::wstring name(hoveredName.begin(), hoveredName.end());
    std::swprintf(title, 256, L"DayX SceneView | hover: %ls | entities %zu | drawn %u | %.0f FPS",
                  name.empty() ? L"-" : name.c_str(), entityCount, drawCount, fps);
    SetWindowTextW(g_hWnd, title);
}
} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR commandLine, int showCommand)
{
    try
    {
        SkylineDebugger::Initialize("DayX_SceneView");
        const std::uint64_t frameLimit = parseFrameLimit(commandLine);
        const std::wstring screenshotPath = parseScreenshotPath(commandLine);
        FrameCapture capture;

        initWindow(hInstance, hPrevInstance, commandLine, showCommand, TEXT("DayX SceneView"));
        if (g_hWnd == nullptr)
            throw std::runtime_error("DayX_SceneView: Failed to create the application window.");

        GraphicsEngine graphicsEngine;
        graphicsEngine.init(g_hWnd, FRAME_BUFFER_W, FRAME_BUFFER_H);
        SkylineDebugger::ConfigureD3D12(Skyline::D3D12::nativeFrameDevice(graphicsEngine.getRhiFrame()));

        SceneRenderer renderer;
        renderer.init(PassInitContext{graphicsEngine.getRhiDevice(), graphicsEngine.getRhiFrame().attachments(),
                                      graphicsEngine.getFrameBufferWidth(), graphicsEngine.GetFrameBufferHeight()});

        // 解構順序：GameWorld 最先解構（行為的 OnDestroy 會用到物理世界），所以它們要宣告在它之前
        const std::unique_ptr<Input::Device> input = Win32::createInputDevice(g_hWnd);
        const std::unique_ptr<Physics::World> physics = Jolt::createWorld();
        Camera camera;
        Viewport viewport;
        GameWorld world;
        world.SetInput(&input->state());

        // ----- 材質 -----
        const auto makeMaterial = [&](const Vector3& color, float metallic, float roughness) {
            SurfaceMaterial material;
            material.baseColor = color;
            material.metallic = metallic;
            material.roughness = roughness;
            return renderer.AddMaterial(material);
        };
        const std::uint32_t groundMaterial = makeMaterial({0.30f, 0.42f, 0.32f}, 0.0f, 0.85f);
        const std::uint32_t pillarMaterial = makeMaterial({0.55f, 0.55f, 0.60f}, 0.0f, 0.6f);
        const std::uint32_t sunMaterial = makeMaterial({1.0f, 0.55f, 0.10f}, 0.0f, 0.4f);
        const std::uint32_t earthMaterial = makeMaterial({0.15f, 0.40f, 0.85f}, 0.0f, 0.5f);
        const std::uint32_t moonMaterial = makeMaterial({0.75f, 0.75f, 0.78f}, 0.0f, 0.7f);
        const std::uint32_t platformMaterial = makeMaterial({0.35f, 0.35f, 0.40f}, 0.6f, 0.4f);
        const std::uint32_t crateMaterial = makeMaterial({0.80f, 0.50f, 0.20f}, 0.0f, 0.6f);

        Scene& scene = world.GetScene();
        const Vector3 unitHalf(0.5f, 0.5f, 0.5f);

        // ----- 地面與柱子（靜態物理）-----
        const EntityId ground = world.CreateEntity("ground");
        scene.SetLocalTransform(ground, MakeTransform({0, 0, 0}, {80, 1, 80}));
        scene.SetRenderable(ground, MakeRenderable(renderer.PlaneMesh(), groundMaterial, {0.5f, 0.02f, 0.5f}));
        {
            Physics::BodyDesc floor;
            floor.shape = Physics::ShapeDesc::MakeBox(Vector3(40, 0.5f, 40));
            floor.type = Physics::BodyType::Static;
            floor.position = Vector3(0, -0.5f, 0);
            floor.restitution = 0.2f;
            physics->createBody(floor);
        }
        const Vector3 pillarPositions[] = {{8, 0.75f, 0}, {5, 0.75f, 6}, {-2, 0.75f, 9}};
        for (const Vector3& position : pillarPositions)
        {
            const EntityId pillar = world.CreateEntity("pillar");
            scene.SetLocalTransform(pillar, MakeTransform(position, {2, 1.5f, 2}));
            scene.SetRenderable(pillar, MakeRenderable(renderer.CubeMesh(), pillarMaterial, unitHalf));

            Physics::BodyDesc body;
            body.shape = Physics::ShapeDesc::MakeBox(Vector3(1, 0.75f, 1));
            body.type = Physics::BodyType::Static;
            body.position = position;
            physics->createBody(body);
        }

        // ----- 太陽系（階層）：縮放只放在有形狀的葉節點，軸心不縮放，所以不會扭曲子節點 -----
        const EntityId solar = world.CreateEntity("solar system");
        scene.SetLocalTransform(solar, MakeTransform({-9, 2.5f, 6}));
        const EntityId sun = world.CreateEntity("sun", solar);
        scene.SetLocalTransform(sun, MakeTransform({0, 0, 0}, {2.4f, 2.4f, 2.4f}));
        scene.SetRenderable(sun, MakeRenderable(renderer.SphereMesh(), sunMaterial, unitHalf));

        const EntityId earthPivot = world.CreateEntity("earth pivot", solar);
        world.AddBehaviour<Spinner>(earthPivot, 0.5f);
        const EntityId earthAnchor = world.CreateEntity("earth anchor", earthPivot);
        scene.SetLocalTransform(earthAnchor, MakeTransform({4.5f, 0, 0}));
        const EntityId earth = world.CreateEntity("earth", earthAnchor);
        scene.SetLocalTransform(earth, MakeTransform({0, 0, 0}, {0.9f, 0.9f, 0.9f}));
        scene.SetRenderable(earth, MakeRenderable(renderer.SphereMesh(), earthMaterial, unitHalf));

        const EntityId moonPivot = world.CreateEntity("moon pivot", earthAnchor);
        world.AddBehaviour<Spinner>(moonPivot, 2.0f);
        const EntityId moon = world.CreateEntity("moon", moonPivot);
        scene.SetLocalTransform(moon, MakeTransform({1.3f, 0, 0}, {0.35f, 0.35f, 0.35f}));
        scene.SetRenderable(moon, MakeRenderable(renderer.SphereMesh(), moonMaterial, unitHalf));

        // ----- 動畫平台：mover 沿 X 滑動，crate 在平台上浮動 -----
        const EntityId mover = world.CreateEntity("mover");
        scene.SetLocalTransform(mover, MakeTransform({-4, 0, -5}));
        const EntityId platform = world.CreateEntity("platform", mover);
        scene.SetLocalTransform(platform, MakeTransform({0, 0.15f, 0}, {3, 0.3f, 3}));
        scene.SetRenderable(platform, MakeRenderable(renderer.CubeMesh(), platformMaterial, unitHalf));
        const EntityId crate = world.CreateEntity("crate", mover);
        scene.SetLocalTransform(crate, MakeTransform({0, 0.8f, 0}));
        scene.SetRenderable(crate, MakeRenderable(renderer.CubeMesh(), crateMaterial, unitHalf));
        world.AddBehaviour<Spinner>(crate, 1.2f);

        auto patrol = std::make_shared<AnimationClip>();
        patrol->name = "patrol";
        patrol->duration = 6.0f;
        AnimationChannel slide;
        slide.target = "mover";
        slide.position.keys = {{0.0f, Vector3(-4, 0, -5)}, {3.0f, Vector3(4, 0, -5)}, {6.0f, Vector3(-4, 0, -5)}};
        AnimationChannel bob;
        bob.target = "crate";
        bob.position.keys = {{0.0f, Vector3(0, 0.8f, 0)},
                             {1.5f, Vector3(0, 1.6f, 0)},
                             {3.0f, Vector3(0, 0.8f, 0)},
                             {4.5f, Vector3(0, 1.6f, 0)},
                             {6.0f, Vector3(0, 0.8f, 0)}};
        patrol->channels = {slide, bob};
        world.AddBehaviour<AnimatorBehaviour>(mover).Play(patrol, true);

        // ----- 攝影機、物理推進、球、滑鼠指向 -----
        camera.transform = Transform::LookAt(Vector3(0, 7, -17), Vector3(0, 1, 3));
        const EntityId systems = world.CreateEntity("systems");
        world.AddBehaviour<PhysicsStepBehaviour>(systems, *physics);
        world.AddBehaviour<FlyCameraBehaviour>(systems, camera);
        BallSpawner& spawner = world.AddBehaviour<BallSpawner>(systems, *physics, renderer, camera);
        HoverPicker& picker = world.AddBehaviour<HoverPicker>(systems, camera, viewport);

        for (int i = 0; i < 12; ++i)
        {
            const float x = -3.0f + static_cast<float>(i % 4) * 2.0f;
            const float z = 2.0f + static_cast<float>(i / 4) * 2.0f;
            spawner.SpawnBall(Vector3(x, 5.0f + static_cast<float>(i) * 1.1f, z), Vector3(0, 0, 0));
        }

        renderer.SetScene(&scene);
        renderer.SetCamera(&camera);

        // ----- 主迴圈 -----
        LARGE_INTEGER frequency{};
        LARGE_INTEGER previous{};
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&previous);
        double titleTimer = 0.0;
        std::uint32_t titleFrames = 0;
        std::uint64_t frameCount = 0;

        while (dispatchWindowMessage())
        {
            LARGE_INTEGER now{};
            QueryPerformanceCounter(&now);
            const float deltaTime = static_cast<float>(now.QuadPart - previous.QuadPart) /
                                    static_cast<float>(frequency.QuadPart);
            previous = now;

            const UINT width = graphicsEngine.getFrameBufferWidth();
            const UINT height = graphicsEngine.GetFrameBufferHeight();
            if (width == 0 || height == 0)
                continue; // 視窗被縮到最小

            input->update();
            if (input->state().wasPressed(Input::Key::Escape))
                break;

            viewport.width = static_cast<float>(width);
            viewport.height = static_cast<float>(height);
            camera.SetViewport(viewport.width, viewport.height);
            world.Tick(deltaTime);

            renderer.SetHighlight(picker.hovered);
            graphicsEngine.beginRender();
            auto& frame = graphicsEngine.getRhiFrame();
            renderer.execute(PassFrameContext{frame.commands(), frame.frameIndex(), frame.width(), frame.height()});
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

            titleTimer += deltaTime;
            ++titleFrames;
            if (titleTimer >= 0.25)
            {
                updateTitle(scene.IsAlive(picker.hovered) ? scene.Name(picker.hovered) : std::string(),
                            scene.EntityCount(), renderer.LastDrawCount(), titleFrames / titleTimer);
                titleTimer = 0.0;
                titleFrames = 0;
            }
            if (frameLimit != 0 && ++frameCount >= frameLimit)
                break;
        }

        graphicsEngine.waitDraw();
        SkylineDebugger::Shutdown();
        return EXIT_SUCCESS;
    }
    catch (const std::exception& exception)
    {
        SkylineDebugger::ShowFatalError("DayX_SceneView failed", exception);
        SkylineDebugger::Shutdown();
        return EXIT_FAILURE;
    }
    catch (...)
    {
        SkylineDebugger::ShowFatalError("DayX_SceneView failed", "An unknown fatal error occurred.");
        SkylineDebugger::Shutdown();
        return EXIT_FAILURE;
    }
}
