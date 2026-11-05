#include "vector_test_helpers.h"

#include "math/intersection.h"

namespace
{
void CheckNear(const Vector3& actual, const Vector3& expected, double tolerance = 1e-5)
{
    CHECK_THAT(actual.x, Catch::Matchers::WithinAbs(expected.x, tolerance));
    CHECK_THAT(actual.y, Catch::Matchers::WithinAbs(expected.y, tolerance));
    CHECK_THAT(actual.z, Catch::Matchers::WithinAbs(expected.z, tolerance));
}
} // namespace

TEST_CASE("OBB with identity rotation behaves like an AABB", "[math][obb]")
{
    const OBB obb = OBB::FromRotation({0, 0, 0}, {1, 1, 1}, Matrix::Identity);
    RayHit hit;

    REQUIRE(Raycast(Ray({0, 0, -5}, {0, 0, 1}), obb, hit));
    CHECK_THAT(hit.t, Catch::Matchers::WithinAbs(4.0, 1e-6));
    CheckNear(hit.normal, {0, 0, -1});

    CHECK(obb.Contains({0.5f, 0.5f, 0.5f}));
    CHECK_FALSE(obb.Contains({1.5f, 0, 0}));
}

TEST_CASE("OBB rotated 45 degrees around Y is hit at its corner distance", "[math][obb]")
{
    Matrix rotation;
    rotation.MakeRotationY(Math::PI * 0.25f);
    const OBB obb = OBB::FromRotation({0, 0, 0}, {1, 1, 1}, rotation);
    RayHit hit;

    // 旋轉 45° 後，沿 Z 軸看到的是一個稜角，距離為 sqrt(2)
    REQUIRE(Raycast(Ray({0, 0, -5}, {0, 0, 1}), obb, hit));
    CHECK_THAT(hit.t, Catch::Matchers::WithinAbs(5.0 - std::sqrt(2.0), 1e-4));

    // (1.2, 0, 0) 在未旋轉的盒子之外，但旋轉 45° 後落在盒內
    CHECK(obb.Contains({1.2f, 0, 0}));
    CHECK_FALSE(obb.Contains({1.5f, 0, 0}));
}

TEST_CASE("OBB rotated 90 degrees swaps its extents", "[math][obb]")
{
    Matrix rotation;
    rotation.MakeRotationY(Math::PI * 0.5f);
    const OBB obb = OBB::FromRotation({0, 0, 0}, {3, 1, 1}, rotation);
    RayHit hit;

    // 原本長邊在 X，旋轉 90° 後長邊轉到 Z 軸
    REQUIRE(Raycast(Ray({0, 0, -10}, {0, 0, 1}), obb, hit));
    CHECK_THAT(hit.t, Catch::Matchers::WithinAbs(7.0, 1e-4));
    CheckNear(hit.normal, {0, 0, -1});

    CHECK_FALSE(Raycast(Ray({2, 0, -10}, {0, 0, 1}), obb, hit));
}

TEST_CASE("OBB can be built from a quaternion", "[math][obb]")
{
    Quaternion rotation;
    rotation.SetRotation(Vector3::AxisY, Math::PI * 0.5f);
    const OBB obb = OBB::FromQuaternion({0, 0, 0}, {3, 1, 1}, rotation);
    RayHit hit;

    REQUIRE(Raycast(Ray({0, 0, -10}, {0, 0, 1}), obb, hit));
    CHECK_THAT(hit.t, Catch::Matchers::WithinAbs(7.0, 1e-4));
}
