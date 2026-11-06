#pragma once

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <DirectXMath.h>

#include "math/vector.h"

namespace SceneTest
{
constexpr float kPi = 3.14159265358979f;

// 繞世界 Y 軸旋轉的四元數（弧度），不依賴 Quaternion 類別自己的旋轉函式
inline Quaternion RotationY(float radians)
{
    Quaternion q;
    DirectX::XMStoreFloat4(&q.vec, DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(0, 1, 0, 0), radians));
    return q;
}

inline Quaternion RotationX(float radians)
{
    Quaternion q;
    DirectX::XMStoreFloat4(&q.vec, DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(1, 0, 0, 0), radians));
    return q;
}
} // namespace SceneTest

#define CHECK_VEC3(actual, ex, ey, ez)                                                                                \
    do                                                                                                                \
    {                                                                                                                 \
        const Vector3 skylineActual = (actual);                                                                       \
        CHECK(skylineActual.x == Catch::Approx(ex).margin(1e-4f));                                                    \
        CHECK(skylineActual.y == Catch::Approx(ey).margin(1e-4f));                                                    \
        CHECK(skylineActual.z == Catch::Approx(ez).margin(1e-4f));                                                    \
    } while (false)
