#pragma once

#include <stdexcept>

#include "gameplay/game_world.h"
#include "physics/physics_world.h"

namespace Skyline
{
/// <summary>
/// 以固定步長推進一個物理世界。通常整個場景只要一個，放在任何實體上都可以。
/// </summary>
/// <remarks>
/// 預設執行順序為 -100，確保在其他行為的 FixedUpdate 之前先推進物理。
/// 物理世界由呼叫端擁有，必須比這個行為活得久。
/// </remarks>
class PhysicsStepBehaviour : public Behaviour
{
  public:
    explicit PhysicsStepBehaviour(Physics::World& world) : m_physics(&world) {}

    void Awake() override
    {
        SetExecutionOrder(-100);
    }

    void FixedUpdate(float fixedDeltaTime) override
    {
        m_physics->step(fixedDeltaTime);
    }

  private:
    Physics::World* m_physics;
};

/// <summary>
/// 在物理世界建立一個剛體，每個 Update 把剛體的位置與旋轉寫回所屬實體。
/// </summary>
/// <remarks>
/// - 剛體的初始位置與旋轉取自 desc；實體的縮放不受影響。
/// - 所屬實體必須是根節點（沒有父節點），否則位置寫回的是世界座標、與本地座標不一致，Awake 會丟出 std::invalid_argument。
/// - 銷毀行為時一併銷毀剛體。物理世界由呼叫端擁有，必須比這個行為活得久。
/// </remarks>
class RigidBodyBehaviour : public Behaviour
{
  public:
    RigidBodyBehaviour(Physics::World& world, const Physics::BodyDesc& desc) : m_physics(&world), m_desc(desc) {}

    void Awake() override
    {
        if (!GetScene().Parent(Entity()).IsNull())
            throw std::invalid_argument("RigidBodyBehaviour: the entity must be a root entity (no parent).");
        m_body = m_physics->createBody(m_desc);
    }

    void OnDestroy() override
    {
        m_physics->destroyBody(m_body);
    }

    void Update(float) override
    {
        Transform transform = LocalTransform();
        transform.position = m_physics->position(m_body);
        transform.rotation = m_physics->rotation(m_body);
        SetLocalTransform(transform);
    }

    [[nodiscard]] Physics::BodyId Body() const noexcept
    {
        return m_body;
    }

  private:
    Physics::World* m_physics;
    Physics::BodyDesc m_desc;
    Physics::BodyId m_body = Physics::invalidBody;
};
} // namespace Skyline
