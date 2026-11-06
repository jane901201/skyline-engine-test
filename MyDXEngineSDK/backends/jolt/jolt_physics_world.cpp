// Jolt 要求 Jolt/Jolt.h 必須是第一個被引入的 Jolt 標頭
#include <Jolt/Jolt.h>

#include "jolt_physics_world.h"

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <stdexcept>
#include <string>

namespace Skyline::Jolt
{
namespace
{
// ----- 型別轉換：引擎座標與 Jolt 座標數值完全相同，不做鏡射 -----

JPH::Vec3 toJolt(const Vector3& v) noexcept
{
    return JPH::Vec3(v.x, v.y, v.z);
}

JPH::RVec3 toJoltPosition(const Vector3& v) noexcept
{
    return JPH::RVec3(v.x, v.y, v.z);
}

JPH::Quat toJolt(const Quaternion& q) noexcept
{
    return JPH::Quat(q.x, q.y, q.z, q.w);
}

Vector3 toEngine(JPH::Vec3 v) noexcept
{
    return Vector3(v.GetX(), v.GetY(), v.GetZ());
}

Vector3 toEnginePosition(JPH::RVec3 v) noexcept
{
    return Vector3(static_cast<float>(v.GetX()), static_cast<float>(v.GetY()), static_cast<float>(v.GetZ()));
}

Quaternion toEngine(JPH::Quat q) noexcept
{
    return Quaternion(q.GetX(), q.GetY(), q.GetZ(), q.GetW());
}

// ----- Jolt 的全域初始化 -----

/// <summary>
/// 管理 Jolt 的全域狀態（配置器、Factory、型別註冊）。
/// 由所有 World 共同持有，最後一個 World 釋放時才反註冊。
/// </summary>
class JoltRuntime
{
  public:
    static std::shared_ptr<JoltRuntime> acquire()
    {
        static std::mutex mutex;
        static std::weak_ptr<JoltRuntime> weak;

        std::lock_guard lock(mutex);
        std::shared_ptr<JoltRuntime> runtime = weak.lock();
        if (runtime == nullptr)
        {
            runtime = std::shared_ptr<JoltRuntime>(new JoltRuntime());
            weak = runtime;
        }
        return runtime;
    }

    ~JoltRuntime()
    {
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }

    JoltRuntime(const JoltRuntime&) = delete;
    JoltRuntime& operator=(const JoltRuntime&) = delete;

  private:
    JoltRuntime()
    {
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory();
        // RegisterTypes 會檢查編譯設定（AVX2、精度等）是否與函式庫一致，不一致時會中止程式
        JPH::RegisterTypes();
    }
};

// ----- 碰撞層設定：靜態物只與動態物碰撞，動態物與所有物件碰撞 -----

namespace Layers
{
constexpr JPH::ObjectLayer nonMoving = 0;
constexpr JPH::ObjectLayer moving = 1;
constexpr JPH::uint count = 2;
} // namespace Layers

namespace BroadPhaseLayers
{
constexpr JPH::BroadPhaseLayer nonMoving(0);
constexpr JPH::BroadPhaseLayer moving(1);
constexpr JPH::uint count = 2;
} // namespace BroadPhaseLayers

class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter
{
  public:
    bool ShouldCollide(JPH::ObjectLayer layer1, JPH::ObjectLayer layer2) const override
    {
        if (layer1 == Layers::nonMoving)
            return layer2 == Layers::moving;
        return true;
    }
};

class BroadPhaseLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
{
  public:
    JPH::uint GetNumBroadPhaseLayers() const override
    {
        return BroadPhaseLayers::count;
    }

    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
    {
        return layer == Layers::nonMoving ? BroadPhaseLayers::nonMoving : BroadPhaseLayers::moving;
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override
    {
        return layer == BroadPhaseLayers::nonMoving ? "NON_MOVING" : "MOVING";
    }
#endif
};

class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter
{
  public:
    bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broadPhaseLayer) const override
    {
        if (layer == Layers::nonMoving)
            return broadPhaseLayer == BroadPhaseLayers::moving;
        return true;
    }
};

// ----- 參數驗證 -----

void validateShape(const Physics::ShapeDesc& shape)
{
    switch (shape.type)
    {
    case Physics::ShapeType::Sphere:
        if (!(shape.radius > 0.0f))
            throw std::invalid_argument("JoltPhysics: Sphere radius must be greater than zero.");
        break;
    case Physics::ShapeType::Box:
        if (!(shape.halfExtents.x > 0.0f) || !(shape.halfExtents.y > 0.0f) || !(shape.halfExtents.z > 0.0f))
            throw std::invalid_argument("JoltPhysics: Box half extents must be greater than zero.");
        break;
    default:
        throw std::invalid_argument("JoltPhysics: Unknown shape type.");
    }
}

JPH::ShapeRefC createShape(const Physics::ShapeDesc& shape)
{
    if (shape.type == Physics::ShapeType::Sphere)
        return new JPH::SphereShape(shape.radius);

    // Jolt 的盒形有圓角半徑，不能大於最小的半尺寸
    const float smallest = std::min({shape.halfExtents.x, shape.halfExtents.y, shape.halfExtents.z});
    return new JPH::BoxShape(toJolt(shape.halfExtents), std::min(JPH::cDefaultConvexRadius, smallest));
}

class JoltWorld final : public Physics::World
{
  public:
    explicit JoltWorld(const Physics::WorldDesc& desc)
    {
        if (desc.maxBodies == 0)
            throw std::invalid_argument("JoltPhysics: maxBodies must be greater than zero.");

        // 接觸對與約束的上限依剛體數量估算
        const JPH::uint maxBodyPairs = std::max<JPH::uint>(desc.maxBodies * 8, 1024);
        const JPH::uint maxContactConstraints = std::max<JPH::uint>(desc.maxBodies * 4, 1024);
        m_system.Init(desc.maxBodies, 0, maxBodyPairs, maxContactConstraints, m_broadPhaseLayers,
                      m_objectVsBroadPhaseFilter, m_objectPairFilter);
        m_system.SetGravity(toJolt(desc.gravity));
    }

    Physics::BodyId createBody(const Physics::BodyDesc& desc) override
    {
        validateShape(desc.shape);
        if (!(desc.friction >= 0.0f))
            throw std::invalid_argument("JoltPhysics: Friction must be greater than or equal to zero.");
        if (!(desc.restitution >= 0.0f && desc.restitution <= 1.0f))
            throw std::invalid_argument("JoltPhysics: Restitution must be between 0 and 1.");

        JPH::EMotionType motion = JPH::EMotionType::Dynamic;
        JPH::ObjectLayer layer = Layers::moving;
        JPH::EActivation activation = JPH::EActivation::Activate;
        switch (desc.type)
        {
        case Physics::BodyType::Static:
            motion = JPH::EMotionType::Static;
            layer = Layers::nonMoving;
            activation = JPH::EActivation::DontActivate;
            break;
        case Physics::BodyType::Kinematic:
            motion = JPH::EMotionType::Kinematic;
            break;
        case Physics::BodyType::Dynamic:
            break;
        default:
            throw std::invalid_argument("JoltPhysics: Unknown body type.");
        }

        JPH::BodyCreationSettings settings(createShape(desc.shape), toJoltPosition(desc.position),
                                           toJolt(desc.rotation).Normalized(), motion, layer);
        settings.mRestitution = desc.restitution;
        settings.mFriction = desc.friction;

        const JPH::BodyID id = m_system.GetBodyInterface().CreateAndAddBody(settings, activation);
        if (id.IsInvalid())
            throw std::runtime_error("JoltPhysics: Cannot create body because the body limit was reached.");
        return id.GetIndexAndSequenceNumber();
    }

    void destroyBody(Physics::BodyId body) override
    {
        const JPH::BodyID id = requireBody(body);
        JPH::BodyInterface& bodies = m_system.GetBodyInterface();
        bodies.RemoveBody(id);
        bodies.DestroyBody(id);
    }

    void step(float deltaTime) override
    {
        if (!(deltaTime > 0.0f))
            throw std::invalid_argument("JoltPhysics: deltaTime must be greater than zero.");

        // Jolt 建議每 1/60 秒做一次碰撞步進，時間較長時增加步進次數以維持穩定
        const int collisionSteps = std::max(1, static_cast<int>(std::ceil(deltaTime * 60.0f)));
        m_system.Update(deltaTime, collisionSteps, &m_tempAllocator, &m_jobSystem);
    }

    Vector3 position(Physics::BodyId body) const override
    {
        return toEnginePosition(m_system.GetBodyInterface().GetPosition(requireBody(body)));
    }

    Quaternion rotation(Physics::BodyId body) const override
    {
        return toEngine(m_system.GetBodyInterface().GetRotation(requireBody(body)));
    }

    Vector3 linearVelocity(Physics::BodyId body) const override
    {
        return toEngine(m_system.GetBodyInterface().GetLinearVelocity(requireBody(body)));
    }

    void setLinearVelocity(Physics::BodyId body, const Vector3& velocity) override
    {
        m_system.GetBodyInterface().SetLinearVelocity(requireBody(body), toJolt(velocity));
    }

    bool isActive(Physics::BodyId body) const override
    {
        return m_system.GetBodyInterface().IsActive(requireBody(body));
    }

    std::uint32_t bodyCount() const override
    {
        return m_system.GetNumBodies();
    }

    std::optional<Physics::RaycastHit> raycast(const Ray& ray) const override
    {
        // Jolt 的射程是方向向量的長度，因此把單位方向乘上有限的射程
        const float length = std::min(ray.tMax, Physics::maxRayDistance);
        if (!(length > 0.0f))
            return std::nullopt;

        const JPH::RRayCast joltRay{toJoltPosition(ray.origin), toJolt(ray.direction) * length};
        JPH::RayCastResult hit;
        if (!m_system.GetNarrowPhaseQuery().CastRay(joltRay, hit))
            return std::nullopt;

        const JPH::RVec3 point = joltRay.GetPointOnRay(hit.mFraction);

        Physics::RaycastHit result;
        result.body = hit.mBodyID.GetIndexAndSequenceNumber();
        result.distance = hit.mFraction * length;
        result.point = toEnginePosition(point);

        // 表面法線要鎖定剛體才能讀取
        const JPH::BodyLockRead lock(m_system.GetBodyLockInterface(), hit.mBodyID);
        if (lock.Succeeded())
            result.normal = toEngine(lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, point));
        return result;
    }

  private:
    // 驗證剛體識別碼仍然有效，這是來自呼叫端的輸入，所以每次都要檢查
    JPH::BodyID requireBody(Physics::BodyId body) const
    {
        const JPH::BodyID id(body);
        if (id.IsInvalid() || !m_system.GetBodyLockInterface().TryGetBody(id))
            throw std::invalid_argument("JoltPhysics: Invalid body id " + std::to_string(body) + ".");
        return id;
    }

    // 宣告順序即建構順序：PhysicsSystem 會參考前面的層設定與配置器，必須最後被銷毀
    std::shared_ptr<JoltRuntime> m_runtime = JoltRuntime::acquire();
    BroadPhaseLayerInterfaceImpl m_broadPhaseLayers;
    ObjectVsBroadPhaseLayerFilterImpl m_objectVsBroadPhaseFilter;
    ObjectLayerPairFilterImpl m_objectPairFilter;
    JPH::TempAllocatorImpl m_tempAllocator{10 * 1024 * 1024};
    // 單執行緒的工作系統：不需要額外執行緒，適合小型引擎與測試
    JPH::JobSystemSingleThreaded m_jobSystem{JPH::cMaxPhysicsJobs};
    JPH::PhysicsSystem m_system;
};
} // namespace

std::unique_ptr<Physics::World> createWorld(const Physics::WorldDesc& desc)
{
    return std::make_unique<JoltWorld>(desc);
}
} // namespace Skyline::Jolt
