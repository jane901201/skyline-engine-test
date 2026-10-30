#include "vector_test_helpers.h"

TEST_CASE("Vector3 construction constants and copying preserve components", "[math][vector3]")
{
    CheckVector(Vector3{}, {0, 0, 0});
    CheckVector(Vector3::Zero, {0, 0, 0});
    CheckVector(Vector3::One, {1, 1, 1});
    CheckVector(Vector3::Right, {1, 0, 0});
    CheckVector(Vector3::Left, {-1, 0, 0});
    CheckVector(Vector3::Up, {0, 1, 0});
    CheckVector(Vector3::Down, {0, -1, 0});
    CheckVector(Vector3::Front, {0, 0, 1});
    CheckVector(Vector3::Back, {0, 0, -1});
    Vector3 value{2, -3, 4};
    CheckVector(value, {2, -3, 4});
    value.Set(-5, 6, 7);
    Vector3 copy;
    copy = value;
    DirectX::XMFLOAT3 destination;
    value.CopyTo(destination);
    Vector3 converted;
    converted.Set(destination);
    value.Set(0, 0, 0);
    CheckVector(copy, {-5, 6, 7});
    CheckVector(converted, {-5, 6, 7});
}

TEST_CASE("Vector3 arithmetic operators preserve their inputs", "[math][vector3]")
{
    const Vector3 a{1, -2, 3};
    const Vector3 b{4, 5, -6};
    CheckVector(a + b, {5, 3, -3});
    CheckVector(a - b, {-3, -7, 9});
    CheckVector(a * -2, {-2, 4, -6});
    CheckVector(a / 2, {0.5f, -1, 1.5f});
    CheckVector(a * 0, {0, 0, 0});
    CheckVector(a, {1, -2, 3});
    CheckVector(b, {4, 5, -6});
}

TEST_CASE("Vector3 compound operators update the receiver", "[math][vector3]")
{
    Vector3 value{1, -2, 3};
    value += Vector3{4, 5, -6};
    CheckVector(value, {5, 3, -3});
    value -= Vector3{1, 2, 3};
    CheckVector(value, {4, 1, -6});
    value *= -2;
    CheckVector(value, {-8, -2, 12});
    value /= 2;
    CheckVector(value, {-4, -1, 6});
}

TEST_CASE("Vector3 dot product measures alignment", "[math][vector3]")
{
    CHECK(Vector3::AxisX.Dot(Vector3::AxisY) == 0);
    CHECK(Dot(Vector3::AxisX, Vector3::AxisX) == 1);
    CHECK(Dot(Vector3::Right, Vector3::Left) == -1);
    CHECK(Vector3(1, 2, 3).Dot(Vector3{4, -5, 6}) == 12);
}

TEST_CASE("Vector3 cross product has the expected orientation", "[math][vector3]")
{
    CheckVector(Cross(Vector3::AxisX, Vector3::AxisY), {0, 0, 1});
    CheckVector(Cross(Vector3::AxisY, Vector3::AxisX), {0, 0, -1});
    CheckVector(Cross(Vector3{1, 2, 3}, Vector3{2, 4, 6}), {0, 0, 0});
    Vector3 value{1, 2, 3};
    value.Cross(Vector3{-2, 1, 4});
    CheckVector(value, {5, -10, 5});
}

TEST_CASE("Vector3 output may alias either arithmetic input", "[math][vector3]")
{
    Vector3 value{1, 2, 3};
    const Vector3 other{-2, 1, 4};
    SECTION("Cross with first input") { value.Cross(value, other); CheckVector(value, {5, -10, 5}); }
    SECTION("Cross with second input") { value.Cross(other, value); CheckVector(value, {-5, 10, -5}); }
    SECTION("Add with first input") { value.Add(value, other); CheckVector(value, {-1, 3, 7}); }
    SECTION("Subtract with second input") { value.Subtract(other, value); CheckVector(value, {-3, -1, 1}); }
}

TEST_CASE("Vector3 length and normalization follow Euclidean geometry", "[math][vector3]")
{
    Vector3 value{3, -4, 0};
    CHECK(value.Length() == 5);
    CHECK(value.LengthSq() == 25);
    CHECK(Vector3::Zero.Length() == 0);
    value.Normalize();
    CheckVector(value, {0.6f, -0.8f, 0});
    CHECK_THAT(value.Length(), Catch::Matchers::WithinAbs(1, 1e-6));
    value.Normalize();
    CheckVector(value, {0.6f, -0.8f, 0});
}

TEST_CASE("Vector3 interpolation reaches endpoints and midpoint", "[math][vector3]")
{
    const Vector3 start{-2, 4, 6};
    const Vector3 end{6, -8, 2};
    Vector3 value;
    value.Lerp(0, start, end);
    CheckVector(value, start);
    value.Lerp(0.5f, start, end);
    CheckVector(value, {2, -2, 4});
    value.Lerp(1, start, end);
    CheckVector(value, end);
}

TEST_CASE("Vector3 minimum and maximum select each component", "[math][vector3]")
{
    Vector3 value{-2, 7, 3};
    SECTION("Minimum") { value.Min(Vector3{4, 1, 3}); CheckVector(value, {-2, 1, 3}); }
    SECTION("Maximum") { value.Max(Vector3{4, 1, 3}); CheckVector(value, {4, 7, 3}); }
}
