#pragma once

#include <memory>
#include <optional>
#include <utility>

#include "animation/animator.h"
#include "gameplay/game_world.h"

namespace Skyline
{
/// <summary>
/// 在所屬實體的子樹上播放 AnimationClip，每次 Update 依遊戲時間推進（受 timeScale 影響）。
/// </summary>
/// <remarks>
/// clip 內通道的 target 名稱，要對應到這個實體底下（含自己）的實體名稱，見 Animator。
/// </remarks>
class AnimatorBehaviour : public Behaviour
{
  public:
    void Awake() override
    {
        m_animator.emplace(GetScene(), Entity());
    }

    /// 從頭播放 clip；失敗時的例外與保持狀態的行為同 Animator::Play
    void Play(std::shared_ptr<const AnimationClip> clip, bool loop = true)
    {
        m_animator->Play(std::move(clip), loop);
    }

    [[nodiscard]] Animator& GetAnimator() noexcept
    {
        return *m_animator;
    }

    void Update(float deltaTime) override
    {
        m_animator->Update(deltaTime);
    }

  private:
    std::optional<Animator> m_animator;
};
} // namespace Skyline
