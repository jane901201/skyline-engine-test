#pragma once
#include "skyline_api.h"

#include <memory>
#include <vector>

#include "common.h"
#include "root_signature.h"
#include "rhi/binding.h"

class CloudPass;
class GraphicsEngine;
class LightPass;
class RenderPass;

class SKYLINE_API Renderer
{
  public:
    explicit Renderer(GraphicsEngine& graphicsEngine);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void init();
    void render();
    void clearRenderItems();

    void setCloudSettings(const CloudSettings& settings);
    const CloudSettings& getCloudSettings() const;

    void setCloudCount(std::uint32_t cloudCount);
    std::uint32_t getCloudCount() const;
    void setCloudBottomHeight(float bottomHeight);
    float getCloudBottomHeight() const;
    void setCloudTopHeight(float topHeight);
    float getCloudTopHeight() const;
    void setCloudCellSize(float cellSize);
    float getCloudCellSize() const;
    void setCloudDensityScale(float densityScale);
    float getCloudDensityScale() const;
    void setCloudSpeed(float speed);
    float getCloudSpeed() const;
    void setCloudRayMarchSteps(std::uint32_t rayMarchSteps);
    std::uint32_t getCloudRayMarchSteps() const;
    void setCloudExtinction(float extinction);
    float getCloudExtinction() const;
    void setCloudColor(const Vector3& color);
    const Vector3& getCloudColor() const;
    void setCloudCoverage(float coverage);
    float getCloudCoverage() const;
    void setCloudDensityMultiplier(float densityMultiplier);
    float getCloudDensityMultiplier() const;
    void setCloudBaseNoiseScale(float baseNoiseScale);
    float getCloudBaseNoiseScale() const;
    void setCloudDetailNoiseScale(float detailNoiseScale);
    float getCloudDetailNoiseScale() const;
    void setCloudDetailErosion(float detailErosion);
    float getCloudDetailErosion() const;
    void setCloudNoiseIterations(std::uint32_t noiseIterations);
    std::uint32_t getCloudNoiseIterations() const;
    void setCloudLightMarchSteps(std::uint32_t lightMarchSteps);
    std::uint32_t getCloudLightMarchSteps() const;

  private:
    GraphicsEngine& m_graphicsEngine;
    std::unique_ptr<Skyline::RHI::BindingLayout> m_bindingLayout;
    std::vector<std::unique_ptr<RenderPass>> m_passes;
    CloudSettings m_cloudSettings{};
    CloudPass* m_cloudPass = nullptr;
    LightPass* m_lightPass = nullptr;
    bool m_initialized = false;
};
