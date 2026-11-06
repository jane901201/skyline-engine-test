#pragma once

#include "core/skyline_api.h"
#include "math/ray.h"
#include "physics_types.h"

#include <cstdint>
#include <optional>

namespace Skyline::Physics
{
/// <summary>
/// 物理世界，負責模擬剛體與回答空間查詢。
/// </summary>
/// <remarks>
/// 這是與物理函式庫無關的介面，使用引擎自己的 Vector3、Quaternion 與 Ray。
/// 座標系與引擎一致，不做任何鏡射轉換。
/// 由後端工廠建立，例如 Skyline::Jolt::createWorld。
/// 對失效的 BodyId 操作會丟出 std::invalid_argument。
/// </remarks>
class SKYLINE_API World
{
  public:
    virtual ~World();
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    /// 建立剛體並加入世界，剛體數量超過上限時丟出 std::runtime_error
    virtual BodyId createBody(const BodyDesc& desc) = 0;
    /// 從世界移除並銷毀剛體
    virtual void destroyBody(BodyId body) = 0;

    /// 推進模擬 deltaTime 秒，必須大於 0
    virtual void step(float deltaTime) = 0;

    [[nodiscard]] virtual Vector3 position(BodyId body) const = 0;
    [[nodiscard]] virtual Quaternion rotation(BodyId body) const = 0;
    [[nodiscard]] virtual Vector3 linearVelocity(BodyId body) const = 0;
    /// 設定線速度，常用於 Kinematic 剛體或發射物
    virtual void setLinearVelocity(BodyId body, const Vector3& velocity) = 0;
    /// 剛體是否仍在運動（靜止一段時間後物理引擎會讓它休眠）
    [[nodiscard]] virtual bool isActive(BodyId body) const = 0;

    /// 目前世界中的剛體數量
    [[nodiscard]] virtual std::uint32_t bodyCount() const = 0;

    /// <summary>
    /// 發射射線，回傳最近的命中點，沒有命中時回傳 std::nullopt。
    /// </summary>
    /// <remarks>
    /// 射線的 tMax 會被限制在 maxRayDistance 以內。
    /// </remarks>
    [[nodiscard]] virtual std::optional<RaycastHit> raycast(const Ray& ray) const = 0;

  protected:
    World();
};
} // namespace Skyline::Physics
