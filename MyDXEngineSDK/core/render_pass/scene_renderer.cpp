#include "scene_renderer.h"

#include <cstddef>
#include <stdexcept>
#include <string>

#include "primitive_mesh.h"

namespace Skyline
{
namespace
{
// 與 assets/shaders/light.hlsl 的 cbuffer PbrConstants 一一對應（192 位元組）
struct PbrConstants
{
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4X4 worldViewProjection;
    DirectX::XMFLOAT3 camera;
    float padding0;
    DirectX::XMFLOAT3 baseColor;
    float metallic;
    DirectX::XMFLOAT3 direction;
    float roughness;
    DirectX::XMFLOAT3 lightColor;
    float lightIntensity;
};
static_assert(sizeof(PbrConstants) == 192, "PbrConstants must match the HLSL constant-buffer layout.");

constexpr const char* kShaderPath = "assets/shaders/light.hlsl";
} // namespace

struct SceneRenderer::DrawSlot
{
    std::shared_ptr<Skyline::RHI::Buffer> buffer;
    std::unique_ptr<Skyline::RHI::BindingSet> bindings;
};

SceneRenderer::SceneRenderer(std::uint32_t maxObjects) : m_maxObjects(maxObjects)
{
    if (maxObjects == 0)
        throw std::invalid_argument("SceneRenderer: maxObjects must be greater than 0.");
}

SceneRenderer::~SceneRenderer() = default;

void SceneRenderer::init(const PassInitContext& context)
{
    using namespace Skyline::RHI;
    m_device = &context.device;

    BindingLayoutDesc layoutDesc;
    layoutDesc.constantBuffers = {{0}};
    m_bindingLayout = context.device.createBindingLayout(layoutDesc);

    auto vertexShader = context.device.createShader({kShaderPath, "VSMain", ShaderStage::Vertex});
    auto pixelShader = context.device.createShader({kShaderPath, "PSMain", ShaderStage::Pixel});
    auto desc = GraphicsPipelineDesc::fullscreen();
    desc.debugName = "SceneRenderer";
    desc.vertexShader = vertexShader.get();
    desc.pixelShader = pixelShader.get();
    desc.bindingLayout = m_bindingLayout.get();
    desc.attachments = context.attachments;
    desc.vertexInput = {{"POSITION", 0, Format::Rgb32Float},
                        {"NORMAL", 0, Format::Rgb32Float, 0, 12},
                        {"TEXCOORD", 0, Format::Rg32Float, 0, 24}};
    desc.depth.test = true;
    desc.depth.write = true;
    m_pipeline = context.device.createGraphicsPipeline(desc);

    // 每個物件一份常數緩衝與綁定，事先建好，execute 時只寫入內容
    m_slots.resize(m_maxObjects);
    const PbrConstants empty{};
    for (DrawSlot& slot : m_slots)
    {
        slot.buffer = context.device.createBuffer(
            {sizeof(PbrConstants), BufferUsage::Constant, MemoryType::Upload, ResourceState::GenericRead});
        slot.buffer->write(0, std::as_bytes(std::span{&empty, 1}));
        auto view = context.device.createBufferView(slot.buffer, {BindingType::ConstantBuffer, 0, sizeof(PbrConstants)});
        slot.bindings = context.device.createBindingSet({m_bindingLayout.get(), {{0, 0, std::move(view)}}});
    }

    // 內建形狀與預設材質（material id 0）
    AddMaterial(SurfaceMaterial{});
    const PrimitiveGeometry cube = MakeCubeGeometry();
    const PrimitiveGeometry sphere = MakeSphereGeometry();
    const PrimitiveGeometry plane = MakePlaneGeometry();
    m_cubeMesh = AddMesh(cube.vertices, cube.indices);
    m_sphereMesh = AddMesh(sphere.vertices, sphere.indices);
    m_planeMesh = AddMesh(plane.vertices, plane.indices);
}

std::uint32_t SceneRenderer::AddMesh(std::span<const MeshVertex> vertices, std::span<const std::uint32_t> indices)
{
    if (m_device == nullptr)
        throw std::logic_error("SceneRenderer::AddMesh: call init() first.");
    auto mesh = std::make_unique<Mesh>();
    mesh->initFromGeometry(vertices, indices, *m_device);
    m_meshes.push_back(std::move(mesh));
    return static_cast<std::uint32_t>(m_meshes.size() - 1);
}

std::uint32_t SceneRenderer::AddMaterial(const SurfaceMaterial& material)
{
    m_materials.push_back(material);
    return static_cast<std::uint32_t>(m_materials.size() - 1);
}

void SceneRenderer::execute(const PassFrameContext& context)
{
    if (m_scene == nullptr || m_camera == nullptr)
        throw std::logic_error("SceneRenderer::execute: SetScene and SetCamera must be called first.");

    const Matrix viewProjection = m_camera->ViewProjectionMatrix();
    const std::vector<EntityId> visible = m_scene->QueryVisible(m_camera->GetFrustum());

    context.commands.setPipeline(*m_pipeline);
    if (m_viewportRect)
    {
        const auto& [x, y, width, height] = *m_viewportRect;
        context.commands.setViewport(
            Skyline::RHI::Viewport{x, y, width, height, 0.0f, 1.0f},
            Skyline::RHI::Scissor{static_cast<std::int32_t>(x), static_cast<std::int32_t>(y),
                                  static_cast<std::int32_t>(x + width), static_cast<std::int32_t>(y + height)});
    }

    std::uint32_t drawCount = 0;
    for (const EntityId id : visible)
    {
        const Renderable& renderable = *m_scene->GetRenderable(id);
        if (renderable.mesh >= m_meshes.size())
            throw std::invalid_argument("SceneRenderer::execute: entity '" + m_scene->Name(id) +
                                        "' has no valid mesh id.");
        if (renderable.material != invalidResource && renderable.material >= m_materials.size())
            throw std::invalid_argument("SceneRenderer::execute: entity '" + m_scene->Name(id) +
                                        "' has an invalid material id.");
        if (drawCount >= m_maxObjects)
            throw std::runtime_error("SceneRenderer::execute: more than " + std::to_string(m_maxObjects) +
                                     " visible objects. Increase maxObjects.");

        const SurfaceMaterial& material =
            m_materials[renderable.material == invalidResource ? 0 : renderable.material];

        const Matrix world = m_scene->WorldMatrix(id);
        const Matrix worldViewProjection = world * viewProjection;

        PbrConstants constants{};
        constants.world = world.mat;
        constants.worldViewProjection = worldViewProjection.mat;
        constants.camera = m_camera->transform.position.vec;
        constants.baseColor = material.baseColor.vec;
        if (id == m_highlight)
        {
            // 被強調的物件往亮黃色偏移
            constexpr DirectX::XMFLOAT3 highlight{1.0f, 0.85f, 0.2f};
            constants.baseColor.x += (highlight.x - constants.baseColor.x) * 0.65f;
            constants.baseColor.y += (highlight.y - constants.baseColor.y) * 0.65f;
            constants.baseColor.z += (highlight.z - constants.baseColor.z) * 0.65f;
        }
        constants.metallic = material.metallic;
        constants.roughness = material.roughness;
        constants.direction = m_lighting.direction.vec;
        constants.lightColor = m_lighting.color.vec;
        constants.lightIntensity = m_lighting.intensity;

        DrawSlot& slot = m_slots[drawCount];
        slot.buffer->write(0, std::as_bytes(std::span{&constants, 1}));
        context.commands.setBindingSet(*slot.bindings);
        m_meshes[renderable.mesh]->draw(context.commands);
        ++drawCount;
    }
    m_lastDrawCount = drawCount;
}
} // namespace Skyline
