#include <algorithm>
#include <cmath>
#include <cstring>

#include "editor_actions.h"
#include "editor_panels.h"
#include "scene/euler.h"

using namespace Skyline;

namespace
{
constexpr float kToDegrees = 180.0f / 3.14159265358979f;
constexpr float kToRadians = 3.14159265358979f / 180.0f;

bool nearlyEqual(const Quaternion& a, const Quaternion& b)
{
    return std::fabs(a.vec.x - b.vec.x) < 1e-6f && std::fabs(a.vec.y - b.vec.y) < 1e-6f &&
           std::fabs(a.vec.z - b.vec.z) < 1e-6f && std::fabs(a.vec.w - b.vec.w) < 1e-6f;
}

void drawTransform(EditorState& state, EntityId id)
{
    Scene& scene = state.world.GetScene();
    Transform transform = scene.LocalTransform(id);

    // Rotation 欄位顯示歐拉角。同一個旋轉有多組歐拉角，如果每個影格都從四元數重算，
    // 拖曳時數值會跳來跳去，所以保存使用者正在編輯的那一組，只在旋轉被外部改變時才重算
    static EntityId cachedEntity = nullEntity;
    static Quaternion cachedRotation;
    static float cachedDegrees[3] = {};
    if (!(cachedEntity == id) || !nearlyEqual(cachedRotation, transform.rotation))
    {
        const Vector3 euler = QuaternionToEuler(transform.rotation);
        cachedDegrees[0] = euler.x * kToDegrees;
        cachedDegrees[1] = euler.y * kToDegrees;
        cachedDegrees[2] = euler.z * kToDegrees;
        // 避免顯示成 "-0.0"（-0.0f == 0.0f 成立，所以這樣會變成正的 0）
        for (float& degrees : cachedDegrees)
            if (degrees == 0.0f)
                degrees = 0.0f;
        cachedEntity = id;
        cachedRotation = transform.rotation;
    }

    bool changed = false;
    float position[3] = {transform.position.x, transform.position.y, transform.position.z};
    if (ImGui::DragFloat3("Position", position, 0.05f))
    {
        transform.position = Vector3(position[0], position[1], position[2]);
        changed = true;
    }
    if (ImGui::DragFloat3("Rotation", cachedDegrees, 0.5f, 0.0f, 0.0f, "%.1f"))
    {
        transform.rotation = QuaternionFromEuler(
            Vector3(cachedDegrees[0] * kToRadians, cachedDegrees[1] * kToRadians, cachedDegrees[2] * kToRadians));
        cachedRotation = transform.rotation;
        changed = true;
    }
    float scale[3] = {transform.scale.x, transform.scale.y, transform.scale.z};
    // 縮放不能是 0 或負數：0 會讓矩陣無法求反矩陣，子節點重新指定父節點時會失敗
    if (ImGui::DragFloat3("Scale", scale, 0.01f, 0.001f, 1000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp))
    {
        transform.scale = Vector3(scale[0], scale[1], scale[2]);
        changed = true;
    }
    if (changed)
        scene.SetLocalTransform(id, transform);
}

void drawRenderable(EditorState& state, EntityId id)
{
    Scene& scene = state.world.GetScene();
    const std::optional<Renderable>& current = scene.GetRenderable(id);
    if (!current)
        return;
    Renderable renderable = *current;

    ImGui::SeparatorText("Mesh");
    ImGui::LabelText("Mesh", "%s", state.assets.MeshName(renderable.mesh).c_str());
    bool visible = renderable.visible;
    if (ImGui::Checkbox("Visible", &visible))
    {
        renderable.visible = visible;
        scene.SetRenderable(id, renderable);
    }

    const AABB bounds = scene.WorldBounds(id);
    if (bounds.IsValid())
    {
        const Vector3 size = bounds.Extents();
        ImGui::LabelText("World size", "%.2f x %.2f x %.2f", size.x * 2.0f, size.y * 2.0f, size.z * 2.0f);
    }

    ImGui::SeparatorText("Material");
    if (renderable.material == invalidResource)
    {
        ImGui::TextDisabled("Default material (shared)");
        if (ImGui::Button("Make Unique"))
        {
            renderable.material = state.renderer->AddMaterial(SurfaceMaterial{});
            scene.SetRenderable(id, renderable);
        }
        return;
    }

    SurfaceMaterial& material = state.renderer->MaterialAt(renderable.material);
    float color[3] = {material.baseColor.x, material.baseColor.y, material.baseColor.z};
    if (ImGui::ColorEdit3("Base Color", color))
        material.baseColor = Vector3(color[0], color[1], color[2]);
    ImGui::SliderFloat("Metallic", &material.metallic, 0.0f, 1.0f);
    ImGui::SliderFloat("Roughness", &material.roughness, 0.02f, 1.0f);
    ImGui::TextDisabled("Shared by every object that uses this material.");
}

void drawEntityDetails(EditorState& state, EntityId id)
{
    Scene& scene = state.world.GetScene();

    char name[256];
    std::strncpy(name, scene.Name(id).c_str(), sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
    if (ImGui::InputText("Name", name, sizeof(name)))
        scene.SetName(id, name);

    const EntityId parent = scene.Parent(id);
    ImGui::LabelText("Parent", "%s", parent.IsNull() ? "(none)" : scene.Name(parent).c_str());
    ImGui::LabelText("Children", "%zu", scene.Children(id).size());

    ImGui::SeparatorText("Transform");
    drawTransform(state, id);
    drawRenderable(state, id);
}

void drawWorldSettings(EditorState& state)
{
    ImGui::SeparatorText("Sun");
    bool changed = false;
    changed |= ImGui::SliderFloat("Yaw", &state.sunYawDegrees, -180.0f, 180.0f, "%.0f deg");
    changed |= ImGui::SliderFloat("Elevation", &state.sunElevationDegrees, 5.0f, 90.0f, "%.0f deg");
    float color[3] = {state.lighting.color.x, state.lighting.color.y, state.lighting.color.z};
    if (ImGui::ColorEdit3("Color", color))
    {
        state.lighting.color = Vector3(color[0], color[1], color[2]);
        changed = true;
    }
    changed |= ImGui::SliderFloat("Intensity", &state.lighting.intensity, 0.0f, 20.0f, "%.1f");
    if (changed)
        UpdateSunDirection(state);

    ImGui::SeparatorText("Editor Camera");
    ImGui::SliderFloat("Move Speed", &state.fly.moveSpeed, 0.5f, 40.0f, "%.1f");
    ImGui::SliderFloat("Boost (Shift)", &state.fly.boostMultiplier, 1.0f, 10.0f, "%.1f");
    float fovDegrees = DirectX::XMConvertToDegrees(state.camera.fovY);
    if (ImGui::SliderFloat("Field of View", &fovDegrees, 20.0f, 110.0f, "%.0f deg"))
        state.camera.fovY = DirectX::XMConvertToRadians(fovDegrees);
    ImGui::SliderFloat("Far Plane", &state.camera.farZ, 50.0f, 5000.0f, "%.0f");
}
} // namespace

void DrawDetailsPanel(EditorState& state, const PanelRect& rect)
{
    if (BeginPanel("Details", rect))
    {
        if (ImGui::BeginTabBar("##detailsTabs"))
        {
            if (ImGui::BeginTabItem("Details"))
            {
                if (state.world.GetScene().IsAlive(state.selected))
                    drawEntityDetails(state, state.selected);
                else
                    ImGui::TextDisabled("Select an object to view details.");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("World Settings", nullptr,
                                    state.forceWorldTab ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None))
            {
                drawWorldSettings(state);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}
