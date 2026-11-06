#include "scene/fly_camera.h"

#include "scene_test_utils.h"

#include <stdexcept>

using namespace Skyline;
using namespace SceneTest;
using Input::Key;
using Input::MouseButton;

namespace
{
// 讓 Input::State 的快照反映目前設定的輸入
Input::State HoldKeys(std::initializer_list<Key> keys)
{
    Input::State state;
    for (const Key key : keys)
        state.setKeyDown(key, true);
    state.update();
    return state;
}

Input::State LookInput(float dx, float dy, bool rightButton)
{
    Input::State state;
    state.setMouseButtonDown(MouseButton::Right, rightButton);
    state.addMouseDelta(dx, dy);
    state.update();
    return state;
}
} // namespace

TEST_CASE("FlyCamera moves forward, back and sideways", "[scene]")
{
    FlyCameraController controller;
    controller.moveSpeed = 4.0f;

    Camera camera;
    controller.Update(camera, HoldKeys({Key::W}), 0.5f);
    CHECK_VEC3(camera.transform.position, 0.0f, 0.0f, 2.0f);

    controller.Update(camera, HoldKeys({Key::S}), 0.5f);
    CHECK_VEC3(camera.transform.position, 0.0f, 0.0f, 0.0f);

    controller.Update(camera, HoldKeys({Key::D}), 0.5f);
    CHECK_VEC3(camera.transform.position, 2.0f, 0.0f, 0.0f);

    controller.Update(camera, HoldKeys({Key::A}), 0.5f);
    CHECK_VEC3(camera.transform.position, 0.0f, 0.0f, 0.0f);
}

TEST_CASE("FlyCamera moves vertically along the world Y axis", "[scene]")
{
    FlyCameraController controller;
    controller.moveSpeed = 2.0f;

    Camera camera;
    controller.Update(camera, HoldKeys({Key::E}), 1.0f);
    CHECK_VEC3(camera.transform.position, 0.0f, 2.0f, 0.0f);
    controller.Update(camera, HoldKeys({Key::Q}), 1.0f);
    CHECK_VEC3(camera.transform.position, 0.0f, 0.0f, 0.0f);
}

TEST_CASE("FlyCamera opposite keys cancel and Shift boosts speed", "[scene]")
{
    FlyCameraController controller;
    controller.moveSpeed = 2.0f;
    controller.boostMultiplier = 3.0f;

    Camera camera;
    controller.Update(camera, HoldKeys({Key::W, Key::S}), 1.0f);
    CHECK_VEC3(camera.transform.position, 0.0f, 0.0f, 0.0f);

    controller.Update(camera, HoldKeys({Key::W, Key::Shift}), 1.0f);
    CHECK_VEC3(camera.transform.position, 0.0f, 0.0f, 6.0f);
}

TEST_CASE("FlyCamera does not move without input", "[scene]")
{
    FlyCameraController controller;
    Camera camera;
    camera.transform.position = Vector3(1, 2, 3);
    controller.Update(camera, Input::State{}, 1.0f);
    CHECK_VEC3(camera.transform.position, 1.0f, 2.0f, 3.0f);
}

TEST_CASE("FlyCamera look requires the right mouse button by default", "[scene]")
{
    FlyCameraController controller;
    Camera camera;

    controller.Update(camera, LookInput(100.0f, 50.0f, false), 0.016f);
    CHECK(controller.Yaw() == Catch::Approx(0.0f).margin(1e-6f));
    CHECK(controller.Pitch() == Catch::Approx(0.0f).margin(1e-6f));

    controller.lookSensitivity = 0.01f;
    controller.Update(camera, LookInput(100.0f, 50.0f, true), 0.016f);
    CHECK(controller.Yaw() == Catch::Approx(1.0f).margin(1e-5f));
    CHECK(controller.Pitch() == Catch::Approx(0.5f).margin(1e-5f));

    // 轉動後，Camera 的朝向要跟著 yaw / pitch
    const Vector3 forward = camera.transform.Forward();
    CHECK(forward.x == Catch::Approx(std::cos(0.5f) * std::sin(1.0f)).margin(1e-4f));
    CHECK(forward.y == Catch::Approx(-std::sin(0.5f)).margin(1e-4f));
    CHECK(forward.z == Catch::Approx(std::cos(0.5f) * std::cos(1.0f)).margin(1e-4f));
}

TEST_CASE("FlyCamera look works without a button when not required", "[scene]")
{
    FlyCameraController controller;
    controller.requireRightMouseButton = false;
    controller.lookSensitivity = 0.01f;
    Camera camera;

    controller.Update(camera, LookInput(100.0f, 0.0f, false), 0.016f);
    CHECK(controller.Yaw() == Catch::Approx(1.0f).margin(1e-5f));
}

TEST_CASE("FlyCamera clamps pitch below the vertical", "[scene]")
{
    FlyCameraController controller;
    controller.lookSensitivity = 1.0f;
    Camera camera;

    controller.Update(camera, LookInput(0.0f, 1000.0f, true), 0.016f);
    CHECK(controller.Pitch() == Catch::Approx(DirectX::XMConvertToRadians(89.0f)).margin(1e-5f));

    controller.Update(camera, LookInput(0.0f, -5000.0f, true), 0.016f);
    CHECK(controller.Pitch() == Catch::Approx(-DirectX::XMConvertToRadians(89.0f)).margin(1e-5f));

    // 夾住之後仍能產生有效的 View 矩陣（forward 與 up 不平行）
    CHECK_NOTHROW(camera.ViewMatrix());
}

TEST_CASE("FlyCamera moves along the looking direction", "[scene]")
{
    FlyCameraController controller;
    controller.moveSpeed = 1.0f;
    controller.lookSensitivity = 1.0f;
    Camera camera;

    // 向右轉 90 度（yaw = pi / 2），前方變成 +X
    controller.Update(camera, LookInput(kPi * 0.5f, 0.0f, true), 0.016f);
    const Vector3 start = camera.transform.position;
    controller.Update(camera, HoldKeys({Key::W}), 1.0f);

    CHECK(camera.transform.position.x - start.x == Catch::Approx(1.0f).margin(1e-4f));
    CHECK(camera.transform.position.z - start.z == Catch::Approx(0.0f).margin(1e-4f));
}

TEST_CASE("FlyCamera SyncFrom keeps the current orientation", "[scene]")
{
    Camera camera;
    camera.transform = Transform::LookAt(Vector3(0, 0, 0), Vector3(3, -2, 5));
    const Vector3 before = camera.transform.Forward();

    FlyCameraController controller;
    controller.SyncFrom(camera);
    controller.Update(camera, Input::State{}, 0.016f);

    CHECK_VEC3(camera.transform.Forward(), before.x, before.y, before.z);
}

TEST_CASE("FlyCamera rejects a negative delta time", "[scene]")
{
    FlyCameraController controller;
    Camera camera;
    CHECK_THROWS_AS(controller.Update(camera, Input::State{}, -0.1f), std::invalid_argument);
    CHECK_NOTHROW(controller.Update(camera, Input::State{}, 0.0f));
}
