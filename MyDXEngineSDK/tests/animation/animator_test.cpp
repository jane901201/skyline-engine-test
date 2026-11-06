#include "animation/animator.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <stdexcept>

#include "../scene/scene_test_utils.h"

using namespace Skyline;
using Catch::Approx;

namespace
{
// 2 秒內 box 沿 X 從 0 移到 10
std::shared_ptr<AnimationClip> MakeSlide(const std::string& target = "box")
{
    auto clip = std::make_shared<AnimationClip>();
    clip->name = "slide";
    clip->duration = 2.0f;
    AnimationChannel channel;
    channel.target = target;
    channel.position.keys = {{0.0f, Vector3(0, 0, 0)}, {2.0f, Vector3(10, 0, 0)}};
    clip->channels = {channel};
    return clip;
}

struct Fixture
{
    Scene scene;
    EntityId root = scene.CreateEntity("root");
    EntityId box = scene.CreateEntity("box", root);
};
} // namespace

TEST_CASE("Animator drives local transforms from a clip", "[animation]")
{
    Fixture f;
    Animator animator(f.scene, f.root);
    animator.Play(MakeSlide());

    CHECK_VEC3(f.scene.WorldPosition(f.box), 0, 0, 0);
    animator.Update(1.0f);
    CHECK_VEC3(f.scene.WorldPosition(f.box), 5, 0, 0);
    CHECK(animator.IsPlaying());
}

TEST_CASE("Animation follows the parent transform", "[animation]")
{
    Fixture f;
    Transform parent;
    parent.position = Vector3(0, 100, 0);
    f.scene.SetLocalTransform(f.root, parent);

    Animator animator(f.scene, f.root);
    animator.Play(MakeSlide());
    animator.Update(1.0f);
    CHECK_VEC3(f.scene.WorldPosition(f.box), 5, 100, 0);
}

TEST_CASE("Looping animation wraps around", "[animation]")
{
    Fixture f;
    Animator animator(f.scene, f.root);
    animator.Play(MakeSlide(), true);
    animator.Update(2.5f);
    CHECK_VEC3(f.scene.WorldPosition(f.box), 2.5f, 0, 0);
    CHECK(animator.IsPlaying());
    CHECK_FALSE(animator.IsFinished());
}

TEST_CASE("Non-looping animation stops at the end", "[animation]")
{
    Fixture f;
    Animator animator(f.scene, f.root);
    animator.Play(MakeSlide(), false);
    animator.Update(5.0f);
    CHECK_VEC3(f.scene.WorldPosition(f.box), 10, 0, 0);
    CHECK(animator.IsFinished());
    CHECK_FALSE(animator.IsPlaying());

    animator.Resume();
    CHECK_FALSE(animator.IsPlaying());
}

TEST_CASE("Speed scales time and negative speed plays backwards", "[animation]")
{
    Fixture f;
    Animator animator(f.scene, f.root);
    animator.Play(MakeSlide(), false);

    animator.SetSpeed(2.0f);
    animator.Update(0.5f);
    CHECK_VEC3(f.scene.WorldPosition(f.box), 5, 0, 0);

    animator.SetSpeed(-1.0f);
    animator.Update(0.5f);
    CHECK_VEC3(f.scene.WorldPosition(f.box), 2.5f, 0, 0);

    animator.Update(10.0f);
    CHECK_VEC3(f.scene.WorldPosition(f.box), 0, 0, 0);
    CHECK(animator.IsFinished());
}

TEST_CASE("Pause holds the pose and Resume continues", "[animation]")
{
    Fixture f;
    Animator animator(f.scene, f.root);
    animator.Play(MakeSlide());
    animator.Update(1.0f);
    animator.Pause();
    animator.Update(1.0f);
    CHECK_VEC3(f.scene.WorldPosition(f.box), 5, 0, 0);

    animator.Resume();
    animator.Update(0.5f);
    CHECK_VEC3(f.scene.WorldPosition(f.box), 7.5f, 0, 0);
}

TEST_CASE("SetTime jumps and applies immediately", "[animation]")
{
    Fixture f;
    Animator animator(f.scene, f.root);
    animator.Play(MakeSlide());
    animator.SetTime(1.5f);
    CHECK_VEC3(f.scene.WorldPosition(f.box), 7.5f, 0, 0);
}

TEST_CASE("Stop leaves the pose and clears the clip", "[animation]")
{
    Fixture f;
    Animator animator(f.scene, f.root);
    animator.Play(MakeSlide());
    animator.Update(1.0f);
    animator.Stop();
    animator.Update(1.0f);
    CHECK_VEC3(f.scene.WorldPosition(f.box), 5, 0, 0);
    CHECK(animator.Clip() == nullptr);
    CHECK_THROWS_AS(animator.SetTime(0.0f), std::logic_error);
}

TEST_CASE("Play validates its input and keeps the old state on failure", "[animation]")
{
    Fixture f;
    Animator animator(f.scene, f.root);
    animator.Play(MakeSlide());
    animator.Update(1.0f);

    CHECK_THROWS_AS(animator.Play(nullptr), std::invalid_argument);
    CHECK_THROWS_AS(animator.Play(MakeSlide("missing")), std::invalid_argument);

    auto bad = MakeSlide();
    bad->duration = 0.0f;
    CHECK_THROWS_AS(animator.Play(bad), std::invalid_argument);

    // 失敗的 Play 不影響原本的播放
    CHECK(animator.Time() == Approx(1.0f));
    CHECK(animator.IsPlaying());
}

TEST_CASE("Duplicate names are rejected", "[animation]")
{
    Fixture f;
    f.scene.CreateEntity("box", f.root);
    Animator animator(f.scene, f.root);
    CHECK_THROWS_AS(animator.Play(MakeSlide()), std::invalid_argument);
}

TEST_CASE("Entities outside the root subtree are not bound", "[animation]")
{
    Fixture f;
    const EntityId other = f.scene.CreateEntity("other");
    f.scene.CreateEntity("elsewhere", other);
    Animator animator(f.scene, f.root);
    CHECK_THROWS_AS(animator.Play(MakeSlide("elsewhere")), std::invalid_argument);
}

TEST_CASE("Animator rejects bad arguments", "[animation]")
{
    Fixture f;
    CHECK_THROWS_AS(Animator(f.scene, nullEntity), std::invalid_argument);

    Animator animator(f.scene, f.root);
    CHECK_THROWS_AS(animator.SetSpeed(std::nanf("")), std::invalid_argument);
    CHECK_THROWS_AS(animator.Update(-1.0f), std::invalid_argument);
}
