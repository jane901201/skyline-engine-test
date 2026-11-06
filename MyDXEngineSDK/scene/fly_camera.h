#pragma once

#include <DirectXMath.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "camera.h"
#include "input/input_state.h"
#include "input/input_types.h"

namespace Skyline
{
/// <summary>
/// 自由飛行攝影機的控制器：WASD 前後左右、Q / E 下上、Shift 加速，按住滑鼠右鍵移動滑鼠轉動視角。
/// </summary>
/// <remarks>
/// 控制器自己記錄 yaw / pitch，每次 Update 都會重新寫入 Camera 的旋轉，
/// 因此不會累積浮點誤差，也不會產生翻滾（roll）。
/// 座標系為左手座標、Y 向上：yaw 為正時向 +X 轉，pitch 為正時向下看。
/// 此類別全部是 inline，不需要匯出。
/// </remarks>
class FlyCameraController
{
  public:
    // 移動速度（世界單位 / 秒）
    float moveSpeed = 5.0f;
    // 按住 Shift 時的速度倍率
    float boostMultiplier = 3.0f;
    // 視角靈敏度（弧度 / 像素）
    float lookSensitivity = 0.0025f;
    // 為 true 時需要按住滑鼠右鍵才會轉動視角
    bool requireRightMouseButton = true;

    /// <summary>
    /// 從攝影機目前的朝向取得 yaw / pitch，之後的 Update 會接著這個方向繼續轉。
    /// </summary>
    void SyncFrom(const Camera& camera)
    {
        const Vector3 forward = camera.transform.Forward();
        m_yaw = std::atan2(forward.x, forward.z);
        m_pitch = ClampPitch(std::asin(std::clamp(-forward.y, -1.0f, 1.0f)));
    }

    [[nodiscard]] float Yaw() const noexcept
    {
        return m_yaw;
    }
    [[nodiscard]] float Pitch() const noexcept
    {
        return m_pitch;
    }

    /// <summary>
    /// 依這個 Frame 的輸入更新攝影機。deltaTime 必須大於或等於 0。
    /// </summary>
    void Update(Camera& camera, const Input::State& input, float deltaTime)
    {
        if (!(deltaTime >= 0.0f))
            throw std::invalid_argument("FlyCameraController::Update: deltaTime must be greater than or equal to zero.");

        if (!requireRightMouseButton || input.isDown(Input::MouseButton::Right))
        {
            m_yaw += input.mouseDelta().x * lookSensitivity;
            m_pitch = ClampPitch(m_pitch + input.mouseDelta().y * lookSensitivity);
        }
        camera.transform.rotation = Rotation();

        // 沿著攝影機自己的方向移動；上下則固定沿世界 Y 軸
        float forwardAxis = 0.0f;
        float rightAxis = 0.0f;
        float upAxis = 0.0f;
        if (input.isDown(Input::Key::W))
            forwardAxis += 1.0f;
        if (input.isDown(Input::Key::S))
            forwardAxis -= 1.0f;
        if (input.isDown(Input::Key::D))
            rightAxis += 1.0f;
        if (input.isDown(Input::Key::A))
            rightAxis -= 1.0f;
        if (input.isDown(Input::Key::E))
            upAxis += 1.0f;
        if (input.isDown(Input::Key::Q))
            upAxis -= 1.0f;

        const float speed = moveSpeed * (input.isDown(Input::Key::Shift) ? boostMultiplier : 1.0f);
        const Vector3 forward = camera.transform.Forward();
        const Vector3 right = camera.transform.Right();
        camera.transform.position.x += (forward.x * forwardAxis + right.x * rightAxis) * speed * deltaTime;
        camera.transform.position.y += (forward.y * forwardAxis + right.y * rightAxis + upAxis) * speed * deltaTime;
        camera.transform.position.z += (forward.z * forwardAxis + right.z * rightAxis) * speed * deltaTime;
    }

  private:
    // 避免看到正上方或正下方時 forward 與 up 平行
    static float ClampPitch(float pitch) noexcept
    {
        constexpr float limit = DirectX::XMConvertToRadians(89.0f);
        return std::clamp(pitch, -limit, limit);
    }

    [[nodiscard]] Quaternion Rotation() const
    {
        Quaternion result;
        DirectX::XMStoreFloat4(&result.vec, DirectX::XMQuaternionRotationRollPitchYaw(m_pitch, m_yaw, 0.0f));
        return result;
    }

    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
};
} // namespace Skyline
