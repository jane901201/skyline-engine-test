#include "physics_world.h"

namespace Skyline::Physics
{
// 虛擬解構函式放在 .cpp 內，讓 vtable 只在 DLL 內產生一份
World::World() = default;
World::~World() = default;
} // namespace Skyline::Physics
