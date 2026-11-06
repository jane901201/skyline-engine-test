// 場景範例：使用 Skyline 的 Scene / Transform / Camera，在主控台顯示結果。
// 內容：父子階層的世界座標、攝影機投影到螢幕、視錐體剔除、滑鼠拾取，以及自由飛行攝影機的移動。

#include "scene/fly_camera.h"
#include "scene/scene.h"

#include <cmath>
#include <cstdio>
#include <exception>
#include <string>
#include <vector>

using namespace Skyline;

namespace
{
constexpr float kViewportWidth = 800.0f;
constexpr float kViewportHeight = 600.0f;

Transform At(float x, float y, float z)
{
    Transform transform;
    transform.position = Vector3(x, y, z);
    return transform;
}

Transform RotatedY(float radians)
{
    Transform transform;
    DirectX::XMStoreFloat4(&transform.rotation.vec,
                           DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(0, 1, 0, 0), radians));
    return transform;
}

Renderable Ball(float radius)
{
    Renderable renderable;
    renderable.localBounds = AABB::FromCenterExtents(Vector3(0, 0, 0), Vector3(radius, radius, radius));
    return renderable;
}

// 俯視圖（X-Z 平面）：X 範圍 -10 到 10，Z 範圍 -10 到 10
void DrawTopView(const Scene& scene, const std::vector<std::pair<EntityId, char>>& marks)
{
    constexpr int columns = 41;
    constexpr int rows = 21;
    std::vector<std::string> grid(rows, std::string(columns, ' '));
    grid[rows / 2][columns / 2] = '+';

    for (const auto& [id, symbol] : marks)
    {
        const Vector3 p = scene.WorldPosition(id);
        const int column = static_cast<int>(std::lround((p.x + 10.0f) / 20.0f * (columns - 1)));
        // Z 向前為正，畫面上方是 +Z
        const int row = rows - 1 - static_cast<int>(std::lround((p.z + 10.0f) / 20.0f * (rows - 1)));
        if (column >= 0 && column < columns && row >= 0 && row < rows)
            grid[row][column] = symbol;
    }
    for (const std::string& line : grid)
        std::printf("    |%s|\n", line.c_str());
}

void PrintPosition(const char* label, const Vector3& p)
{
    std::printf("%-22s (%7.2f, %7.2f, %7.2f)\n", label, p.x, p.y, p.z);
}
} // namespace

int main()
{
    try
    {
        Scene scene;

        // 太陽 -> 地球軸心 -> 地球 -> 月球軸心 -> 月球：軸心旋轉，子節點跟著繞行
        const EntityId sun = scene.CreateEntity("sun");
        scene.SetRenderable(sun, Ball(1.5f));

        const EntityId earthPivot = scene.CreateEntity("earth pivot", sun);
        const EntityId earth = scene.CreateEntity("earth", earthPivot);
        scene.SetLocalTransform(earth, At(6.0f, 0.0f, 0.0f));
        scene.SetRenderable(earth, Ball(0.6f));

        const EntityId moonPivot = scene.CreateEntity("moon pivot", earth);
        const EntityId moon = scene.CreateEntity("moon", moonPivot);
        scene.SetLocalTransform(moon, At(1.5f, 0.0f, 0.0f));
        scene.SetRenderable(moon, Ball(0.25f));

        // 遠處的石頭，用來示範視錐體剔除
        const EntityId rockAhead = scene.CreateEntity("rock ahead");
        scene.SetLocalTransform(rockAhead, At(-5.0f, 0.0f, 10.0f));
        scene.SetRenderable(rockAhead, Ball(0.5f));
        const EntityId rockBehind = scene.CreateEntity("rock behind camera");
        scene.SetLocalTransform(rockBehind, At(0.0f, 0.0f, -60.0f));
        scene.SetRenderable(rockBehind, Ball(0.5f));
        const EntityId rockFar = scene.CreateEntity("rock far right");
        scene.SetLocalTransform(rockFar, At(300.0f, 0.0f, 0.0f));
        scene.SetRenderable(rockFar, Ball(0.5f));

        std::printf("Skyline scene demo, %zu entities\n", scene.EntityCount());

        // ----- 1. 階層與世界座標 -----
        std::printf("\n[1] Hierarchy: world positions over time (earth pivot 0.5 rad/s, moon pivot 2 rad/s)\n");
        const std::vector<std::pair<EntityId, char>> marks = {{sun, 'S'}, {earth, 'E'}, {moon, 'm'}};
        for (const float time : {0.0f, 2.0f, 4.0f})
        {
            scene.SetLocalTransform(earthPivot, RotatedY(0.5f * time));
            scene.SetLocalTransform(moonPivot, RotatedY(2.0f * time));
            std::printf("  t = %.0f s\n", time);
            std::printf("    ");
            PrintPosition("earth", scene.WorldPosition(earth));
            std::printf("    ");
            PrintPosition("moon", scene.WorldPosition(moon));
            DrawTopView(scene, marks);
        }

        // 回到 t = 0，後面的示範使用固定的位置
        scene.SetLocalTransform(earthPivot, RotatedY(0.0f));
        scene.SetLocalTransform(moonPivot, RotatedY(0.0f));

        // ----- 2. 攝影機投影 -----
        Camera camera;
        camera.SetViewport(kViewportWidth, kViewportHeight);
        camera.transform = Transform::LookAt(Vector3(0.0f, 8.0f, -20.0f), Vector3(2.0f, 0.0f, 0.0f));

        std::printf("\n[2] Camera at (%.1f, %.1f, %.1f), screen %.0fx%.0f: world -> screen\n",
                    camera.transform.position.x, camera.transform.position.y, camera.transform.position.z,
                    kViewportWidth, kViewportHeight);
        scene.ForEach([&](EntityId id) {
            if (!scene.GetRenderable(id))
                return;
            const auto screen = camera.WorldToScreen(scene.WorldPosition(id), kViewportWidth, kViewportHeight);
            if (screen)
                std::printf("  %-22s screen (%6.1f, %6.1f)  depth %.4f\n", scene.Name(id).c_str(), screen->x, screen->y,
                            screen->z);
            else
                std::printf("  %-22s behind the camera\n", scene.Name(id).c_str());
        });

        // ----- 3. 視錐體剔除 -----
        const Frustum frustum = camera.GetFrustum();
        const std::vector<EntityId> visible = scene.QueryVisible(frustum);
        std::printf("\n[3] Frustum culling: %zu of 6 renderables are visible\n", visible.size());
        for (const EntityId id : visible)
            std::printf("  visible: %s\n", scene.Name(id).c_str());

        // ----- 4. 滑鼠拾取 -----
        std::printf("\n[4] Picking: click positions -> entity\n");
        const auto click = [&](const char* label, float x, float y) {
            const Ray ray = camera.ScreenPointToRay(x, y, kViewportWidth, kViewportHeight);
            const auto hit = scene.Raycast(ray);
            if (hit)
                std::printf("  click (%5.1f, %5.1f) %-18s -> %-10s distance %6.2f\n", x, y, label,
                            scene.Name(hit->entity).c_str(), hit->distance);
            else
                std::printf("  click (%5.1f, %5.1f) %-18s -> nothing\n", x, y, label);
        };
        for (const EntityId target : {sun, earth, moon})
        {
            const auto screen = camera.WorldToScreen(scene.WorldPosition(target), kViewportWidth, kViewportHeight);
            click(("on " + scene.Name(target)).c_str(), screen->x, screen->y);
        }
        click("top-left corner", 5.0f, 5.0f);

        // ----- 5. 自由飛行攝影機 -----
        std::printf("\n[5] Fly camera: hold W for 1 s, then move the mouse with the right button down\n");
        FlyCameraController controller;
        controller.SyncFrom(camera);
        PrintPosition("start position", camera.transform.position);

        Input::State forward;
        forward.setKeyDown(Input::Key::W, true);
        forward.update();
        for (int frame = 0; frame < 60; ++frame)
            controller.Update(camera, forward, 1.0f / 60.0f);
        PrintPosition("after holding W", camera.transform.position);

        Input::State look;
        look.setMouseButtonDown(Input::MouseButton::Right, true);
        look.addMouseDelta(200.0f, 0.0f);
        look.update();
        controller.Update(camera, look, 1.0f / 60.0f);
        PrintPosition("forward after look", camera.transform.Forward());
        std::printf("yaw %.1f deg, pitch %.1f deg\n", DirectX::XMConvertToDegrees(controller.Yaw()),
                    DirectX::XMConvertToDegrees(controller.Pitch()));
        return 0;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }
}
