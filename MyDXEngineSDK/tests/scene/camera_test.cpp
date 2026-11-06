#include "scene/camera.h"

#include "scene_test_utils.h"

#include <stdexcept>

using namespace Skyline;
using namespace SceneTest;

namespace
{
constexpr float kWidth = 800.0f;
constexpr float kHeight = 600.0f;

// 位於原點、朝 +Z 的攝影機，長寬比與視埠一致
Camera MakeCamera()
{
    Camera camera;
    camera.SetViewport(kWidth, kHeight);
    camera.nearZ = 0.1f;
    camera.farZ = 100.0f;
    return camera;
}

AABB BoxAt(const Vector3& center, float halfSize = 0.5f)
{
    return AABB::FromCenterExtents(center, Vector3(halfSize, halfSize, halfSize));
}
} // namespace

TEST_CASE("Camera view matrix moves the camera to the origin", "[scene]")
{
    Camera camera = MakeCamera();
    camera.transform.position = Vector3(3.0f, 2.0f, -4.0f);

    Vector3 cameraInView = camera.transform.position;
    camera.ViewMatrix().Apply(cameraInView);
    CHECK_VEC3(cameraInView, 0.0f, 0.0f, 0.0f);

    // 攝影機前方 5 單位的點，在 View 空間中位於 +Z 軸上
    Vector3 ahead = camera.transform.position + camera.transform.Forward() * 5.0f;
    camera.ViewMatrix().Apply(ahead);
    CHECK_VEC3(ahead, 0.0f, 0.0f, 5.0f);
}

TEST_CASE("Camera view matrix matches XMMatrixLookAtLH", "[scene]")
{
    Camera camera = MakeCamera();
    camera.transform = Transform::LookAt(Vector3(1, 2, 3), Vector3(-2, 0, 8));

    Matrix expected;
    expected.MakeLookAt(Vector3(1, 2, 3), Vector3(-2, 0, 8), Vector3(0, 1, 0));
    const Matrix actual = camera.ViewMatrix();
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
            CHECK(actual.m[row][column] == Catch::Approx(expected.m[row][column]).margin(1e-4f));
    }
}

TEST_CASE("Camera rejects invalid projection parameters", "[scene]")
{
    Camera camera = MakeCamera();
    CHECK_NOTHROW(camera.ProjectionMatrix());

    camera.nearZ = 0.0f;
    CHECK_THROWS_AS(camera.ProjectionMatrix(), std::invalid_argument);
    camera.nearZ = 10.0f;
    camera.farZ = 5.0f;
    CHECK_THROWS_AS(camera.ProjectionMatrix(), std::invalid_argument);

    camera = MakeCamera();
    camera.fovY = 0.0f;
    CHECK_THROWS_AS(camera.ProjectionMatrix(), std::invalid_argument);
    camera.fovY = kPi;
    CHECK_THROWS_AS(camera.ProjectionMatrix(), std::invalid_argument);

    camera = MakeCamera();
    camera.aspect = 0.0f;
    CHECK_THROWS_AS(camera.ProjectionMatrix(), std::invalid_argument);

    CHECK_THROWS_AS(camera.SetViewport(0.0f, 100.0f), std::invalid_argument);
    CHECK_THROWS_AS(camera.SetViewport(100.0f, 0.0f), std::invalid_argument);
}

TEST_CASE("Camera WorldToScreen maps directions to the right screen side", "[scene]")
{
    const Camera camera = MakeCamera();

    const auto center = camera.WorldToScreen(Vector3(0, 0, 10), kWidth, kHeight);
    REQUIRE(center.has_value());
    CHECK(center->x == Catch::Approx(kWidth * 0.5f).margin(1e-2f));
    CHECK(center->y == Catch::Approx(kHeight * 0.5f).margin(1e-2f));
    CHECK(center->z > 0.0f);
    CHECK(center->z < 1.0f);

    const auto right = camera.WorldToScreen(Vector3(2, 0, 10), kWidth, kHeight);
    REQUIRE(right.has_value());
    CHECK(right->x > kWidth * 0.5f);

    // 螢幕座標 y 向下，所以世界上方的點 y 比較小
    const auto above = camera.WorldToScreen(Vector3(0, 2, 10), kWidth, kHeight);
    REQUIRE(above.has_value());
    CHECK(above->y < kHeight * 0.5f);

    CHECK_FALSE(camera.WorldToScreen(Vector3(0, 0, -5), kWidth, kHeight).has_value());
}

TEST_CASE("Camera ScreenPointToRay passes through the projected point", "[scene]")
{
    Camera camera = MakeCamera();
    camera.transform = Transform::LookAt(Vector3(2, 3, -6), Vector3(0, 1, 4));

    const Vector3 target(1.0f, 2.0f, 3.0f);
    const auto screen = camera.WorldToScreen(target, kWidth, kHeight);
    REQUIRE(screen.has_value());

    const Ray ray = camera.ScreenPointToRay(screen->x, screen->y, kWidth, kHeight);

    // 目標點到射線的垂直距離應該接近 0
    Vector3 toTarget = target - ray.origin;
    const float along = toTarget.Dot(ray.direction);
    const Vector3 closest = ray.origin + ray.direction * along;
    CHECK((target - closest).Length() < 1e-3f);
    CHECK(along > 0.0f);
}

TEST_CASE("Camera center ray points along the view direction", "[scene]")
{
    const Camera camera = MakeCamera();
    const Ray ray = camera.ScreenPointToRay(kWidth * 0.5f, kHeight * 0.5f, kWidth, kHeight);
    CHECK_VEC3(ray.direction, 0.0f, 0.0f, 1.0f);
    CHECK(ray.origin.z == Catch::Approx(0.1f).margin(1e-3f));
}

TEST_CASE("Camera orthographic projection keeps a constant size", "[scene]")
{
    Camera camera = MakeCamera();
    camera.orthographic = true;
    camera.orthographicHeight = 10.0f;

    const auto near = camera.WorldToScreen(Vector3(2.5f, 0, 5), kWidth, kHeight);
    const auto far = camera.WorldToScreen(Vector3(2.5f, 0, 50), kWidth, kHeight);
    REQUIRE(near.has_value());
    REQUIRE(far.has_value());
    // 正交投影下，距離不影響螢幕位置
    CHECK(near->x == Catch::Approx(far->x).margin(1e-2f));
}

TEST_CASE("Frustum contains points inside the view volume only", "[scene]")
{
    const Frustum frustum = MakeCamera().GetFrustum();

    CHECK(frustum.Contains(Vector3(0, 0, 10)));
    CHECK_FALSE(frustum.Contains(Vector3(0, 0, -1)));    // 在後方
    CHECK_FALSE(frustum.Contains(Vector3(0, 0, 0.05f))); // 比近平面還近
    CHECK_FALSE(frustum.Contains(Vector3(0, 0, 150)));   // 超過遠平面
    CHECK_FALSE(frustum.Contains(Vector3(100, 0, 10)));  // 太靠右
    CHECK_FALSE(frustum.Contains(Vector3(-100, 0, 10))); // 太靠左
    CHECK_FALSE(frustum.Contains(Vector3(0, 100, 10)));  // 太靠上
    CHECK_FALSE(frustum.Contains(Vector3(0, -100, 10))); // 太靠下
}

TEST_CASE("Frustum culls boxes outside and keeps boxes inside", "[scene]")
{
    const Frustum frustum = MakeCamera().GetFrustum();

    CHECK(frustum.Intersects(BoxAt(Vector3(0, 0, 10))));
    // 部分在視錐體內也要保留
    CHECK(frustum.Intersects(BoxAt(Vector3(0, 0, 100), 5.0f)));
    CHECK(frustum.Intersects(BoxAt(Vector3(0, 0, 0.0f), 1.0f)));

    CHECK_FALSE(frustum.Intersects(BoxAt(Vector3(0, 0, -10))));
    CHECK_FALSE(frustum.Intersects(BoxAt(Vector3(0, 0, 200))));
    CHECK_FALSE(frustum.Intersects(BoxAt(Vector3(500, 0, 10))));
    CHECK_FALSE(frustum.Intersects(AABB()));
}

TEST_CASE("Frustum follows the camera transform", "[scene]")
{
    Camera camera = MakeCamera();
    camera.transform = Transform::LookAt(Vector3(0, 0, 0), Vector3(1, 0, 0));
    const Frustum frustum = camera.GetFrustum();

    CHECK(frustum.Contains(Vector3(10, 0, 0)));
    CHECK_FALSE(frustum.Contains(Vector3(0, 0, 10)));
    CHECK_FALSE(frustum.Contains(Vector3(-10, 0, 0)));
}
