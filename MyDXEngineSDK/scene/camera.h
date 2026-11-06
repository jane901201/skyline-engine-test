#pragma once

#include <DirectXMath.h>

#include <array>
#include <cmath>
#include <optional>
#include <stdexcept>

#include "math/aabb.h"
#include "math/matrix.h"
#include "math/ray.h"
#include "math/vector.h"
#include "transform.h"

namespace Skyline
{
/// <summary>
/// 平面，法線指向「內側」，距離 >= 0 代表在內側。
/// </summary>
struct Plane
{
    Vector3 normal{0.0f, 1.0f, 0.0f};
    float d = 0.0f;

    /// 點到平面的有號距離（內側為正）
    [[nodiscard]] float Distance(const Vector3& point) const
    {
        return normal.x * point.x + normal.y * point.y + normal.z * point.z + d;
    }
};

/// <summary>
/// 視錐體，由 6 個朝內的平面組成（左、右、下、上、近、遠）。
/// </summary>
class Frustum
{
  public:
    /// <summary>
    /// 從 View * Projection 矩陣取出視錐體。
    /// </summary>
    /// <remarks>
    /// 採用 row-vector 慣例與 D3D 的 NDC 深度範圍 [0, 1]。
    /// </remarks>
    [[nodiscard]] static Frustum FromViewProjection(const Matrix& viewProjection)
    {
        // row-vector：clip = (x, y, z, 1) * M，所以 clip 的每個分量是矩陣「欄」與點的內積
        auto column = [&](int c) { return DirectX::XMFLOAT4(viewProjection.m[0][c], viewProjection.m[1][c], viewProjection.m[2][c], viewProjection.m[3][c]); };
        const DirectX::XMFLOAT4 c0 = column(0);
        const DirectX::XMFLOAT4 c1 = column(1);
        const DirectX::XMFLOAT4 c2 = column(2);
        const DirectX::XMFLOAT4 c3 = column(3);

        Frustum frustum;
        // -w <= x <= w、-w <= y <= w、0 <= z <= w
        frustum.m_planes[0] = MakePlane(c3.x + c0.x, c3.y + c0.y, c3.z + c0.z, c3.w + c0.w); // 左
        frustum.m_planes[1] = MakePlane(c3.x - c0.x, c3.y - c0.y, c3.z - c0.z, c3.w - c0.w); // 右
        frustum.m_planes[2] = MakePlane(c3.x + c1.x, c3.y + c1.y, c3.z + c1.z, c3.w + c1.w); // 下
        frustum.m_planes[3] = MakePlane(c3.x - c1.x, c3.y - c1.y, c3.z - c1.z, c3.w - c1.w); // 上
        frustum.m_planes[4] = MakePlane(c2.x, c2.y, c2.z, c2.w);                             // 近
        frustum.m_planes[5] = MakePlane(c3.x - c2.x, c3.y - c2.y, c3.z - c2.z, c3.w - c2.w); // 遠
        return frustum;
    }

    [[nodiscard]] const std::array<Plane, 6>& Planes() const noexcept
    {
        return m_planes;
    }

    /// 點是否在視錐體內（含邊界）
    [[nodiscard]] bool Contains(const Vector3& point) const
    {
        for (const Plane& plane : m_planes)
        {
            if (plane.Distance(point) < 0.0f)
                return false;
        }
        return true;
    }

    /// <summary>
    /// AABB 是否可能與視錐體相交。
    /// </summary>
    /// <remarks>
    /// 這是保守的測試：回傳 false 一定看不見，回傳 true 可能只是在視錐體角落外側。
    /// 無效（空）的 AABB 一律回傳 false。
    /// </remarks>
    [[nodiscard]] bool Intersects(const AABB& box) const
    {
        if (!box.IsValid())
            return false;

        for (const Plane& plane : m_planes)
        {
            // 取沿著平面法線方向最遠的角點，連它都在外側就整個盒子都在外側
            const Vector3 farthest(plane.normal.x >= 0.0f ? box.max.x : box.min.x,
                                   plane.normal.y >= 0.0f ? box.max.y : box.min.y,
                                   plane.normal.z >= 0.0f ? box.max.z : box.min.z);
            if (plane.Distance(farthest) < 0.0f)
                return false;
        }
        return true;
    }

  private:
    [[nodiscard]] static Plane MakePlane(float a, float b, float c, float d)
    {
        const float length = std::sqrt(a * a + b * b + c * c);
        Plane plane;
        plane.normal = Vector3(a / length, b / length, c / length);
        plane.d = d / length;
        return plane;
    }

    std::array<Plane, 6> m_planes;
};

/// <summary>
/// 攝影機：位置與朝向（Transform）加上投影參數。
/// </summary>
/// <remarks>
/// Transform 的縮放會被忽略。投影參數不合法時，ProjectionMatrix 會丟出 std::invalid_argument。
/// 螢幕座標的原點在左上角，y 向下。
/// </remarks>
class Camera
{
  public:
    Transform transform;

    // 垂直視角（弧度），必須在 (0, pi) 之間
    float fovY = DirectX::XMConvertToRadians(60.0f);
    // 寬 / 高
    float aspect = 16.0f / 9.0f;
    float nearZ = 0.1f;
    float farZ = 1000.0f;

    // 為 true 時使用正交投影，視錐體高度為 orthographicHeight
    bool orthographic = false;
    float orthographicHeight = 10.0f;

    /// 以視埠大小更新長寬比，寬或高為 0 時丟出 std::invalid_argument
    void SetViewport(float width, float height)
    {
        if (!(width > 0.0f) || !(height > 0.0f))
            throw std::invalid_argument("Camera::SetViewport: Viewport size must be greater than zero.");
        aspect = width / height;
    }

    [[nodiscard]] Matrix ViewMatrix() const
    {
        Matrix result;
        DirectX::XMStoreFloat4x4(&result.mat,
                                 DirectX::XMMatrixLookToLH(transform.position, transform.Forward(), transform.Up()));
        return result;
    }

    [[nodiscard]] Matrix ProjectionMatrix() const
    {
        Validate();
        Matrix result;
        if (orthographic)
            result.MakeOrthoProjectionMatrix(orthographicHeight * aspect, orthographicHeight, nearZ, farZ);
        else
            result.MakeProjectionMatrix(fovY, aspect, nearZ, farZ);
        return result;
    }

    [[nodiscard]] Matrix ViewProjectionMatrix() const
    {
        return ViewMatrix() * ProjectionMatrix();
    }

    [[nodiscard]] Frustum GetFrustum() const
    {
        return Frustum::FromViewProjection(ViewProjectionMatrix());
    }

    /// <summary>
    /// 螢幕座標（像素）轉世界空間射線，用於滑鼠拾取。
    /// </summary>
    [[nodiscard]] Ray ScreenPointToRay(float screenX, float screenY, float viewportWidth, float viewportHeight) const
    {
        return ::ScreenPointToRay(screenX, screenY, viewportWidth, viewportHeight, ViewMatrix(), ProjectionMatrix());
    }

    /// <summary>
    /// 世界座標轉螢幕座標。
    /// </summary>
    /// <returns>
    /// x、y 為像素，z 為 [0, 1] 的深度。點在攝影機後方時回傳 std::nullopt；
    /// 點在畫面外仍會回傳座標（x、y 超出視埠範圍），請自行判斷。
    /// </returns>
    [[nodiscard]] std::optional<Vector3> WorldToScreen(const Vector3& point, float viewportWidth,
                                                       float viewportHeight) const
    {
        Vector4 clip(point.x, point.y, point.z, 1.0f);
        ViewProjectionMatrix().Apply(clip);
        if (!(clip.w > 0.0f))
            return std::nullopt;

        const float ndcX = clip.x / clip.w;
        const float ndcY = clip.y / clip.w;
        const float ndcZ = clip.z / clip.w;
        return Vector3((ndcX * 0.5f + 0.5f) * viewportWidth, (0.5f - ndcY * 0.5f) * viewportHeight, ndcZ);
    }

    /// <summary>
    /// 驗證投影參數。
    /// </summary>
    void Validate() const
    {
        if (!(aspect > 0.0f))
            throw std::invalid_argument("Camera: aspect must be greater than zero.");
        if (!(nearZ > 0.0f) || !(farZ > nearZ))
            throw std::invalid_argument("Camera: Require 0 < nearZ < farZ.");
        if (orthographic)
        {
            if (!(orthographicHeight > 0.0f))
                throw std::invalid_argument("Camera: orthographicHeight must be greater than zero.");
        }
        else if (!(fovY > 0.0f) || !(fovY < DirectX::XM_PI))
        {
            throw std::invalid_argument("Camera: fovY must be between 0 and pi.");
        }
    }
};
} // namespace Skyline
