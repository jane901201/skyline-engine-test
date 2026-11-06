#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "input/input_state.h"

using namespace Skyline::Input;

TEST_CASE("Input state starts with nothing pressed", "[input]")
{
    State state;
    CHECK_FALSE(state.isDown(Key::W));
    CHECK_FALSE(state.wasPressed(Key::W));
    CHECK_FALSE(state.wasReleased(Key::W));
    CHECK_FALSE(state.isDown(MouseButton::Left));
    CHECK(state.mouseDelta().x == 0.0f);
    CHECK(state.mouseDelta().y == 0.0f);
    CHECK(state.mouseWheel() == 0.0f);
}

TEST_CASE("Input events are invisible until update", "[input]")
{
    State state;
    state.setKeyDown(Key::W, true);
    CHECK_FALSE(state.isDown(Key::W));
    CHECK_FALSE(state.wasPressed(Key::W));

    state.update();
    CHECK(state.isDown(Key::W));
    CHECK(state.wasPressed(Key::W));
}

TEST_CASE("Input key edges last for exactly one frame", "[input]")
{
    State state;
    state.setKeyDown(Key::Space, true);
    state.update();
    CHECK(state.isDown(Key::Space));
    CHECK(state.wasPressed(Key::Space));

    // 按住不放：仍是按下狀態，但不再是「剛按下」
    state.update();
    CHECK(state.isDown(Key::Space));
    CHECK_FALSE(state.wasPressed(Key::Space));

    state.setKeyDown(Key::Space, false);
    state.update();
    CHECK_FALSE(state.isDown(Key::Space));
    CHECK(state.wasReleased(Key::Space));

    state.update();
    CHECK_FALSE(state.wasReleased(Key::Space));
}

TEST_CASE("Input key repeat events do not create new presses", "[input]")
{
    State state;
    state.setKeyDown(Key::A, true);
    state.update();
    REQUIRE(state.wasPressed(Key::A));

    // 系統在按住時會重複送出按下事件
    state.setKeyDown(Key::A, true);
    state.setKeyDown(Key::A, true);
    state.update();
    CHECK(state.isDown(Key::A));
    CHECK_FALSE(state.wasPressed(Key::A));
}

TEST_CASE("Input quick tap within one frame is not lost", "[input]")
{
    State state;
    state.setKeyDown(Key::E, true);
    state.setKeyDown(Key::E, false);
    state.update();

    CHECK_FALSE(state.isDown(Key::E));
    CHECK(state.wasPressed(Key::E));
    CHECK(state.wasReleased(Key::E));
}

TEST_CASE("Input keys are tracked independently", "[input]")
{
    State state;
    state.setKeyDown(Key::W, true);
    state.setKeyDown(Key::Shift, true);
    state.update();

    CHECK(state.isDown(Key::W));
    CHECK(state.isDown(Key::Shift));
    CHECK_FALSE(state.isDown(Key::A));
    CHECK_FALSE(state.isDown(Key::Control));
}

TEST_CASE("Input mouse buttons follow the same edge rules", "[input]")
{
    State state;
    state.setMouseButtonDown(MouseButton::Right, true);
    state.update();
    CHECK(state.isDown(MouseButton::Right));
    CHECK(state.wasPressed(MouseButton::Right));
    CHECK_FALSE(state.isDown(MouseButton::Left));

    state.setMouseButtonDown(MouseButton::Right, false);
    state.update();
    CHECK_FALSE(state.isDown(MouseButton::Right));
    CHECK(state.wasReleased(MouseButton::Right));
}

TEST_CASE("Input mouse delta and wheel accumulate then reset per frame", "[input]")
{
    State state;
    state.addMouseDelta(3.0f, -1.0f);
    state.addMouseDelta(2.0f, 4.0f);
    state.addMouseWheel(1.0f);
    state.addMouseWheel(-0.5f);
    state.update();

    CHECK_THAT(state.mouseDelta().x, Catch::Matchers::WithinAbs(5.0, 1e-6));
    CHECK_THAT(state.mouseDelta().y, Catch::Matchers::WithinAbs(3.0, 1e-6));
    CHECK_THAT(state.mouseWheel(), Catch::Matchers::WithinAbs(0.5, 1e-6));

    // 沒有新事件時，下一個 Frame 的位移回到 0
    state.update();
    CHECK(state.mouseDelta().x == 0.0f);
    CHECK(state.mouseDelta().y == 0.0f);
    CHECK(state.mouseWheel() == 0.0f);
}

TEST_CASE("Input mouse position persists across frames", "[input]")
{
    State state;
    state.setMousePosition(120.0f, 45.0f);
    state.update();
    CHECK(state.mousePosition().x == 120.0f);
    CHECK(state.mousePosition().y == 45.0f);

    state.update();
    CHECK(state.mousePosition().x == 120.0f);
    CHECK(state.mousePosition().y == 45.0f);
}

TEST_CASE("Input releaseAll releases keys and mouse buttons with release edges", "[input]")
{
    State state;
    state.setKeyDown(Key::W, true);
    state.setKeyDown(Key::Shift, true);
    state.setMouseButtonDown(MouseButton::Left, true);
    state.update();
    REQUIRE(state.isDown(Key::W));

    state.releaseAll();
    state.update();

    CHECK_FALSE(state.isDown(Key::W));
    CHECK_FALSE(state.isDown(Key::Shift));
    CHECK_FALSE(state.isDown(MouseButton::Left));
    CHECK(state.wasReleased(Key::W));
    CHECK(state.wasReleased(Key::Shift));
    CHECK(state.wasReleased(MouseButton::Left));
    CHECK_FALSE(state.wasReleased(Key::A));
}
