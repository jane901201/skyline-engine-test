#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "gameplay/behaviour.h"
#include "gameplay/game_time.h"
#include "scene/scene.h"

namespace Skyline
{
/// <summary>
/// 擁有 Scene 與所有 Behaviour，並負責驅動它們的生命週期（Unity 風格）。
/// </summary>
/// <remarks>
/// 每次 Tick 的順序：
///   1. 排序（依 ExecutionOrder）並決定本次要更新的行為（Tick 期間新增的行為不參與本次）
///   2. 固定步長：依累積時間呼叫 0 到多次 FixedUpdate
///   3. Update
///   4. LateUpdate
///   5. 處理銷毀要求（OnDisable -> OnDestroy -> 移除行為與實體）
/// 每個階段開始對某個行為呼叫之前，若它尚未 Start，會先呼叫 Start。
///
/// 實體必須透過 GameWorld::DestroyEntity 銷毀，才能讓行為收到 OnDestroy。
/// 直接呼叫 GetScene().DestroyEntity 會讓行為失去實體，下一次 Tick 會丟出 std::logic_error。
///
/// 非執行緒安全，所有呼叫都必須在同一個執行緒。
/// </remarks>
class GameWorld
{
  public:
    GameWorld() = default;
    GameWorld(const GameWorld&) = delete;
    GameWorld& operator=(const GameWorld&) = delete;

    ~GameWorld()
    {
        // 結束時讓還存在的行為收到 OnDisable / OnDestroy；解構函式不可丟例外，所以吞掉
        try
        {
            for (const auto& behaviour : m_behaviours)
                behaviour->m_destroyRequested = true;
            m_destroyPending = true;
            FlushDestroyed();
        }
        catch (...)
        {
        }
    }

    // ----- 場景 -----

    [[nodiscard]] Scene& GetScene() noexcept
    {
        return m_scene;
    }
    [[nodiscard]] const Scene& GetScene() const noexcept
    {
        return m_scene;
    }

    EntityId CreateEntity(std::string name = {}, EntityId parent = nullEntity)
    {
        return m_scene.CreateEntity(std::move(name), parent);
    }

    /// <summary>
    /// 要求銷毀實體及其所有子孫（連同上面的行為）。
    /// Tick 期間延到該次 Tick 結束；其他時候立刻執行。實體已不存在時不做事。
    /// </summary>
    void DestroyEntity(EntityId id)
    {
        if (!m_scene.IsAlive(id))
            return;
        // 立刻讓子樹上的行為停止接收更新，真正的銷毀仍在 Tick 結束時進行
        std::vector<EntityId> subtree;
        CollectSubtree(id, subtree);
        for (const EntityId member : subtree)
        {
            const auto found = m_byEntity.find(Key(member));
            if (found == m_byEntity.end())
                continue;
            for (Behaviour* behaviour : found->second)
                behaviour->m_destroyRequested = true;
        }
        m_destroyEntities.push_back(id);
        m_destroyPending = true;
        if (!m_ticking && !m_flushing)
            FlushDestroyed();
    }

    // ----- 行為 -----

    /// <summary>
    /// 在實體上建立並加入一個行為，立刻呼叫 Awake（與 OnEnable），回傳的參照在行為被銷毀前有效。
    /// </summary>
    /// <remarks>
    /// entity 不存在或正在被銷毀時丟出 std::invalid_argument。
    /// Awake 丟出例外時，行為不會被加入。
    /// </remarks>
    template <typename T, typename... Args> T& AddBehaviour(EntityId entity, Args&&... args)
    {
        static_assert(std::is_base_of_v<Behaviour, T>, "T must derive from Skyline::Behaviour.");
        if (!m_scene.IsAlive(entity))
            throw std::invalid_argument("GameWorld::AddBehaviour: entity is not alive.");
        if (m_dying.count(Key(entity)))
            throw std::invalid_argument("GameWorld::AddBehaviour: entity is being destroyed.");

        auto owned = std::make_unique<T>(std::forward<Args>(args)...);
        T& behaviour = *owned;
        // 透過基底類別存取私有欄位，避免被衍生類別中同名的成員遮蔽
        Behaviour& base = behaviour;
        base.m_world = this;
        base.m_entity = entity;
        base.m_sequence = m_nextSequence++;

        base.m_awake = true;
        behaviour.Awake();
        if (base.m_enabled)
            behaviour.OnEnable();

        // Awake 成功之後才登記，所以 Awake 丟例外時不會留下半成品
        m_byEntity[Key(entity)].push_back(&behaviour);
        m_behaviours.push_back(std::move(owned));
        m_orderDirty = true;
        return behaviour;
    }

    /// 取得實體上第一個型別為 T（或其子類別）的行為，沒有時回傳 nullptr
    template <typename T> [[nodiscard]] T* GetBehaviour(EntityId entity) const
    {
        const auto found = m_byEntity.find(Key(entity));
        if (found == m_byEntity.end())
            return nullptr;
        for (Behaviour* behaviour : found->second)
            if (T* typed = dynamic_cast<T*>(behaviour))
                return typed;
        return nullptr;
    }

    /// 要求銷毀行為。Tick 期間延到該次 Tick 結束；其他時候立刻執行。重複呼叫沒有影響。
    void Destroy(Behaviour& behaviour)
    {
        if (behaviour.m_destroyRequested)
            return;
        behaviour.m_destroyRequested = true;
        m_destroyPending = true;
        if (!m_ticking && !m_flushing)
            FlushDestroyed();
    }

    [[nodiscard]] std::size_t BehaviourCount() const noexcept
    {
        return m_behaviours.size();
    }

    // ----- 時間與輸入 -----

    [[nodiscard]] GameTime& Time() noexcept
    {
        return m_time;
    }
    [[nodiscard]] const GameTime& Time() const noexcept
    {
        return m_time;
    }

    /// 設定行為透過 InputState() 讀取的輸入狀態。GameWorld 不擁有它，呼叫端必須確保它比 GameWorld 活得久
    void SetInput(const Input::State* input) noexcept
    {
        m_input = input;
    }
    [[nodiscard]] const Input::State& InputState() const
    {
        if (!m_input)
            throw std::logic_error("GameWorld::InputState: no input state was set. Call SetInput first.");
        return *m_input;
    }

    // ----- 主迴圈 -----

    /// <summary>
    /// 推進一個影格。
    /// </summary>
    /// <param name="realDeltaSeconds">這個影格經過的真實時間（秒）</param>
    /// <remarks>
    /// 時間設定不合法、deltaSeconds 不是有限的非負數時丟出 std::invalid_argument；
    /// 在 Tick 裡面再次呼叫 Tick 時丟出 std::logic_error。
    /// 行為丟出的例外會直接傳出去，世界之後仍可繼續 Tick，但那一次的銷毀處理會延到下一次 Tick。
    /// </remarks>
    void Tick(float realDeltaSeconds)
    {
        if (m_ticking)
            throw std::logic_error("GameWorld::Tick: Tick must not be called from inside a behaviour callback.");
        if (!std::isfinite(realDeltaSeconds) || realDeltaSeconds < 0.0f)
            throw std::invalid_argument("GameWorld::Tick: delta must be finite and non-negative.");
        if (!(m_time.fixedDeltaTime > 0.0f) || !(m_time.maximumDeltaTime > 0.0f) || !(m_time.timeScale >= 0.0f) ||
            !std::isfinite(m_time.fixedDeltaTime) || !std::isfinite(m_time.maximumDeltaTime) ||
            !std::isfinite(m_time.timeScale))
            throw std::invalid_argument("GameWorld::Tick: invalid GameTime settings.");

        struct TickGuard
        {
            explicit TickGuard(bool& flag) : flag(flag)
            {
                flag = true;
            }
            ~TickGuard()
            {
                flag = false;
            }
            bool& flag;
        } guard(m_ticking);

        const float unscaled = std::min(realDeltaSeconds, m_time.maximumDeltaTime);
        const float scaled = unscaled * m_time.timeScale;
        m_time.unscaledDeltaTime = unscaled;
        m_time.deltaTime = scaled;
        m_time.unscaledTime += unscaled;
        m_time.time += scaled;

        SortIfNeeded();
        // 只更新 Tick 開始時就存在的行為；之後新增的會附加在後面，下一次 Tick 才參與
        m_activeCount = m_behaviours.size();

        // 與 Unity 相同：所有尚未 Start 的行為，在任何 FixedUpdate / Update 之前先 Start
        RunPhase([](Behaviour&) {});

        // 固定步長
        m_fixedAccumulator += scaled;
        while (m_fixedAccumulator >= m_time.fixedDeltaTime)
        {
            m_fixedAccumulator -= m_time.fixedDeltaTime;
            const float fixed = m_time.fixedDeltaTime;
            m_time.deltaTime = fixed;
            RunPhase([fixed](Behaviour& b) { b.FixedUpdate(fixed); });
        }
        m_time.deltaTime = scaled;

        RunPhase([scaled](Behaviour& b) { b.Update(scaled); });
        RunPhase([scaled](Behaviour& b) { b.LateUpdate(scaled); });

        ++m_time.frameCount;
        FlushDestroyed();
    }

  private:
    friend class Behaviour;

    static std::uint64_t Key(EntityId id) noexcept
    {
        return (static_cast<std::uint64_t>(id.generation) << 32) | id.index;
    }

    // 排序只在 Tick 開始與銷毀處理時做，不會發生在迭代的途中
    void SortIfNeeded()
    {
        if (!m_orderDirty)
            return;
        std::stable_sort(m_behaviours.begin(), m_behaviours.end(),
                         [](const std::unique_ptr<Behaviour>& a, const std::unique_ptr<Behaviour>& b) {
                             if (a->m_order != b->m_order)
                                 return a->m_order < b->m_order;
                             return a->m_sequence < b->m_sequence;
                         });
        m_orderDirty = false;
    }

    // 逐一對本次要更新的行為呼叫 callback。迴圈中行為可以被停用、被要求銷毀、或新增其他行為。
    template <typename Callback> void RunPhase(Callback&& callback)
    {
        for (std::size_t i = 0; i < m_activeCount; ++i)
        {
            Behaviour& behaviour = *m_behaviours[i];
            if (!CanRun(behaviour))
                continue;
            if (!m_scene.IsAlive(behaviour.m_entity))
                throw std::logic_error("GameWorld: an entity was destroyed with Scene::DestroyEntity directly. "
                                       "Use GameWorld::DestroyEntity so its behaviours are cleaned up.");

            if (!behaviour.m_started)
            {
                behaviour.m_started = true;
                behaviour.Start();
                if (!CanRun(behaviour)) // Start 裡可能把自己停用或銷毀
                    continue;
            }
            callback(behaviour);
        }
    }

    static bool CanRun(const Behaviour& behaviour) noexcept
    {
        return behaviour.m_enabled && !behaviour.m_destroyRequested;
    }

    // 收集實體與其子孫
    void CollectSubtree(EntityId id, std::vector<EntityId>& out) const
    {
        out.push_back(id);
        for (const EntityId child : m_scene.Children(id))
            CollectSubtree(child, out);
    }

    // 處理所有銷毀要求。OnDestroy 裡又要求銷毀時，會繼續處理，直到沒有為止
    void FlushDestroyed()
    {
        struct FlushGuard
        {
            explicit FlushGuard(bool& flag) : flag(flag)
            {
                flag = true;
            }
            ~FlushGuard()
            {
                flag = false;
            }
            bool& flag;
        } guard(m_flushing);

        while (m_destroyPending)
        {
            m_destroyPending = false;

            // 1. 把要銷毀的實體展開成子樹，標記上面所有的行為
            std::vector<EntityId> roots = std::move(m_destroyEntities);
            m_destroyEntities.clear();
            std::vector<EntityId> rootsAlive;
            for (const EntityId root : roots)
            {
                if (!m_scene.IsAlive(root))
                    continue; // 已經被另一個要求的子樹包含
                rootsAlive.push_back(root);
                std::vector<EntityId> subtree;
                CollectSubtree(root, subtree);
                for (const EntityId id : subtree)
                {
                    m_dying.insert(Key(id));
                    const auto found = m_byEntity.find(Key(id));
                    if (found == m_byEntity.end())
                        continue;
                    for (Behaviour* behaviour : found->second)
                        behaviour->m_destroyRequested = true;
                }
            }

            // 2. 呼叫 OnDisable / OnDestroy（照執行順序；OnDestroy 裡新增的行為會附加在後面，一併處理）
            SortIfNeeded();
            for (std::size_t i = 0; i < m_behaviours.size(); ++i)
            {
                Behaviour& behaviour = *m_behaviours[i];
                if (!behaviour.m_destroyRequested || behaviour.m_destroyed)
                    continue;
                behaviour.m_destroyed = true;
                if (behaviour.m_enabled)
                    behaviour.OnDisable();
                behaviour.OnDestroy();
            }

            // 3. 移除行為
            for (const auto& behaviour : m_behaviours)
            {
                if (!behaviour->m_destroyed)
                    continue;
                auto& list = m_byEntity[Key(behaviour->m_entity)];
                list.erase(std::remove(list.begin(), list.end(), behaviour.get()), list.end());
                if (list.empty())
                    m_byEntity.erase(Key(behaviour->m_entity));
            }
            m_behaviours.erase(std::remove_if(m_behaviours.begin(), m_behaviours.end(),
                                              [](const std::unique_ptr<Behaviour>& b) { return b->m_destroyed; }),
                               m_behaviours.end());

            // 4. 銷毀實體
            for (const EntityId root : rootsAlive)
                if (m_scene.IsAlive(root))
                    m_scene.DestroyEntity(root);
            m_dying.clear();
        }
    }

    Scene m_scene;
    GameTime m_time;
    const Input::State* m_input = nullptr;

    std::vector<std::unique_ptr<Behaviour>> m_behaviours;
    std::unordered_map<std::uint64_t, std::vector<Behaviour*>> m_byEntity;
    std::uint64_t m_nextSequence = 0;
    std::size_t m_activeCount = 0;
    bool m_orderDirty = false;

    std::vector<EntityId> m_destroyEntities;
    std::unordered_set<std::uint64_t> m_dying;
    bool m_destroyPending = false;

    float m_fixedAccumulator = 0.0f;
    bool m_ticking = false;
    bool m_flushing = false;
};

// ----- Behaviour 中需要 GameWorld 完整定義的成員 -----

inline void Behaviour::SetExecutionOrder(int order) noexcept
{
    m_order = order;
    m_world->m_orderDirty = true;
}

inline Scene& Behaviour::GetScene() const noexcept
{
    return m_world->GetScene();
}

inline const GameTime& Behaviour::Time() const noexcept
{
    return m_world->Time();
}

inline const Input::State& Behaviour::InputState() const
{
    return m_world->InputState();
}

inline const Transform& Behaviour::LocalTransform() const
{
    return m_world->GetScene().LocalTransform(m_entity);
}

inline void Behaviour::SetLocalTransform(const Transform& transform) const
{
    m_world->GetScene().SetLocalTransform(m_entity, transform);
}

template <typename T> T* Behaviour::GetBehaviour() const
{
    return m_world->GetBehaviour<T>(m_entity);
}

inline void Behaviour::Destroy()
{
    m_world->Destroy(*this);
}
} // namespace Skyline
