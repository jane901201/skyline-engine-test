#pragma once
#include "core/skyline_api.h"

#include <DirectXMath.h>

#include "math.h"

class Matrix;

/// <summary>
/// 二維向量類別。
/// </summary>
class SKYLINE_API Vector2
{
  public:
    static const Vector2 Zero;
    /// <summary>
    /// 建構函式。
    /// </summary>
    Vector2()
    {
        x = 0.0f;
        y = 0.0f;
    }
    /// <summary>
    /// 建構函式。
    /// </summary>
    /// <param name="x"></param>
    /// <param name="y"></param>
    Vector2(float x, float y)
    {
        this->x = x;
        this->y = y;
    }

    /// <summary>
    /// 指派運算子
    /// </summary>
    /// <param name="_v"></param>
    /// <returns></returns>
    Vector2& operator=(const Vector2& _v)
    {
        vec = _v.vec;
        return *this;
    }
    void Set(float _x, float _y)
    {
        vec.x = _x;
        vec.y = _y;
    }
    union
    {
        DirectX::XMFLOAT2 vec;
        struct
        {
            float x, y;
        };
        float v[2];
    };

    /// <summary>
    /// 線性插值。
    /// </summary>
    /// <param name="t">插值比例</param>
    /// <param name="v0">插值起始向量。</param>
    /// <param name="v1">插值結束向量。</param>
    void Lerp(float t, const Vector2& v0, const Vector2& v1)
    {
        x = v0.x + (v1.x - v0.x) * t;
        y = v0.y + (v1.y - v0.y) * t;
    }
    /// <summary>
    /// 正規化。
    /// </summary>
    void Normalize()
    {
        DirectX::XMVECTOR xmv = DirectX::XMLoadFloat2(&vec);
        xmv = DirectX::XMVector2Normalize(xmv);
        DirectX::XMStoreFloat2(&vec, xmv);
    }
};
static_assert(sizeof(Vector2) == sizeof(float) * 2, "Vector2 must remain tightly packed.");
/// <summary>
/// 三維向量。
/// </summary>
class SKYLINE_API Vector3
{
  public:
    union
    {
        DirectX::XMFLOAT3 vec;
        float v[3];
        struct
        {
            float x, y, z;
        };
    };
    static const Vector3 Zero;
    static const Vector3 Right;
    static const Vector3 Left;
    static const Vector3 Up;
    static const Vector3 Down;
    static const Vector3 Front;
    static const Vector3 Back;
    static const Vector3 AxisX;
    static const Vector3 AxisY;
    static const Vector3 AxisZ;
    static const Vector3 One;

  public:
    /// <summary>
    /// 隱式轉型為 XMVECTOR。
    /// </summary>
    /// <returns></returns>
    operator DirectX::XMVECTOR() const
    {
        return DirectX::XMLoadFloat3(&vec);
    }
    /// <summary>
    /// 指派運算子。
    /// </summary>
    /// <param name="_v"></param>
    /// <returns></returns>
    Vector3& operator=(const Vector3& _v)
    {
        vec = _v.vec;
        return *this;
    }
    /// <summary>
    /// 建構函式。
    /// </summary>
    /// <remarks>
    /// x、y、z 的所有元素皆初始化為 0。
    /// </remarks>
    Vector3()
    {
        x = y = z = 0.0f;
    }
    /// <summary>
    /// 建構函式。
    /// </summary>
    /// <param name="x"></param>
    /// <param name="y"></param>
    /// <param name="z"></param>
    Vector3(float x, float y, float z)
    {
        Set(x, y, z);
    }
    /// <summary>
    /// 線性插值
    /// </summary>
    /// <remarks>
    /// this = v0 + (v1-v0) * t;
    /// </remarks>
    /// <param name="t">插值比例。</param>
    /// <param name="v0">插值起始向量。</param>
    /// <param name="v1">插值結束向量。</param>
    void Lerp(float t, const Vector3& v0, const Vector3& v1)
    {
        DirectX::XMVECTOR _v = DirectX::XMVectorLerp(DirectX::XMLoadFloat3(&v0.vec), DirectX::XMLoadFloat3(&v1.vec), t);
        DirectX::XMStoreFloat3(&vec, _v);
    }
    /// <summary>
    /// 複製向量。
    /// </summary>
    template <class TVector> void CopyTo(TVector& dst) const
    {
        dst.x = x;
        dst.y = y;
        dst.z = z;
    }
    /// <summary>
    /// 設定向量。
    /// </summary>
    /// <param name="_x"></param>
    /// <param name="_y"></param>
    /// <param name="_z"></param>
    void Set(float _x, float _y, float _z)
    {
        vec.x = _x;
        vec.y = _y;
        vec.z = _z;
    }
    template <class TVector> void Set(TVector& _v)
    {
        Set(_v.x, _v.y, _v.z);
    }

    /// <summary>
    /// 向量加法。
    /// </summary>
    /// <remarks>
    /// this += _v;
    /// </remarks>
    /// <param name="_v">要相加的向量。</param>
    void Add(const Vector3& _v)
    {
        DirectX::XMVECTOR xmv0 = DirectX::XMLoadFloat3(&vec);
        DirectX::XMVECTOR xmv1 = DirectX::XMLoadFloat3(&_v.vec);
        DirectX::XMVECTOR xmvr = DirectX::XMVectorAdd(xmv0, xmv1);
        DirectX::XMStoreFloat3(&vec, xmvr);
    }
    /// <summary>
    /// 向量加法。
    /// </summary>
    /// <remarks>
    /// this = v0 + v1;
    /// </remarks>
    void Add(const Vector3& v0, const Vector3& v1)
    {
        DirectX::XMVECTOR xmv0 = DirectX::XMLoadFloat3(&v0.vec);
        DirectX::XMVECTOR xmv1 = DirectX::XMLoadFloat3(&v1.vec);
        DirectX::XMVECTOR xmvr = DirectX::XMVectorAdd(xmv0, xmv1);
        DirectX::XMStoreFloat3(&vec, xmvr);
    }
    /// <summary>
    /// 向量減法。
    /// </summary>
    /// <remarks>
    /// this -= _v;
    /// </remarks>
    void Subtract(const Vector3& _v)
    {
        DirectX::XMVECTOR xmv0 = DirectX::XMLoadFloat3(&vec);
        DirectX::XMVECTOR xmv1 = DirectX::XMLoadFloat3(&_v.vec);
        DirectX::XMVECTOR xmvr = DirectX::XMVectorSubtract(xmv0, xmv1);
        DirectX::XMStoreFloat3(&vec, xmvr);
    }
    /// <summary>
    /// 向量減法。
    /// </summary>
    /// <remarks>
    /// this = v0 - v1;
    /// </remarks>
    void Subtract(const Vector3& v0, const Vector3& v1)
    {
        DirectX::XMVECTOR xmv0 = DirectX::XMLoadFloat3(&v0.vec);
        DirectX::XMVECTOR xmv1 = DirectX::XMLoadFloat3(&v1.vec);
        DirectX::XMVECTOR xmvr = DirectX::XMVectorSubtract(xmv0, xmv1);
        DirectX::XMStoreFloat3(&vec, xmvr);
    }

    /// <summary>
    /// 計算內積。
    /// </summary>
    /// <remarks>
    /// float d = this->x * _v.x + this->y * _v.y + this->z * _v.z;
    /// return d;
    /// </remarks>
    float Dot(const Vector3& _v) const
    {
        DirectX::XMVECTOR xmv0 = DirectX::XMLoadFloat3(&vec);
        DirectX::XMVECTOR xmv1 = DirectX::XMLoadFloat3(&_v.vec);
        return DirectX::XMVector3Dot(xmv0, xmv1).m128_f32[0];
    }

    /// <summary>
    /// 外積。
    /// </summary>
    /// <remarks>
    /// Vector3 v ;
    /// v.x = this->y * _v.z - this->z * _v.y;
    /// v.y = thiz->z * _v.x - this->x * _v.z;
    /// v.z = this->x * _v.y - this->y * _v.x;
    /// this = v;
    /// </remarks>
    void Cross(const Vector3& _v)
    {
        DirectX::XMVECTOR xmv0 = DirectX::XMLoadFloat3(&vec);
        DirectX::XMVECTOR xmv1 = DirectX::XMLoadFloat3(&_v.vec);
        DirectX::XMVECTOR xmvr = DirectX::XMVector3Cross(xmv0, xmv1);
        DirectX::XMStoreFloat3(&vec, xmvr);
    }
    /// <summary>
    /// 外積。
    /// </summary>
    /// <remarks>
    /// this->x = v0.y * v1.z - v0.z * v1.y;
    /// this->y = v0.z * v1.x - v0.x * v1.z;
    /// this->z = v0.x * v1.y - v0.y * v1.x;
    /// </remarks>
    void Cross(const Vector3& v0, const Vector3& v1)
    {
        DirectX::XMVECTOR xmv0 = DirectX::XMLoadFloat3(&v0.vec);
        DirectX::XMVECTOR xmv1 = DirectX::XMLoadFloat3(&v1.vec);
        DirectX::XMVECTOR xmvr = DirectX::XMVector3Cross(xmv0, xmv1);
        DirectX::XMStoreFloat3(&vec, xmvr);
    }
    /// <summary>
    /// 取得向量長度
    /// </summary>
    float Length() const
    {
        DirectX::XMVECTOR xmv = DirectX::XMLoadFloat3(&vec);
        return DirectX::XMVector3Length(xmv).m128_f32[0];
    }
    /// <summary>
    /// 取得向量長度的平方。
    /// </summary>
    float LengthSq() const
    {
        DirectX::XMVECTOR xmv = DirectX::XMLoadFloat3(&vec);
        return DirectX::XMVector3LengthSq(xmv).m128_f32[0];
    }

    /// <summary>
    /// 以純量縮放向量。
    /// </summary>
    /// <remarks>
    /// 執行如下處理。
    /// this->x *= s;
    /// this->y *= s;
    /// this->z *= s;
    /// </remarks>
    void Scale(float s)
    {
        DirectX::XMVECTOR xmv = DirectX::XMLoadFloat3(&vec);
        xmv = DirectX::XMVectorScale(xmv, s);
        DirectX::XMStoreFloat3(&vec, xmv);
    }

    /// <summary>
    /// 將向量正規化。
    /// </summary>
    /// <remarks>
    ///		執行如下處理。
    ///		//計算向量的大小。
    ///		float len = this->x * this->x + this->y * this->y + this->z * this->z;
    ///		len = sqrt(len);
    ///		//將各元素除以向量的大小。
    ///		this->x /= len;
    ///		this->y /= len;
    ///		this->z /= len;
    /// </remarks>
    void Normalize()
    {
        DirectX::XMVECTOR xmv = DirectX::XMLoadFloat3(&vec);
        xmv = DirectX::XMVector3Normalize(xmv);
        DirectX::XMStoreFloat3(&vec, xmv);
    }
    /// <summary>
    /// 將向量除以純量。
    /// </summary>
    /// <remarks>
    ///		執行如下處理。
    ///		this->x /= d;
    ///		this->y /= d;
    ///		this->z /= d;
    /// </remarks>
    void Div(float d)
    {
        float scale = 1.0f / d;
        Scale(scale);
    }

    /// <summary>
    /// 設定最大值。
    /// </summary>
    /// <remarks>
    /// 執行如下處理。
    /// this->x = max( this->x, v.x );
    /// this->y = max( this->y, v.y );
    /// this->z = max( this->z, v.z );
    /// </remarks>
    void Max(const Vector3& v)
    {
        DirectX::XMVECTOR xmv0 = DirectX::XMLoadFloat3(&vec);
        DirectX::XMVECTOR xmv1 = DirectX::XMLoadFloat3(&v.vec);
        DirectX::XMStoreFloat3(&vec, DirectX::XMVectorMax(xmv0, xmv1));
    }

    /// <summary>
    /// 設定最小值。
    /// </summary>
    /// <remarks>
    /// 執行如下處理。
    /// this->x = min( this->x, v.x );
    /// this->y = min( this->y, v.y );
    /// this->z = min( this->z, v.z );
    /// </remarks>
    void Min(const Vector3& v)
    {
        DirectX::XMVECTOR xmv0 = DirectX::XMLoadFloat3(&vec);
        DirectX::XMVECTOR xmv1 = DirectX::XMLoadFloat3(&v.vec);
        DirectX::XMStoreFloat3(&vec, DirectX::XMVectorMin(xmv0, xmv1));
    }

    /// <summary>
    /// 加法指派運算子。
    /// </summary>
    /// <remarks>
    ///	為向量類別提供如下加法指派功能。
    ///	Vector3 v0 = {20.0f, 30.0f, 40.0f};
    ///	Vector3 v1 = {10.0f, 20.0f, 30.0f};
    ///	v0 += v1;
    /// </remarks>
    const Vector3& operator+=(const Vector3& _v)
    {
        Add(_v);
        return *this;
    }
    /// <summary>
    /// 乘法指派運算子。
    /// </summary>
    /// <remarks>
    ///	為向量類別提供如下乘法指派功能。
    /// Vector3 v = {20.0f, 30.0f, 40.0f};
    ///	v *= 10;
    /// </remarks>
    const Vector3& operator*=(float s)
    {
        Scale(s);
        return *this;
    }
    /// <summary>
    /// 減法指派運算子。
    /// </summary>
    /// <remarks>
    /// 為向量類別提供如下減法指派功能。
    /// Vector3 v0 = {20.0f, 30.0f, 40.0f};
    ///	Vector3 v1 = {10.0f, 20.0f, 30.0f};
    ///	v0 -= v1;
    /// </remarks>
    const Vector3& operator-=(const Vector3& _v)
    {
        Subtract(_v);
        return *this;
    }
    /// <summary>
    /// 除法指派運算子。
    /// </summary>
    /// <remarks>
    /// 為向量類別提供如下除法指派功能。
    /// Vector3 v = {20.0f, 30.0f, 40.0f};
    ///	v /= 10;
    /// </remarks>
    const Vector3& operator/=(const float s)
    {
        Div(s);
        return *this;
    }
};
static_assert(sizeof(Vector3) == sizeof(float) * 3, "Vector3 must remain tightly packed.");
/// <summary>
/// 四維向量類別。
/// </summary>
class SKYLINE_API Vector4
{
  public:
    union
    {
        DirectX::XMFLOAT4 vec;
        struct
        {
            float x, y, z, w;
        };
        struct
        {
            float r, g, b, a;
        };
        float v[4];
    };
    static const Vector4 White;
    static const Vector4 Black;
    static const Vector4 Yellow;
    static const Vector4 Gray;

  public:
    /// <summary>
    /// 隱式轉型為 XMVECTOR 型別。
    /// </summary>
    /// <returns></returns>
    operator DirectX::XMVECTOR() const
    {
        return DirectX::XMLoadFloat4(&vec);
    }
    /// <summary>
    /// 建構函式。
    /// </summary>
    /// <remarks>
    /// x、y、z 初始化為 0.0，w 初始化為 1.0。
    /// </remarks>
    Vector4()
    {
        x = y = z = 0.0f;
        w = 1.0f;
    }
    /// <summary>
    /// 複製向量。
    /// </summary>
    template <class TVector4> void CopyTo(TVector4& dst) const
    {
        dst.x = x;
        dst.y = y;
        dst.z = z;
        dst.w = w;
    }

    /// <summary>
    /// 指派運算子。
    /// </summary>
    /// <remarks>
    /// 為向量類別提供如下指派運算功能。
    /// Vector4 v0 = {10.0f, 20.0f, 30.0f, 10.0f};
    /// Vector4 v1;
    /// v1 = v0;
    /// </remarks>
    Vector4& operator=(const Vector4& _v)
    {
        vec = _v.vec;
        return *this;
    }

    /// <summary>
    /// 建構函式。
    /// </summary>
    Vector4(float x, float y, float z, float w)
    {
        Set(x, y, z, w);
    }
    /// <summary>
    /// 以三維向量資料作為引數的建構函式
    /// </summary>
    /// <remarks>
    /// w 會儲存 1.0。
    /// </remarks>
    Vector4(const Vector3& v)
    {
        Set(v);
    }
    /// <summary>
    /// 設定向量的各個元素。
    /// </summary>
    void Set(float _x, float _y, float _z, float _w)
    {
        this->x = _x;
        this->y = _y;
        this->z = _z;
        this->w = _w;
    }
    /// <summary>
    /// 將向量正規化。
    /// </summary>
    void Normalize()
    {
        DirectX::XMVECTOR xmv = DirectX::XMLoadFloat4(&vec);
        xmv = DirectX::XMVector4Normalize(xmv);
        DirectX::XMStoreFloat4(&vec, xmv);
    }
    /// <summary>
    /// 設定向量。
    /// </summary>
    /// <param name="_v"></param>
    void Set(const Vector4& _v)
    {
        *this = _v;
    }
    /// <summary>
    /// 根據三維向量資料設定向量。
    /// </summary>
    /// <remarks>
    /// w 會設定為 1.0。
    /// </remarks>
    void Set(const Vector3& _v)
    {
        this->x = _v.x;
        this->y = _v.y;
        this->z = _v.z;
        this->w = 1.0f;
    }
    /// <summary>
    /// 向量相加。
    /// </summary>
    /// <remarks>
    /// this += _v;
    /// </remarks>
    void Add(const Vector4& _v)
    {
        DirectX::XMVECTOR xmv0 = DirectX::XMLoadFloat4(&vec);
        DirectX::XMVECTOR xmv1 = DirectX::XMLoadFloat4(&_v.vec);
        DirectX::XMVECTOR xmvr = DirectX::XMVectorAdd(xmv0, xmv1);
        DirectX::XMStoreFloat4(&vec, xmvr);
    }
    /// <summary>
    /// 向量加法。
    /// </summary>
    /// <remarks>
    /// this = v0 + v1;
    /// </remarks>
    void Add(const Vector4& v0, const Vector4& v1)
    {
        DirectX::XMVECTOR xmv0 = DirectX::XMLoadFloat4(&v0.vec);
        DirectX::XMVECTOR xmv1 = DirectX::XMLoadFloat4(&v1.vec);
        DirectX::XMVECTOR xmvr = DirectX::XMVectorAdd(xmv0, xmv1);
        DirectX::XMStoreFloat4(&vec, xmvr);
    }
    /// <summary>
    /// 向量減法。
    /// </summary>
    /// <remarks>
    /// this -= _v;
    /// </remarks>
    void Subtract(const Vector4& _v)
    {
        DirectX::XMVECTOR xmv0 = DirectX::XMLoadFloat4(&vec);
        DirectX::XMVECTOR xmv1 = DirectX::XMLoadFloat4(&_v.vec);
        DirectX::XMVECTOR xmvr = DirectX::XMVectorSubtract(xmv0, xmv1);
        DirectX::XMStoreFloat4(&vec, xmvr);
    }
    /// <summary>
    /// 向量減法。
    /// </summary>
    /// <remarks>
    /// this = v0 - v1;
    /// </remarks>
    void Subtract(const Vector4& v0, const Vector4& v1)
    {
        DirectX::XMVECTOR xmv0 = DirectX::XMLoadFloat4(&v0.vec);
        DirectX::XMVECTOR xmv1 = DirectX::XMLoadFloat4(&v1.vec);
        DirectX::XMVECTOR xmvr = DirectX::XMVectorSubtract(xmv0, xmv1);
        DirectX::XMStoreFloat4(&vec, xmvr);
    }

    /// <summary>
    /// 計算內積。
    /// </summary>
    /// <remarks>
    /// float d = this->x * _v.x + this->y * _v.y + this->z * _v.z + this->w * _v.w;
    /// return d;
    /// </remarks>
    float Dot(const Vector4& _v) const
    {
        DirectX::XMVECTOR xmv0 = DirectX::XMLoadFloat4(&vec);
        DirectX::XMVECTOR xmv1 = DirectX::XMLoadFloat4(&_v.vec);
        return DirectX::XMVector4Dot(xmv0, xmv1).m128_f32[0];
    }
    /// <summary>
    /// 取得向量長度
    /// </summary>
    float Length()
    {
        DirectX::XMVECTOR xmv = DirectX::XMLoadFloat4(&vec);
        return DirectX::XMVector4Length(xmv).m128_f32[0];
    }
    /// <summary>
    /// 取得向量長度的平方。
    /// </summary>
    float LengthSq()
    {
        DirectX::XMVECTOR xmv = DirectX::XMLoadFloat4(&vec);
        return DirectX::XMVector4LengthSq(xmv).m128_f32[0];
    }
    /// <summary>
    /// 以純量縮放向量。
    /// </summary>
    /// <remarks>
    /// 執行如下處理。
    /// this->x *= s;
    /// this->y *= s;
    /// this->z *= s;
    /// this->w *= s;
    /// </remarks>
    void Scale(float s)
    {
        DirectX::XMVECTOR xmv = DirectX::XMLoadFloat4(&vec);
        xmv = DirectX::XMVectorScale(xmv, s);
        DirectX::XMStoreFloat4(&vec, xmv);
    }
};
static_assert(sizeof(Vector4) == sizeof(float) * 4, "Vector4 must remain tightly packed.");

const Vector2 g_vec2Zero = {0.0f, 0.0f};
const Vector3 g_vec3Zero = {0.0f, 0.0f, 0.0f};
const Vector3 g_vec3Right = {1.0f, 0.0f, 0.0f};
const Vector3 g_vec3Left = {-1.0f, 0.0f, 0.0f};
const Vector3 g_vec3Up = {0.0f, 1.0f, 0.0f};
const Vector3 g_vec3Down = {0.0f, -1.0f, 0.0f};
const Vector3 g_vec3Front = {0.0f, 0.0f, 1.0f};
const Vector3 g_vec3Back = {0.0f, 0.0f, -1.0f};
const Vector3 g_vec3AxisX = {1.0f, 0.0f, 0.0f};
const Vector3 g_vec3AxisY = {0.0f, 1.0f, 0.0f};
const Vector3 g_vec3AxisZ = {0.0f, 0.0f, 1.0f};
const Vector3 g_vec3One = {1.0f, 1.0f, 1.0f};

const Vector4 g_vec4White = {1.0f, 1.0f, 1.0f, 1.0f};
const Vector4 g_vec4Black = {0.0f, 0.0f, 0.0f, 1.0f};
const Vector4 g_vec4Gray = {0.5f, 0.5f, 0.5f, 1.0f};
const Vector4 g_vec4Yellow = {1.0f, 1.0f, 0.0f, 1.0f};

/// <summary>
/// 四元數類別。
/// </summary>
class SKYLINE_API Quaternion : public Vector4
{
  public:
    static const Quaternion Identity; //!< 單位四元數。
    Quaternion()
    {
        x = y = z = 0.0f;
        w = 1.0f;
    }
    /// <summary>
    /// 建構函式。
    /// </summary>
    Quaternion(float x, float y, float z, float w) : Vector4(x, y, z, w) {}
    /// <summary>
    /// 建立繞 X 軸旋轉的四元數。
    /// </summary>
    /// <param name="axis"></param>
    /// <param name="angle"></param>
    void SetRotationX(float angle)
    {
        SetRotation(g_vec3AxisX, angle);
    }
    /// <summary>
    /// 建立繞 X 軸旋轉的四元數。
    /// </summary>
    /// <param name="angle">旋轉角度，單位為 Degree</param>
    void SetRotationDegX(float angle)
    {
        SetRotationDeg(g_vec3AxisX, angle);
    }
    /// <summary>
    /// 建立繞 Y 軸旋轉的四元數。
    /// </summary>
    /// <param name="axis"></param>
    /// <param name="angle"></param>
    void SetRotationY(float angle)
    {
        SetRotation(g_vec3AxisY, angle);
    }
    /// <summary>
    /// 建立繞 Y 軸旋轉的四元數。
    /// </summary>
    /// <param name="angle">旋轉角度，單位為 Degree</param>
    void SetRotationDegY(float angle)
    {
        SetRotationDeg(g_vec3AxisY, angle);
    }

    /// <summary>
    /// 建立繞 Z 軸旋轉的四元數。
    /// </summary>
    /// <param name="axis"></param>
    /// <param name="angle"></param>
    void SetRotationZ(float angle)
    {
        SetRotation(g_vec3AxisZ, angle);
    }
    /// <summary>
    /// 建立繞 Z 軸旋轉的四元數。
    /// </summary>
    /// <param name="angle">旋轉角度，單位為 Degree</param>
    void SetRotationDegZ(float angle)
    {
        SetRotationDeg(g_vec3AxisZ, angle);
    }

    /// <summary>
    /// 建立繞任意軸旋轉的四元數。
    /// </summary>
    /// <param name="axis">旋轉軸</param>
    /// <param name="angle">旋轉角度，單位為弧度。</param>
    void SetRotation(const Vector3& axis, float angle)
    {
        float s;
        float halfAngle = angle * 0.5f;
        s = sinf(halfAngle);
        w = cosf(halfAngle);
        x = axis.x * s;
        y = axis.y * s;
        z = axis.z * s;
    }
    /// <summary>
    /// 建立繞任意軸旋轉的四元數。
    /// </summary>
    /// <param name="axis">旋轉軸</param>
    /// <param name="angle">旋轉角度，單位為 Degree</param>
    void SetRotationDeg(const Vector3& axis, float angle)
    {
        float s;
        float halfAngle = Math::DegToRad(angle) * 0.5f;
        s = sinf(halfAngle);
        w = cosf(halfAngle);
        x = axis.x * s;
        y = axis.y * s;
        z = axis.z * s;
    }
    /// <summary>
    /// 從矩陣建立四元數。
    /// </summary>
    /// <param name="m">矩陣</param>
    void SetRotation(const Matrix& m);
    /// <summary>
    /// 建立將 from 向量旋轉至 to 向量的四元數。
    /// </summary>
    /// <param name="from">旋轉前的向量</param>
    /// <param name="to">旋轉後的向量</param>
    void SetRotation(Vector3 from, Vector3 to);
    /// <summary>
    /// 球面線性插值
    /// </summary>
    /// <param name="t">插值比例</param>
    /// <param name="q1">起始四元數。</param>
    /// <param name="q2">結束四元數。</param>
    void Slerp(float t, Quaternion q1, Quaternion q2)
    {
        DirectX::XMVECTOR xmv =
            DirectX::XMQuaternionSlerp(DirectX::XMLoadFloat4(&q1.vec), DirectX::XMLoadFloat4(&q2.vec), t);
        DirectX::XMStoreFloat4(&vec, xmv);
    }
    /// <summary>
    /// 增加繞 Y 軸的旋轉。
    /// </summary>
    /// <returns>要增加的旋轉角度，單位為弧度。</returns>
    void AddRotationY(float angle)
    {
        Quaternion addRot;
        addRot.SetRotation(Vector3::AxisY, angle);
        *this *= addRot;
    }
    /// <summary>
    /// 四元數相乘
    /// </summary>
    /// <param name="rot"></param>
    void Multiply(const Quaternion& rot)
    {
        float pw, px, py, pz;
        float qw, qx, qy, qz;

        qw = w;
        qx = x;
        qy = y;
        qz = z;
        pw = rot.w;
        px = rot.x;
        py = rot.y;
        pz = rot.z;

        w = pw * qw - px * qx - py * qy - pz * qz;
        x = pw * qx + px * qw + py * qz - pz * qy;
        y = pw * qy - px * qz + py * qw + pz * qx;
        z = pw * qz + px * qy - py * qx + pz * qw;
    }
    /// <summary>
    /// 四元數相乘。
    /// </summary>
    /// <param name="rot0"></param>
    /// <param name="rot1"></param>
    void Multiply(const Quaternion& rot0, const Quaternion& rot1)
    {
        float pw, px, py, pz;
        float qw, qx, qy, qz;

        qw = rot0.w;
        qx = rot0.x;
        qy = rot0.y;
        qz = rot0.z;
        pw = rot1.w;
        px = rot1.x;
        py = rot1.y;
        pz = rot1.z;

        w = pw * qw - px * qx - py * qy - pz * qz;
        x = pw * qx + px * qw + py * qz - pz * qy;
        y = pw * qy - px * qz + py * qw + pz * qx;
        z = pw * qz + px * qy - py * qx + pz * qw;
    }
    /// <summary>
    /// 四元數的乘法指派運算子
    /// </summary>
    const Quaternion& operator*=(const Quaternion& rot0)
    {
        Multiply(rot0, *this);
        return *this;
    }
    /// <summary>
    /// 將四元數套用至向量。
    /// </summary>
    void Apply(Vector4& _v) const
    {
        DirectX::XMVECTOR xmv = DirectX::XMVector3Rotate(_v, *this);
        DirectX::XMStoreFloat4(&_v.vec, xmv);
    }
    /// <summary>
    /// 將四元數套用至向量。
    /// </summary>
    void Apply(Vector3& _v) const
    {
        DirectX::XMVECTOR xmv = DirectX::XMVector3Rotate(_v, *this);
        DirectX::XMStoreFloat3(&_v.vec, xmv);
    }
};

// 整數型向量類別。
__declspec(align(16)) class Vector4i
{
  public:
    union
    {
        struct
        {
            int x, y, z, w;
        };
        int v[4];
    };
};
/// <summary>
/// 向量相加。
/// </summary>
static inline Vector3 operator+(const Vector3& v0, const Vector3& v1)
{
    Vector3 result;
    result.Add(v0, v1);
    return result;
}
/// <summary>
/// 向量與純量相乘。
/// </summary>

static inline Vector3 operator*(const Vector3& v, float s)
{
    Vector3 result;
    result = v;
    result.Scale(s);
    return result;
}
/// <summary>
/// 向量除以純量。
/// </summary>
static inline Vector3 operator/(const Vector3& v, float s)
{
    Vector3 result;
    result = v;
    result.Div(s);
    return result;
}
/// <summary>
/// 向量相減。
/// </summary>
static inline Vector3 operator-(const Vector3& v0, const Vector3& v1)
{
    Vector3 result;
    result.Subtract(v0, v1);
    return result;
}

/// <summary>
/// 計算外積。
/// </summary>
static inline Vector3 Cross(const Vector3& v0, const Vector3& v1)
{
    Vector3 result;
    result.Cross(v0, v1);
    return result;
}
/// <summary>
/// 計算內積。
/// </summary>
static inline float Dot(const Vector3& v0, const Vector3& v1)
{
    return v0.Dot(v1);
}

/// <summary>
/// 向量相加。
/// </summary>

static inline Vector4 operator+(const Vector4& v0, const Vector4& v1)
{
    Vector4 result;
    result.Add(v0, v1);
    return result;
}
/// <summary>
/// 向量與純量相乘。
/// </summary>

static inline Vector4 operator*(const Vector4& v, float s)
{
    Vector4 result;
    result = v;
    result.Scale(s);
    return result;
}

/// <summary>
/// 向量相減。
/// </summary>
static inline Vector4 operator-(const Vector4& v0, const Vector4& v1)
{
    Vector4 result;
    result.Subtract(v0, v1);
    return result;
}

/// <summary>
/// 計算內積。
/// </summary>
static inline float Dot(const Vector4& v0, const Vector4& v1)
{
    return v0.Dot(v1);
}

/// <summary>
/// 四元數相乘。
/// </summary>
static inline Quaternion operator*(const Quaternion& q1, const Quaternion q2)
{
    Quaternion qRet;
    qRet.Multiply(q2, q1);
    return qRet;
}

const Quaternion g_quatIdentity = {0.0f, 0.0f, 0.0f, 1.0f};
