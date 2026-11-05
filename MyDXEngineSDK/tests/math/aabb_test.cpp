#include "vector_test_helpers.h"

#include "math/intersection.h"

TEST_CASE("AABB default is empty and Encapsulate grows it", "[math][aabb]")
{
    AABB box;
    CHECK_FALSE(box.IsValid());

    box.Encapsulate({1, 2, 3});
    CHECK(box.IsValid());
    CheckVector(box.min, {1, 2, 3});
    CheckVector(box.max, {1, 2, 3});

    box.Encapsulate({-1, 5, 0});
    CheckVector(box.min, {-1, 2, 0});
    CheckVector(box.max, {1, 5, 3});
    CheckVector(box.Center(), {0, 3.5f, 1.5f});
    CheckVector(box.Extents(), {1, 1.5f, 1.5f});
    CheckVector(box.Size(), {2, 3, 3});
}

TEST_CASE("AABB Merge and Contains", "[math][aabb]")
{
    AABB a({0, 0, 0}, {1, 1, 1});
    const AABB b({2, -1, 0}, {3, 0, 4});
    a.Merge(b);
    CheckVector(a.min, {0, -1, 0});
    CheckVector(a.max, {3, 1, 4});

    CHECK(a.Contains({1, 0, 1}));
    CHECK(a.Contains({3, 1, 4}));
    CHECK_FALSE(a.Contains({3.1f, 0, 0}));
}

TEST_CASE("AABB Transform handles translation and rotation", "[math][aabb]")
{
    const AABB box = AABB::FromCenterExtents({0, 0, 0}, {1, 2, 3});

    Matrix translation;
    translation.MakeTranslation(10, 0, 0);
    const AABB moved = box.Transform(translation);
    CheckVector(moved.Center(), {10, 0, 0});
    CheckVector(moved.Extents(), {1, 2, 3});

    Matrix rotation;
    rotation.MakeRotationY(Math::PI * 0.5f);
    const AABB rotated = box.Transform(rotation);
    CHECK_THAT(rotated.Extents().x, Catch::Matchers::WithinAbs(3.0, 1e-5));
    CHECK_THAT(rotated.Extents().y, Catch::Matchers::WithinAbs(2.0, 1e-5));
    CHECK_THAT(rotated.Extents().z, Catch::Matchers::WithinAbs(1.0, 1e-5));
}

TEST_CASE("Raycast against AABB reports distance and face normal", "[math][aabb][ray]")
{
    const AABB box({-1, -1, -1}, {1, 1, 1});
    RayHit hit;

    // 從 -Z 方向打向盒子，命中 -Z 面
    REQUIRE(Raycast(Ray({0, 0, -5}, {0, 0, 1}), box, hit));
    CHECK_THAT(hit.t, Catch::Matchers::WithinAbs(4.0, 1e-6));
    CheckVector(hit.normal, {0, 0, -1});

    // 從 +X 方向打向盒子，命中 +X 面
    REQUIRE(Raycast(Ray({5, 0, 0}, {-1, 0, 0}), box, hit));
    CHECK_THAT(hit.t, Catch::Matchers::WithinAbs(4.0, 1e-6));
    CheckVector(hit.normal, {1, 0, 0});
}

TEST_CASE("Raycast against AABB handles misses and edge cases", "[math][aabb][ray]")
{
    const AABB box({-1, -1, -1}, {1, 1, 1});
    RayHit hit;

    // 背向盒子
    CHECK_FALSE(Raycast(Ray({0, 0, -5}, {0, 0, -1}), box, hit));
    // 擦過盒子旁邊
    CHECK_FALSE(Raycast(Ray({2, 0, -5}, {0, 0, 1}), box, hit));
    // 平行於 slab 且在 slab 外（direction 的 x、y 分量為 0）
    CHECK_FALSE(Raycast(Ray({0, 3, -5}, {0, 0, 1}), box, hit));
    // 距離超過 tMax
    CHECK_FALSE(Raycast(Ray({0, 0, -5}, {0, 0, 1}, 3.0f), box, hit));

    // 起點在盒內 → t = 0，法線為 -direction
    REQUIRE(Raycast(Ray({0, 0, 0}, {0, 0, 1}), box, hit));
    CHECK(hit.t == 0.0f);
    CheckVector(hit.normal, {0, 0, -1});

    // 剛好擦到邊界視為命中
    CHECK(Raycast(Ray({1, 0, -5}, {0, 0, 1}), box, hit));
}

TEST_CASE("AABB overlap tests", "[math][aabb]")
{
    const AABB a({0, 0, 0}, {2, 2, 2});
    CHECK(Intersects(a, AABB({1, 1, 1}, {3, 3, 3})));
    CHECK(Intersects(a, AABB({2, 0, 0}, {3, 1, 1})));
    CHECK_FALSE(Intersects(a, AABB({2.1f, 0, 0}, {3, 1, 1})));

    CHECK(Intersects(a, Sphere({3, 1, 1}, 1.0f)));
    CHECK_FALSE(Intersects(a, Sphere({4, 1, 1}, 1.0f)));
    // 球心在盒內
    CHECK(Intersects(Sphere({1, 1, 1}, 0.1f), a));
    // 球在盒角外：與角的距離為 sqrt(3) > 1
    CHECK_FALSE(Intersects(a, Sphere({3, 3, 3}, 1.0f)));
}
