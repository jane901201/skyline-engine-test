#include "scene/euler.h"
#include "scene/transform.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "scene_test_utils.h"

using namespace Skyline;
using Catch::Approx;
using SceneTest::kPi;

namespace
{
constexpr float kDegrees = kPi / 180.0f;

Vector3 Rotate(const Quaternion& q, float x, float y, float z)
{
    Vector3 result;
    DirectX::XMStoreFloat3(&result.vec, DirectX::XMVector3Rotate(DirectX::XMVectorSet(x, y, z, 0), q));
    return result;
}
} // namespace

TEST_CASE("Euler angles round trip away from gimbal lock", "[scene][euler]")
{
    for (const float pitch : {-80.0f, -45.0f, 0.0f, 30.0f, 85.0f})
        for (const float yaw : {-170.0f, -90.0f, 0.0f, 45.0f, 170.0f})
            for (const float roll : {-160.0f, -30.0f, 0.0f, 90.0f, 150.0f})
            {
                const Vector3 euler(pitch * kDegrees, yaw * kDegrees, roll * kDegrees);
                const Vector3 back = QuaternionToEuler(QuaternionFromEuler(euler));
                CHECK(back.x == Approx(euler.x).margin(2e-3f));
                CHECK(back.y == Approx(euler.y).margin(2e-3f));
                CHECK(back.z == Approx(euler.z).margin(2e-3f));
            }
}

TEST_CASE("Euler conversion gives the same rotation at gimbal lock", "[scene][euler]")
{
    // pitch = ±90° 時 yaw 與 roll 無法分開，但重組後的旋轉必須與原本相同
    for (const float pitch : {90.0f, -90.0f})
    {
        const Quaternion original = QuaternionFromEuler(Vector3(pitch * kDegrees, 30.0f * kDegrees, 20.0f * kDegrees));
        const Quaternion rebuilt = QuaternionFromEuler(QuaternionToEuler(original));
        for (const Vector3 axis : {Vector3(1, 0, 0), Vector3(0, 1, 0), Vector3(0, 0, 1)})
        {
            const Vector3 a = Rotate(original, axis.x, axis.y, axis.z);
            const Vector3 b = Rotate(rebuilt, axis.x, axis.y, axis.z);
            CHECK_VEC3(b, a.x, a.y, a.z);
        }
    }
}

TEST_CASE("Euler angles follow the left-handed convention", "[scene][euler]")
{
    // 繞 Y 軸轉 +90 度：+Z 轉向 +X
    Transform yaw;
    yaw.rotation = QuaternionFromEuler(Vector3(0, 90.0f * kDegrees, 0));
    CHECK_VEC3(yaw.Forward(), 1, 0, 0);

    // 正的 pitch 往下看：+Z 轉向 -Y
    Transform pitch;
    pitch.rotation = QuaternionFromEuler(Vector3(90.0f * kDegrees, 0, 0));
    CHECK_VEC3(pitch.Forward(), 0, -1, 0);

    const Vector3 euler = QuaternionToEuler(yaw.rotation);
    CHECK(euler.x == Approx(0.0f).margin(1e-4f));
    CHECK(euler.y == Approx(90.0f * kDegrees).margin(1e-4f));
    CHECK(euler.z == Approx(0.0f).margin(1e-4f));
}

TEST_CASE("Identity rotation has zero Euler angles", "[scene][euler]")
{
    const Vector3 euler = QuaternionToEuler(Quaternion(0, 0, 0, 1));
    CHECK(euler.x == Approx(0.0f).margin(1e-6f));
    CHECK(euler.y == Approx(0.0f).margin(1e-6f));
    CHECK(euler.z == Approx(0.0f).margin(1e-6f));
}
