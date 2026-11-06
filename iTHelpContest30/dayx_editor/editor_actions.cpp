#include "editor_actions.h"

#include <Windows.h>
#include <commdlg.h>

#include <algorithm>
#include <cmath>
#include <exception>

using namespace Skyline;

std::string Utf8(const std::filesystem::path& path)
{
    const std::u8string text = path.u8string();
    return std::string(text.begin(), text.end());
}

std::filesystem::path PathFromUtf8(const std::string& text)
{
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

EntityId CreateEmptyEntity(EditorState& state, EntityId parent)
{
    const EntityId entity = state.world.CreateEntity("Empty", parent);
    state.selected = entity;
    state.log.Info("Created an empty entity.");
    return entity;
}

EntityId CreatePrimitiveEntity(EditorState& state, PrimitiveKind kind, EntityId parent)
{
    SceneRenderer& renderer = *state.renderer;
    Scene& scene = state.world.GetScene();

    const char* name = "Cube";
    std::uint32_t mesh = renderer.CubeMesh();
    Vector3 halfExtents(0.5f, 0.5f, 0.5f);
    float height = 0.5f;
    switch (kind)
    {
    case PrimitiveKind::Sphere:
        name = "Sphere";
        mesh = renderer.SphereMesh();
        break;
    case PrimitiveKind::Plane:
        name = "Plane";
        mesh = renderer.PlaneMesh();
        halfExtents = Vector3(0.5f, 0.02f, 0.5f);
        height = 0.0f;
        break;
    case PrimitiveKind::Cube:
        break;
    }

    const EntityId entity = state.world.CreateEntity(name, parent);
    Transform transform;
    transform.position = Vector3(0.0f, height, 0.0f);
    scene.SetLocalTransform(entity, transform);

    Renderable renderable;
    renderable.mesh = mesh;
    renderable.material = renderer.AddMaterial(SurfaceMaterial{});
    renderable.localBounds = AABB::FromCenterExtents(Vector3(0, 0, 0), halfExtents);
    scene.SetRenderable(entity, renderable);

    state.selected = entity;
    state.log.Info(std::string("Created a ") + name + ".");
    return entity;
}

void DeleteEntity(EditorState& state, EntityId entity)
{
    Scene& scene = state.world.GetScene();
    if (!scene.IsAlive(entity))
        return;

    // 被銷毀的是選取的實體本身或它的祖先時，清除選取
    for (EntityId walk = state.selected; !walk.IsNull() && scene.IsAlive(walk); walk = scene.Parent(walk))
    {
        if (walk == entity)
        {
            state.selected = nullEntity;
            break;
        }
    }
    const std::string name = scene.Name(entity);
    state.world.DestroyEntity(entity);
    state.log.Info("Deleted '" + name + "'.");
}

void FocusOnEntity(EditorState& state, EntityId entity)
{
    const Scene& scene = state.world.GetScene();
    if (!scene.IsAlive(entity))
        return;

    Vector3 center = scene.WorldPosition(entity);
    float radius = 1.0f;
    const AABB bounds = scene.WorldBounds(entity);
    if (bounds.IsValid())
    {
        center = bounds.Center();
        const Vector3 extents = bounds.Extents();
        radius = std::max(0.5f, std::sqrt(extents.x * extents.x + extents.y * extents.y + extents.z * extents.z));
    }

    // 沿視線往後退，退到整個包圍球放得進視野
    const float distance = std::max(radius / std::sin(state.camera.fovY * 0.5f), 2.0f);
    const Vector3 forward = state.camera.transform.Forward();
    state.camera.transform.position =
        Vector3(center.x - forward.x * distance, center.y - forward.y * distance, center.z - forward.z * distance);
}

void ReparentEntity(EditorState& state, EntityId entity, EntityId newParent)
{
    try
    {
        state.world.GetScene().SetParent(entity, newParent, true);
    }
    catch (const std::exception& error)
    {
        state.log.Error(std::string("Reparent failed: ") + error.what());
    }
}

EntityId InstantiateAsset(EditorState& state, const std::filesystem::path& file, const Vector3& position)
{
    try
    {
        const bool firstLoad = !state.assets.IsLoaded(file);
        const EntityId root = state.assets.Instantiate(state.world, file, position);
        if (firstLoad)
        {
            const LoadedModel& loaded = state.assets.Load(file);
            state.log.Info("Loaded '" + Utf8(file.filename()) + "': " + std::to_string(loaded.model.meshes.size()) +
                           " meshes, " + std::to_string(loaded.model.nodes.size()) + " nodes.");
            for (const std::string& warning : loaded.model.warnings)
                state.log.Warning(warning);
        }
        state.selected = root;
        state.log.Info("Added '" + Utf8(file.stem()) + "' to the scene.");
        return root;
    }
    catch (const std::exception& error)
    {
        state.log.Error("Failed to add '" + Utf8(file.filename()) + "': " + error.what());
        return nullEntity;
    }
}

void ImportModelFromDialog(EditorState& state)
{
    wchar_t buffer[MAX_PATH] = {};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = GetActiveWindow();
    dialog.lpstrFilter = L"3D models (*.fbx;*.obj;*.gltf;*.glb)\0*.fbx;*.obj;*.gltf;*.glb\0All files\0*.*\0";
    dialog.lpstrFile = buffer;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrTitle = L"Import a 3D model";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&dialog))
        return; // 使用者取消

    try
    {
        const std::filesystem::path source(buffer);
        std::filesystem::create_directories(state.currentFolder);
        const std::filesystem::path destination = state.currentFolder / source.filename();
        std::error_code error;
        if (!std::filesystem::equivalent(source, destination, error))
            std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing);

        const LoadedModel& loaded = state.assets.Load(destination);
        state.selectedAsset = destination;
        state.contentCache.nextRefreshTime = 0.0; // 立刻重新掃描資料夾
        state.log.Info("Imported '" + Utf8(destination.filename()) + "': " +
                       std::to_string(loaded.model.meshes.size()) + " meshes, " +
                       std::to_string(loaded.model.nodes.size()) + " nodes, " +
                       std::to_string(loaded.model.materials.size()) + " materials.");
        for (const std::string& warning : loaded.model.warnings)
            state.log.Warning(warning);
    }
    catch (const std::exception& exception)
    {
        state.log.Error(std::string("Import failed: ") + exception.what());
    }
}

Vector3 GroundPointUnderCursor(const EditorState& state, float screenX, float screenY)
{
    const PanelRect& viewport = state.viewport;
    const Ray ray = state.camera.ScreenPointToRay(screenX - viewport.x, screenY - viewport.y, viewport.width,
                                                  viewport.height);
    if (ray.direction.y < -1e-4f)
    {
        const float t = -ray.origin.y / ray.direction.y;
        if (t > 0.0f)
            return Vector3(ray.origin.x + ray.direction.x * t, 0.0f, ray.origin.z + ray.direction.z * t);
    }
    return Vector3(ray.origin.x + ray.direction.x * 8.0f, ray.origin.y + ray.direction.y * 8.0f,
                   ray.origin.z + ray.direction.z * 8.0f);
}

void UpdateSunDirection(EditorState& state)
{
    constexpr float toRadians = 3.14159265358979f / 180.0f;
    const float yaw = state.sunYawDegrees * toRadians;
    const float elevation = state.sunElevationDegrees * toRadians;
    state.lighting.direction =
        Vector3(std::cos(elevation) * std::sin(yaw), std::sin(elevation), std::cos(elevation) * std::cos(yaw));
    state.renderer->SetLighting(state.lighting);
}
