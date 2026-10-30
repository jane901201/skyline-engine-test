#pragma once

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "math/vector.h"

inline void CheckVector(const Vector2& actual, const Vector2& expected)
{
    CHECK_THAT(actual.x, Catch::Matchers::WithinAbs(expected.x, 1e-6));
    CHECK_THAT(actual.y, Catch::Matchers::WithinAbs(expected.y, 1e-6));
}

inline void CheckVector(const Vector3& actual, const Vector3& expected)
{
    CHECK_THAT(actual.x, Catch::Matchers::WithinAbs(expected.x, 1e-6));
    CHECK_THAT(actual.y, Catch::Matchers::WithinAbs(expected.y, 1e-6));
    CHECK_THAT(actual.z, Catch::Matchers::WithinAbs(expected.z, 1e-6));
}

inline void CheckVector(const Vector4& actual, const Vector4& expected)
{
    CHECK_THAT(actual.x, Catch::Matchers::WithinAbs(expected.x, 1e-6));
    CHECK_THAT(actual.y, Catch::Matchers::WithinAbs(expected.y, 1e-6));
    CHECK_THAT(actual.z, Catch::Matchers::WithinAbs(expected.z, 1e-6));
    CHECK_THAT(actual.w, Catch::Matchers::WithinAbs(expected.w, 1e-6));
}
