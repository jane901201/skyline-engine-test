#pragma once

#include <DirectXMath.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "camera.h"
#include "math/aabb.h"
#include "math/intersection.h"
#include "math/matrix.h"
#include "math/ray.h"
#include "transform.h"

namespace Skyline
{
/// <summary>
/// 實體識別碼：索引加上世代，實體被銷毀後舊的識別碼會失效，即使索引被重複使用也不會誤認。
/// </summary>
struct EntityId
{
    std::uint32_t index = 0xFFFFFFFFu;
    std::uint32_t generation = 0;

    [[nodiscard]] bool IsNull() const noexcept
    {
        return index == 0xFFFFFFFFu;
    }
    friend bool operator==(const EntityId&, const EntityId&) = default;
};

/// 不指向任何實體（例如「沒有父節點」）
inline constexpr EntityId nullEntity{};

/// 尚未指定的資源識別碼
inline constexpr std::uint32_t invalidResource = 0xFFFFFFFFu;

/// <summary>
/// 可繪製的元件：記錄要畫哪個資源與本地空間的包圍盒。
/// </summary>
/// <remarks>
/// mesh 與 material 是給之後的資源管理器使用的識別碼，Scene 本身不解讀它們。
/// localBounds 為空（無效）時，這個實體不會參與拾取與視錐體剔除。
/// </remarks>
struct Renderable
{
    std::uint32_t mesh = invalidResource;
    std::uint32_t material = invalidResource;
    AABB localBounds;
    bool visible = true;
};

/// <summary>
/// 射線拾取的結果。
/// </summary>
struct SceneHit
{
    EntityId entity;
    // 從射線起點到命中點的距離（世界單位）
    float distance = 0.0f;
    Vector3 point{0.0f, 0.0f, 0.0f};
    // 命中包圍盒表面的法線
    Vector3 normal{0.0f, 0.0f, 0.0f};
};

/// <summary>
/// 場景：擁有一組有父子關係的實體，每個實體有本地 Transform，可選擇性附加 Renderable。
/// </summary>
/// <remarks>
/// 世界矩陣 = 本地矩陣 * 父節點世界矩陣（row-vector 慣例），會延遲計算並快取，
/// 修改 Transform 或父子關係時只會標記受影響的子樹。
/// 對失效或不存在的 EntityId 操作，一律丟出 std::invalid_argument。
/// 空間查詢（raycast、queryVisible）目前是線性掃描，只使用包圍盒，適合數百個實體；
/// 實體更多時再加入 BVH。
/// 此類別全部是 inline，不需要匯出。
/// </remarks>
class Scene
{
  public:
    // ----- 實體生命週期 -----

    /// <summary>
    /// 建立實體，parent 為 nullEntity 時是根節點。
    /// </summary>
    EntityId CreateEntity(std::string name = {}, EntityId parent = nullEntity)
    {
        if (!parent.IsNull())
            Require(parent);

        std::uint32_t index;
        if (!m_freeSlots.empty())
        {
            index = m_freeSlots.back();
            m_freeSlots.pop_back();
        }
        else
        {
            index = static_cast<std::uint32_t>(m_nodes.size());
            m_nodes.emplace_back();
        }

        // 重複使用的槽位在銷毀時已經被重設並增加世代
        Node& node = m_nodes[index];
        node.alive = true;
        node.name = std::move(name);
        node.parent = parent;
        const EntityId id{index, node.generation};
        if (!parent.IsNull())
            m_nodes[parent.index].children.push_back(id);

        ++m_aliveCount;
        return id;
    }

    /// <summary>
    /// 銷毀實體與它所有的子孫。
    /// </summary>
    void DestroyEntity(EntityId id)
    {
        Require(id);
        const EntityId parentId = m_nodes[id.index].parent;
        if (!parentId.IsNull())
            RemoveChild(m_nodes[parentId.index], id);

        std::vector<EntityId> pending{id};
        while (!pending.empty())
        {
            const EntityId current = pending.back();
            pending.pop_back();

            Node& node = m_nodes[current.index];
            pending.insert(pending.end(), node.children.begin(), node.children.end());

            node.Release();
            m_freeSlots.push_back(current.index);
            --m_aliveCount;
        }
    }

    [[nodiscard]] bool IsAlive(EntityId id) const noexcept
    {
        return !id.IsNull() && id.index < m_nodes.size() && m_nodes[id.index].alive &&
               m_nodes[id.index].generation == id.generation;
    }

    [[nodiscard]] std::size_t EntityCount() const noexcept
    {
        return m_aliveCount;
    }

    // ----- 名稱與階層 -----

    [[nodiscard]] const std::string& Name(EntityId id) const
    {
        return Require(id).name;
    }
    void SetName(EntityId id, std::string name)
    {
        Require(id).name = std::move(name);
    }

    [[nodiscard]] EntityId Parent(EntityId id) const
    {
        return Require(id).parent;
    }
    [[nodiscard]] const std::vector<EntityId>& Children(EntityId id) const
    {
        return Require(id).children;
    }

    /// <summary>
    /// 變更父節點，parent 為 nullEntity 時變成根節點。
    /// </summary>
    /// <param name="keepWorldTransform">
    /// 為 true 時調整本地 Transform，讓實體在世界中的位置、旋轉、縮放維持不變；
    /// 為 false 時保留本地 Transform，實體會跟著新父節點移動。
    /// </param>
    /// <remarks>
    /// 新父節點是自己或自己的子孫時（會形成迴圈）丟出 std::invalid_argument。
    /// keepWorldTransform 需要反矩陣與分解矩陣，新父節點的世界縮放為 0 時丟出 std::invalid_argument。
    /// </remarks>
    void SetParent(EntityId id, EntityId parent, bool keepWorldTransform = true)
    {
        Node& node = Require(id);
        if (!parent.IsNull())
        {
            Require(parent);
            for (EntityId cursor = parent; !cursor.IsNull(); cursor = m_nodes[cursor.index].parent)
            {
                if (cursor == id)
                    throw std::invalid_argument("Scene::SetParent: The new parent would create a cycle.");
            }
        }
        if (node.parent == parent)
            return;

        const Matrix world = WorldMatrix(id);
        Matrix parentWorldInverse;
        if (keepWorldTransform && !parent.IsNull())
        {
            const Matrix parentWorld = WorldMatrix(parent);
            // 縮放為 0 的父節點無法反矩陣，保留世界 Transform 沒有意義
            if (std::fabs(DirectX::XMVectorGetX(DirectX::XMMatrixDeterminant(parentWorld))) < 1e-12f)
                throw std::invalid_argument("Scene::SetParent: The new parent's world matrix is not invertible.");
            parentWorldInverse.Inverse(parentWorld);
        }

        if (!node.parent.IsNull())
            RemoveChild(m_nodes[node.parent.index], id);
        node.parent = parent;
        if (!parent.IsNull())
            m_nodes[parent.index].children.push_back(id);

        if (keepWorldTransform)
            node.local = Transform::FromMatrix(parent.IsNull() ? world : world * parentWorldInverse);
        MarkDirty(id);
    }

    // ----- Transform -----

    [[nodiscard]] const Transform& LocalTransform(EntityId id) const
    {
        return Require(id).local;
    }
    void SetLocalTransform(EntityId id, const Transform& transform)
    {
        Require(id).local = transform;
        MarkDirty(id);
    }

    /// 世界矩陣（本地矩陣 * 父節點世界矩陣）
    [[nodiscard]] Matrix WorldMatrix(EntityId id) const
    {
        const Node& node = Require(id);
        if (node.dirty)
        {
            const Matrix local = node.local.ToMatrix();
            node.world = node.parent.IsNull() ? local : local * WorldMatrix(node.parent);
            node.dirty = false;
        }
        return node.world;
    }

    [[nodiscard]] Vector3 WorldPosition(EntityId id) const
    {
        const Matrix world = WorldMatrix(id);
        return Vector3(world._41, world._42, world._43);
    }

    // ----- Renderable -----

    void SetRenderable(EntityId id, const Renderable& renderable)
    {
        Require(id).renderable = renderable;
    }
    void ClearRenderable(EntityId id)
    {
        Require(id).renderable.reset();
    }
    [[nodiscard]] const std::optional<Renderable>& GetRenderable(EntityId id) const
    {
        return Require(id).renderable;
    }

    /// <summary>
    /// 世界空間的包圍盒，沒有 Renderable 或 localBounds 無效時回傳空的 AABB。
    /// </summary>
    [[nodiscard]] AABB WorldBounds(EntityId id) const
    {
        const Node& node = Require(id);
        if (!node.renderable || !node.renderable->localBounds.IsValid())
            return AABB();
        return node.renderable->localBounds.Transform(WorldMatrix(id));
    }

    // ----- 走訪與查詢 -----

    /// <summary>
    /// 依建立順序走訪所有存活的實體，callback 簽名為 void(EntityId)。
    /// </summary>
    /// <remarks>
    /// 走訪期間不可建立或銷毀實體。
    /// </remarks>
    template <typename Callback>
    void ForEach(Callback&& callback) const
    {
        for (std::uint32_t index = 0; index < m_nodes.size(); ++index)
        {
            const Node& node = m_nodes[index];
            if (node.alive)
                callback(EntityId{index, node.generation});
        }
    }

    /// <summary>
    /// 射線拾取：回傳最近被命中的可見實體（以世界包圍盒判斷）。
    /// </summary>
    [[nodiscard]] std::optional<SceneHit> Raycast(const Ray& ray) const
    {
        std::optional<SceneHit> best;
        ForEach([&](EntityId id) {
            const Node& node = m_nodes[id.index];
            if (!node.renderable || !node.renderable->visible)
                return;

            const AABB bounds = WorldBounds(id);
            if (!bounds.IsValid())
                return;

            RayHit hit;
            if (!::Raycast(ray, bounds, hit))
                return;
            if (!best || hit.t < best->distance)
                best = SceneHit{id, hit.t, ray.PointAt(hit.t), hit.normal};
        });
        return best;
    }

    /// <summary>
    /// 視錐體剔除：回傳世界包圍盒可能落在視錐體內的可見實體。
    /// </summary>
    /// <remarks>
    /// 這是保守的測試，可能包含少數實際上在視錐體角落外側的實體。
    /// </remarks>
    [[nodiscard]] std::vector<EntityId> QueryVisible(const Frustum& frustum) const
    {
        std::vector<EntityId> result;
        ForEach([&](EntityId id) {
            const Node& node = m_nodes[id.index];
            if (node.renderable && node.renderable->visible && frustum.Intersects(WorldBounds(id)))
                result.push_back(id);
        });
        return result;
    }

  private:
    struct Node
    {
        std::uint32_t generation = 0;
        bool alive = false;
        std::string name;
        EntityId parent;
        std::vector<EntityId> children;
        Transform local;
        std::optional<Renderable> renderable;
        // 世界矩陣的快取，延遲計算
        mutable Matrix world;
        mutable bool dirty = true;

        // 釋放節點：世代加一讓舊的識別碼失效，其餘資料重設
        void Release()
        {
            const std::uint32_t nextGeneration = generation + 1;
            *this = Node{};
            generation = nextGeneration;
        }
    };

    // 驗證識別碼是來自呼叫端的輸入，每次都要檢查
    const Node& Require(EntityId id) const
    {
        if (!IsAlive(id))
            throw std::invalid_argument("Scene: Invalid or destroyed entity id.");
        return m_nodes[id.index];
    }
    Node& Require(EntityId id)
    {
        return const_cast<Node&>(static_cast<const Scene*>(this)->Require(id));
    }

    static void RemoveChild(Node& parent, EntityId child)
    {
        for (auto it = parent.children.begin(); it != parent.children.end(); ++it)
        {
            if (*it == child)
            {
                parent.children.erase(it);
                return;
            }
        }
    }

    // 標記這個節點與所有子孫的世界矩陣需要重算。
    // 不變量：節點是 dirty 時，它的所有子孫也都是 dirty，所以遇到 dirty 節點可以直接停止。
    void MarkDirty(EntityId id)
    {
        Node& node = m_nodes[id.index];
        if (node.dirty)
            return;
        node.dirty = true;
        for (const EntityId child : node.children)
            MarkDirty(child);
    }

    std::vector<Node> m_nodes;
    std::vector<std::uint32_t> m_freeSlots;
    std::size_t m_aliveCount = 0;
};
} // namespace Skyline
