#pragma once

#include "vector.h"

/// <summary>
/// 包圍球。
/// </summary>
struct Sphere
{
    // 球心
    Vector3 center;
    // 半徑
    float radius = 0.0f;

    Sphere() = default;

    Sphere(const Vector3& center, float radius) : center(center), radius(radius)
    {
    }

    /// <summary>
    /// 點是否在球內（含邊界）。
    /// </summary>
    bool Contains(const Vector3& point) const
    {
        return (point - center).LengthSq() <= radius * radius;
    }
};
