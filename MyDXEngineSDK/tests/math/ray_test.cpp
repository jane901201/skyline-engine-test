#include "vector_test_helpers.h"

#include "math/intersection.h"

namespace
{
void CheckNear(const Vector3& actual, const Vector3& expected, double tolerance = 1e-4)
{
    CHECK_THAT(actual.x, Catch::Matchers::WithinAbs(expected.x, tolerance));
    CHECK_THAT(actual.y, Catch::Matchers::WithinAbs(expected.y, tolerance));
    CHECK_THAT(actual.z, Catch::Matchers::WithinAbs(expected.z, tolerance));
}
} // namespace

TEST_CASE("Ray constructor normalizes direction and PointAt walks along it", "[math][ray]")
{
    const Ray ray({1, 2, 3}, {0, 0, 10});
    CheckVector(ray.direction, {0, 0, 1});
    CheckVector(ray.PointAt(2.5f), {1, 2, 5.5f});
}

TEST_CASE("Raycast against triangle", "[math][ray][triangle]")
{
    const Vector3 a{-1, -1, 0};
    const Vector3 b{1, -1, 0};
    const Vector3 c{0, 1, 0};
    RayHit hit;

    REQUIRE(Raycast(Ray({0, 0, -3}, {0, 0, 1}), a, b, c, hit));
    CHECK_THAT(hit.t, Catch::Matchers::WithinAbs(3.0, 1e-6));
    // 法線一定朝向射線來的一側
    CheckVector(hit.normal, {0, 0, -1});

    // 從背面打也會命中（雙面）
    REQUIRE(Raycast(Ray({0, 0, 3}, {0, 0, -1}), a, b, c, hit));
    CHECK_THAT(hit.t, Catch::Matchers::WithinAbs(3.0, 1e-6));
    CheckVector(hit.normal, {0, 0, 1});

    // 三角形外
    CHECK_FALSE(Raycast(Ray({2, 0, -3}, {0, 0, 1}), a, b, c, hit));
    // 背向三角形
    CHECK_FALSE(Raycast(Ray({0, 0, -3}, {0, 0, -1}), a, b, c, hit));
    // 平行於三角形平面
    CHECK_FALSE(Raycast(Ray({0, 0, -3}, {1, 0, 0}), a, b, c, hit));
    // 超過 tMax
    CHECK_FALSE(Raycast(Ray({0, 0, -3}, {0, 0, 1}, 2.0f), a, b, c, hit));
}

TEST_CASE("ScreenPointToRay unprojects through the camera", "[math][ray][camera]")
{
    Matrix view;
    view.MakeLookAt({0, 0, -5}, {0, 0, 0}, {0, 1, 0});
    Matrix projection;
    // 垂直視角 90°、長寬比 1，使 NDC 邊緣剛好對應 45° 方向
    projection.MakeProjectionMatrix(Math::PI * 0.5f, 1.0f, 0.1f, 100.0f);

    // 畫面正中央：沿 +Z 直視
    const Ray center = ScreenPointToRay(400, 400, 800, 800, view, projection);
    CheckNear(center.direction, {0, 0, 1});
    CHECK_THAT(center.origin.z, Catch::Matchers::WithinAbs(-4.9, 1e-3));

    // 畫面右上角：x 向右、y 向上、z 向前，三軸分量相等
    const Ray topRight = ScreenPointToRay(800, 0, 800, 800, view, projection);
    const float s = 1.0f / std::sqrt(3.0f);
    CheckNear(topRight.direction, {s, s, s});

    // 畫面左下角
    const Ray bottomLeft = ScreenPointToRay(0, 800, 800, 800, view, projection);
    CheckNear(bottomLeft.direction, {-s, -s, s});
}

TEST_CASE("ScreenPointToRay can pick an object in front of the camera", "[math][ray][camera]")
{
    Matrix view;
    view.MakeLookAt({0, 0, -5}, {0, 0, 0}, {0, 1, 0});
    Matrix projection;
    projection.MakeProjectionMatrix(Math::PI * 0.5f, 1.0f, 0.1f, 100.0f);

    const AABB box({-1, -1, -1}, {1, 1, 1});
    RayHit hit;

    // 點擊畫面中央應命中箱子，距離從近平面起算：4 - 0.1 = 3.9
    const Ray hitRay = ScreenPointToRay(400, 400, 800, 800, view, projection);
    REQUIRE(Raycast(hitRay, box, hit));
    CHECK_THAT(hit.t, Catch::Matchers::WithinAbs(3.9, 1e-3));

    // 點擊畫面邊角應該沒命中
    const Ray missRay = ScreenPointToRay(0, 0, 800, 800, view, projection);
    CHECK_FALSE(Raycast(missRay, box, hit));
}
