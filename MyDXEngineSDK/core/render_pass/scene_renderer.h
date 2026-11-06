#pragma once
#include "skyline_api.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

#include "math/vector.h"
#include "mesh.h"
#include "render_pass.h"
#include "rhi/binding.h"
#include "rhi/pipeline.h"
#include "scene/camera.h"
#include "scene/scene.h"

namespace Skyline
{
/// 一個表面材質：基礎色、金屬度、粗糙度（對應 assets/shaders/light.hlsl 的 PBR 參數）
struct SurfaceMaterial
{
    Vector3 baseColor{0.8f, 0.8f, 0.8f};
    float metallic = 0.0f;
    float roughness = 0.5f;
};

/// 場景的方向光（direction 是「指向光源」的方向，不必是單位向量）
struct SceneLighting
{
    Vector3 direction{0.45f, 0.72f, 0.53f};
    Vector3 color{1.0f, 0.91f, 0.78f};
    float intensity = 5.0f;
};

/// <summary>
/// 把 Scene 畫出來的渲染階段：用 Camera 做視錐體剔除，再逐一繪製每個可見的 Renderable。
/// </summary>
/// <remarks>
/// 使用方式：
///   1. init(PassInitContext)；之後 CubeMesh() / SphereMesh() / PlaneMesh() 可用
///   2. AddMaterial、必要時 AddMesh，並把回傳的 id 填進 Renderable.mesh / Renderable.material
///   3. 每個 Frame：SetScene / SetCamera，然後在 Frame 的命令中呼叫 execute(PassFrameContext)
///
/// Renderable.mesh 必須是這個 SceneRenderer 回傳的 mesh id；material 為 invalidResource 時使用預設材質。
/// 引擎每個 Frame 結束時都會等 GPU 完成，所以每個物件固定一份常數緩衝即可，不需要依 Frame 數量複製。
/// 物件使用均勻縮放時光照才正確（法線以世界矩陣轉換，沒有使用反轉置矩陣）。
/// </remarks>
class SKYLINE_API SceneRenderer final : public RenderPass
{
  public:
    /// maxObjects 是單一 Frame 最多能畫的物件數，超過時 execute 會丟出 std::runtime_error
    explicit SceneRenderer(std::uint32_t maxObjects = 1024);
    ~SceneRenderer() override;

    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;

    void init(const PassInitContext& context) override;
    void execute(const PassFrameContext& context) override;

    /// 以頂點與索引（Triangle List）建立 Mesh，回傳 mesh id。必須在 init 之後呼叫
    std::uint32_t AddMesh(std::span<const MeshVertex> vertices, std::span<const std::uint32_t> indices);
    /// 新增材質，回傳 material id
    std::uint32_t AddMaterial(const SurfaceMaterial& material);

    /// 取得材質以便修改（例如編輯器的屬性面板）；id 不存在時丟出 std::out_of_range
    [[nodiscard]] SurfaceMaterial& MaterialAt(std::uint32_t id)
    {
        return m_materials.at(id);
    }
    [[nodiscard]] std::uint32_t MeshCount() const noexcept
    {
        return static_cast<std::uint32_t>(m_meshes.size());
    }
    [[nodiscard]] std::uint32_t MaterialCount() const noexcept
    {
        return static_cast<std::uint32_t>(m_materials.size());
    }

    /// init 時建立的內建形狀（大小見 primitive_mesh.h）
    [[nodiscard]] std::uint32_t CubeMesh() const noexcept
    {
        return m_cubeMesh;
    }
    [[nodiscard]] std::uint32_t SphereMesh() const noexcept
    {
        return m_sphereMesh;
    }
    [[nodiscard]] std::uint32_t PlaneMesh() const noexcept
    {
        return m_planeMesh;
    }

    /// 要畫的場景與攝影機，呼叫端必須保證它們在 execute 時仍然存在
    void SetScene(const Scene* scene) noexcept
    {
        m_scene = scene;
    }
    void SetCamera(const Camera* camera) noexcept
    {
        m_camera = camera;
    }
    void SetLighting(const SceneLighting& lighting) noexcept
    {
        m_lighting = lighting;
    }
    /// 被強調顯示的實體（例如滑鼠指到的物件），nullEntity 代表沒有
    void SetHighlight(EntityId entity) noexcept
    {
        m_highlight = entity;
    }

    /// <summary>
    /// 只畫在畫面的一塊矩形內（Client 區像素座標），例如編輯器的 Viewport 面板。
    /// Camera 的長寬比要由呼叫端對應設定。寬或高不大於 0 時丟出 std::invalid_argument。
    /// </summary>
    void SetViewportRect(float x, float y, float width, float height)
    {
        if (!(width > 0.0f) || !(height > 0.0f))
            throw std::invalid_argument("SceneRenderer::SetViewportRect: width and height must be greater than 0.");
        m_viewportRect = std::array<float, 4>{x, y, width, height};
    }
    /// 恢復成畫滿整個畫面
    void ClearViewportRect() noexcept
    {
        m_viewportRect.reset();
    }

    /// 上一次 execute 畫了幾個物件
    [[nodiscard]] std::uint32_t LastDrawCount() const noexcept
    {
        return m_lastDrawCount;
    }

  private:
    struct DrawSlot;
    struct Constants;

    Scene const* m_scene = nullptr;
    Camera const* m_camera = nullptr;
    SceneLighting m_lighting;
    EntityId m_highlight = nullEntity;
    std::optional<std::array<float, 4>> m_viewportRect;

    std::uint32_t m_maxObjects;
    std::uint32_t m_lastDrawCount = 0;
    std::uint32_t m_cubeMesh = invalidResource;
    std::uint32_t m_sphereMesh = invalidResource;
    std::uint32_t m_planeMesh = invalidResource;

    Skyline::RHI::Device* m_device = nullptr;
    std::unique_ptr<Skyline::RHI::BindingLayout> m_bindingLayout;
    std::unique_ptr<Skyline::RHI::Pipeline> m_pipeline;
    std::vector<std::unique_ptr<Mesh>> m_meshes;
    std::vector<SurfaceMaterial> m_materials;
    std::vector<DrawSlot> m_slots;
};
} // namespace Skyline
