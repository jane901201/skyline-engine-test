#pragma once

#include <cstdint>

#include "gameplay/game_time.h"
#include "input/input_state.h"
#include "scene/scene.h"

namespace Skyline
{
class GameWorld;

/// <summary>
/// 掛在場景實體上的行為，對應 Unity 的 MonoBehaviour。
/// </summary>
/// <remarks>
/// 生命週期（與 Unity 相同的順序）：
///   Awake -> OnEnable -> Start -> [FixedUpdate*] -> Update -> LateUpdate -> OnDisable -> OnDestroy
/// - Awake / OnEnable：AddBehaviour 時立刻呼叫（Awake 不論是否啟用都會呼叫）。
/// - Start：第一次要執行 FixedUpdate / Update / LateUpdate 之前呼叫一次；停用中的行為會等到啟用後才呼叫。
/// - 在 Tick 期間新增的行為，要到下一次 Tick 才會開始被更新。
/// - 在 Tick 期間要求的銷毀，會延到該次 Tick 的 LateUpdate 之後才執行（與 Unity 的 Destroy 相同）。
///
/// 使用方式：繼承並覆寫需要的函式，再用 GameWorld::AddBehaviour&lt;T&gt;(entity) 加入。
/// 行為由 GameWorld 擁有，不可自行建立或複製。
/// </remarks>
class Behaviour
{
  public:
    Behaviour() = default;
    Behaviour(const Behaviour&) = delete;
    Behaviour& operator=(const Behaviour&) = delete;
    virtual ~Behaviour() = default;

    // ----- 生命週期（依需要覆寫）-----

    /// 被加入時立刻呼叫，只呼叫一次
    virtual void Awake() {}
    /// 變成啟用狀態時呼叫（Awake 之後若為啟用、或 SetEnabled(true)）
    virtual void OnEnable() {}
    /// 第一次更新之前呼叫一次
    virtual void Start() {}
    /// 固定步長更新（fixedDeltaTime），一次 Tick 可能呼叫 0 次或多次
    virtual void FixedUpdate(float fixedDeltaTime) { (void)fixedDeltaTime; }
    /// 每次 Tick 呼叫一次
    virtual void Update(float deltaTime) { (void)deltaTime; }
    /// 所有 Update 完成之後呼叫，適合攝影機跟隨這類要等其他物件移動完的邏輯
    virtual void LateUpdate(float deltaTime) { (void)deltaTime; }
    /// 變成停用狀態、或銷毀之前呼叫
    virtual void OnDisable() {}
    /// 銷毀時呼叫（此時所屬實體仍然存在）
    virtual void OnDestroy() {}

    // ----- 狀態 -----

    [[nodiscard]] bool IsEnabled() const noexcept
    {
        return m_enabled;
    }
    /// 啟用或停用。狀態改變時立刻呼叫 OnEnable / OnDisable（Awake 之前不會呼叫）
    void SetEnabled(bool enabled)
    {
        if (m_enabled == enabled)
            return;
        m_enabled = enabled;
        if (!m_awake)
            return;
        if (enabled)
            OnEnable();
        else
            OnDisable();
    }

    /// 已被要求銷毀（會在該次 Tick 結束時真正銷毀）
    [[nodiscard]] bool IsDestroyed() const noexcept
    {
        return m_destroyRequested;
    }

    /// 執行順序，數字小的先執行，相同時依加入順序（對應 Unity 的 Script Execution Order）。預設 0
    [[nodiscard]] int ExecutionOrder() const noexcept
    {
        return m_order;
    }
    inline void SetExecutionOrder(int order) noexcept;

    // ----- 存取所屬的世界（定義在 game_world.h）-----

    [[nodiscard]] EntityId Entity() const noexcept
    {
        return m_entity;
    }
    [[nodiscard]] GameWorld& World() const noexcept
    {
        return *m_world;
    }
    /// 所屬實體的場景（與 World().GetScene() 相同）
    [[nodiscard]] inline Scene& GetScene() const noexcept;
    [[nodiscard]] inline const GameTime& Time() const noexcept;
    /// 目前這個影格的輸入狀態；GameWorld 沒有設定輸入時丟出 std::logic_error
    [[nodiscard]] inline const Input::State& InputState() const;

    [[nodiscard]] inline const Transform& LocalTransform() const;
    inline void SetLocalTransform(const Transform& transform) const;

    /// 取得同一個實體上的另一個行為（型別 T 或其子類別），沒有時回傳 nullptr
    template <typename T> [[nodiscard]] T* GetBehaviour() const;

    /// 要求銷毀這個行為（Tick 期間延到結束時，否則立刻）
    inline void Destroy();

  private:
    friend class GameWorld;

    GameWorld* m_world = nullptr;
    EntityId m_entity = nullEntity;
    int m_order = 0;
    std::uint64_t m_sequence = 0; ///< 加入順序，用來讓相同 order 的行為保持穩定
    bool m_enabled = true;
    bool m_awake = false;
    bool m_started = false;
    bool m_destroyRequested = false;
    bool m_destroyed = false; ///< OnDestroy 已經呼叫過
};
} // namespace Skyline
