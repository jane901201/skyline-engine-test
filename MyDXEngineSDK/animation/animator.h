#pragma once

#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "animation/animation_clip.h"
#include "scene/scene.h"

namespace Skyline
{
/// <summary>
/// 在 Scene 的一棵子樹上播放 AnimationClip。
/// </summary>
/// <remarks>
/// Play 時依通道的 target 名稱，在 root 的子樹（含 root）中找到對應實體。
/// 找不到、或同名實體不只一個時丟出 std::invalid_argument，之後的 Update 不再檢查。
/// Animator 不擁有 Scene，Scene 必須比 Animator 活得久；被綁定的實體在播放中被銷毀屬於呼叫端錯誤。
/// </remarks>
class Animator
{
  public:
    /// root 不是有效實體時丟出 std::invalid_argument
    Animator(Scene& scene, EntityId root) : m_scene(&scene), m_root(root)
    {
        if (!scene.IsAlive(root))
            throw std::invalid_argument("Animator: root entity is not alive.");
    }

    /// <summary>
    /// 從頭播放 clip。clip 不合法或有通道綁不到實體時丟出 std::invalid_argument，且保持原本的播放狀態。
    /// </summary>
    void Play(std::shared_ptr<const AnimationClip> clip, bool loop = true)
    {
        if (!clip)
            throw std::invalid_argument("Animator::Play: clip must not be null.");
        clip->Validate();

        std::vector<EntityId> bound = Bind(*clip);

        m_clip = std::move(clip);
        m_bound = std::move(bound);
        m_loop = loop;
        m_time = 0.0f;
        m_playing = true;
        Evaluate();
    }

    /// 停止並清除目前的 clip；實體維持目前的姿勢
    void Stop() noexcept
    {
        m_clip.reset();
        m_bound.clear();
        m_playing = false;
        m_time = 0.0f;
    }

    void Pause() noexcept
    {
        m_playing = false;
    }
    /// 繼續播放；沒有 clip、或非循環動畫已播完時不做事
    void Resume() noexcept
    {
        if (m_clip && !IsFinished())
            m_playing = true;
    }

    /// 播放速度倍率（可為負數倒著播，0 等同暫停）；非有限值時丟出 std::invalid_argument
    void SetSpeed(float speed)
    {
        if (!std::isfinite(speed))
            throw std::invalid_argument("Animator::SetSpeed: speed must be finite.");
        m_speed = speed;
    }
    [[nodiscard]] float Speed() const noexcept
    {
        return m_speed;
    }

    /// 跳到指定的播放時間（秒）並立刻套用到場景
    void SetTime(float seconds)
    {
        if (!m_clip)
            throw std::logic_error("Animator::SetTime: no clip is playing.");
        if (!std::isfinite(seconds))
            throw std::invalid_argument("Animator::SetTime: time must be finite.");
        m_time = seconds;
        ClampIfNotLooping();
        Evaluate();
    }

    /// 前進 deltaSeconds（乘上播放速度）並把結果寫回場景的本地 Transform
    void Update(float deltaSeconds)
    {
        if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0f)
            throw std::invalid_argument("Animator::Update: delta must be finite and non-negative.");
        if (!m_playing)
            return;

        m_time += deltaSeconds * m_speed;
        ClampIfNotLooping();
        Evaluate();
        if (IsFinished())
            m_playing = false;
    }

    [[nodiscard]] bool IsPlaying() const noexcept
    {
        return m_playing;
    }
    /// 非循環動畫已經到達（或倒回）端點
    [[nodiscard]] bool IsFinished() const noexcept
    {
        if (!m_clip || m_loop)
            return false;
        return (m_speed >= 0.0f && m_time >= m_clip->duration) || (m_speed < 0.0f && m_time <= 0.0f);
    }
    [[nodiscard]] bool IsLooping() const noexcept
    {
        return m_loop;
    }
    /// 目前播放時間（秒）。循環動畫會持續累加；實際取樣的時間是 Clip()->WrapTime(Time(), true)
    [[nodiscard]] float Time() const noexcept
    {
        return m_time;
    }
    [[nodiscard]] const std::shared_ptr<const AnimationClip>& Clip() const noexcept
    {
        return m_clip;
    }

  private:
    // 沒有循環的動畫，時間停在 [0, duration]
    void ClampIfNotLooping() noexcept
    {
        if (!m_loop)
            m_time = std::clamp(m_time, 0.0f, m_clip->duration);
    }

    void Evaluate()
    {
        const float sampleTime = m_clip->WrapTime(m_time, m_loop);
        for (std::size_t i = 0; i < m_bound.size(); ++i)
        {
            const EntityId entity = m_bound[i];
            m_scene->SetLocalTransform(entity, m_clip->channels[i].Apply(m_scene->LocalTransform(entity), sampleTime));
        }
    }

    // 回傳與 clip.channels 一一對應的實體
    [[nodiscard]] std::vector<EntityId> Bind(const AnimationClip& clip) const
    {
        std::unordered_map<std::string, EntityId> byName;
        std::unordered_map<std::string, bool> duplicated;
        CollectNames(m_root, byName, duplicated);

        std::vector<EntityId> bound;
        bound.reserve(clip.channels.size());
        for (const AnimationChannel& channel : clip.channels)
        {
            const auto found = byName.find(channel.target);
            if (found == byName.end())
                throw std::invalid_argument("Animator::Play: no entity named '" + channel.target + "' under the root.");
            if (duplicated.count(channel.target))
                throw std::invalid_argument("Animator::Play: more than one entity is named '" + channel.target + "'.");
            bound.push_back(found->second);
        }
        return bound;
    }

    void CollectNames(EntityId id, std::unordered_map<std::string, EntityId>& byName,
                      std::unordered_map<std::string, bool>& duplicated) const
    {
        const std::string& name = m_scene->Name(id);
        if (!byName.emplace(name, id).second)
            duplicated[name] = true;
        for (const EntityId child : m_scene->Children(id))
            CollectNames(child, byName, duplicated);
    }

    Scene* m_scene;
    EntityId m_root;
    std::shared_ptr<const AnimationClip> m_clip;
    std::vector<EntityId> m_bound;
    float m_time = 0.0f;
    float m_speed = 1.0f;
    bool m_loop = true;
    bool m_playing = false;
};
} // namespace Skyline
