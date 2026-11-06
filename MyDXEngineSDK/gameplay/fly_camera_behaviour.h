#pragma once

#include "gameplay/game_world.h"
#include "scene/camera.h"
#include "scene/fly_camera.h"

namespace Skyline
{
/// <summary>
/// 用 GameWorld 的輸入驅動一台 Camera 自由飛行（WASD、Q/E、Shift、按住右鍵轉視角）。
/// </summary>
/// <remarks>
/// Camera 由呼叫端擁有，必須比這個行為活得久。使用真實時間（不受 timeScale 影響），所以遊戲暫停時仍可移動攝影機。
/// 使用前必須先對 GameWorld 呼叫 SetInput。
/// </remarks>
class FlyCameraBehaviour : public Behaviour
{
  public:
    explicit FlyCameraBehaviour(Camera& camera) : m_camera(&camera) {}

    /// 移動速度、滑鼠靈敏度等設定
    FlyCameraController controller;

    void Awake() override
    {
        controller.SyncFrom(*m_camera);
    }

    void Update(float) override
    {
        controller.Update(*m_camera, InputState(), Time().unscaledDeltaTime);
    }

  private:
    Camera* m_camera;
};
} // namespace Skyline
