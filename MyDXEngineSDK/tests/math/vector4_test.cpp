#include "vector_test_helpers.h"

TEST_CASE("Vector4 construction and Vector3 conversion set w correctly", "[math][vector4]")
{
    CheckVector(Vector4{}, {0, 0, 0, 1});
    CheckVector(Vector4{1, -2, 3, -4}, {1, -2, 3, -4});
    CheckVector(Vector4{Vector3{2, 3, 4}}, {2, 3, 4, 1});
    Vector4 value{0, 0, 0, 9};
    value.Set(Vector3{-2, 5, 7});
    CheckVector(value, {-2, 5, 7, 1});
    value.Set(1, 2, 3, 4);
    Vector4 copy;
    copy.Set(value);
    DirectX::XMFLOAT4 destination;
    copy.CopyTo(destination);
    value.Set(0, 0, 0, 0);
    CheckVector(copy, {1, 2, 3, 4});
    CheckVector(Vector4{destination.x, destination.y, destination.z, destination.w}, {1, 2, 3, 4});
}

TEST_CASE("Vector4 arithmetic includes w and preserves inputs", "[math][vector4]")
{
    const Vector4 a{1, -2, 3, 4};
    const Vector4 b{4, 5, -6, 2};
    CheckVector(a + b, {5, 3, -3, 6});
    CheckVector(a - b, {-3, -7, 9, 2});
    CheckVector(a * -2, {-2, 4, -6, -8});
    CheckVector(a * 0, {0, 0, 0, 0});
    CheckVector(a, {1, -2, 3, 4});
    CheckVector(b, {4, 5, -6, 2});
    Vector4 value = a;
    value.Add(b);
    CheckVector(value, {5, 3, -3, 6});
    value.Subtract(b);
    CheckVector(value, a);
}

TEST_CASE("Vector4 dot product and length include w", "[math][vector4]")
{
    Vector4 value{0, 0, 3, 4};
    CHECK(value.Length() == 5);
    CHECK(value.LengthSq() == 25);
    CHECK(value.Dot(Vector4{0, 0, 0, 2}) == 8);
    CHECK(Dot(Vector4{1, 2, 3, 4}, Vector4{4, -5, 6, 2}) == 20);
}

TEST_CASE("Vector4 normalization includes w", "[math][vector4]")
{
    Vector4 value{0, 0, 3, -4};
    value.Normalize();
    CheckVector(value, {0, 0, 0.6f, -0.8f});
    CHECK_THAT(value.Length(), Catch::Matchers::WithinAbs(1, 1e-6));
    value.Normalize();
    CheckVector(value, {0, 0, 0.6f, -0.8f});
}

TEST_CASE("Vector4 color constants include alpha", "[math][vector4]")
{
    CheckVector(Vector4::White, {1, 1, 1, 1});
    CheckVector(Vector4::Black, {0, 0, 0, 1});
    CheckVector(Vector4::Yellow, {1, 1, 0, 1});
    CheckVector(Vector4::Gray, {0.5f, 0.5f, 0.5f, 1});
}
