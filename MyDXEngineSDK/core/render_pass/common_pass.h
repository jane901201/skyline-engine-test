#pragma once

#include "constant_buffer.h"
#include "math/matrix.h"
#include "pipeline_state.h"
#include "render_pass.h"
#include "shader.h"
#include "math/vector.h"
#include "rhi_pass_resources.h"

class CommonPass final : public RenderPass
{
  public:
    [[deprecated("Replaced by RHI ownership. Use init(PassInitContext).")]]
    void init(const RenderPassInitContext& context) override;
    [[deprecated("Replaced by neutral PassInitContext. Use init(PassInitContext).")]]
    void init(const RhiRenderPassInitContext& context) override;
    void init(const PassInitContext& context) override;
    [[deprecated("Replaced by neutral commands. Use execute(PassFrameContext).")]]
    void execute(const RenderFrameContext& context) override;
    void execute(const PassFrameContext& context) override;

  private:
    struct CameraConstants
    {
        Matrix inverseView;
        Matrix inverseProjection;
        Vector3 cameraPosition;
        float padding;
    };

    static_assert(sizeof(CameraConstants) == 144,
                  "CameraConstants must match the HLSL constant-buffer layout.");

    void updateCameraConstants(UINT width, UINT height);
    [[deprecated("Legacy resource path retained after RHI migration. Use execute() with RHI initialization.")]]
    void executeLegacy(const RenderFrameContext& context);
    bool m_useRhiResources = false;
    std::unique_ptr<Skyline::RHI::Pipeline> m_rhiPipeline;
    Skyline::RhiPassResources m_rhiResources;

    Shader m_vertexShader;
    std::unique_ptr<Skyline::RHI::Shader> m_rhiVertexShader;
    std::unique_ptr<Skyline::RHI::Shader> m_rhiPixelShader;
    Shader m_pixelShader;
    PipelineState m_pipelineState;
    ConstantBuffer m_cameraConstantBuffer;
    CameraConstants m_cameraConstants{};
};
