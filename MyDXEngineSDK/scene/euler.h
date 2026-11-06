#pragma once

#include <DirectXMath.h>

#include <algorithm>
#include <cmath>

#include "math/vector.h"

namespace Skyline
{
/// <summary>
/// 歐拉角（弧度）與四元數互轉，給編輯器的 Rotation 欄位使用。
/// </summary>
/// <remarks>
/// x = pitch（繞 X 軸）、y = yaw（繞 Y 軸）、z = roll（繞 Z 軸）。
/// 套用順序與 DirectXMath 的 XMQuaternionRotationRollPitchYaw 相同：先 roll、再 pitch、最後 yaw
/// （row-vector：v * Rz * Rx * Ry）。
/// 同一個旋轉可以有多組歐拉角，QuaternionToEuler 回傳的是 pitch 在 [-90°, 90°] 內的那一組；
/// pitch 接近 ±90°（萬向鎖）時 roll 固定為 0。
/// </remarks>
[[nodiscard]] inline Quaternion QuaternionFromEuler(const Vector3& euler)
{
    Quaternion result;
    DirectX::XMStoreFloat4(&result.vec, DirectX::XMQuaternionRotationRollPitchYaw(euler.x, euler.y, euler.z));
    return result;
}

[[nodiscard]] inline Vector3 QuaternionToEuler(const Quaternion& rotation)
{
    // 用 double 計算：pitch 接近 ±90° 時 cos(pitch) 很小，用 float 的矩陣元素會被捨入誤差蓋過，
    // 導致誤判成「不在萬向鎖」而算出雜訊
    double x = rotation.vec.x;
    double y = rotation.vec.y;
    double z = rotation.vec.z;
    double w = rotation.vec.w;
    const double length = std::sqrt(x * x + y * y + z * z + w * w);
    if (length > 0.0)
    {
        x /= length;
        y /= length;
        z /= length;
        w /= length;
    }

    // row-vector 的旋轉矩陣（與 XMMatrixRotationQuaternion 相同），只需要用到的元素：
    //   m20 = 2(xz + wy)   m21 = 2(yz - wx)   m22 = 1 - 2(x² + y²)
    //   m01 = 2(xy + wz)   m11 = 1 - 2(x² + z²)
    //   m02 = 2(xz - wy)   m00 = 1 - 2(y² + z²)
    // Rz(roll) * Rx(pitch) * Ry(yaw) 展開後：m21 = -sin(pitch)，m20 = cos(pitch)*sin(yaw)，
    // m22 = cos(pitch)*cos(yaw)，m01 = sin(roll)*cos(pitch)，m11 = cos(roll)*cos(pitch)
    const double m21 = 2.0 * (y * z - w * x);
    const double sinPitch = std::clamp(-m21, -1.0, 1.0);
    const double pitch = std::asin(sinPitch);

    double yaw;
    double roll;
    if (std::sqrt(1.0 - sinPitch * sinPitch) > 1e-6)
    {
        yaw = std::atan2(2.0 * (x * z + w * y), 1.0 - 2.0 * (x * x + y * y));
        roll = std::atan2(2.0 * (x * y + w * z), 1.0 - 2.0 * (x * x + z * z));
    }
    else
    {
        // 萬向鎖：yaw 與 roll 轉的是同一條軸，只能決定兩者的和，把 roll 固定為 0
        yaw = std::atan2(-2.0 * (x * z - w * y), 1.0 - 2.0 * (y * y + z * z));
        roll = 0.0;
    }
    return Vector3(static_cast<float>(pitch), static_cast<float>(yaw), static_cast<float>(roll));
}
} // namespace Skyline
