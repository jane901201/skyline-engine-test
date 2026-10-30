#pragma once

#include "math/vector.h"

// 三個天空相關階段共用同一組相機，避免雲、背景與太陽的位置不同步。
namespace CloudscapeCamera
{
inline const Vector3 position{0.0f, -5.0f, 17.0f};
inline const Vector3 target{0.0f, -4.0f, 16.94f};
inline const Vector3 up{0.0f, 0.0f, 1.0f};
}
