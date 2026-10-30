#include "vector_test_helpers.h"
#include "math/matrix.h"

namespace
{
void CheckMatrix(const Matrix& actual, const Matrix& expected)
{
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            CAPTURE(row, column);
            CHECK_THAT(actual.m[row][column], Catch::Matchers::WithinAbs(expected.m[row][column], 1e-5));
        }
    }
}
}

TEST_CASE("Matrix default construction and constants are identity", "[math][matrix]")
{
    const Matrix expected{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    CheckMatrix(Matrix{}, expected);
    CheckMatrix(Matrix::Identity, expected);
    CheckMatrix(g_matIdentity, expected);
    Vector3 point{2, -3, 4};
    expected.Apply(point);
    CheckVector(point, {2, -3, 4});
}

TEST_CASE("Matrix construction copying and conversion preserve row order", "[math][matrix]")
{
    const Matrix original{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            CAPTURE(row, column);
            CHECK(original.m[row][column] == row * 4 + column + 1);
        }
    }
    Matrix copy;
    copy = original;
    CheckMatrix(copy, original);
    DirectX::XMFLOAT4X4 stored;
    DirectX::XMStoreFloat4x4(&stored, original);
    CheckMatrix(Matrix{stored}, original);
}

TEST_CASE("Matrix translation occupies the last row and respects homogeneous w", "[math][matrix]")
{
    Matrix matrix;
    SECTION("Scalar overload") { matrix.MakeTranslation(5, -2, 7); }
    SECTION("Vector overload") { matrix.MakeTranslation(Vector3{5, -2, 7}); }
    CheckMatrix(matrix, Matrix{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 5, -2, 7, 1});
    Vector3 point{1, 2, 3};
    matrix.Apply(point);
    CheckVector(point, {6, 0, 10});
    Vector4 homogeneous{1, 2, 3, 2};
    matrix.Apply(homogeneous);
    CheckVector(homogeneous, {11, -2, 17, 2});
    Vector4 direction{1, 2, 3, 0};
    matrix.Apply(direction);
    CheckVector(direction, {1, 2, 3, 0});
}

TEST_CASE("Matrix Apply3x3 uses row vectors and ignores translation", "[math][matrix]")
{
    const Matrix matrix{1, 2, 3, 0, 4, 5, 6, 0, 7, 8, 9, 0, 20, 30, 40, 1};
    Vector3 direction{2, 3, 4};
    matrix.Apply3x3(direction);
    CheckVector(direction, {42, 51, 60});
    Vector3 point{2, 3, 4};
    matrix.Apply(point);
    CheckVector(point, {62, 81, 100});
}

TEST_CASE("Matrix scaling supports nonuniform negative and zero scales", "[math][matrix]")
{
    Matrix matrix;
    matrix.MakeTranslation(5, 6, 7);
    matrix.MakeScaling(Vector3{2, -3, 0});
    CheckMatrix(matrix, Matrix{2, 0, 0, 0, 0, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1});
    Vector4 point{3, 4, 5, 1};
    matrix.Apply(point);
    CheckVector(point, {6, -12, 0, 1});
}

TEST_CASE("Matrix axis rotations use radians and the expected orientation", "[math][matrix]")
{
    Matrix matrix;
    Vector3 input;
    Vector3 expected;
    SECTION("Positive X")
    {
        matrix.MakeRotationX(Math::PI / 2);
        input = Vector3{0, 1, 0};
        expected = Vector3{0, 0, 1};
    }
    SECTION("Positive Y")
    {
        matrix.MakeRotationY(Math::PI / 2);
        input = Vector3{0, 0, 1};
        expected = Vector3{1, 0, 0};
    }
    SECTION("Positive Z")
    {
        matrix.MakeRotationZ(Math::PI / 2);
        input = Vector3{1, 0, 0};
        expected = Vector3{0, 1, 0};
    }
    SECTION("Negative Z")
    {
        matrix.MakeRotationZ(-Math::PI / 2);
        input = Vector3{1, 0, 0};
        expected = Vector3{0, -1, 0};
    }
    matrix.Apply(input);
    CheckVector(input, expected);
}

TEST_CASE("Matrix arbitrary axis rotation normalizes the axis", "[math][matrix]")
{
    Matrix matrix;
    matrix.MakeRotationAxis(Vector3{2, 2, 2}, 2 * Math::PI / 3);
    Vector3 point{1, 0, 0};
    matrix.Apply(point);
    CheckVector(point, {0, 1, 0});
}

TEST_CASE("Matrix quaternion rotation transforms a known direction", "[math][matrix]")
{
    Matrix matrix;
    const float halfSqrtTwo = 0.7071067811865475f;
    matrix.MakeRotationFromQuaternion(Quaternion{0, 0, halfSqrtTwo, halfSqrtTwo});
    Vector3 point{1, 0, 0};
    matrix.Apply(point);
    CheckVector(point, {0, 1, 0});
}

TEST_CASE("Matrix multiplication applies transformations left to right", "[math][matrix]")
{
    Matrix scale;
    scale.MakeScaling(Vector3{2, 3, 4});
    Matrix translation;
    translation.MakeTranslation(5, 6, 7);
    Matrix combined;
    SECTION("Binary operator") { combined = scale * translation; }
    SECTION("Multiply") { combined.Multiply(scale, translation); }
    SECTION("Compound operator") { combined = scale; combined *= translation; }
    SECTION("First input aliases output") { combined = scale; combined.Multiply(combined, translation); }
    SECTION("Second input aliases output") { combined = translation; combined.Multiply(scale, combined); }
    Vector3 point{1, 2, 3};
    combined.Apply(point);
    CheckVector(point, {7, 12, 19});
    Vector3 reversed{1, 2, 3};
    (translation * scale).Apply(reversed);
    CheckVector(reversed, {12, 24, 40});
    CheckMatrix(scale * Matrix::Identity, scale);
    CheckMatrix(Matrix::Identity * translation, translation);
}

TEST_CASE("Matrix inverse undoes an affine transform", "[math][matrix]")
{
    const Matrix original{2, 0, 0, 0, 0, -4, 0, 0, 0, 0, 0.5f, 0, 6, 8, -2, 1};
    Matrix inverse;
    SECTION("Separate output") { inverse.Inverse(original); }
    SECTION("In place") { inverse = original; inverse.Inverse(); }
    CheckMatrix(inverse, Matrix{0.5f, 0, 0, 0, 0, -0.25f, 0, 0, 0, 0, 2, 0, -3, 2, 4, 1});
    CheckMatrix(original * inverse, Matrix::Identity);
    CheckMatrix(inverse * original, Matrix::Identity);
    Vector3 point{3, -5, 7};
    original.Apply(point);
    inverse.Apply(point);
    CheckVector(point, {3, -5, 7});
}

TEST_CASE("Matrix transpose exchanges rows and columns", "[math][matrix]")
{
    const Matrix original{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    Matrix matrix = original;
    matrix.Transpose();
    CheckMatrix(matrix, Matrix{1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15, 4, 8, 12, 16});
    matrix.Transpose();
    CheckMatrix(matrix, original);
}

TEST_CASE("Matrix look-at maps the camera to the origin and forward to positive Z", "[math][matrix]")
{
    Matrix view;
    view.MakeLookAt(Vector3{3, 2, 1}, Vector3{4, 2, 1}, Vector3{0, 1, 0});
    Vector3 eye{3, 2, 1};
    view.Apply(eye);
    CheckVector(eye, {0, 0, 0});
    Vector3 target{4, 2, 1};
    view.Apply(target);
    CheckVector(target, {0, 0, 1});
    Vector3 above{3, 3, 1};
    view.Apply(above);
    CheckVector(above, {0, 1, 0});
    Vector3 right{3, 2, 0};
    view.Apply(right);
    CheckVector(right, {1, 0, 0});
}

TEST_CASE("Matrix perspective maps near and far planes to zero and one after division", "[math][matrix]")
{
    Matrix projection;
    projection.MakeProjectionMatrix(Math::PI / 2, 2, 1, 11);
    Vector4 nearPoint{0, 0, 1, 1};
    Vector4 farPoint{0, 0, 11, 1};
    projection.Apply(nearPoint);
    projection.Apply(farPoint);
    CHECK_THAT(nearPoint.w, Catch::Matchers::WithinAbs(1, 1e-6));
    CHECK_THAT(farPoint.w, Catch::Matchers::WithinAbs(11, 1e-6));
    CHECK_THAT(nearPoint.z / nearPoint.w, Catch::Matchers::WithinAbs(0, 1e-6));
    CHECK_THAT(farPoint.z / farPoint.w, Catch::Matchers::WithinAbs(1, 1e-6));
    Vector4 edge{4, 2, 2, 1};
    projection.Apply(edge);
    CHECK_THAT(edge.x / edge.w, Catch::Matchers::WithinAbs(1, 1e-6));
    CHECK_THAT(edge.y / edge.w, Catch::Matchers::WithinAbs(1, 1e-6));
    Vector3 point{4, 2, 2};
    projection.Apply(point);
    // Apply(Vector3&) 保留裁切空間座標，不執行透視除法。
    CheckVector(point, {2, 2, 1.1f});
}

TEST_CASE("Matrix orthographic projection maps view bounds and depth", "[math][matrix]")
{
    Matrix projection;
    projection.MakeOrthoProjectionMatrix(8, 4, 1, 11);
    Vector4 nearCorner{-4, -2, 1, 1};
    Vector4 farCorner{4, 2, 11, 1};
    projection.Apply(nearCorner);
    projection.Apply(farCorner);
    CheckVector(nearCorner, {-1, -1, 0, 1});
    CheckVector(farCorner, {1, 1, 1, 1});
}
