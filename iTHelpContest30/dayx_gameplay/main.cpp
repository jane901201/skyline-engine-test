// 遊戲迴圈範例：使用 Skyline 的 GameWorld / Behaviour，在主控台顯示 Unity 風格的生命週期。
// 內容：Awake -> Start -> FixedUpdate -> Update -> LateUpdate -> OnDestroy 的呼叫順序、
// 執行順序（ExecutionOrder）、Tick 期間生成與銷毀，以及用 Behaviour 驅動 Transform。

#include "gameplay/game_world.h"

#include <cstdio>
#include <exception>

using namespace Skyline;

namespace
{
// 印出每個生命週期呼叫的行為
class Tracer : public Behaviour
{
  public:
    explicit Tracer(const char* label) : label(label) {}

    void Awake() override { std::printf("    %-8s Awake\n", label); }
    void OnEnable() override { std::printf("    %-8s OnEnable\n", label); }
    void Start() override { std::printf("    %-8s Start\n", label); }
    void FixedUpdate(float) override { std::printf("    %-8s FixedUpdate\n", label); }
    void Update(float) override { std::printf("    %-8s Update\n", label); }
    void LateUpdate(float) override { std::printf("    %-8s LateUpdate\n", label); }
    void OnDisable() override { std::printf("    %-8s OnDisable\n", label); }
    void OnDestroy() override { std::printf("    %-8s OnDestroy\n", label); }

    const char* label;
};

// 每秒沿 X 軸移動，到達終點後銷毀自己
class Mover : public Behaviour
{
  public:
    float speed = 4.0f;
    float limit = 3.0f;

    void Update(float dt) override
    {
        Transform t = LocalTransform();
        t.position.x += speed * dt;
        SetLocalTransform(t);
        if (t.position.x >= limit)
        {
            std::printf("    mover reached x = %.2f, destroying itself\n", t.position.x);
            World().DestroyEntity(Entity());
        }
    }
};

// 在 LateUpdate 才跟隨目標：此時所有 Update 都已完成，目標位置是最終位置
class Follower : public Behaviour
{
  public:
    EntityId target = nullEntity;

    void LateUpdate(float) override
    {
        Transform t = LocalTransform();
        t.position.x = GetScene().WorldPosition(target).x - 1.0f;
        SetLocalTransform(t);
    }
};

// 計時生成：每隔一段時間在 World 裡建立一個 Mover
class Spawner : public Behaviour
{
  public:
    float interval = 0.5f;
    int remaining = 3;

    void Update(float dt) override
    {
        elapsed += dt;
        if (remaining > 0 && elapsed >= interval)
        {
            elapsed -= interval;
            --remaining;
            const EntityId id = World().CreateEntity("spawned");
            World().AddBehaviour<Mover>(id);
            std::printf("    spawner created an entity at t = %.2f s\n", Time().time);
        }
    }

  private:
    float elapsed = 0.0f;
};
} // namespace

int main()
{
    try
    {
        GameWorld world;

        // ----- 1. 生命週期順序 -----
        std::printf("[1] Lifecycle order (two behaviours, \"B\" has ExecutionOrder -1)\n");
        const EntityId entity = world.CreateEntity("demo");
        std::printf("  AddBehaviour:\n");
        world.AddBehaviour<Tracer>(entity, "A");
        world.AddBehaviour<Tracer>(entity, "B").SetExecutionOrder(-1);

        std::printf("  Tick 1 (0.05 s -> 2 fixed steps):\n");
        world.Tick(0.05f);
        std::printf("  Tick 2 (0.005 s -> no fixed step):\n");
        world.Tick(0.005f);
        std::printf("  DestroyEntity:\n");
        world.DestroyEntity(entity);

        // ----- 2. 用 Behaviour 驅動場景 -----
        std::printf("\n[2] Behaviours driving the scene (spawner makes 3 movers, each destroys itself at x = 3)\n");
        const EntityId spawnerEntity = world.CreateEntity("spawner");
        world.AddBehaviour<Spawner>(spawnerEntity);

        const EntityId leader = world.CreateEntity("leader");
        world.AddBehaviour<Mover>(leader).limit = 100.0f;
        const EntityId trailer = world.CreateEntity("trailer");
        Follower& follower = world.AddBehaviour<Follower>(trailer);
        follower.target = leader;

        for (int frame = 1; frame <= 40; ++frame)
        {
            world.Tick(0.05f);
            if (frame % 10 == 0)
                std::printf("  t = %.2f s: %zu entities, %zu behaviours, leader x = %.2f, trailer x = %.2f\n",
                            world.Time().time, world.GetScene().EntityCount(), world.BehaviourCount(),
                            world.GetScene().WorldPosition(leader).x, world.GetScene().WorldPosition(trailer).x);
        }

        // ----- 3. 時間縮放 -----
        std::printf("\n[3] timeScale = 0 pauses the leader but the clock keeps counting\n");
        const float before = world.GetScene().WorldPosition(leader).x;
        world.Time().timeScale = 0.0f;
        world.Tick(0.5f);
        std::printf("  leader x before %.2f, after %.2f; unscaled time %.2f s, scaled time %.2f s\n", before,
                    world.GetScene().WorldPosition(leader).x, world.Time().unscaledTime, world.Time().time);
        return 0;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }
}
