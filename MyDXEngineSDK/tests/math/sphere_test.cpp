#include "vector_test_helpers.h"

#include "math/intersection.h"

TEST_CASE("Sphere Contains includes the boundary", "[math][sphere]")
{
    const Sphere sphere({1, 2, 3}, 2.0f);
    CHECK(sphere.Contains({1, 2, 3}));
    CHECK(sphere.Contains({3, 2, 3}));
    CHECK_FALSE(sphere.Contains({3.1f, 2, 3}));
}

TEST_CASE("Raycast against Sphere reports distance and normal", "[math][sphere][ray]")
{
    const Sphere sphere({0, 0, 0}, 1.0f);
    RayHit hit;

    REQUIRE(Raycast(Ray({0, 0, -5}, {0, 0, 1}), sphere, hit));
    CHECK_THAT(hit.t, Catch::Matchers::WithinAbs(4.0, 1e-6));
    CheckVector(hit.normal, {0, 0, -1});

    // 與球相切
    CHECK(Raycast(Ray({1, 0, -5}, {0, 0, 1}), sphere, hit));
}

TEST_CASE("Raycast against Sphere handles misses and inside origin", "[math][sphere][ray]")
{
    const Sphere sphere({0, 0, 0}, 1.0f);
    RayHit hit;

    CHECK_FALSE(Raycast(Ray({0, 0, -5}, {0, 0, -1}), sphere, hit));
    CHECK_FALSE(Raycast(Ray({2, 0, -5}, {0, 0, 1}), sphere, hit));
    CHECK_FALSE(Raycast(Ray({0, 0, -5}, {0, 0, 1}, 3.0f), sphere, hit));

    // 起點在球內 → t = 0
    REQUIRE(Raycast(Ray({0, 0, 0}, {1, 0, 0}), sphere, hit));
    CHECK(hit.t == 0.0f);
    CheckVector(hit.normal, {-1, 0, 0});
}

TEST_CASE("Sphere overlap test", "[math][sphere]")
{
    CHECK(Intersects(Sphere({0, 0, 0}, 1.0f), Sphere({2, 0, 0}, 1.0f)));
    CHECK(Intersects(Sphere({0, 0, 0}, 1.0f), Sphere({1, 0, 0}, 0.5f)));
    CHECK_FALSE(Intersects(Sphere({0, 0, 0}, 1.0f), Sphere({2.1f, 0, 0}, 1.0f)));
}
