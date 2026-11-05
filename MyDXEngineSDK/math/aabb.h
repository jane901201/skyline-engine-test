#pragma once

#include <algorithm>
#include <cfloat>
#include <cmath>

#include "matrix.h"
#include "vector.h"

/// <summary>
/// 軸對齊包圍盒 (Axis-Aligned Bounding Box)。
/// </summary>
/// <remarks>
/// 預設建構為「空盒」(min = +FLT_MAX, max = -FLT_MAX)，
/// 第一次呼叫 Encapsulate 後即成為有效的盒子。
/// </remarks>
struct AABB
{
    Vector3 min{FLT_MAX, FLT_MAX, FLT_MAX};
    Vector3 max{-FLT_MAX, -FLT_MAX, -FLT_MAX};

    AABB() = default;

    AABB(const Vector3& min, const Vector3& max) : min(min), max(max)
    {
    }

    /// <summary>
    /// 以中心點與半長建立包圍盒。
    /// </summary>
    static AABB FromCenterExtents(const Vector3& center, const Vector3& extents)
    {
        return AABB(center - extents, center + extents);
    }

    /// <summary>
    /// 是否為有效盒子（min 在每個軸上都不大於 max）。
    /// </summary>
    bool IsValid() const
    {
        return min.x <= max.x && min.y <= max.y && min.z <= max.z;
    }

    /// <summary>
    /// 中心點。
    /// </summary>
    Vector3 Center() const
    {
        return (min + max) * 0.5f;
    }

    /// <summary>
    /// 半長（中心到各面的距離）。
    /// </summary>
    Vector3 Extents() const
    {
        return (max - min) * 0.5f;
    }

    /// <summary>
    /// 完整尺寸。
    /// </summary>
    Vector3 Size() const
    {
        return max - min;
    }

    /// <summary>
    /// 擴張盒子使其包含指定的點。
    /// </summary>
    void Encapsulate(const Vector3& point)
    {
        min.Min(point);
        max.Max(point);
    }

    /// <summary>
    /// 擴張盒子使其包含另一個盒子。
    /// </summary>
    void Merge(const AABB& other)
    {
        min.Min(other.min);
        max.Max(other.max);
    }

    /// <summary>
    /// 點是否在盒內（含邊界）。
    /// </summary>
    bool Contains(const Vector3& point) const
    {
        return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y && point.z >= min.z &&
               point.z <= max.z;
    }

    /// <summary>
    /// 計算套用矩陣後的新 AABB（Arvo 方法）。
    /// </summary>
    /// <remarks>
    /// 採用 row-vector 慣例：p' = p * M。
    /// 旋轉後的結果是「能包住原盒子變換後形狀的最小 AABB」，因此可能比原本更鬆。
    /// </remarks>
    AABB Transform(const Matrix& matrix) const
    {
        const Vector3 center = Center();
        const Vector3 extents = Extents();

        Vector3 newCenter;
        Vector3 newExtents;
        for (int i = 0; i < 3; ++i)
        {
            newCenter.v[i] = center.x * matrix.m[0][i] + center.y * matrix.m[1][i] + center.z * matrix.m[2][i] + matrix.m[3][i];
            newExtents.v[i] = std::fabs(matrix.m[0][i]) * extents.x + std::fabs(matrix.m[1][i]) * extents.y +
                              std::fabs(matrix.m[2][i]) * extents.z;
        }
        return FromCenterExtents(newCenter, newExtents);
    }
};
