#pragma once

#include <chrono>

#include "constant_buffer.h"
#include "common.h"
#include "math/matrix.h"
#include "pipeline_state.h"
#include "render_pass.h"
#include "render_state.h"
#include "rw_structured_buffer.h"
#include "shader.h"
#include "math/vector.h"
#include "rhi_pass_resources.h"

class CloudPass final : public RenderPass
{
  public:
    explicit CloudPass(const CloudSettings& settings);

    [[deprecated("Replaced by RHI ownership. Use init(PassInitContext).")]]
    void init(const RenderPassInitContext& context) override;
    [[deprecated("Replaced by neutral PassInitContext. Use init(PassInitContext).")]]
    void init(const RhiRenderPassInitContext& context) override;
    void init(const PassInitContext& context) override;
    [[deprecated("Replaced by neutral commands. Use execute(PassFrameContext).")]]
    void execute(const RenderFrameContext& context) override;
    void execute(const PassFrameContext& context) override;
    void setSettings(const CloudSettings& settings);

  private:
    struct CloudData
    {
        Vector3 position;
        float density;
    };

    struct CloudConstants
    {
        UINT cloudCount;
        float cloudBottomHeight;
        float cloudTopHeight;
        float cloudCellSize;
        float time;
        float densityScale;
        float cloudSpeed;
        float extinction;
        UINT rayMarchSteps;
        Vector3 color;
        float coverage;
        float densityMultiplier;
        float baseNoiseScale;
        float detailNoiseScale;
        float detailErosion;
        float bottomFadeDistance;
        float topFadeDistance;
        float lightAbsorption;
        UINT noiseIterations;
        UINT lightMarchSteps;
        float lightStepSize;
        float padding;
        Vector3 sunDirection;
        float sunIntensity;
        Vector3 sunColor;
        float sunPadding;
    };

    struct CameraConstants
    {
        Matrix inverseView;
        Matrix inverseProjection;
        Vector3 cameraPosition;
        float padding;
    };

    static_assert(sizeof(CloudData) == 16, "CloudData must match the HLSL structured-buffer layout.");
    static_assert(sizeof(CloudConstants) == 128, "CloudConstants must match the HLSL constant-buffer layout.");
    static_assert(sizeof(CameraConstants) == 144, "CameraConstants must match the HLSL constant-buffer layout.");

    void updateCameraConstants(UINT width, UINT height);
    [[deprecated("Legacy resource path retained after RHI migration. Use execute() with RHI initialization.")]]
    void executeLegacy(const RenderFrameContext& context);
    bool m_useRhiResources = false;
    std::unique_ptr<Skyline::RHI::Pipeline> m_rhiComputePipeline;
    std::unique_ptr<Skyline::RHI::Pipeline> m_rhiGraphicsPipeline;
    std::array<std::shared_ptr<Skyline::RHI::Buffer>, Skyline::RhiPassResources::frameCount> m_rhiCameraBuffers;
    std::array<std::shared_ptr<Skyline::RHI::Buffer>, Skyline::RhiPassResources::frameCount> m_rhiCloudConstants;
    std::array<std::shared_ptr<Skyline::RHI::Buffer>, Skyline::RhiPassResources::frameCount> m_rhiCloudBuffers;
    std::array<std::unique_ptr<Skyline::RHI::BindingSet>, Skyline::RhiPassResources::frameCount> m_rhiBindings;

    Shader m_computeShader;
    std::unique_ptr<Skyline::RHI::Shader> m_rhiComputeShader;
    std::unique_ptr<Skyline::RHI::Shader> m_rhiVertexShader;
    std::unique_ptr<Skyline::RHI::Shader> m_rhiPixelShader;
    Shader m_vertexShader;
    Shader m_pixelShader;
    PipelineState m_computePipelineState;
    PipelineState m_graphicsPipelineState;
    ConstantBuffer m_cameraConstantBuffer;
    ConstantBuffer m_cloudConstantBuffer;
    RWStructuredBuffer m_cloudBuffer;
    RenderState m_renderState;
    ID3D12RootSignature* m_rootSignature = nullptr;
    CloudSettings m_settings;
    CloudConstants m_cloudConstants{};
    CameraConstants m_cameraConstants{};
    std::chrono::steady_clock::time_point m_startTime{};
};
