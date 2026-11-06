#include "gameplay/game_world.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace Skyline;
using Catch::Approx;

namespace
{
using Log = std::vector<std::string>;

// 把每個生命週期呼叫記錄成 "名稱.函式"
class Recorder : public Behaviour
{
  public:
    Recorder(std::string name, Log& log) : name(std::move(name)), log(&log) {}

    std::string name;
    Log* log;
    std::function<void(Recorder&)> onUpdate;
    std::function<void(Recorder&)> onStart;
    std::function<void(Recorder&)> onDestroy;

    void Awake() override { Add("Awake"); }
    void OnEnable() override { Add("OnEnable"); }
    void Start() override
    {
        Add("Start");
        if (onStart)
            onStart(*this);
    }
    void FixedUpdate(float) override { Add("FixedUpdate"); }
    void Update(float) override
    {
        Add("Update");
        if (onUpdate)
            onUpdate(*this);
    }
    void LateUpdate(float) override { Add("LateUpdate"); }
    void OnDisable() override { Add("OnDisable"); }
    void OnDestroy() override
    {
        Add("OnDestroy");
        if (onDestroy)
            onDestroy(*this);
    }

  private:
    void Add(const char* what) { log->push_back(name + "." + what); }
};

int Count(const Log& log, const std::string& entry)
{
    return static_cast<int>(std::count(log.begin(), log.end(), entry));
}
} // namespace

TEST_CASE("Lifecycle runs in Unity order", "[gameplay]")
{
    Log log; // 必須先宣告：GameWorld 解構時還會呼叫 OnDestroy 寫入 log
    GameWorld world;
    const EntityId e = world.CreateEntity("e");
    world.AddBehaviour<Recorder>(e, "a", log);

    // Awake / OnEnable 在加入時立刻呼叫
    CHECK(log == Log{"a.Awake", "a.OnEnable"});

    log.clear();
    world.Tick(0.02f); // 剛好一個固定步長
    CHECK(log == Log{"a.Start", "a.FixedUpdate", "a.Update", "a.LateUpdate"});

    log.clear();
    world.Tick(0.02f);
    CHECK(log == Log{"a.FixedUpdate", "a.Update", "a.LateUpdate"}); // Start 只有一次

    log.clear();
    world.DestroyEntity(e);
    CHECK(log == Log{"a.OnDisable", "a.OnDestroy"});
    CHECK_FALSE(world.GetScene().IsAlive(e));
    CHECK(world.BehaviourCount() == 0);
}

TEST_CASE("Fixed timestep accumulates and carries the remainder", "[gameplay]")
{
    Log log; // 必須先宣告：GameWorld 解構時還會呼叫 OnDestroy 寫入 log
    GameWorld world;
    world.AddBehaviour<Recorder>(world.CreateEntity("e"), "a", log);

    world.Tick(0.05f); // 0.05 = 2 步 + 0.01 餘數
    CHECK(Count(log, "a.FixedUpdate") == 2);
    world.Tick(0.015f); // 餘數 0.01 + 0.015 = 0.025，一步（避開剛好等於步長的浮點誤差）
    CHECK(Count(log, "a.FixedUpdate") == 3);
    CHECK(Count(log, "a.Update") == 2);
}

TEST_CASE("Delta time is clamped by maximumDeltaTime", "[gameplay]")
{
    Log log; // 必須先宣告：GameWorld 解構時還會呼叫 OnDestroy 寫入 log
    GameWorld world;
    world.AddBehaviour<Recorder>(world.CreateEntity("e"), "a", log);
    world.Time().maximumDeltaTime = 0.105f;

    world.Tick(10.0f);
    CHECK(Count(log, "a.FixedUpdate") == 5);
    CHECK(world.Time().unscaledDeltaTime == Approx(0.105f));
    CHECK(world.Time().time == Approx(0.105));
}

TEST_CASE("timeScale zero keeps Update but stops FixedUpdate", "[gameplay]")
{
    Log log; // 必須先宣告：GameWorld 解構時還會呼叫 OnDestroy 寫入 log
    GameWorld world;
    world.AddBehaviour<Recorder>(world.CreateEntity("e"), "a", log);
    world.Time().timeScale = 0.0f;

    world.Tick(0.1f);
    CHECK(Count(log, "a.Update") == 1);
    CHECK(Count(log, "a.FixedUpdate") == 0);
    CHECK(world.Time().deltaTime == 0.0f);
    CHECK(world.Time().unscaledDeltaTime == Approx(0.1f));
    CHECK(world.Time().time == Approx(0.0));
    CHECK(world.Time().unscaledTime == Approx(0.1));
}

TEST_CASE("timeScale scales delta time and total time", "[gameplay]")
{
    GameWorld world;
    world.Time().timeScale = 0.5f;
    world.Tick(0.2f);
    CHECK(world.Time().deltaTime == Approx(0.1f));
    CHECK(world.Time().time == Approx(0.1));
    CHECK(world.Time().frameCount == 1);
}

TEST_CASE("Behaviours run in ExecutionOrder, then in creation order", "[gameplay]")
{
    Log log; // 必須先宣告：GameWorld 解構時還會呼叫 OnDestroy 寫入 log
    GameWorld world;
    const EntityId e = world.CreateEntity("e");
    world.AddBehaviour<Recorder>(e, "late", log).SetExecutionOrder(10);
    world.AddBehaviour<Recorder>(e, "first", log);
    world.AddBehaviour<Recorder>(e, "second", log);
    world.AddBehaviour<Recorder>(e, "early", log).SetExecutionOrder(-5);

    log.clear();
    world.Tick(0.001f);
    CHECK(log == Log{"early.Start", "first.Start", "second.Start", "late.Start", "early.Update", "first.Update",
                     "second.Update", "late.Update", "early.LateUpdate", "first.LateUpdate", "second.LateUpdate",
                     "late.LateUpdate"});
}

TEST_CASE("Disabled behaviours skip updates and Start waits for enabling", "[gameplay]")
{
    Log log; // 必須先宣告：GameWorld 解構時還會呼叫 OnDestroy 寫入 log
    GameWorld world;
    Recorder& a = world.AddBehaviour<Recorder>(world.CreateEntity("e"), "a", log);
    a.SetEnabled(false);
    CHECK(log == Log{"a.Awake", "a.OnEnable", "a.OnDisable"});

    log.clear();
    world.Tick(0.02f);
    CHECK(log.empty());

    a.SetEnabled(true);
    world.Tick(0.02f);
    CHECK(log == Log{"a.OnEnable", "a.Start", "a.FixedUpdate", "a.Update", "a.LateUpdate"});
}

TEST_CASE("Awake is called even when the behaviour will be disabled later", "[gameplay]")
{
    Log log; // 必須先宣告：GameWorld 解構時還會呼叫 OnDestroy 寫入 log
    GameWorld world;
    Recorder& a = world.AddBehaviour<Recorder>(world.CreateEntity("e"), "a", log);
    a.SetEnabled(false);
    a.SetEnabled(false); // 重複設定不會再呼叫
    CHECK(Count(log, "a.OnDisable") == 1);
}

TEST_CASE("A behaviour added during Update joins on the next Tick", "[gameplay]")
{
    Log log; // 必須先宣告：GameWorld 解構時還會呼叫 OnDestroy 寫入 log
    GameWorld world;
    const EntityId e = world.CreateEntity("e");
    Recorder& spawner = world.AddBehaviour<Recorder>(e, "spawner", log);
    bool spawned = false;
    spawner.onUpdate = [&](Recorder& self) {
        if (spawned)
            return;
        spawned = true;
        self.World().AddBehaviour<Recorder>(self.Entity(), "child", log);
    };

    log.clear();
    world.Tick(0.001f);
    // Awake 在加入時立刻發生，但這次 Tick 不會 Start / Update
    CHECK(Count(log, "child.Awake") == 1);
    CHECK(Count(log, "child.Start") == 0);
    CHECK(Count(log, "child.Update") == 0);

    log.clear();
    world.Tick(0.001f);
    CHECK(Count(log, "child.Start") == 1);
    CHECK(Count(log, "child.Update") == 1);
}

TEST_CASE("Destroying a behaviour is deferred to the end of the Tick", "[gameplay]")
{
    Log log; // 必須先宣告：GameWorld 解構時還會呼叫 OnDestroy 寫入 log
    GameWorld world;
    const EntityId e = world.CreateEntity("e");
    Recorder& a = world.AddBehaviour<Recorder>(e, "a", log);
    world.AddBehaviour<Recorder>(e, "b", log);
    a.onUpdate = [](Recorder& self) {
        self.Destroy();
        self.Destroy(); // 重複要求沒有影響
        CHECK(self.IsDestroyed());
    };

    log.clear();
    world.Tick(0.001f);
    // a 銷毀之後，同一次 Tick 的 LateUpdate 不再呼叫它；b 照常
    CHECK(log == Log{"a.Start", "b.Start", "a.Update", "b.Update", "b.LateUpdate", "a.OnDisable", "a.OnDestroy"});
    CHECK(world.BehaviourCount() == 1);
    CHECK(world.GetBehaviour<Recorder>(e)->name == "b");
}

TEST_CASE("Destroying an entity destroys its subtree and behaviours", "[gameplay]")
{
    Log log; // 必須先宣告：GameWorld 解構時還會呼叫 OnDestroy 寫入 log
    GameWorld world;
    const EntityId parent = world.CreateEntity("parent");
    const EntityId child = world.CreateEntity("child", parent);
    const EntityId other = world.CreateEntity("other");
    world.AddBehaviour<Recorder>(parent, "p", log);
    world.AddBehaviour<Recorder>(child, "c", log);
    world.AddBehaviour<Recorder>(other, "o", log);

    log.clear();
    world.DestroyEntity(parent);
    CHECK(Count(log, "p.OnDestroy") == 1);
    CHECK(Count(log, "c.OnDestroy") == 1);
    CHECK(Count(log, "o.OnDestroy") == 0);
    CHECK_FALSE(world.GetScene().IsAlive(parent));
    CHECK_FALSE(world.GetScene().IsAlive(child));
    CHECK(world.GetScene().IsAlive(other));
    CHECK(world.BehaviourCount() == 1);
}

TEST_CASE("DestroyEntity inside Tick keeps the entity alive until the end", "[gameplay]")
{
    Log log; // 必須先宣告：GameWorld 解構時還會呼叫 OnDestroy 寫入 log
    GameWorld world;
    const EntityId e = world.CreateEntity("e");
    Recorder& a = world.AddBehaviour<Recorder>(e, "a", log);
    bool aliveDuringDestroy = false;
    a.onUpdate = [&](Recorder& self) {
        self.World().DestroyEntity(self.Entity());
        aliveDuringDestroy = self.World().GetScene().IsAlive(self.Entity());
    };
    a.onDestroy = [&](Recorder& self) {
        // OnDestroy 時實體仍然存在
        CHECK(self.World().GetScene().IsAlive(self.Entity()));
    };

    world.Tick(0.001f);
    CHECK(aliveDuringDestroy);
    CHECK_FALSE(world.GetScene().IsAlive(e));
    CHECK(Count(log, "a.LateUpdate") == 0);
}

TEST_CASE("Destroy requests made inside OnDestroy are processed too", "[gameplay]")
{
    Log log; // 必須先宣告：GameWorld 解構時還會呼叫 OnDestroy 寫入 log
    GameWorld world;
    const EntityId first = world.CreateEntity("first");
    const EntityId second = world.CreateEntity("second");
    Recorder& a = world.AddBehaviour<Recorder>(first, "a", log);
    world.AddBehaviour<Recorder>(second, "b", log);
    a.onDestroy = [&](Recorder& self) { self.World().DestroyEntity(second); };

    world.DestroyEntity(first);
    CHECK(Count(log, "b.OnDestroy") == 1);
    CHECK_FALSE(world.GetScene().IsAlive(second));
}

TEST_CASE("Adding a behaviour to a dying entity is rejected", "[gameplay]")
{
    Log log; // 必須先宣告：GameWorld 解構時還會呼叫 OnDestroy 寫入 log
    GameWorld world;
    const EntityId e = world.CreateEntity("e");
    Recorder& a = world.AddBehaviour<Recorder>(e, "a", log);
    bool threw = false;
    a.onDestroy = [&](Recorder& self) {
        try
        {
            self.World().AddBehaviour<Recorder>(e, "late", log);
        }
        catch (const std::invalid_argument&)
        {
            threw = true;
        }
    };
    world.DestroyEntity(e);
    CHECK(threw);
}

TEST_CASE("GetBehaviour finds derived types and returns null when missing", "[gameplay]")
{
    struct Base : Behaviour
    {
    };
    struct Derived : Base
    {
    };
    struct Unrelated : Behaviour
    {
    };

    GameWorld world;
    const EntityId e = world.CreateEntity("e");
    Derived& d = world.AddBehaviour<Derived>(e);
    CHECK(world.GetBehaviour<Derived>(e) == &d);
    CHECK(world.GetBehaviour<Base>(e) == &d);
    CHECK(world.GetBehaviour<Unrelated>(e) == nullptr);
    CHECK(d.GetBehaviour<Derived>() == &d);
    CHECK(world.GetBehaviour<Derived>(world.CreateEntity("none")) == nullptr);
}

TEST_CASE("Behaviours can read and write their entity transform", "[gameplay]")
{
    struct Mover : Behaviour
    {
        void Update(float dt) override
        {
            Transform t = LocalTransform();
            t.position.x += 10.0f * dt;
            SetLocalTransform(t);
        }
    };

    GameWorld world;
    const EntityId e = world.CreateEntity("e");
    world.AddBehaviour<Mover>(e);
    world.Tick(0.1f);
    world.Tick(0.1f);
    CHECK(world.GetScene().LocalTransform(e).position.x == Approx(2.0f));
}

TEST_CASE("Awake can add behaviours and an Awake exception leaves nothing behind", "[gameplay]")
{
    struct Throwing : Behaviour
    {
        void Awake() override { throw std::runtime_error("boom"); }
    };

    GameWorld world;
    const EntityId e = world.CreateEntity("e");
    CHECK_THROWS_AS(world.AddBehaviour<Throwing>(e), std::runtime_error);
    CHECK(world.BehaviourCount() == 0);
    CHECK(world.GetBehaviour<Throwing>(e) == nullptr);
}

TEST_CASE("Input state is available only when set", "[gameplay]")
{
    struct Reader : Behaviour
    {
        bool wDown = false;
        void Update(float) override { wDown = InputState().isDown(Input::Key::W); }
    };

    GameWorld world;
    Reader& reader = world.AddBehaviour<Reader>(world.CreateEntity("e"));
    CHECK_THROWS_AS(world.Tick(0.01f), std::logic_error);

    Input::State input;
    input.setKeyDown(Input::Key::W, true);
    input.update();
    world.SetInput(&input);
    world.Tick(0.01f);
    CHECK(reader.wDown);
}

TEST_CASE("Tick validates its arguments and settings", "[gameplay]")
{
    GameWorld world;
    CHECK_THROWS_AS(world.Tick(-1.0f), std::invalid_argument);
    CHECK_THROWS_AS(world.Tick(std::nanf("")), std::invalid_argument);

    world.Time().fixedDeltaTime = 0.0f;
    CHECK_THROWS_AS(world.Tick(0.1f), std::invalid_argument);
    world.Time().fixedDeltaTime = 0.02f;
    world.Time().timeScale = -1.0f;
    CHECK_THROWS_AS(world.Tick(0.1f), std::invalid_argument);
}

TEST_CASE("AddBehaviour rejects dead entities", "[gameplay]")
{
    GameWorld world;
    const EntityId e = world.CreateEntity("e");
    world.DestroyEntity(e);
    CHECK_THROWS_AS(world.AddBehaviour<Behaviour>(e), std::invalid_argument);
    CHECK_THROWS_AS(world.AddBehaviour<Behaviour>(nullEntity), std::invalid_argument);
}

TEST_CASE("Destroying an entity behind the world's back is detected", "[gameplay]")
{
    Log log; // 必須先宣告：GameWorld 解構時還會呼叫 OnDestroy 寫入 log
    GameWorld world;
    const EntityId e = world.CreateEntity("e");
    world.AddBehaviour<Recorder>(e, "a", log);
    world.GetScene().DestroyEntity(e);
    CHECK_THROWS_AS(world.Tick(0.01f), std::logic_error);
}

TEST_CASE("Tick cannot be re-entered and the world survives exceptions", "[gameplay]")
{
    Log log; // 必須先宣告：GameWorld 解構時還會呼叫 OnDestroy 寫入 log
    GameWorld world;
    Recorder& a = world.AddBehaviour<Recorder>(world.CreateEntity("e"), "a", log);

    bool reentered = false;
    a.onUpdate = [&](Recorder& self) {
        try
        {
            self.World().Tick(0.01f);
        }
        catch (const std::logic_error&)
        {
            reentered = true;
        }
    };
    world.Tick(0.01f);
    CHECK(reentered);

    a.onUpdate = [](Recorder&) { throw std::runtime_error("boom"); };
    CHECK_THROWS_AS(world.Tick(0.01f), std::runtime_error);

    // 例外之後還能繼續 Tick
    a.onUpdate = nullptr;
    CHECK_NOTHROW(world.Tick(0.01f));
}

TEST_CASE("Destroying the world calls OnDestroy on remaining behaviours", "[gameplay]")
{
    Log log;
    {
        GameWorld world;
        world.AddBehaviour<Recorder>(world.CreateEntity("e"), "a", log);
    }
    CHECK(Count(log, "a.OnDestroy") == 1);
}
