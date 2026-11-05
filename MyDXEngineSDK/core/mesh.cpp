#include "mesh.h"

#include <cstdint>
#include <limits>
#include <stdexcept>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "render_context.h"

namespace
{
// 檢查 Buffer 元素數量是否能安全轉換成 int
int checkedBufferCount(std::size_t count, const char* bufferName)
{
    // 確認元素數量沒有超過 int 可表示的最大值
    if (count > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        // 超過支援範圍時拋出例外並指出是哪一種 Buffer
        throw std::runtime_error(std::string("Mesh: ") + bufferName + " exceeds the supported size.");
    }

    // 將確認安全的元素數量轉換成 int
    return static_cast<int>(count);
}
} // namespace

// 從指定的 FBX 檔案載入 Mesh 資料並建立 GPU Buffer
namespace
{
struct Geometry
{
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
};
std::vector<Geometry> loadGeometry(const std::string& path)
{
    if (path.empty()) throw std::invalid_argument("Mesh: Path must not be empty.");
    Assimp::Importer importer;
    const auto* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_ConvertToLeftHanded |
        aiProcess_GenNormals | aiProcess_JoinIdenticalVertices);
    if (!scene) throw std::runtime_error("Mesh: Failed to load '" + path + "': " + importer.GetErrorString());
    if (!scene->HasMeshes()) throw std::runtime_error("Mesh: File contains no meshes: " + path);
    std::vector<Geometry> result;
    for (unsigned meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
    {
        const auto& mesh = *scene->mMeshes[meshIndex];
        if (!mesh.HasPositions() || !mesh.HasNormals())
            throw std::runtime_error("Mesh: Position and normal data are required.");
        Geometry geometry;
        for (unsigned index = 0; index < mesh.mNumVertices; ++index)
        {
            const auto& position = mesh.mVertices[index];
            const auto& normal = mesh.mNormals[index];
            const auto uv = mesh.HasTextureCoords(0) ? mesh.mTextureCoords[0][index] : aiVector3D{};
            geometry.vertices.push_back({{position.x, position.y, position.z}, {normal.x, normal.y, normal.z}, {uv.x, uv.y}});
        }
        for (unsigned index = 0; index < mesh.mNumFaces; ++index)
        {
            const auto& face = mesh.mFaces[index];
            if (face.mNumIndices != 3) throw std::runtime_error("Mesh: Non-triangle face after triangulation.");
            for (unsigned corner = 0; corner < 3; ++corner)
            {
                if (face.mIndices[corner] >= mesh.mNumVertices) throw std::runtime_error("Mesh: Index exceeds vertex range.");
                geometry.indices.push_back(face.mIndices[corner]);
            }
        }
        if (geometry.vertices.empty() || geometry.indices.empty() || geometry.indices.size() > (std::numeric_limits<std::uint32_t>::max)())
            throw std::runtime_error("Mesh: Geometry is empty or exceeds the supported size.");
        result.push_back(std::move(geometry));
    }
    return result;
}

// 將已驗證的幾何資料上傳到 GPU，FBX 載入與程式碼建構共用
std::vector<RhiMesh> uploadGeometry(Skyline::RHI::Device& device, const std::vector<Geometry>& geometry)
{
    using namespace Skyline::RHI;
    auto commands = device.createCommandContext();
    commands->begin();
    std::vector<RhiMesh> meshes;
    for (const auto& source : geometry)
    {
        const auto vertices = std::as_bytes(std::span{source.vertices});
        const auto indices = std::as_bytes(std::span{source.indices});
        RhiMesh mesh;
        mesh.vertices = device.createBuffer({vertices.size(), BufferUsage::Vertex | BufferUsage::CopyDestination});
        mesh.indices = device.createBuffer({indices.size(), BufferUsage::Index | BufferUsage::CopyDestination});
        commands->uploadBuffer(*mesh.vertices, 0, vertices, ResourceState::Vertex);
        commands->uploadBuffer(*mesh.indices, 0, indices, ResourceState::Index);
        mesh.indexCount = static_cast<std::uint32_t>(source.indices.size());
        meshes.push_back(std::move(mesh));
    }
    commands->submitAndWait();
    return meshes;
}
}
void Mesh::initFromFbxFile(const std::string& path)
{
    auto geometry = loadGeometry(path);
    std::vector<std::unique_ptr<SMesh>> meshes;
    for (const auto& source : geometry)
    {
        auto mesh = std::make_unique<SMesh>();
        mesh->m_vertexBuffer.init(checkedBufferCount(source.vertices.size(), "vertex count"), sizeof(MeshVertex));
        mesh->m_vertexBuffer.copy(const_cast<MeshVertex*>(source.vertices.data()));
        mesh->m_indexBuffers.emplace_back();
        auto& indices = mesh->m_indexBuffers.back();
        indices.init(checkedBufferCount(source.indices.size(), "index count"), sizeof(std::uint32_t));
        indices.copy(const_cast<std::uint32_t*>(source.indices.data()));
        meshes.push_back(std::move(mesh));
    }
    m_meshes = std::move(meshes);
}

void Mesh::initFromFbxFile(const std::string& path, Skyline::RHI::Device& device)
{
    m_rhiMeshes = uploadGeometry(device, loadGeometry(path));
}

void Mesh::initFromGeometry(std::span<const MeshVertex> vertices, std::span<const std::uint32_t> indices,
                            Skyline::RHI::Device& device)
{
    if (vertices.empty() || indices.empty() || indices.size() % 3 != 0)
        throw std::invalid_argument("Mesh: Geometry requires vertices and a triangle-list index count.");
    for (const std::uint32_t index : indices)
        if (index >= vertices.size())
            throw std::out_of_range("Mesh: Index exceeds vertex range.");

    Geometry geometry;
    geometry.vertices.assign(vertices.begin(), vertices.end());
    geometry.indices.assign(indices.begin(), indices.end());

    std::vector<Geometry> geometries;
    geometries.push_back(std::move(geometry));
    m_rhiMeshes = uploadGeometry(device, geometries);
}
std::uint32_t Mesh::addVertex(const MeshVertex& vertex)
{
    if (m_pendingVertices.size() >= (std::numeric_limits<std::uint32_t>::max)())
        throw std::length_error("Mesh: Vertex count exceeds the supported size.");
    m_pendingVertices.push_back(vertex);
    return static_cast<std::uint32_t>(m_pendingVertices.size() - 1);
}

void Mesh::addIndex(std::uint32_t index)
{
    m_pendingIndices.push_back(index);
}

void Mesh::addTriangle(std::uint32_t first, std::uint32_t second, std::uint32_t third)
{
    addIndex(first);
    addIndex(second);
    addIndex(third);
}

void Mesh::upload(Skyline::RHI::Device& device)
{
    // 驗證（空資料、索引數量、索引範圍）集中在 initFromGeometry
    initFromGeometry(m_pendingVertices, m_pendingIndices, device);
}

void Mesh::draw(RenderContext& renderContext) const
{
    // 逐一處理所有子 Mesh
    for (const auto& mesh : m_meshes)
    {
        // 將目前 Mesh 的 Vertex Buffer 綁定到 Render Pipeline
        renderContext.setVertexBuffer(mesh->m_vertexBuffer);

        // 逐一處理目前 Mesh 的所有 Index Buffer
        for (const IndexBuffer& indexBuffer : mesh->m_indexBuffers)
        {
            // 綁定目前的 Index Buffer
            renderContext.setIndexBuffer(indexBuffer);

            // 根據 Index 數量送出 Indexed Draw Call
            renderContext.drawIndexed(indexBuffer.getCount());
        }
    }
}
void Mesh::draw(Skyline::RHI::CommandContext& commands) const
{
    if (m_rhiMeshes.empty()) throw std::logic_error("Mesh: Initialize with an RHI device before drawing.");
    for (const auto& mesh : m_rhiMeshes)
    {
        commands.setVertexBuffer(*mesh.vertices, sizeof(MeshVertex));
        commands.setIndexBuffer(*mesh.indices, Skyline::RHI::IndexType::Uint32);
        commands.drawIndexed(mesh.indexCount);
    }
}
