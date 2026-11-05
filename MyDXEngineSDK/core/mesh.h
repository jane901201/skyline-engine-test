#pragma once
#include "skyline_api.h"
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include <DirectXMath.h>

#include "index_buffer.h"
#include "vertex_buffer.h"
#include "rhi/device.h"
#include "rhi/command_context.h"

class RenderContext;

struct MeshVertex
{
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 normal;
    DirectX::XMFLOAT2 uv;
};

struct SMesh
{
    VertexBuffer m_vertexBuffer;
    std::vector<IndexBuffer> m_indexBuffers;
};
struct RhiMesh
{
    std::shared_ptr<Skyline::RHI::Buffer> vertices, indices;
    std::uint32_t indexCount = 0;
};

class Mesh
{
  public:
    [[deprecated("Replaced by RHI resource ownership. Use initFromFbxFile(path, device).")]]
    SKYLINE_API void initFromFbxFile(const std::string& path);
    SKYLINE_API void initFromFbxFile(const std::string& path, Skyline::RHI::Device& device);
    // 直接以程式碼產生的頂點與索引（Triangle List）建立單一 Mesh
    SKYLINE_API void initFromGeometry(std::span<const MeshVertex> vertices, std::span<const std::uint32_t> indices,
                                      Skyline::RHI::Device& device);
    // 加入一個頂點並回傳它的 index，之後可用於 addIndex / addTriangle
    SKYLINE_API std::uint32_t addVertex(const MeshVertex& vertex);
    // 加入一個索引；索引必須指向已經加入的頂點，並在 upload 時驗證
    SKYLINE_API void addIndex(std::uint32_t index);
    // 一次加入組成一個三角形的三個索引
    SKYLINE_API void addTriangle(std::uint32_t first, std::uint32_t second, std::uint32_t third);
    // 以目前累積的頂點與索引建立 GPU Buffer；可在繼續加入資料後再次呼叫以重建，
    // 呼叫端必須確保 GPU 已不再使用舊的 Buffer
    SKYLINE_API void upload(Skyline::RHI::Device& device);
    [[deprecated("Replaced by neutral commands. Use draw(RHI::CommandContext).")]]
    SKYLINE_API void draw(RenderContext& renderContext) const;
    SKYLINE_API void draw(Skyline::RHI::CommandContext& commands) const;

  private:
    std::vector<std::unique_ptr<SMesh>> m_meshes;
    std::vector<RhiMesh> m_rhiMeshes;
    // addVertex / addIndex 累積的 CPU 端資料，upload 時才會送到 GPU
    std::vector<MeshVertex> m_pendingVertices;
    std::vector<std::uint32_t> m_pendingIndices;
};
