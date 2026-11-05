#pragma once

#include <algorithm>
#include <cmath>
#include <utility>

#include "aabb.h"
#include "obb.h"
#include "ray.h"
#include "sphere.h"
#include "vector.h"

// =====================================================================
// 射線查詢 (Raycast)
//
// 共同規則：
// - Ray::direction 必須是單位向量，hit.t 即世界單位距離。
// - 只接受 0 <= t <= ray.tMax 的命中。
// - 射線起點在形狀內部時，視為命中且 t = 0，法線為 -direction。
// =====================================================================

/// <summary>
/// 射線與 AABB 相交（Slab 方法）。
/// </summary>
inline bool Raycast(const Ray& ray, const AABB& box, RayHit& hit)
{
    constexpr float kParallelEpsilon = 1e-8f;

    float tNear = 0.0f;
    float tFar = ray.tMax;
    int nearAxis = -1;
    float nearSign = 0.0f;

    for (int i = 0; i < 3; ++i)
    {
        const float origin = ray.origin.v[i];
        const float direction = ray.direction.v[i];
        const float lo = box.min.v[i];
        const float hi = box.max.v[i];

        // 射線與該軸的 slab 平行：起點必須落在 slab 內，否則不可能命中
        if (std::fabs(direction) < kParallelEpsilon)
        {
            if (origin < lo || origin > hi)
                return false;
            continue;
        }

        const float inverse = 1.0f / direction;
        float t1 = (lo - origin) * inverse;
        float t2 = (hi - origin) * inverse;
        // 從 min 面進入 → 法線為 -軸；從 max 面進入 → 法線為 +軸
        float sign = -1.0f;
        if (t1 > t2)
        {
            std::swap(t1, t2);
            sign = 1.0f;
        }

        if (t1 > tNear)
        {
            tNear = t1;
            nearAxis = i;
            nearSign = sign;
        }
        tFar = std::min(tFar, t2);
        if (tNear > tFar)
            return false;
    }

    hit.t = tNear;
    if (nearAxis >= 0)
    {
        hit.normal = Vector3::Zero;
        hit.normal.v[nearAxis] = nearSign;
    }
    else
    {
        hit.normal = ray.direction * -1.0f;
    }
    return true;
}

/// <summary>
/// 射線與球相交。
/// </summary>
inline bool Raycast(const Ray& ray, const Sphere& sphere, RayHit& hit)
{
    const Vector3 toOrigin = ray.origin - sphere.center;
    const float b = Dot(toOrigin, ray.direction);
    const float c = toOrigin.LengthSq() - sphere.radius * sphere.radius;

    // 起點在球外且射線背向球
    if (c > 0.0f && b > 0.0f)
        return false;

    const float discriminant = b * b - c;
    if (discriminant < 0.0f)
        return false;

    // 起點在球內時 c <= 0，t 夾到 0
    const float t = std::max(0.0f, -b - std::sqrt(discriminant));
    if (t > ray.tMax)
        return false;

    hit.t = t;
    if (c <= 0.0f)
    {
        hit.normal = ray.direction * -1.0f;
    }
    else
    {
        hit.normal = (ray.PointAt(t) - sphere.center) / sphere.radius;
    }
    return true;
}

/// <summary>
/// 射線與 OBB 相交。
/// </summary>
/// <remarks>
/// 將射線轉到 OBB 局部空間後，用 AABB 的 Slab 測試，再把法線轉回世界空間。
/// 因為只有旋轉（無縮放），局部空間的 t 與世界空間相同。
/// </remarks>
inline bool Raycast(const Ray& ray, const OBB& obb, RayHit& hit)
{
    Ray localRay;
    localRay.origin = obb.ToLocal(ray.origin);
    localRay.direction.Set(Dot(ray.direction, obb.axes[0]), Dot(ray.direction, obb.axes[1]), Dot(ray.direction, obb.axes[2]));
    localRay.tMax = ray.tMax;

    const AABB localBox(obb.halfExtents * -1.0f, obb.halfExtents);
    RayHit localHit;
    if (!Raycast(localRay, localBox, localHit))
        return false;

    hit.t = localHit.t;
    hit.normal = obb.axes[0] * localHit.normal.x + obb.axes[1] * localHit.normal.y + obb.axes[2] * localHit.normal.z;
    return true;
}

/// <summary>
/// 射線與三角形相交（Möller–Trumbore，雙面）。
/// </summary>
/// <remarks>
/// 法線為三角形幾何法線，並會翻轉到面向射線的一側。
/// </remarks>
inline bool Raycast(const Ray& ray, const Vector3& a, const Vector3& b, const Vector3& c, RayHit& hit)
{
    constexpr float kParallelEpsilon = 1e-8f;

    const Vector3 edge1 = b - a;
    const Vector3 edge2 = c - a;
    const Vector3 p = Cross(ray.direction, edge2);
    const float determinant = Dot(edge1, p);
    if (std::fabs(determinant) < kParallelEpsilon)
        return false;

    const float inverse = 1.0f / determinant;
    const Vector3 s = ray.origin - a;
    const float u = Dot(s, p) * inverse;
    if (u < 0.0f || u > 1.0f)
        return false;

    const Vector3 q = Cross(s, edge1);
    const float v = Dot(ray.direction, q) * inverse;
    if (v < 0.0f || u + v > 1.0f)
        return false;

    const float t = Dot(edge2, q) * inverse;
    if (t < 0.0f || t > ray.tMax)
        return false;

    Vector3 normal = Cross(edge1, edge2);
    normal.Normalize();
    if (Dot(normal, ray.direction) > 0.0f)
        normal = normal * -1.0f;

    hit.t = t;
    hit.normal = normal;
    return true;
}

// =====================================================================
// 形狀對形狀的重疊測試 (Intersects)
// 邊界剛好接觸視為相交。
// =====================================================================

/// <summary>
/// AABB 與 AABB 是否重疊。
/// </summary>
inline bool Intersects(const AABB& a, const AABB& b)
{
    return a.min.x <= b.max.x && a.max.x >= b.min.x && a.min.y <= b.max.y && a.max.y >= b.min.y && a.min.z <= b.max.z &&
           a.max.z >= b.min.z;
}

/// <summary>
/// 球與球是否重疊。
/// </summary>
inline bool Intersects(const Sphere& a, const Sphere& b)
{
    const float radiusSum = a.radius + b.radius;
    return (a.center - b.center).LengthSq() <= radiusSum * radiusSum;
}

/// <summary>
/// AABB 與球是否重疊。
/// </summary>
inline bool Intersects(const AABB& box, const Sphere& sphere)
{
    // 球心夾到盒內後，與球心的距離即為球心到盒的最短距離
    Vector3 closest = sphere.center;
    closest.Max(box.min);
    closest.Min(box.max);
    return (closest - sphere.center).LengthSq() <= sphere.radius * sphere.radius;
}

inline bool Intersects(const Sphere& sphere, const AABB& box)
{
    return Intersects(box, sphere);
}
