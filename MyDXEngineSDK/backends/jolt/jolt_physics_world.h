#pragma once

#include "core/skyline_api.h"
#include "physics/physics_world.h"

#include <memory>

namespace Skyline::Jolt
{
/// <summary>
/// 建立以 Jolt Physics 為後端的物理世界。
/// </summary>
/// <remarks>
/// Jolt 的標頭只在後端的 .cpp 內使用，不會出現在公開標頭中。
/// 參數不合法時丟出 std::invalid_argument。
/// </remarks>
SKYLINE_API std::unique_ptr<Physics::World> createWorld(const Physics::WorldDesc& desc = {});
} // namespace Skyline::Jolt
