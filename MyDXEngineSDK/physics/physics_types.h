#pragma once

#include "math/vector.h"

#include <cstdint>

namespace Skyline::Physics
{
/// <summary>
/// 物理世界中的剛體識別碼。
/// </summary>
/// <remarks>
/// 銷毀剛體後，舊的識別碼會失效；對失效的識別碼操作會丟出 std::invalid_argument。
/// </remarks>
using BodyId = std::uint32_t;

/// <summary>
/// 不指向任何剛體的識別碼。
/// </summary>
constexpr BodyId invalidBody = 0xFFFFFFFFu;

/// <summary>
/// 射線查詢使用的最大距離（世界單位），超過的 tMax 會被截斷。
/// </summary>
/// <remarks>
/// 物理引擎的射線以「方向向量的長度」表示射程，因此射程必須是有限值，不能使用 FLT_MAX。
/// </remarks>
constexpr float maxRayDistance = 100000.0f;

/// <summary>
/// 剛體的運動方式。
/// </summary>
enum class BodyType
{
    /// 不會移動（地板、牆壁）
    Static,
    /// 由物理模擬推動（受重力與碰撞影響）
    Dynamic,
    /// 由程式設定速度移動，不受力影響，但會推動動態剛體
    Kinematic,
};

/// <summary>
/// 碰撞形狀種類。
/// </summary>
enum class ShapeType
{
    Sphere,
    Box,
};

/// <summary>
/// 碰撞形狀描述。
/// </summary>
struct ShapeDesc
{
    ShapeType type = ShapeType::Sphere;
    // 球的半徑，必須大於 0（type 為 Sphere 時使用）
    float radius = 0.5f;
    // 盒的半尺寸，每個分量都必須大於 0（type 為 Box 時使用）
    Vector3 halfExtents{0.5f, 0.5f, 0.5f};

    static ShapeDesc MakeSphere(float radius)
    {
        ShapeDesc desc;
        desc.type = ShapeType::Sphere;
        desc.radius = radius;
        return desc;
    }

    static ShapeDesc MakeBox(const Vector3& halfExtents)
    {
        ShapeDesc desc;
        desc.type = ShapeType::Box;
        desc.halfExtents = halfExtents;
        return desc;
    }
};

/// <summary>
/// 建立剛體所需的資料。
/// </summary>
struct BodyDesc
{
    ShapeDesc shape;
    BodyType type = BodyType::Dynamic;
    Vector3 position{0.0f, 0.0f, 0.0f};
    Quaternion rotation{0.0f, 0.0f, 0.0f, 1.0f};
    // 反彈係數，0 到 1
    float restitution = 0.0f;
    // 摩擦係數，必須大於或等於 0
    float friction = 0.5f;
};

/// <summary>
/// 建立物理世界所需的資料。
/// </summary>
struct WorldDesc
{
    // 重力加速度（公尺 / 秒平方），預設為 Y 軸向下
    Vector3 gravity{0.0f, -9.81f, 0.0f};
    // 世界中最多同時存在的剛體數量，必須大於 0
    std::uint32_t maxBodies = 1024;
};

/// <summary>
/// 射線命中的結果。
/// </summary>
struct RaycastHit
{
    // 被命中的剛體
    BodyId body = invalidBody;
    // 從射線起點到命中點的距離（世界單位）
    float distance = 0.0f;
    // 命中點（世界座標）
    Vector3 point{0.0f, 0.0f, 0.0f};
    // 命中點的表面法線（世界座標，單位向量）
    Vector3 normal{0.0f, 0.0f, 0.0f};
};
} // namespace Skyline::Physics
