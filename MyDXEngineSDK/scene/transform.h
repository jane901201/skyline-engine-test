#pragma once

#include <DirectXMath.h>

#include <cmath>
#include <stdexcept>

#include "math/matrix.h"
#include "math/vector.h"

namespace Skyline
{
/// <summary>
/// 位置、旋轉、縮放。
/// </summary>
/// <remarks>
/// 採用 DirectXMath 的 row-vector 慣例與 D3D 的左手座標（Y 向上、Z 向前）。
/// ToMatrix 的順序是「先縮放、再旋轉、最後平移」，即 S * R * T。
/// </remarks>
struct Transform
{
    Vector3 position{0.0f, 0.0f, 0.0f};
    Quaternion rotation{0.0f, 0.0f, 0.0f, 1.0f};
    Vector3 scale{1.0f, 1.0f, 1.0f};

    Transform() = default;
    Transform(const Vector3& position, const Quaternion& rotation = Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
              const Vector3& scale = Vector3(1.0f, 1.0f, 1.0f))
        : position(position), rotation(rotation), scale(scale)
    {
    }

    /// <summary>
    /// 轉成 4x4 矩陣（S * R * T）。
    /// </summary>
    [[nodiscard]] Matrix ToMatrix() const
    {
        Matrix result;
        DirectX::XMStoreFloat4x4(&result.mat, DirectX::XMMatrixAffineTransformation(scale, DirectX::XMVectorZero(),
                                                                                  rotation, position));
        return result;
    }

    /// <summary>
    /// 從矩陣分解出位置、旋轉、縮放。
    /// </summary>
    /// <remarks>
    /// 矩陣含有投影成分等無法分解的內容時，丟出 std::invalid_argument。
    /// 零縮放的矩陣可以分解（縮放分量為 0），但無法還原出正確的旋轉。
    /// </remarks>
    [[nodiscard]] static Transform FromMatrix(const Matrix& matrix)
    {
        // 仿射矩陣的最後一欄必須是 (0, 0, 0, 1)，否則含有投影成分，不是 Transform
        constexpr float epsilon = 1e-5f;
        if (std::fabs(matrix._14) > epsilon || std::fabs(matrix._24) > epsilon || std::fabs(matrix._34) > epsilon ||
            std::fabs(matrix._44 - 1.0f) > epsilon)
            throw std::invalid_argument("Transform::FromMatrix: The matrix is not an affine transform.");

        DirectX::XMVECTOR scale;
        DirectX::XMVECTOR rotation;
        DirectX::XMVECTOR translation;
        if (!DirectX::XMMatrixDecompose(&scale, &rotation, &translation, matrix))
            throw std::invalid_argument("Transform::FromMatrix: The matrix cannot be decomposed.");

        Transform result;
        DirectX::XMStoreFloat3(&result.scale.vec, scale);
        DirectX::XMStoreFloat4(&result.rotation.vec, rotation);
        DirectX::XMStoreFloat3(&result.position.vec, translation);
        return result;
    }

    /// <summary>
    /// 建立從 position 看向 target 的 Transform（縮放為 1）。
    /// </summary>
    /// <remarks>
    /// position 與 target 重合，或 up 與視線方向平行時，丟出 std::invalid_argument。
    /// </remarks>
    [[nodiscard]] static Transform LookAt(const Vector3& position, const Vector3& target,
                                          const Vector3& up = Vector3(0.0f, 1.0f, 0.0f))
    {
        using namespace DirectX;
        const XMVECTOR forward = XMVectorSubtract(target, position);
        if (XMVector3NearEqual(XMVector3LengthSq(forward), XMVectorZero(), XMVectorReplicate(1e-12f)))
            throw std::invalid_argument("Transform::LookAt: position and target must be different.");

        const XMVECTOR z = XMVector3Normalize(forward);
        // 左手座標：right = up x forward（與 XMMatrixLookAtLH 相同）
        const XMVECTOR rightRaw = XMVector3Cross(up, z);
        if (XMVector3NearEqual(XMVector3LengthSq(rightRaw), XMVectorZero(), XMVectorReplicate(1e-12f)))
            throw std::invalid_argument("Transform::LookAt: up must not be parallel to the view direction.");

        const XMVECTOR x = XMVector3Normalize(rightRaw);
        const XMVECTOR y = XMVector3Cross(z, x);

        XMMATRIX basis = XMMatrixIdentity();
        basis.r[0] = x;
        basis.r[1] = y;
        basis.r[2] = z;

        Transform result;
        result.position = position;
        XMStoreFloat4(&result.rotation.vec, XMQuaternionRotationMatrix(basis));
        return result;
    }

    /// 本地 +Z 軸在世界中的方向（單位向量）
    [[nodiscard]] Vector3 Forward() const
    {
        return RotateAxis(0.0f, 0.0f, 1.0f);
    }
    /// 本地 +X 軸在世界中的方向（單位向量）
    [[nodiscard]] Vector3 Right() const
    {
        return RotateAxis(1.0f, 0.0f, 0.0f);
    }
    /// 本地 +Y 軸在世界中的方向（單位向量）
    [[nodiscard]] Vector3 Up() const
    {
        return RotateAxis(0.0f, 1.0f, 0.0f);
    }

    /// <summary>
    /// 將本地座標的點轉成這個 Transform 的父空間座標。
    /// </summary>
    [[nodiscard]] Vector3 TransformPoint(const Vector3& point) const
    {
        Vector3 result = point;
        ToMatrix().Apply(result);
        return result;
    }

  private:
    [[nodiscard]] Vector3 RotateAxis(float x, float y, float z) const
    {
        Vector3 result;
        DirectX::XMStoreFloat3(&result.vec, DirectX::XMVector3Rotate(DirectX::XMVectorSet(x, y, z, 0.0f), rotation));
        return result;
    }
};
} // namespace Skyline
