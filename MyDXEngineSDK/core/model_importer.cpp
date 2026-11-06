#include "model_importer.h"

#include <assimp/Importer.hpp>
#include <assimp/config.h>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace Skyline
{
namespace
{
constexpr std::uint32_t kInvalidMaterial = ImportedModel::invalidMaterial;

struct Context
{
    const aiScene* scene = nullptr;
    ImportedModel* model = nullptr;
    // Assimp 的網格索引 -> ImportedModel::meshes 的索引，被略過的網格為 -1
    std::vector<int> meshMap;
};

// Assimp 的 aiMatrix4x4 是 column-vector 慣例（平移在 a4、b4、c4），
// 本引擎是 DirectXMath 的 row-vector 慣例，所以要轉置
Transform toTransform(const aiMatrix4x4& m, const std::string& nodeName, std::vector<std::string>& warnings)
{
    const DirectX::XMFLOAT4X4 converted(m.a1, m.b1, m.c1, m.d1, //
                                        m.a2, m.b2, m.c2, m.d2, //
                                        m.a3, m.b3, m.c3, m.d3, //
                                        m.a4, m.b4, m.c4, m.d4);
    try
    {
        return Transform::FromMatrix(Matrix(converted));
    }
    catch (const std::invalid_argument&)
    {
        warnings.push_back("Node '" + nodeName + "': the transform cannot be decomposed. Identity was used.");
        return Transform();
    }
}

std::string nameOr(const aiString& name, const std::string& fallback)
{
    return name.length > 0 ? std::string(name.C_Str()) : fallback;
}

void visitNode(const aiNode& node, int parent, Context& context)
{
    ImportedModel& model = *context.model;
    const int index = static_cast<int>(model.nodes.size());
    const std::string name = nameOr(node.mName, "Node " + std::to_string(index));

    ImportedNode imported;
    imported.name = name;
    imported.parent = parent;
    imported.local = toTransform(node.mTransformation, name, model.warnings);
    model.nodes.push_back(imported);

    // 只保留有成功匯入的網格
    std::vector<int> meshes;
    for (unsigned i = 0; i < node.mNumMeshes; ++i)
        if (context.meshMap[node.mMeshes[i]] >= 0)
            meshes.push_back(context.meshMap[node.mMeshes[i]]);

    if (meshes.size() == 1)
    {
        model.nodes[static_cast<std::size_t>(index)].mesh = meshes.front();
    }
    else
    {
        for (const int mesh : meshes)
        {
            ImportedNode child;
            child.name = model.meshes[static_cast<std::size_t>(mesh)].name;
            child.parent = index;
            child.mesh = mesh;
            model.nodes.push_back(child);
        }
    }

    for (unsigned i = 0; i < node.mNumChildren; ++i)
        visitNode(*node.mChildren[i], index, context);
}
} // namespace

ImportedModel ImportModelFile(const std::filesystem::path& path)
{
    if (path.empty())
        throw std::invalid_argument("ImportModelFile: path must not be empty.");

    Assimp::Importer importer;
    // 點與線不能畫成實心網格，直接移除
    importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);
    const aiScene* scene =
        importer.ReadFile(path.string(), aiProcess_Triangulate | aiProcess_ConvertToLeftHanded |
                                             aiProcess_GenNormals | aiProcess_JoinIdenticalVertices |
                                             aiProcess_SortByPType);
    if (scene == nullptr || scene->mRootNode == nullptr)
        throw std::runtime_error("ImportModelFile: failed to load '" + path.string() + "': " + importer.GetErrorString());

    ImportedModel model;

    // ----- 材質 -----
    for (unsigned i = 0; i < scene->mNumMaterials; ++i)
    {
        const aiMaterial& source = *scene->mMaterials[i];
        ImportedMaterial material;
        aiString name;
        if (source.Get(AI_MATKEY_NAME, name) == AI_SUCCESS)
            material.name = name.C_Str();
        aiColor3D color;
        if (source.Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
            material.baseColor = Vector3(color.r, color.g, color.b);
        float value = 0.0f;
        if (source.Get(AI_MATKEY_METALLIC_FACTOR, value) == AI_SUCCESS)
            material.metallic = value;
        if (source.Get(AI_MATKEY_ROUGHNESS_FACTOR, value) == AI_SUCCESS)
            material.roughness = value;
        model.materials.push_back(material);
    }

    // ----- 網格 -----
    Context context;
    context.scene = scene;
    context.model = &model;
    context.meshMap.assign(scene->mNumMeshes, -1);

    for (unsigned meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
    {
        const aiMesh& source = *scene->mMeshes[meshIndex];
        const std::string meshName = nameOr(source.mName, "Mesh " + std::to_string(meshIndex));
        if (!source.HasPositions() || source.mNumFaces == 0)
        {
            model.warnings.push_back("Mesh '" + meshName + "' has no triangles and was skipped.");
            continue;
        }
        if (!source.HasNormals())
            throw std::runtime_error("ImportModelFile: mesh '" + meshName + "' has no normals.");

        ImportedMesh mesh;
        mesh.name = meshName;
        mesh.material = source.mMaterialIndex < model.materials.size() ? source.mMaterialIndex : kInvalidMaterial;
        mesh.geometry.vertices.reserve(source.mNumVertices);
        for (unsigned i = 0; i < source.mNumVertices; ++i)
        {
            const aiVector3D& position = source.mVertices[i];
            const aiVector3D& normal = source.mNormals[i];
            const aiVector3D uv = source.HasTextureCoords(0) ? source.mTextureCoords[0][i] : aiVector3D();
            mesh.geometry.vertices.push_back({{position.x, position.y, position.z}, {normal.x, normal.y, normal.z}, {uv.x, uv.y}});
            mesh.bounds.Encapsulate(Vector3(position.x, position.y, position.z));
        }
        for (unsigned i = 0; i < source.mNumFaces; ++i)
        {
            const aiFace& face = source.mFaces[i];
            if (face.mNumIndices != 3)
                continue; // SortByPType 之後應該都是三角形，保險起見略過其他
            for (unsigned corner = 0; corner < 3; ++corner)
            {
                if (face.mIndices[corner] >= source.mNumVertices)
                    throw std::runtime_error("ImportModelFile: mesh '" + meshName + "' has an index out of range.");
                mesh.geometry.indices.push_back(face.mIndices[corner]);
            }
        }
        if (mesh.geometry.indices.empty())
        {
            model.warnings.push_back("Mesh '" + meshName + "' has no triangles and was skipped.");
            continue;
        }
        context.meshMap[meshIndex] = static_cast<int>(model.meshes.size());
        model.meshes.push_back(std::move(mesh));
    }
    if (model.meshes.empty())
        throw std::runtime_error("ImportModelFile: '" + path.string() + "' contains no triangle meshes.");

    // ----- 節點階層 -----
    visitNode(*scene->mRootNode, -1, context);

    // FBX 的 UnitScaleFactor 是「每個單位幾公分」（1 代表公分，節點上常帶著 100 倍的縮放來補償）。
    // 把整個場景縮放到公尺，匯入後 1 單位 = 1 公尺。縮放套用在根節點，其下的階層都會跟著縮放
    // Assimp 把它存成 float，但保險起見也接受 double
    double centimetersPerUnit = 0.0;
    if (scene->mMetaData != nullptr)
    {
        float asFloat = 0.0f;
        double asDouble = 0.0;
        if (scene->mMetaData->Get("UnitScaleFactor", asFloat))
            centimetersPerUnit = asFloat;
        else if (scene->mMetaData->Get("UnitScaleFactor", asDouble))
            centimetersPerUnit = asDouble;
    }
    if (centimetersPerUnit > 0.0)
    {
        const float metersPerUnit = static_cast<float>(centimetersPerUnit * 0.01);
        Transform& root = model.nodes.front().local;
        root.position = Vector3(root.position.x * metersPerUnit, root.position.y * metersPerUnit,
                                root.position.z * metersPerUnit);
        root.scale = Vector3(root.scale.x * metersPerUnit, root.scale.y * metersPerUnit, root.scale.z * metersPerUnit);
    }
    return model;
}
} // namespace Skyline
