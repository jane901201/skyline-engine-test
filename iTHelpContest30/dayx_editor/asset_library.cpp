#include "asset_library.h"

#include <stdexcept>

using namespace Skyline;

std::string AssetLibrary::Key(const std::filesystem::path& path)
{
    const std::u8string key = std::filesystem::absolute(path).lexically_normal().u8string();
    return std::string(key.begin(), key.end());
}

bool AssetLibrary::IsLoaded(const std::filesystem::path& path) const
{
    return m_models.find(Key(path)) != m_models.end();
}

const LoadedModel& AssetLibrary::Load(const std::filesystem::path& path)
{
    const std::string key = Key(path);
    const auto found = m_models.find(key);
    if (found != m_models.end())
        return found->second;

    LoadedModel loaded;
    loaded.model = ImportModelFile(path);

    const std::string stem = path.stem().string();
    for (const ImportedMesh& mesh : loaded.model.meshes)
    {
        const std::uint32_t id = m_renderer->AddMesh(mesh.geometry.vertices, mesh.geometry.indices);
        loaded.meshIds.push_back(id);
        NameMesh(id, stem + " / " + mesh.name);
    }
    for (const ImportedMaterial& material : loaded.model.materials)
    {
        SurfaceMaterial surface;
        surface.baseColor = material.baseColor;
        surface.metallic = material.metallic;
        surface.roughness = material.roughness;
        loaded.materialIds.push_back(m_renderer->AddMaterial(surface));
    }
    return m_models.emplace(key, std::move(loaded)).first->second;
}

EntityId AssetLibrary::Instantiate(GameWorld& world, const std::filesystem::path& path, const Vector3& position)
{
    const LoadedModel& loaded = Load(path);
    Scene& scene = world.GetScene();

    Transform rootTransform;
    rootTransform.position = position;
    const EntityId root = world.CreateEntity(path.stem().string());
    scene.SetLocalTransform(root, rootTransform);

    std::vector<EntityId> created;
    created.reserve(loaded.model.nodes.size());
    for (const ImportedNode& node : loaded.model.nodes)
    {
        // 父節點一定排在前面，所以 created[node.parent] 已經存在
        const EntityId parent = node.parent < 0 ? root : created[static_cast<std::size_t>(node.parent)];
        const EntityId entity = world.CreateEntity(node.name, parent);
        scene.SetLocalTransform(entity, node.local);
        created.push_back(entity);

        if (node.mesh >= 0)
        {
            const ImportedMesh& mesh = loaded.model.meshes[static_cast<std::size_t>(node.mesh)];
            Renderable renderable;
            renderable.mesh = loaded.meshIds[static_cast<std::size_t>(node.mesh)];
            renderable.material = mesh.material == ImportedModel::invalidMaterial
                                      ? invalidResource
                                      : loaded.materialIds[mesh.material];
            renderable.localBounds = mesh.bounds;
            scene.SetRenderable(entity, renderable);
        }
    }
    return root;
}

std::string AssetLibrary::MeshName(std::uint32_t meshId) const
{
    const auto found = m_meshNames.find(meshId);
    return found != m_meshNames.end() ? found->second : "Mesh #" + std::to_string(meshId);
}
