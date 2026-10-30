
#pragma once

#include "vector.h"

/// <summary>
/// 矩陣類別。
/// </summary>
class Matrix
{
  public:
    union
    {
        DirectX::XMFLOAT4X4 mat;
        struct
        {
            float _11, _12, _13, _14;
            float _21, _22, _23, _24;
            float _31, _32, _33, _34;
            float _41, _42, _43, _44;
        };
        Vector4 v[4];
        float m[4][4];
    };
    // 單位矩陣
    static const Matrix Identity;

  public:
    /// <summary>
    /// 隱式轉型為 DirectX::XMMATRIX 型別。
    /// </summary>
    /// <returns></returns>
    operator DirectX::XMMATRIX() const
    {
        return DirectX::XMLoadFloat4x4(&mat);
    }
    /// <summary>
    /// 建構函式。
    /// </summary>
    /// <remarks>
    /// 初始化為單位矩陣。
    /// </remarks>
    Matrix()
    {
        mat._11 = 1.0f;
        mat._12 = 0.0f;
        mat._13 = 0.0f;
        mat._14 = 0.0f;

        mat._21 = 0.0f;
        mat._22 = 1.0f;
        mat._23 = 0.0f;
        mat._24 = 0.0f;

        mat._31 = 0.0f;
        mat._32 = 0.0f;
        mat._33 = 1.0f;
        mat._34 = 0.0f;

        mat._41 = 0.0f;
        mat._42 = 0.0f;
        mat._43 = 0.0f;
        mat._44 = 1.0f;
    }
    Matrix(float m00, float m01, float m02, float m03, float m10, float m11, float m12, float m13, float m20, float m21,
           float m22, float m23, float m30, float m31, float m32, float m33)
        : mat(m00, m01, m02, m03, m10, m11, m12, m13, m20, m21, m22, m23, m30, m31, m32, m33)
    {
    }
    /// <summary>
    /// 建構函式
    /// </summary>
    /// <param name="m"></param>
    Matrix(const DirectX::XMFLOAT4X4& m)
    {
        mat = m;
    }
    /// <summary>
    /// 指派運算子。
    /// </summary>
    /// <param name="_m"></param>
    /// <returns></returns>
    Matrix& operator=(const Matrix& _m)
    {
        mat = _m.mat;
        return *this;
    }
    /// <summary>
    /// 將向量與 3x3 矩陣相乘。
    /// </summary>
    /// <param name="vOut">要進行乘法運算的向量。</param>
    void Apply3x3(Vector3& vOut) const
    {
        Vector3 vTmp = vOut;
        vOut.x = vTmp.x * m[0][0] + vTmp.y * m[1][0] + vTmp.z * m[2][0];
        vOut.y = vTmp.x * m[0][1] + vTmp.y * m[1][1] + vTmp.z * m[2][1];
        vOut.z = vTmp.x * m[0][2] + vTmp.y * m[1][2] + vTmp.z * m[2][2];
    }

    /// <summary>
    /// 將向量乘上矩陣。
    /// </summary>
    /// <param name="vOut">要進行乘法運算的向量。</param>
    void Apply(Vector3& vOut) const
    {
        DirectX::XMStoreFloat3(&vOut.vec, DirectX::XMVector3Transform(vOut, *this));
    }
    /// <summary>
    /// 將向量乘上矩陣。
    /// </summary>
    /// <param name="vOut">要進行乘法運算的向量。</param>
    void Apply(Vector4& vOut) const
    {
        DirectX::XMStoreFloat4(&vOut.vec, DirectX::XMVector4Transform(vOut, *this));
    }
    /// <summary>
    /// 建立平移矩陣。
    /// </summary>
    /// <param name="trans">平移量。</param>
    void MakeTranslation(const Vector3& trans)
    {
        DirectX::XMStoreFloat4x4(&mat, DirectX::XMMatrixTranslationFromVector(trans));
    }
    void MakeTranslation(float x, float y, float z)
    {
        MakeTranslation(Vector3(x, y, z));
    }
    /// <summary>
    /// 建立繞 Y 軸旋轉的矩陣。
    /// </summary>
    /// <param name="angle">旋轉角度（單位：弧度）</param>
    void MakeRotationY(float angle)
    {
        DirectX::XMStoreFloat4x4(&mat, DirectX::XMMatrixRotationY(angle));
    }
    /// <summary>
    /// 建立繞 Z 軸旋轉的矩陣。
    /// </summary>
    /// <param name="angle">旋轉角度（單位：弧度）</param>
    void MakeRotationZ(float angle)
    {
        DirectX::XMStoreFloat4x4(&mat, DirectX::XMMatrixRotationZ(angle));
    }
    /// <summary>
    /// 建立繞 X 軸旋轉的矩陣。
    /// </summary>
    /// <param name="angle">旋轉角度（單位：弧度）</param>
    void MakeRotationX(float angle)
    {
        DirectX::XMStoreFloat4x4(&mat, DirectX::XMMatrixRotationX(angle));
    }
    /// <summary>
    /// 從四元數建立旋轉矩陣。
    /// </summary>
    /// <param name="q">四元數。</param>
    void MakeRotationFromQuaternion(const Quaternion& q)
    {
        DirectX::XMStoreFloat4x4(&mat, DirectX::XMMatrixRotationQuaternion(q));
    }

    /// <summary>
    /// 建立繞任意軸旋轉的矩陣。
    /// </summary>
    /// <param name="axis">旋轉軸。</param>
    /// <param name="angle">旋轉角度。</param>
    void MakeRotationAxis(const Vector3& axis, float angle)
    {
        DirectX::XMStoreFloat4x4(&mat, DirectX::XMMatrixRotationAxis(axis, angle));
    }
    /// <summary>
    /// 建立縮放矩陣。
    /// </summary>
    /// <param name="scale">縮放比例。</param>
    void MakeScaling(const Vector3& scale)
    {
        DirectX::XMStoreFloat4x4(&mat, DirectX::XMMatrixScalingFromVector(scale));
    }
    /// <summary>
    /// 建立透視投影矩陣。
    /// </summary>
    /// <param name="viewAngle">視角（單位：弧度）</param>
    /// <param name="aspect">長寬比</param>
    /// <param name="fNear">近裁切平面。</param>
    /// <param name="fFar">遠裁切平面。</param>
    void MakeProjectionMatrix(float viewAngle, float aspect, float fNear, float fFar)
    {
        DirectX::XMStoreFloat4x4(&mat, DirectX::XMMatrixPerspectiveFovLH(viewAngle, aspect, fNear, fFar));
    }
    /// <summary>
    /// 建立正交投影矩陣。
    /// </summary>
    /// <param name="w">視錐體的寬度。</param>
    /// <param name="h">視錐體的高度。</param>
    /// <param name="fNear">近裁切平面。</param>
    /// <param name="fFar">遠裁切平面。</param>
    void MakeOrthoProjectionMatrix(float w, float h, float fNear, float fFar)
    {
        DirectX::XMStoreFloat4x4(&mat, DirectX::XMMatrixOrthographicLH(w, h, fNear, fFar));
    }
    /// <summary>
    /// 建立攝影機矩陣。
    /// </summary>
    /// <param name="position">視點</param>
    /// <param name="target">注視點</param>
    /// <param name="up">攝影機的上方向</param>
    void MakeLookAt(const Vector3& position, const Vector3& target, const Vector3& up)
    {
        DirectX::XMStoreFloat4x4(&mat, DirectX::XMMatrixLookAtLH(position, target, up));
    }
    /// <summary>
    /// 矩陣相乘
    /// </summary>
    /// <remarks>
    /// *this = m0 * m1;
    /// </remarks>
    void Multiply(const Matrix& m0, const Matrix& m1)
    {
        DirectX::XMFLOAT4X4 lm;
        DirectX::XMStoreFloat4x4(&lm, DirectX::XMMatrixMultiply(m0, m1));
        mat = lm;
    }

    /// <summary>
    /// 矩陣的指派運算子
    /// </summary>
    /// <remarks>
    /// *this *= _m;
    /// return *this;
    /// </remarks>
    const Matrix& operator*=(const Matrix& _m)
    {
        Multiply(*this, _m);
        return *this;
    }
    /// <summary>
    /// 計算反矩陣。
    /// </summary>
    /// <param name="_m">原始矩陣。</param>
    void Inverse(const Matrix& _m)
    {
        DirectX::XMStoreFloat4x4(&mat, DirectX::XMMatrixInverse(NULL, _m));
    }
    /// <summary>
    /// 計算自身的反矩陣。
    /// </summary>
    void Inverse()
    {
        Inverse(*this);
    }
    /// <summary>
    /// 計算轉置矩陣。
    /// </summary>
    void Transpose()
    {
        DirectX::XMStoreFloat4x4(&mat, DirectX::XMMatrixTranspose(*this));
    }
};
static_assert(sizeof(Matrix) == sizeof(float) * 16, "Matrix must remain tightly packed.");
/// <summary>
/// 矩陣相乘
/// </summary>
/// <remarks>
/// 乘法運算依照由左至右的順序進行。
/// </remarks>
static inline Matrix operator*(const Matrix& m1, const Matrix m2)
{
    Matrix mRet;
    mRet.Multiply(m1, m2);
    return mRet;
}
/// <summary>
/// 單位矩陣
/// </summary>
const Matrix g_matIdentity = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                              0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

inline const Matrix Matrix::Identity = g_matIdentity;
