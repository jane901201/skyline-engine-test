// 物理範例：使用 Skyline::Physics 介面與 Jolt 後端，在主控台顯示模擬結果。
// 情境：三個物體從空中落到地板上（彈性不同），另有一顆被水平拋出的球，最後用射線查詢場景。

#include "backends/jolt/jolt_physics_world.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <string>
#include <vector>

using namespace Skyline;

namespace
{
constexpr float kFrame = 1.0f / 60.0f;
constexpr float kDuration = 4.0f;
constexpr float kPrintInterval = 0.5f;

struct Item
{
    const char* name;
    char symbol;
    Physics::BodyId id;
};

// 用文字畫出 X-Y 平面的側視圖（X 範圍 -6 到 6，Y 範圍 0 到 8）
void drawSideView(const Physics::World& world, const std::vector<Item>& items)
{
    constexpr int columns = 49;
    constexpr int rows = 17;
    std::vector<std::string> grid(rows, std::string(columns, ' '));

    for (const Item& item : items)
    {
        const Vector3 p = world.position(item.id);
        const int column = static_cast<int>(std::lround((p.x + 6.0f) / 12.0f * (columns - 1)));
        const int row = rows - 1 - static_cast<int>(std::lround(p.y / 8.0f * (rows - 1)));
        if (column >= 0 && column < columns && row >= 0 && row < rows)
            grid[row][column] = item.symbol;
    }

    for (const std::string& line : grid)
        std::printf("  |%s|\n", line.c_str());
    std::printf("  +%s+  (floor at y = 0)\n", std::string(columns, '=').c_str());
}

void printHeader(const std::vector<Item>& items)
{
    std::printf("  time  ");
    for (const Item& item : items)
        std::printf("| %-18s", item.name);
    std::printf("\n  ------");
    for (std::size_t i = 0; i < items.size(); ++i)
        std::printf("+-------------------");
    std::printf("\n");
}

void printRow(float time, const Physics::World& world, const std::vector<Item>& items)
{
    std::printf("  %4.1fs ", time);
    for (const Item& item : items)
    {
        const Vector3 p = world.position(item.id);
        std::printf("| x%6.2f y%6.2f   ", p.x, p.y);
    }
    std::printf("\n");
}

void printRay(const Physics::World& world, const std::vector<Item>& items, const char* label, const Ray& ray)
{
    std::printf("  %-38s", label);
    const auto hit = world.raycast(ray);
    if (!hit)
    {
        std::printf("-> no hit\n");
        return;
    }

    const char* name = "?";
    for (const Item& item : items)
    {
        if (item.id == hit->body)
            name = item.name;
    }
    std::printf("-> hit %-10s distance %6.3f  point (%5.2f, %5.2f, %5.2f)  normal (%5.2f, %5.2f, %5.2f)\n", name,
                hit->distance, hit->point.x, hit->point.y, hit->point.z, hit->normal.x, hit->normal.y, hit->normal.z);
}
} // namespace

int main()
{
    try
    {
        auto world = Jolt::createWorld();

        // 地板：頂面在 y = 0
        Physics::BodyDesc floorDesc;
        floorDesc.type = Physics::BodyType::Static;
        floorDesc.shape = Physics::ShapeDesc::MakeBox(Vector3(20.0f, 0.5f, 20.0f));
        floorDesc.position = Vector3(0.0f, -0.5f, 0.0f);
        floorDesc.friction = 0.6f;
        const Physics::BodyId floor = world->createBody(floorDesc);

        std::vector<Item> items;

        // 沒有彈性的球，落地後停住
        Physics::BodyDesc dead;
        dead.shape = Physics::ShapeDesc::MakeSphere(0.5f);
        dead.position = Vector3(-4.0f, 6.0f, 0.0f);
        dead.restitution = 0.0f;
        items.push_back({"dead ball", 'D', world->createBody(dead)});

        // 彈性很高的球，會反覆彈跳
        Physics::BodyDesc bouncy = dead;
        bouncy.position = Vector3(-1.5f, 6.0f, 0.0f);
        bouncy.restitution = 0.8f;
        items.push_back({"bouncy ball", 'B', world->createBody(bouncy)});

        // 旋轉過的盒子，落地後會翻動
        Physics::BodyDesc crate;
        crate.shape = Physics::ShapeDesc::MakeBox(Vector3(0.5f, 0.5f, 0.5f));
        crate.position = Vector3(1.5f, 6.0f, 0.0f);
        const float halfAngle = 0.5f * (30.0f * 3.14159265f / 180.0f);
        crate.rotation = Quaternion(0.0f, 0.0f, std::sin(halfAngle), std::cos(halfAngle));
        crate.restitution = 0.1f;
        items.push_back({"crate", 'C', world->createBody(crate)});

        // 水平拋出的球
        Physics::BodyDesc shot = dead;
        // 放在 z = 2 的另一條線上，避免撞到其他物體而干擾觀察
        shot.position = Vector3(-5.5f, 1.0f, 2.0f);
        shot.restitution = 0.3f;
        const Physics::BodyId shotId = world->createBody(shot);
        world->setLinearVelocity(shotId, Vector3(3.0f, 6.0f, 0.0f));
        items.push_back({"thrown ball", 'T', shotId});

        std::printf("Skyline physics demo (Jolt backend), %u bodies, step = 1/60 s\n\n", world->bodyCount());

        std::printf("Start:\n");
        drawSideView(*world, items);

        std::printf("\nPositions over time:\n");
        printHeader(items);
        printRow(0.0f, *world, items);

        const int totalFrames = static_cast<int>(std::round(kDuration / kFrame));
        const int printEvery = static_cast<int>(std::round(kPrintInterval / kFrame));
        for (int frame = 1; frame <= totalFrames; ++frame)
        {
            world->step(kFrame);
            if (frame % printEvery == 0)
                printRow(static_cast<float>(frame) * kFrame, *world, items);
        }

        std::printf("\nAfter %.1f s:\n", kDuration);
        drawSideView(*world, items);

        // 射線可能命中地板，所以命名清單要包含地板
        std::vector<Item> named = items;
        named.push_back({"floor", ' ', floor});

        std::printf("\nRaycasts:\n");
        printRay(*world, named, "straight down over the dead ball",
                 Ray(Vector3(world->position(items[0].id).x, 10.0f, 0.0f), Vector3(0.0f, -1.0f, 0.0f)));
        printRay(*world, named, "straight down over empty floor", Ray(Vector3(4.0f, 10.0f, 0.0f), Vector3(0.0f, -1.0f, 0.0f)));
        printRay(*world, named, "sideways along y = 0.5 from the left", Ray(Vector3(-10.0f, 0.5f, 0.0f), Vector3(1.0f, 0.0f, 0.0f)));
        printRay(*world, named, "upward (nothing above)", Ray(Vector3(0.0f, 1.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f)));
        printRay(*world, named, "downward with range 2 (too short)", Ray(Vector3(4.0f, 10.0f, 0.0f), Vector3(0.0f, -1.0f, 0.0f), 2.0f));

        std::printf("\nSleeping bodies (at rest):\n");
        for (const Item& item : items)
            std::printf("  %-12s %s\n", item.name, world->isActive(item.id) ? "active" : "asleep");


        return 0;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }
}
