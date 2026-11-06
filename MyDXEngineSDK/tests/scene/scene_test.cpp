#include "scene/scene.h"

#include "scene_test_utils.h"

#include <algorithm>
#include <string>
#include <stdexcept>
#include <vector>

using namespace Skyline;
using namespace SceneTest;

namespace
{
Transform At(float x, float y, float z)
{
    Transform transform;
    transform.position = Vector3(x, y, z);
    return transform;
}

Renderable UnitBox()
{
    Renderable renderable;
    renderable.localBounds = AABB::FromCenterExtents(Vector3(0, 0, 0), Vector3(0.5f, 0.5f, 0.5f));
    return renderable;
}
} // namespace

TEST_CASE("Scene creates and destroys entities", "[scene]")
{
    Scene scene;
    CHECK(scene.EntityCount() == 0);

    const EntityId a = scene.CreateEntity("a");
    const EntityId b = scene.CreateEntity("b");
    CHECK(scene.EntityCount() == 2);
    CHECK(scene.IsAlive(a));
    CHECK(scene.Name(a) == "a");
    CHECK(scene.Name(b) == "b");

    scene.SetName(a, "renamed");
    CHECK(scene.Name(a) == "renamed");

    scene.DestroyEntity(a);
    CHECK(scene.EntityCount() == 1);
    CHECK_FALSE(scene.IsAlive(a));
    CHECK_THROWS_AS(scene.Name(a), std::invalid_argument);
    CHECK_THROWS_AS(scene.DestroyEntity(a), std::invalid_argument);
    CHECK_FALSE(scene.IsAlive(nullEntity));
    CHECK_THROWS_AS(scene.WorldMatrix(nullEntity), std::invalid_argument);
}

TEST_CASE("Scene entity ids are not confused after slot reuse", "[scene]")
{
    Scene scene;
    const EntityId first = scene.CreateEntity("first");
    scene.DestroyEntity(first);

    const EntityId second = scene.CreateEntity("second");
    // 槽位被重複使用，但舊的識別碼必須保持失效
    CHECK(second.index == first.index);
    CHECK(second.generation != first.generation);
    CHECK_FALSE(scene.IsAlive(first));
    CHECK(scene.IsAlive(second));
    CHECK(scene.Name(second) == "second");
    CHECK_THROWS_AS(scene.Name(first), std::invalid_argument);
    // 重複使用的槽位不能帶著舊資料
    CHECK(scene.Children(second).empty());
    CHECK_VEC3(scene.WorldPosition(second), 0.0f, 0.0f, 0.0f);
}

TEST_CASE("Scene creating a child requires a valid parent", "[scene]")
{
    Scene scene;
    const EntityId parent = scene.CreateEntity("parent");
    scene.DestroyEntity(parent);
    CHECK_THROWS_AS(scene.CreateEntity("child", parent), std::invalid_argument);
    CHECK(scene.EntityCount() == 0);
}

TEST_CASE("Scene world transform combines parent and local", "[scene]")
{
    Scene scene;
    const EntityId parent = scene.CreateEntity("parent");
    const EntityId child = scene.CreateEntity("child", parent);
    CHECK(scene.Parent(child) == parent);
    REQUIRE(scene.Children(parent).size() == 1);
    CHECK(scene.Children(parent)[0] == child);

    scene.SetLocalTransform(parent, At(10, 0, 0));
    scene.SetLocalTransform(child, At(0, 5, 0));
    CHECK_VEC3(scene.WorldPosition(child), 10.0f, 5.0f, 0.0f);

    // 父節點繞 Y 軸轉 90 度：子節點本地的 (0,0,3) 轉到 +X 方向
    Transform rotated = At(10, 0, 0);
    rotated.rotation = RotationY(kPi * 0.5f);
    scene.SetLocalTransform(parent, rotated);
    scene.SetLocalTransform(child, At(0, 0, 3));
    CHECK_VEC3(scene.WorldPosition(child), 13.0f, 0.0f, 0.0f);
}

TEST_CASE("Scene parent changes invalidate cached descendants", "[scene]")
{
    Scene scene;
    const EntityId root = scene.CreateEntity("root");
    const EntityId middle = scene.CreateEntity("middle", root);
    const EntityId leaf = scene.CreateEntity("leaf", middle);
    scene.SetLocalTransform(root, At(1, 0, 0));
    scene.SetLocalTransform(middle, At(0, 2, 0));
    scene.SetLocalTransform(leaf, At(0, 0, 3));

    // 先讀一次，讓快取生效
    CHECK_VEC3(scene.WorldPosition(leaf), 1.0f, 2.0f, 3.0f);

    // 改最上層，孫節點必須跟著更新
    scene.SetLocalTransform(root, At(100, 0, 0));
    CHECK_VEC3(scene.WorldPosition(leaf), 100.0f, 2.0f, 3.0f);

    // 只改中間，也要更新
    scene.SetLocalTransform(middle, At(0, 20, 0));
    CHECK_VEC3(scene.WorldPosition(leaf), 100.0f, 20.0f, 3.0f);
}

TEST_CASE("Scene SetParent keeps the world transform by default", "[scene]")
{
    Scene scene;
    const EntityId parent = scene.CreateEntity("parent");
    Transform parentTransform = At(10, 0, 0);
    parentTransform.rotation = RotationY(kPi * 0.5f);
    parentTransform.scale = Vector3(2.0f, 2.0f, 2.0f);
    scene.SetLocalTransform(parent, parentTransform);

    const EntityId child = scene.CreateEntity("child");
    scene.SetLocalTransform(child, At(4, 5, 6));
    CHECK_VEC3(scene.WorldPosition(child), 4.0f, 5.0f, 6.0f);

    scene.SetParent(child, parent);
    CHECK(scene.Parent(child) == parent);
    CHECK_VEC3(scene.WorldPosition(child), 4.0f, 5.0f, 6.0f);

    // 回到根節點，世界位置仍然不變
    scene.SetParent(child, nullEntity);
    CHECK(scene.Parent(child).IsNull());
    CHECK(scene.Children(parent).empty());
    CHECK_VEC3(scene.WorldPosition(child), 4.0f, 5.0f, 6.0f);
}

TEST_CASE("Scene SetParent can keep the local transform instead", "[scene]")
{
    Scene scene;
    const EntityId parent = scene.CreateEntity("parent");
    scene.SetLocalTransform(parent, At(10, 0, 0));
    const EntityId child = scene.CreateEntity("child");
    scene.SetLocalTransform(child, At(1, 2, 3));

    scene.SetParent(child, parent, false);

    CHECK_VEC3(scene.LocalTransform(child).position, 1.0f, 2.0f, 3.0f);
    CHECK_VEC3(scene.WorldPosition(child), 11.0f, 2.0f, 3.0f);
}

TEST_CASE("Scene SetParent rejects a parent that cannot be inverted", "[scene]")
{
    Scene scene;
    const EntityId flat = scene.CreateEntity("flat");
    Transform zeroScale;
    zeroScale.scale = Vector3(0.0f, 1.0f, 1.0f);
    scene.SetLocalTransform(flat, zeroScale);

    const EntityId child = scene.CreateEntity("child");
    scene.SetLocalTransform(child, At(1, 2, 3));

    CHECK_THROWS_AS(scene.SetParent(child, flat), std::invalid_argument);
    // 失敗時不能改動階層；不保留世界 Transform 的版本則可以成功
    CHECK(scene.Parent(child).IsNull());
    CHECK(scene.Children(flat).empty());
    CHECK_NOTHROW(scene.SetParent(child, flat, false));
}

TEST_CASE("Scene SetParent rejects cycles", "[scene]")
{
    Scene scene;
    const EntityId a = scene.CreateEntity("a");
    const EntityId b = scene.CreateEntity("b", a);
    const EntityId c = scene.CreateEntity("c", b);

    CHECK_THROWS_AS(scene.SetParent(a, a), std::invalid_argument);
    CHECK_THROWS_AS(scene.SetParent(a, c), std::invalid_argument);
    CHECK_THROWS_AS(scene.SetParent(a, b), std::invalid_argument);
    // 失敗後階層必須維持原樣
    CHECK(scene.Parent(a).IsNull());
    CHECK(scene.Parent(c) == b);
}

TEST_CASE("Scene destroying an entity destroys its descendants", "[scene]")
{
    Scene scene;
    const EntityId root = scene.CreateEntity("root");
    const EntityId child = scene.CreateEntity("child", root);
    const EntityId grandchild = scene.CreateEntity("grandchild", child);
    const EntityId sibling = scene.CreateEntity("sibling", root);
    const EntityId other = scene.CreateEntity("other");
    CHECK(scene.EntityCount() == 5);

    scene.DestroyEntity(child);
    CHECK(scene.EntityCount() == 3);
    CHECK_FALSE(scene.IsAlive(child));
    CHECK_FALSE(scene.IsAlive(grandchild));
    CHECK(scene.IsAlive(sibling));
    CHECK(scene.IsAlive(other));
    // 父節點的子清單不能留著已銷毀的實體
    REQUIRE(scene.Children(root).size() == 1);
    CHECK(scene.Children(root)[0] == sibling);

    scene.DestroyEntity(root);
    CHECK(scene.EntityCount() == 1);
    CHECK_FALSE(scene.IsAlive(sibling));
    CHECK(scene.IsAlive(other));
}

TEST_CASE("Scene ForEach visits living entities in creation order", "[scene]")
{
    Scene scene;
    const EntityId a = scene.CreateEntity("a");
    const EntityId b = scene.CreateEntity("b");
    const EntityId c = scene.CreateEntity("c");
    scene.DestroyEntity(b);

    std::vector<std::string> names;
    scene.ForEach([&](EntityId id) { names.push_back(scene.Name(id)); });

    REQUIRE(names.size() == 2);
    CHECK(names[0] == "a");
    CHECK(names[1] == "c");
    (void)a;
    (void)c;
}

TEST_CASE("Scene world bounds follow the entity transform", "[scene]")
{
    Scene scene;
    const EntityId entity = scene.CreateEntity("box");
    CHECK_FALSE(scene.WorldBounds(entity).IsValid());

    scene.SetRenderable(entity, UnitBox());
    scene.SetLocalTransform(entity, At(10, 0, 0));
    const AABB bounds = scene.WorldBounds(entity);
    REQUIRE(bounds.IsValid());
    CHECK_VEC3(bounds.min, 9.5f, -0.5f, -0.5f);
    CHECK_VEC3(bounds.max, 10.5f, 0.5f, 0.5f);

    scene.ClearRenderable(entity);
    CHECK_FALSE(scene.GetRenderable(entity).has_value());
    CHECK_FALSE(scene.WorldBounds(entity).IsValid());
}

TEST_CASE("Scene raycast returns the nearest visible renderable", "[scene]")
{
    Scene scene;
    const EntityId far = scene.CreateEntity("far");
    const EntityId near = scene.CreateEntity("near");
    const EntityId noRenderable = scene.CreateEntity("empty");
    scene.SetRenderable(far, UnitBox());
    scene.SetRenderable(near, UnitBox());
    scene.SetLocalTransform(far, At(0, 0, 20));
    scene.SetLocalTransform(near, At(0, 0, 10));
    scene.SetLocalTransform(noRenderable, At(0, 0, 5));

    const Ray ray(Vector3(0, 0, 0), Vector3(0, 0, 1));
    auto hit = scene.Raycast(ray);
    REQUIRE(hit.has_value());
    CHECK(hit->entity == near);
    CHECK(hit->distance == Catch::Approx(9.5f).margin(1e-3f));
    CHECK_VEC3(hit->point, 0.0f, 0.0f, 9.5f);
    CHECK_VEC3(hit->normal, 0.0f, 0.0f, -1.0f);

    // 隱藏最近的，應該改命中遠的
    Renderable hidden = UnitBox();
    hidden.visible = false;
    scene.SetRenderable(near, hidden);
    hit = scene.Raycast(ray);
    REQUIRE(hit.has_value());
    CHECK(hit->entity == far);

    // 射程不夠
    CHECK_FALSE(scene.Raycast(Ray(Vector3(0, 0, 0), Vector3(0, 0, 1), 5.0f)).has_value());
    // 方向相反
    CHECK_FALSE(scene.Raycast(Ray(Vector3(0, 0, 0), Vector3(0, 0, -1))).has_value());
}

TEST_CASE("Scene raycast uses the full parent chain", "[scene]")
{
    Scene scene;
    const EntityId parent = scene.CreateEntity("parent");
    const EntityId child = scene.CreateEntity("child", parent);
    scene.SetLocalTransform(parent, At(0, 0, 10));
    scene.SetLocalTransform(child, At(0, 0, 10));
    scene.SetRenderable(child, UnitBox());

    const auto hit = scene.Raycast(Ray(Vector3(0, 0, 0), Vector3(0, 0, 1)));
    REQUIRE(hit.has_value());
    CHECK(hit->entity == child);
    CHECK(hit->distance == Catch::Approx(19.5f).margin(1e-3f));
}

TEST_CASE("Scene QueryVisible culls by frustum and visibility", "[scene]")
{
    Scene scene;
    const EntityId inFront = scene.CreateEntity("front");
    const EntityId behind = scene.CreateEntity("behind");
    const EntityId toTheSide = scene.CreateEntity("side");
    const EntityId hidden = scene.CreateEntity("hidden");
    for (const EntityId id : {inFront, behind, toTheSide, hidden})
        scene.SetRenderable(id, UnitBox());
    scene.SetLocalTransform(inFront, At(0, 0, 10));
    scene.SetLocalTransform(behind, At(0, 0, -10));
    scene.SetLocalTransform(toTheSide, At(500, 0, 10));
    scene.SetLocalTransform(hidden, At(0, 0, 12));
    Renderable invisible = UnitBox();
    invisible.visible = false;
    scene.SetRenderable(hidden, invisible);

    Camera camera;
    camera.SetViewport(800.0f, 600.0f);
    const std::vector<EntityId> visible = scene.QueryVisible(camera.GetFrustum());

    REQUIRE(visible.size() == 1);
    CHECK(visible[0] == inFront);
}

TEST_CASE("Scene picking with a camera ray selects the clicked entity", "[scene]")
{
    Scene scene;
    const EntityId left = scene.CreateEntity("left");
    const EntityId right = scene.CreateEntity("right");
    scene.SetRenderable(left, UnitBox());
    scene.SetRenderable(right, UnitBox());
    scene.SetLocalTransform(left, At(-3, 0, 10));
    scene.SetLocalTransform(right, At(3, 0, 10));

    Camera camera;
    camera.SetViewport(800.0f, 600.0f);

    // 點擊物體投影到螢幕上的位置，應該選到該物體
    for (const EntityId target : {left, right})
    {
        const auto screen = camera.WorldToScreen(scene.WorldPosition(target), 800.0f, 600.0f);
        REQUIRE(screen.has_value());
        const auto hit = scene.Raycast(camera.ScreenPointToRay(screen->x, screen->y, 800.0f, 600.0f));
        REQUIRE(hit.has_value());
        CHECK(hit->entity == target);
    }

    // 點擊畫面角落什麼都沒有
    CHECK_FALSE(scene.Raycast(camera.ScreenPointToRay(5.0f, 5.0f, 800.0f, 600.0f)).has_value());
}
