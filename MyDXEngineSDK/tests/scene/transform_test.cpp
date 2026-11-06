#include "scene/transform.h"

#include "scene_test_utils.h"

#include <stdexcept>

using namespace Skyline;
using namespace SceneTest;

TEST_CASE("Transform default is identity", "[scene]")
{
    const Transform transform;
    const Matrix matrix = transform.ToMatrix();
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
            CHECK(matrix.m[row][column] == Catch::Approx(row == column ? 1.0f : 0.0f).margin(1e-6f));
    }
    CHECK_VEC3(transform.Forward(), 0.0f, 0.0f, 1.0f);
    CHECK_VEC3(transform.Right(), 1.0f, 0.0f, 0.0f);
    CHECK_VEC3(transform.Up(), 0.0f, 1.0f, 0.0f);
}

TEST_CASE("Transform applies scale then rotation then translation", "[scene]")
{
    Transform transform;
    transform.scale = Vector3(2.0f, 1.0f, 1.0f);
    transform.rotation = RotationY(kPi * 0.5f);
    transform.position = Vector3(10.0f, 0.0f, 0.0f);

    // (1,0,0) 先縮放成 (2,0,0)，繞 Y 軸轉 90 度（左手座標）變成 (0,0,-2)，再平移
    CHECK_VEC3(transform.TransformPoint(Vector3(1.0f, 0.0f, 0.0f)), 10.0f, 0.0f, -2.0f);
}

TEST_CASE("Transform direction vectors follow rotation", "[scene]")
{
    Transform transform;
    transform.rotation = RotationY(kPi * 0.5f);

    // 左手座標下，繞 Y 軸正向旋轉 90 度，前方從 +Z 轉到 +X
    CHECK_VEC3(transform.Forward(), 1.0f, 0.0f, 0.0f);
    CHECK_VEC3(transform.Right(), 0.0f, 0.0f, -1.0f);
    CHECK_VEC3(transform.Up(), 0.0f, 1.0f, 0.0f);
}

TEST_CASE("Transform survives a matrix round trip", "[scene]")
{
    Transform original;
    original.position = Vector3(1.0f, -2.0f, 3.0f);
    original.rotation = RotationX(0.7f);
    original.scale = Vector3(2.0f, 3.0f, 0.5f);

    const Transform restored = Transform::FromMatrix(original.ToMatrix());

    CHECK_VEC3(restored.position, 1.0f, -2.0f, 3.0f);
    CHECK_VEC3(restored.scale, 2.0f, 3.0f, 0.5f);
    // 四元數 q 與 -q 代表相同旋轉，所以用方向向量比較
    CHECK_VEC3(restored.Forward(), original.Forward().x, original.Forward().y, original.Forward().z);
    CHECK_VEC3(restored.Up(), original.Up().x, original.Up().y, original.Up().z);
}

TEST_CASE("Transform FromMatrix rejects a matrix that cannot be decomposed", "[scene]")
{
    // 投影矩陣含有透視成分，不是 Transform
    Matrix projection;
    projection.MakeProjectionMatrix(1.0f, 1.5f, 0.1f, 100.0f);
    CHECK_THROWS_AS(Transform::FromMatrix(projection), std::invalid_argument);
}

TEST_CASE("Transform LookAt faces the target", "[scene]")
{
    CHECK_VEC3(Transform::LookAt(Vector3(0, 0, 0), Vector3(0, 0, 5)).Forward(), 0.0f, 0.0f, 1.0f);
    CHECK_VEC3(Transform::LookAt(Vector3(0, 0, 0), Vector3(5, 0, 0)).Forward(), 1.0f, 0.0f, 0.0f);
    CHECK_VEC3(Transform::LookAt(Vector3(0, 0, 0), Vector3(0, 5, 5)).Up(), 0.0f, 0.70710678f, -0.70710678f);

    const Transform lookAt = Transform::LookAt(Vector3(1, 2, 3), Vector3(4, 2, 3));
    CHECK_VEC3(lookAt.position, 1.0f, 2.0f, 3.0f);
    CHECK_VEC3(lookAt.Right(), 0.0f, 0.0f, -1.0f);
}

TEST_CASE("Transform LookAt rejects degenerate input", "[scene]")
{
    CHECK_THROWS_AS(Transform::LookAt(Vector3(1, 1, 1), Vector3(1, 1, 1)), std::invalid_argument);
    // up 與視線方向平行
    CHECK_THROWS_AS(Transform::LookAt(Vector3(0, 0, 0), Vector3(0, 5, 0), Vector3(0, 1, 0)), std::invalid_argument);
}
