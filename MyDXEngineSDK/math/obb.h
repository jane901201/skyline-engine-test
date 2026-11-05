#pragma once

#include <cmath>

#include "matrix.h"
#include "vector.h"

/// <summary>
/// 有向包圍盒 (Oriented Bounding Box)。
/// </summary>
/// <remarks>
/// axes 為盒子三個局部軸在世界空間中的單位向量，彼此需互相垂直。
/// 不支援非等比縮放，縮放請直接反映在 halfExtents。
/// </remarks>
struct OBB
{
    // 中心點
    Vector3 center;
    // 各局部軸方向上的半長
    Vector3 halfExtents{0.5f, 0.5f, 0.5f};
    // 局部軸（世界空間單位向量）
    Vector3 axes[3] = {Vector3::AxisX, Vector3::AxisY, Vector3::AxisZ};

    OBB() = default;

    /// <summary>
    /// 以旋轉矩陣建立 OBB。
    /// </summary>
    /// <remarks>
    /// row-vector 慣例下，旋轉矩陣的第 0~2 列就是局部軸在世界空間的方向。
    /// </remarks>
    static OBB FromRotation(const Vector3& center, const Vector3& halfExtents, const Matrix& rotation)
    {
        OBB obb;
        obb.center = center;
        obb.halfExtents = halfExtents;
        for (int i = 0; i < 3; ++i)
        {
            obb.axes[i].Set(rotation.m[i][0], rotation.m[i][1], rotation.m[i][2]);
            obb.axes[i].Normalize();
        }
        return obb;
    }

    /// <summary>
    /// 以四元數建立 OBB。
    /// </summary>
    static OBB FromQuaternion(const Vector3& center, const Vector3& halfExtents, const Quaternion& rotation)
    {
        Matrix rotationMatrix;
        rotationMatrix.MakeRotationFromQuaternion(rotation);
        return FromRotation(center, halfExtents, rotationMatrix);
    }

    /// <summary>
    /// 將世界座標的點轉換到 OBB 的局部空間。
    /// </summary>
    Vector3 ToLocal(const Vector3& worldPoint) const
    {
        const Vector3 offset = worldPoint - center;
        return Vector3(Dot(offset, axes[0]), Dot(offset, axes[1]), Dot(offset, axes[2]));
    }

    /// <summary>
    /// 點是否在盒內（含邊界）。
    /// </summary>
    bool Contains(const Vector3& worldPoint) const
    {
        const Vector3 local = ToLocal(worldPoint);
        return std::fabs(local.x) <= halfExtents.x && std::fabs(local.y) <= halfExtents.y &&
               std::fabs(local.z) <= halfExtents.z;
    }
};
