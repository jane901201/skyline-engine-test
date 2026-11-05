#pragma once

#include <cassert>
#include <cfloat>

#include "matrix.h"
#include "vector.h"

/// <summary>
/// 射線。
/// </summary>
/// <remarks>
/// direction 必須是單位向量，所有 Raycast 的 t 皆以世界單位距離表示。
/// </remarks>
struct Ray
{
    // 射線起點
    Vector3 origin;
    // 射線方向（單位向量）
    Vector3 direction{0.0f, 0.0f, 1.0f};
    // 射線有效的最大距離
    float tMax = FLT_MAX;

    Ray() = default;

    /// <summary>
    /// 建構函式，會自動將方向正規化。
    /// </summary>
    Ray(const Vector3& origin, const Vector3& direction, float tMax = FLT_MAX) : origin(origin), direction(direction), tMax(tMax)
    {
        this->direction.Normalize();
    }

    /// <summary>
    /// 取得射線上距離起點 t 處的點。
    /// </summary>
    Vector3 PointAt(float t) const
    {
        return origin + direction * t;
    }
};

/// <summary>
/// 射線命中資訊。
/// </summary>
struct RayHit
{
    // 命中距離
    float t = 0.0f;
    // 命中點的表面法線（單位向量）
    Vector3 normal;
};

/// <summary>
/// 將螢幕座標轉換為世界空間射線。
/// </summary>
/// <remarks>
/// 採用 row-vector 慣例與 D3D 的 LH 座標，NDC 深度範圍為 [0, 1]。
/// 螢幕座標原點在左上角，y 向下。
/// </remarks>
/// <param name="screenX">螢幕 X 座標（像素）。</param>
/// <param name="screenY">螢幕 Y 座標（像素）。</param>
/// <param name="viewportWidth">視埠寬度（像素）。</param>
/// <param name="viewportHeight">視埠高度（像素）。</param>
/// <param name="view">View 矩陣。</param>
/// <param name="projection">Projection 矩陣。</param>
inline Ray ScreenPointToRay(float screenX, float screenY, float viewportWidth, float viewportHeight, const Matrix& view,
                            const Matrix& projection)
{
    assert(viewportWidth > 0.0f && viewportHeight > 0.0f);

    const float ndcX = 2.0f * screenX / viewportWidth - 1.0f;
    const float ndcY = 1.0f - 2.0f * screenY / viewportHeight;

    Matrix viewProjection = view * projection;
    Matrix inverseViewProjection;
    inverseViewProjection.Inverse(viewProjection);

    // 近平面 (z = 0) 與遠平面 (z = 1) 上的點
    Vector4 nearPoint(ndcX, ndcY, 0.0f, 1.0f);
    Vector4 farPoint(ndcX, ndcY, 1.0f, 1.0f);
    inverseViewProjection.Apply(nearPoint);
    inverseViewProjection.Apply(farPoint);

    const Vector3 nearWorld(nearPoint.x / nearPoint.w, nearPoint.y / nearPoint.w, nearPoint.z / nearPoint.w);
    const Vector3 farWorld(farPoint.x / farPoint.w, farPoint.y / farPoint.w, farPoint.z / farPoint.w);

    return Ray(nearWorld, farWorld - nearWorld);
}
