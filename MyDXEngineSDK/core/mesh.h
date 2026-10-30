#pragma once
#include "skyline_api.h"
#include <memory>
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
    [[deprecated("Replaced by neutral commands. Use draw(RHI::CommandContext).")]]
    SKYLINE_API void draw(RenderContext& renderContext) const;
    SKYLINE_API void draw(Skyline::RHI::CommandContext& commands) const;

  private:
    std::vector<std::unique_ptr<SMesh>> m_meshes;
    std::vector<RhiMesh> m_rhiMeshes;
};
