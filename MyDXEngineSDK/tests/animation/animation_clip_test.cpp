#include "animation/animation_clip.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include "../scene/scene_test_utils.h"

using namespace Skyline;
using Catch::Approx;

namespace
{
Track<Vector3> MoveAlongX()
{
    Track<Vector3> track;
    track.keys = {{0.0f, Vector3(0, 0, 0)}, {1.0f, Vector3(10, 0, 0)}, {3.0f, Vector3(10, 4, 0)}};
    return track;
}
} // namespace

TEST_CASE("Track samples linearly between keys", "[animation]")
{
    const Track<Vector3> track = MoveAlongX();
    CHECK_VEC3(track.Sample(0.5f), 5, 0, 0);
    CHECK_VEC3(track.Sample(2.0f), 10, 2, 0);
}

TEST_CASE("Track clamps outside the key range", "[animation]")
{
    const Track<Vector3> track = MoveAlongX();
    CHECK_VEC3(track.Sample(-5.0f), 0, 0, 0);
    CHECK_VEC3(track.Sample(99.0f), 10, 4, 0);
}

TEST_CASE("Track with one key always returns that key", "[animation]")
{
    Track<Vector3> track;
    track.keys = {{2.0f, Vector3(1, 2, 3)}};
    CHECK_VEC3(track.Sample(0.0f), 1, 2, 3);
    CHECK_VEC3(track.Sample(7.0f), 1, 2, 3);
}

TEST_CASE("Step interpolation holds the previous key", "[animation]")
{
    Track<Vector3> track = MoveAlongX();
    track.interpolation = Interpolation::Step;
    CHECK_VEC3(track.Sample(0.99f), 0, 0, 0);
    CHECK_VEC3(track.Sample(1.0f), 10, 0, 0);
    CHECK_VEC3(track.Sample(2.9f), 10, 0, 0);
}

TEST_CASE("Rotation keys take the shortest path", "[animation]")
{
    // 0 到 270 度：最短路徑是反方向轉 90 度，中點應是 -45 度
    Track<Quaternion> track;
    track.keys = {{0.0f, SceneTest::RotationY(0.0f)}, {1.0f, SceneTest::RotationY(1.5f * SceneTest::kPi)}};
    Transform transform;
    transform.rotation = track.Sample(0.5f);
    // -45 度繞 Y：+Z 轉向 -X
    const float s = std::sqrt(0.5f);
    CHECK_VEC3(transform.Forward(), -s, 0, s);
}

TEST_CASE("Sampled rotations stay unit length", "[animation]")
{
    Track<Quaternion> track;
    track.keys = {{0.0f, SceneTest::RotationY(0.0f)}, {1.0f, SceneTest::RotationX(2.0f)}};
    for (float t = 0.0f; t <= 1.0f; t += 0.1f)
    {
        const Quaternion q = track.Sample(t);
        const float lengthSq = q.vec.x * q.vec.x + q.vec.y * q.vec.y + q.vec.z * q.vec.z + q.vec.w * q.vec.w;
        CHECK(lengthSq == Approx(1.0f).margin(1e-4f));
    }
}

TEST_CASE("Channel overrides only the animated components", "[animation]")
{
    AnimationChannel channel;
    channel.target = "box";
    channel.position = MoveAlongX();

    Transform base;
    base.scale = Vector3(2, 2, 2);
    base.position = Vector3(-1, -1, -1);

    const Transform result = channel.Apply(base, 0.5f);
    CHECK_VEC3(result.position, 5, 0, 0);
    CHECK_VEC3(result.scale, 2, 2, 2);
}

TEST_CASE("WrapTime loops or clamps", "[animation]")
{
    AnimationClip clip;
    clip.duration = 2.0f;
    CHECK(clip.WrapTime(5.0f, true) == Approx(1.0f));
    CHECK(clip.WrapTime(-0.5f, true) == Approx(1.5f));
    CHECK(clip.WrapTime(5.0f, false) == Approx(2.0f));
    CHECK(clip.WrapTime(-1.0f, false) == Approx(0.0f));
}

TEST_CASE("Clip validation rejects bad data", "[animation]")
{
    AnimationClip clip;
    clip.name = "bad";
    clip.duration = 3.0f;
    AnimationChannel channel;
    channel.target = "box";
    channel.position = MoveAlongX();
    clip.channels = {channel};
    REQUIRE_NOTHROW(clip.Validate());

    SECTION("zero duration")
    {
        clip.duration = 0.0f;
        CHECK_THROWS_AS(clip.Validate(), std::invalid_argument);
    }
    SECTION("empty target")
    {
        clip.channels[0].target.clear();
        CHECK_THROWS_AS(clip.Validate(), std::invalid_argument);
    }
    SECTION("unsorted keys")
    {
        clip.channels[0].position.keys[1].time = 0.0f;
        CHECK_THROWS_AS(clip.Validate(), std::invalid_argument);
    }
    SECTION("negative time")
    {
        clip.channels[0].position.keys[0].time = -1.0f;
        CHECK_THROWS_AS(clip.Validate(), std::invalid_argument);
    }
    SECTION("key after the end of the clip")
    {
        clip.duration = 2.0f;
        CHECK_THROWS_AS(clip.Validate(), std::invalid_argument);
    }
    SECTION("non-unit quaternion")
    {
        clip.channels[0].rotation.keys = {{0.0f, Quaternion(0, 0, 0, 2)}};
        CHECK_THROWS_AS(clip.Validate(), std::invalid_argument);
    }
}
