#include "vector_test_helpers.h"

TEST_CASE("Vector2 construction and assignment preserve components", "[math][vector2]")
{
    CheckVector(Vector2{}, {0, 0});
    CheckVector(Vector2::Zero, {0, 0});
    Vector2 value{2, -3};
    CheckVector(value, {2, -3});
    value.Set(-4, 5);
    Vector2 copy;
    copy = value;
    value.Set(0, 0);
    CheckVector(copy, {-4, 5});
}

TEST_CASE("Vector2 interpolation reaches endpoints and midpoint", "[math][vector2]")
{
    const Vector2 start{-2, 4};
    const Vector2 end{6, -8};
    Vector2 value;
    value.Lerp(0, start, end);
    CheckVector(value, start);
    value.Lerp(0.5f, start, end);
    CheckVector(value, {2, -2});
    value.Lerp(1, start, end);
    CheckVector(value, end);
}

TEST_CASE("Vector2 normalization preserves direction", "[math][vector2]")
{
    Vector2 value{3, -4};
    value.Normalize();
    CheckVector(value, {0.6f, -0.8f});
    value.Normalize();
    CheckVector(value, {0.6f, -0.8f});
}
