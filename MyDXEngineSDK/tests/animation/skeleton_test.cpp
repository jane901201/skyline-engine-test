#include "animation/skeleton.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include "../scene/scene_test_utils.h"

using namespace Skyline;

namespace
{
Transform At(float x, float y, float z)
{
    Transform t;
    t.position = Vector3(x, y, z);
    return t;
}

Matrix InverseOf(const Transform& t)
{
    Matrix m = t.ToMatrix();
    m.Inverse();
    return m;
}

// 手臂：root(0,0,0) -> upper(0,2,0) -> lower(0,2,0) -> hand(0,2,0)（皆為相對父關節）
struct Arm
{
    Skeleton::Pose bindPose{At(0, 0, 0), At(0, 2, 0), At(0, 2, 0), At(0, 2, 0)};
    Skeleton skeleton = MakeSkeleton();

    Skeleton MakeSkeleton() const
    {
        std::vector<Joint> joints(4);
        const char* names[] = {"root", "upper", "lower", "hand"};
        Matrix world = Matrix::Identity;
        for (int i = 0; i < 4; ++i)
        {
            world = i == 0 ? bindPose[0].ToMatrix() : bindPose[i].ToMatrix() * world;
            joints[i].name = names[i];
            joints[i].parent = i - 1;
            joints[i].inverseBind = world;
            joints[i].inverseBind.Inverse();
        }
        return Skeleton(joints);
    }
};

bool IsIdentity(const Matrix& m)
{
    const Matrix id = Matrix::Identity;
    const float* a = &m._11;
    const float* b = &id._11;
    for (int i = 0; i < 16; ++i)
        if (std::fabs(a[i] - b[i]) > 1e-4f)
            return false;
    return true;
}
} // namespace

TEST_CASE("Skinning matrices are identity in the bind pose", "[animation]")
{
    const Arm arm;
    for (const Matrix& m : arm.skeleton.SkinningMatrices(arm.bindPose))
        CHECK(IsIdentity(m));
}

TEST_CASE("Model matrices accumulate down the chain", "[animation]")
{
    const Arm arm;
    const std::vector<Matrix> model = arm.skeleton.ModelMatrices(arm.bindPose);
    Vector3 hand(0, 0, 0);
    model[3].Apply(hand);
    CHECK_VEC3(hand, 0, 6, 0);
}

TEST_CASE("Rotating a joint moves its descendants but not its ancestors", "[animation]")
{
    const Arm arm;
    Skeleton::Pose pose = arm.bindPose;
    // upper 繞 Z 轉 90 度：upper 以上的鏈會從 +Y 倒向 -X（左手座標下繞 +Z 為順時針看向 +Z）
    DirectX::XMStoreFloat4(&pose[1].rotation.vec,
                           DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(0, 0, 1, 0), SceneTest::kPi / 2));

    const std::vector<Matrix> skin = arm.skeleton.SkinningMatrices(pose);
    CHECK(IsIdentity(skin[0]));

    // 綁定姿勢下的 hand 位置 (0,6,0) 經過蒙皮矩陣後，應位在 upper(0,2,0) 之上 4 單位、方向被旋轉
    Vector3 hand(0, 6, 0);
    skin[3].Apply(hand);
    CHECK(std::fabs(hand.y - 2.0f) < 1e-4f);
    CHECK(std::fabs(std::fabs(hand.x) - 4.0f) < 1e-4f);
    CHECK(std::fabs(hand.z) < 1e-4f);
}

TEST_CASE("Sampling a clip writes only the targeted joints", "[animation]")
{
    const Arm arm;
    AnimationClip clip;
    clip.name = "raise";
    clip.duration = 1.0f;
    AnimationChannel channel;
    channel.target = "lower";
    channel.position.keys = {{0.0f, Vector3(0, 2, 0)}, {1.0f, Vector3(0, 4, 0)}};
    clip.channels = {channel};

    Skeleton::Pose pose = arm.bindPose;
    arm.skeleton.Sample(clip, 0.5f, false, pose);
    CHECK_VEC3(pose[2].position, 0, 3, 0);
    CHECK_VEC3(pose[1].position, 0, 2, 0);

    const std::vector<Matrix> model = arm.skeleton.ModelMatrices(pose);
    Vector3 hand(0, 0, 0);
    model[3].Apply(hand);
    CHECK_VEC3(hand, 0, 7, 0);
}

TEST_CASE("Skeleton validates joints and arguments", "[animation]")
{
    Joint a;
    a.name = "a";
    Joint b;
    b.name = "b";
    b.parent = 0;

    SECTION("valid")
    {
        CHECK_NOTHROW(Skeleton({a, b}));
    }
    SECTION("parent must precede child")
    {
        Joint first = a;
        first.parent = 1;
        CHECK_THROWS_AS(Skeleton({first, b}), std::invalid_argument);
    }
    SECTION("self parent")
    {
        Joint self = a;
        self.parent = 0;
        CHECK_THROWS_AS(Skeleton({self}), std::invalid_argument);
    }
    SECTION("duplicate names")
    {
        Joint dup = b;
        dup.name = "a";
        CHECK_THROWS_AS(Skeleton({a, dup}), std::invalid_argument);
    }
    SECTION("empty name")
    {
        Joint unnamed = a;
        unnamed.name.clear();
        CHECK_THROWS_AS(Skeleton({unnamed}), std::invalid_argument);
    }
    SECTION("pose size mismatch and unknown joint")
    {
        const Skeleton skeleton({a, b});
        CHECK_THROWS_AS(skeleton.ModelMatrices(Skeleton::Pose(1)), std::invalid_argument);
        CHECK_THROWS_AS(skeleton.IndexOf("nope"), std::invalid_argument);
        CHECK(skeleton.IndexOf("b") == 1);
    }
}
