#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "backends/jolt/jolt_physics_world.h"

#include <cmath>
#include <stdexcept>

using Catch::Approx;
using namespace Skyline;

namespace
{
constexpr float kFrame = 1.0f / 60.0f;

void stepSeconds(Physics::World& world, float seconds)
{
    const int frames = static_cast<int>(std::round(seconds / kFrame));
    for (int i = 0; i < frames; ++i)
        world.step(kFrame);
}

// 建立一個頂面位於 y = 0 的靜態地板
Physics::BodyId addFloor(Physics::World& world)
{
    Physics::BodyDesc floor;
    floor.type = Physics::BodyType::Static;
    floor.shape = Physics::ShapeDesc::MakeBox(Vector3(50.0f, 0.5f, 50.0f));
    floor.position = Vector3(0.0f, -0.5f, 0.0f);
    return world.createBody(floor);
}

Physics::BodyId addSphere(Physics::World& world, const Vector3& position, float radius = 0.5f)
{
    Physics::BodyDesc sphere;
    sphere.shape = Physics::ShapeDesc::MakeSphere(radius);
    sphere.position = position;
    return world.createBody(sphere);
}
} // namespace

TEST_CASE("Physics world rejects invalid descriptions", "[physics]")
{
    Physics::WorldDesc worldDesc;
    worldDesc.maxBodies = 0;
    CHECK_THROWS_AS(Jolt::createWorld(worldDesc), std::invalid_argument);

    auto world = Jolt::createWorld();

    Physics::BodyDesc desc;
    desc.shape = Physics::ShapeDesc::MakeSphere(0.0f);
    CHECK_THROWS_AS(world->createBody(desc), std::invalid_argument);

    desc.shape = Physics::ShapeDesc::MakeBox(Vector3(1.0f, 0.0f, 1.0f));
    CHECK_THROWS_AS(world->createBody(desc), std::invalid_argument);

    desc.shape = Physics::ShapeDesc::MakeSphere(1.0f);
    desc.friction = -1.0f;
    CHECK_THROWS_AS(world->createBody(desc), std::invalid_argument);

    desc.friction = 0.5f;
    desc.restitution = 1.5f;
    CHECK_THROWS_AS(world->createBody(desc), std::invalid_argument);

    CHECK_THROWS_AS(world->step(0.0f), std::invalid_argument);
    CHECK_THROWS_AS(world->step(-1.0f), std::invalid_argument);
    CHECK(world->bodyCount() == 0);
}

TEST_CASE("Physics world rejects invalid body ids", "[physics]")
{
    auto world = Jolt::createWorld();
    CHECK_THROWS_AS(world->position(Physics::invalidBody), std::invalid_argument);

    const Physics::BodyId body = addSphere(*world, Vector3(0.0f, 1.0f, 0.0f));
    CHECK(world->bodyCount() == 1);

    world->destroyBody(body);
    CHECK(world->bodyCount() == 0);
    // 銷毀後舊的識別碼必須失效
    CHECK_THROWS_AS(world->position(body), std::invalid_argument);
    CHECK_THROWS_AS(world->destroyBody(body), std::invalid_argument);
}

TEST_CASE("Physics world enforces the body limit", "[physics]")
{
    Physics::WorldDesc worldDesc;
    worldDesc.maxBodies = 2;
    auto world = Jolt::createWorld(worldDesc);

    addSphere(*world, Vector3(0.0f, 1.0f, 0.0f));
    addSphere(*world, Vector3(5.0f, 1.0f, 0.0f));
    CHECK_THROWS_AS(addSphere(*world, Vector3(10.0f, 1.0f, 0.0f)), std::runtime_error);
}

TEST_CASE("Physics dynamic body falls under gravity", "[physics]")
{
    auto world = Jolt::createWorld();
    const Physics::BodyId body = addSphere(*world, Vector3(0.0f, 10.0f, 0.0f));

    stepSeconds(*world, 1.0f);

    // 自由落體 1 秒約下降 0.5 * g * t^2 = 4.9
    const Vector3 position = world->position(body);
    CHECK(position.y == Approx(10.0f - 4.905f).margin(0.2f));
    CHECK(position.x == Approx(0.0f).margin(1e-4f));
    // Jolt 預設有線性阻尼（0.05），所以 1 秒後的速度會略小於 g
    CHECK(world->linearVelocity(body).y == Approx(-9.81f).margin(0.5f));
}

TEST_CASE("Physics sphere comes to rest on a static floor", "[physics]")
{
    auto world = Jolt::createWorld();
    addFloor(*world);
    const Physics::BodyId body = addSphere(*world, Vector3(0.0f, 3.0f, 0.0f));

    stepSeconds(*world, 5.0f);

    CHECK(world->position(body).y == Approx(0.5f).margin(0.05f));
    CHECK(std::abs(world->linearVelocity(body).y) < 0.05f);
    // 靜止一段時間後應進入休眠
    CHECK_FALSE(world->isActive(body));
}

TEST_CASE("Physics custom gravity is applied", "[physics]")
{
    Physics::WorldDesc worldDesc;
    worldDesc.gravity = Vector3(0.0f, 0.0f, 0.0f);
    auto world = Jolt::createWorld(worldDesc);
    const Physics::BodyId body = addSphere(*world, Vector3(0.0f, 5.0f, 0.0f));

    stepSeconds(*world, 1.0f);

    CHECK(world->position(body).y == Approx(5.0f).margin(1e-3f));
}

TEST_CASE("Physics kinematic body follows its velocity", "[physics]")
{
    auto world = Jolt::createWorld();
    Physics::BodyDesc desc;
    desc.type = Physics::BodyType::Kinematic;
    desc.shape = Physics::ShapeDesc::MakeSphere(0.5f);
    desc.position = Vector3(0.0f, 5.0f, 0.0f);
    const Physics::BodyId body = world->createBody(desc);

    world->setLinearVelocity(body, Vector3(2.0f, 0.0f, 0.0f));
    stepSeconds(*world, 1.0f);

    const Vector3 position = world->position(body);
    CHECK(position.x == Approx(2.0f).margin(0.05f));
    // 運動學剛體不受重力影響
    CHECK(position.y == Approx(5.0f).margin(1e-3f));
}

TEST_CASE("Physics raycast hits a box from above", "[physics]")
{
    auto world = Jolt::createWorld();
    Physics::BodyDesc box;
    box.type = Physics::BodyType::Static;
    box.shape = Physics::ShapeDesc::MakeBox(Vector3(1.0f, 1.0f, 1.0f));
    const Physics::BodyId body = world->createBody(box);

    const auto hit = world->raycast(Ray(Vector3(0.0f, 5.0f, 0.0f), Vector3(0.0f, -1.0f, 0.0f)));

    REQUIRE(hit.has_value());
    CHECK(hit->body == body);
    CHECK(hit->distance == Approx(4.0f).margin(1e-3f));
    CHECK(hit->point.y == Approx(1.0f).margin(1e-3f));
    CHECK(hit->normal.y == Approx(1.0f).margin(1e-3f));
    CHECK(hit->normal.x == Approx(0.0f).margin(1e-3f));
}

TEST_CASE("Physics raycast hits a sphere from the side", "[physics]")
{
    auto world = Jolt::createWorld();
    Physics::BodyDesc sphere;
    sphere.type = Physics::BodyType::Static;
    sphere.shape = Physics::ShapeDesc::MakeSphere(1.0f);
    world->createBody(sphere);

    const auto hit = world->raycast(Ray(Vector3(-5.0f, 0.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f)));

    REQUIRE(hit.has_value());
    CHECK(hit->distance == Approx(4.0f).margin(1e-3f));
    CHECK(hit->point.x == Approx(-1.0f).margin(1e-3f));
    CHECK(hit->normal.x == Approx(-1.0f).margin(1e-3f));
}

TEST_CASE("Physics raycast respects direction and range", "[physics]")
{
    auto world = Jolt::createWorld();
    Physics::BodyDesc box;
    box.type = Physics::BodyType::Static;
    box.shape = Physics::ShapeDesc::MakeBox(Vector3(1.0f, 1.0f, 1.0f));
    world->createBody(box);

    // 朝反方向沒有命中
    CHECK_FALSE(world->raycast(Ray(Vector3(0.0f, 5.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f))).has_value());
    // 射程不夠（命中距離是 4）
    CHECK_FALSE(world->raycast(Ray(Vector3(0.0f, 5.0f, 0.0f), Vector3(0.0f, -1.0f, 0.0f), 3.0f)).has_value());
    // 方向不是單位向量時，Ray 會正規化，距離仍以世界單位表示
    const auto hit = world->raycast(Ray(Vector3(0.0f, 5.0f, 0.0f), Vector3(0.0f, -10.0f, 0.0f)));
    REQUIRE(hit.has_value());
    CHECK(hit->distance == Approx(4.0f).margin(1e-3f));
}

TEST_CASE("Physics raycast returns the nearest body", "[physics]")
{
    auto world = Jolt::createWorld();
    Physics::BodyDesc desc;
    desc.type = Physics::BodyType::Static;
    desc.shape = Physics::ShapeDesc::MakeSphere(1.0f);

    desc.position = Vector3(10.0f, 0.0f, 0.0f);
    const Physics::BodyId far = world->createBody(desc);
    desc.position = Vector3(5.0f, 0.0f, 0.0f);
    const Physics::BodyId near = world->createBody(desc);
    REQUIRE(far != near);

    const auto hit = world->raycast(Ray(Vector3(0.0f, 0.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f)));

    REQUIRE(hit.has_value());
    CHECK(hit->body == near);
    CHECK(hit->distance == Approx(4.0f).margin(1e-3f));
}

TEST_CASE("Physics body rotation is applied to the shape", "[physics]")
{
    auto world = Jolt::createWorld();
    Physics::BodyDesc box;
    box.type = Physics::BodyType::Static;
    // 沿 X 軸很長的盒子，繞 Y 軸轉 90 度後會沿 Z 軸延伸
    box.shape = Physics::ShapeDesc::MakeBox(Vector3(3.0f, 1.0f, 1.0f));
    const float half = std::sqrt(0.5f);
    box.rotation = Quaternion(0.0f, half, 0.0f, half);
    world->createBody(box);

    // z = 2.5 只在旋轉後的盒子範圍內（未旋轉時 z 範圍只有 -1 到 1）
    const auto inside = world->raycast(Ray(Vector3(0.0f, 5.0f, 2.5f), Vector3(0.0f, -1.0f, 0.0f)));
    CHECK(inside.has_value());
    // x = 2.5 只在未旋轉的盒子範圍內
    const auto outside = world->raycast(Ray(Vector3(2.5f, 5.0f, 0.0f), Vector3(0.0f, -1.0f, 0.0f)));
    CHECK_FALSE(outside.has_value());

    // 回讀的旋轉應與設定一致
    auto world2 = Jolt::createWorld();
    box.type = Physics::BodyType::Dynamic;
    const Physics::BodyId dynamicBox = world2->createBody(box);
    const Quaternion read = world2->rotation(dynamicBox);
    CHECK(read.y == Approx(half).margin(1e-4f));
    CHECK(read.w == Approx(half).margin(1e-4f));
}

TEST_CASE("Physics worlds can coexist and be recreated", "[physics]")
{
    auto first = Jolt::createWorld();
    auto second = Jolt::createWorld();
    const Physics::BodyId body = addSphere(*second, Vector3(0.0f, 5.0f, 0.0f));

    // 釋放其中一個世界不應影響另一個
    first.reset();
    stepSeconds(*second, 0.5f);
    CHECK(second->position(body).y < 5.0f);

    // 全部釋放後可以重新建立（全域狀態會重新註冊）
    second.reset();
    auto third = Jolt::createWorld();
    const Physics::BodyId again = addSphere(*third, Vector3(0.0f, 5.0f, 0.0f));
    stepSeconds(*third, 0.5f);
    CHECK(third->position(again).y < 5.0f);
}
