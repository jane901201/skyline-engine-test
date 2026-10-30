#include "renderer.h"

#include <cmath>
#include <stdexcept>

#include "cloud_pass.h"
#include "common_pass.h"
#include "graphics_engine.h"
#include "light_pass.h"
#include "render_context.h"
#include "render_pass.h"

#ifdef SKYLINE_ENABLE_TRACY
#include <tracy/Tracy.hpp>
#endif

namespace
{
// 驗證雲層設定是否符合 Renderer 所要求的有效範圍
void validateCloudSettings(const CloudSettings& settings)
{
    // 雲朵數量不可為零
    if (settings.cloudCount == 0)
        // 參數無效時直接拋出例外
        throw std::invalid_argument("Renderer: Cloud count must be greater than zero.");

    // 高度必須是有限值，且雲層底部必須低於頂部
    if (!std::isfinite(settings.bottomHeight) || !std::isfinite(settings.topHeight) ||
        settings.bottomHeight >= settings.topHeight)
        throw std::invalid_argument("Renderer: Cloud bottom height must be lower than the top height.");

    // 每個雲層 Cell 的尺寸必須大於零且不可為 NaN 或 Infinity
    if (!std::isfinite(settings.cellSize) || settings.cellSize <= 0.0f)
        throw std::invalid_argument("Renderer: Cloud cell size must be finite and greater than zero.");

    // 密度縮放值不可為負數
    if (!std::isfinite(settings.densityScale) || settings.densityScale < 0.0f)
        throw std::invalid_argument("Renderer: Cloud density scale must be finite and non-negative.");

    // 移動速度必須是有限數值
    if (!std::isfinite(settings.speed))
        throw std::invalid_argument("Renderer: Cloud speed must be finite.");

    // Ray March 至少需要執行一次取樣
    if (settings.rayMarchSteps == 0)
        throw std::invalid_argument("Renderer: Cloud ray-march steps must be greater than zero.");

    // 消光係數不可為負數
    if (!std::isfinite(settings.extinction) || settings.extinction < 0.0f)
        throw std::invalid_argument("Renderer: Cloud extinction must be finite and non-negative.");

    // 雲朵 RGB 色彩分量必須是有限且非負數值
    if (!std::isfinite(settings.color.x) || !std::isfinite(settings.color.y) || !std::isfinite(settings.color.z) ||
        settings.color.x < 0.0f || settings.color.y < 0.0f || settings.color.z < 0.0f)
        throw std::invalid_argument("Renderer: Cloud color components must be finite and non-negative.");

    // Coverage 使用 0 到 1 表示雲層覆蓋程度
    if (!std::isfinite(settings.coverage) || settings.coverage < 0.0f || settings.coverage > 1.0f)
        throw std::invalid_argument("Renderer: Cloud coverage must be between zero and one.");

    // 額外的雲密度倍率不可為負數
    if (!std::isfinite(settings.densityMultiplier) || settings.densityMultiplier < 0.0f)
        throw std::invalid_argument("Renderer: Cloud density multiplier must be finite and non-negative.");

    // Base Noise 與 Detail Noise 的取樣尺度都必須大於零
    if (!std::isfinite(settings.baseNoiseScale) || settings.baseNoiseScale <= 0.0f ||
        !std::isfinite(settings.detailNoiseScale) || settings.detailNoiseScale <= 0.0f)
        throw std::invalid_argument("Renderer: Cloud noise scales must be finite and greater than zero.");

    // Detail Noise 的侵蝕強度不可為負數
    if (!std::isfinite(settings.detailErosion) || settings.detailErosion < 0.0f)
        throw std::invalid_argument("Renderer: Cloud detail erosion must be finite and non-negative.");

    // 雲層上下邊界的 Fade 距離皆必須大於零
    if (!std::isfinite(settings.bottomFadeDistance) || settings.bottomFadeDistance <= 0.0f ||
        !std::isfinite(settings.topFadeDistance) || settings.topFadeDistance <= 0.0f)
        throw std::invalid_argument("Renderer: Cloud fade distances must be finite and greater than zero.");

    // 光線在雲層中的吸收係數不可為負數
    if (!std::isfinite(settings.lightAbsorption) || settings.lightAbsorption < 0.0f)
        throw std::invalid_argument("Renderer: Cloud light absorption must be finite and non-negative.");

    // Noise 疊加次數限制在 1 到 8 次之間
    if (settings.noiseIterations == 0 || settings.noiseIterations > 8)
        throw std::invalid_argument("Renderer: Cloud noise iterations must be between one and eight.");

    // 光照 Ray March 次數限制在 1 到 16 次之間
    if (settings.lightMarchSteps == 0 || settings.lightMarchSteps > 16)
        throw std::invalid_argument("Renderer: Cloud light-march steps must be between one and sixteen.");

    // 每次光照 Ray March 的步進距離必須大於零
    if (!std::isfinite(settings.lightStepSize) || settings.lightStepSize <= 0.0f)
        throw std::invalid_argument("Renderer: Cloud light step size must be finite and greater than zero.");

    // 太陽方向向量各分量必須有效，且向量長度不可為零
    if (!std::isfinite(settings.sunDirection.x) || !std::isfinite(settings.sunDirection.y) ||
        !std::isfinite(settings.sunDirection.z) || settings.sunDirection.LengthSq() <= 0.0f)
        throw std::invalid_argument("Renderer: Cloud sun direction must be finite and non-zero.");

    // 太陽光強度與 RGB 色彩皆必須是有限且非負數值
    if (!std::isfinite(settings.sunIntensity) || settings.sunIntensity < 0.0f || !std::isfinite(settings.sunColor.x) ||
        !std::isfinite(settings.sunColor.y) || !std::isfinite(settings.sunColor.z) || settings.sunColor.x < 0.0f ||
        settings.sunColor.y < 0.0f || settings.sunColor.z < 0.0f)
        throw std::invalid_argument("Renderer: Cloud sun lighting values must be finite and non-negative.");
}
} // namespace

// 保存 GraphicsEngine 的參考供 Renderer 後續使用
Renderer::Renderer(GraphicsEngine& graphics_engine) : m_graphicsEngine(graphics_engine)
{
    // Renderer 建立前必須確認 D3D12 Device 已完成初始化
    (void)m_graphicsEngine.getRhiDevice();
}

// 使用編譯器產生的預設解構行為
Renderer::~Renderer() = default;

// 初始化 Renderer 所需的 Root Signature 與各個 Render Pass
void Renderer::init()
{
    // 防止同一個 Renderer 被重複初始化
    if (m_initialized)
        throw std::logic_error("Renderer: Renderer is already initialized.");

    // 在建立 GPU 資源前先確認目前雲層設定合法
    validateCloudSettings(m_cloudSettings);

    auto& device = m_graphicsEngine.getRhiDevice();
    m_bindingLayout = device.createBindingLayout(Skyline::RHI::BindingLayoutDesc::scene());

    // 明確提供裝置與場景配置，舊初始化情境仍保留供相容範例使用。
    PassInitContext initContext{device, m_graphicsEngine.getBackBufferLayout(), m_graphicsEngine.getFrameBufferWidth(), m_graphicsEngine.GetFrameBufferHeight()};

    // 建立處理共用繪製工作的 Common Pass
    auto commonPass = std::make_unique<CommonPass>();

    // 初始化 Common Pass 所需的 GPU 資源
    commonPass->init(initContext);

    // 將 Common Pass 所有權移交給 Renderer 統一管理
    m_passes.push_back(std::move(commonPass));

    // 建立負責雲層光照資料的 Light Pass
    auto lightPass = std::make_unique<LightPass>(m_cloudSettings);

    // 保存非擁有型指標，供執行期間更新 Light Pass 設定
    m_lightPass = lightPass.get();

    // 初始化 Light Pass
    lightPass->init(initContext);

    // 將 Light Pass 加入執行佇列
    m_passes.push_back(std::move(lightPass));

    // 建立負責實際繪製體積雲的 Cloud Pass
    auto cloudPass = std::make_unique<CloudPass>(m_cloudSettings);

    // 保存非擁有型指標，供執行期間更新 Cloud Pass 設定
    m_cloudPass = cloudPass.get();

    // 初始化 Cloud Pass
    cloudPass->init(initContext);

    // 將 Cloud Pass 加入執行佇列
    m_passes.push_back(std::move(cloudPass));

    // 標記 Renderer 已成功完成初始化
    m_initialized = true;
}

// 依照 Render Pass 順序執行目前影格的所有繪製工作
void Renderer::render()
{
#ifdef SKYLINE_ENABLE_TRACY
    ZoneScoped;
#endif
    // 取得目前 GraphicsEngine 使用的 RenderContext
    auto& frame = m_graphicsEngine.getRhiFrame();
    PassFrameContext frameContext{frame.commands(), frame.frameIndex(), frame.width(), frame.height()};

    // 依照加入順序執行所有 Render Pass
    for (const auto& pass : m_passes)
    {
        // 讓目前 Pass 使用本影格的 RenderContext 執行繪製
        pass->execute(frameContext);
    }
}

// 更新完整的雲層設定
void Renderer::setCloudSettings(const CloudSettings& settings)
{
    // 套用設定前先檢查所有欄位是否合法
    validateCloudSettings(settings);

    // Cloud Count 會決定 GPU Buffer 大小，因此初始化後禁止變更
    if (m_initialized && settings.cloudCount != m_cloudSettings.cloudCount)
        throw std::logic_error(
            "Renderer: Cloud count cannot change after initialization because it determines the GPU buffer size.");

    // 保存新的雲層設定
    m_cloudSettings = settings;

    // Renderer 已初始化時同步更新現有 Render Pass
    if (m_initialized)
    {
        // 更新 Light Pass 使用的設定
        m_lightPass->setSettings(m_cloudSettings);

        // 更新 Cloud Pass 使用的設定
        m_cloudPass->setSettings(m_cloudSettings);
    }
}

// 取得目前完整的雲層設定
const CloudSettings& Renderer::getCloudSettings() const
{
    return m_cloudSettings;
}

// 更新雲朵數量
void Renderer::setCloudCount(std::uint32_t cloudCount)
{
    // 先複製目前設定，避免只修改單一欄位時遺失其他參數
    CloudSettings settings = m_cloudSettings;

    settings.cloudCount = cloudCount;

    // 統一透過 setCloudSettings 執行驗證與同步
    setCloudSettings(settings);
}

std::uint32_t Renderer::getCloudCount() const
{
    return m_cloudSettings.cloudCount;
}

// 更新雲層底部高度
void Renderer::setCloudBottomHeight(float bottomHeight)
{
    CloudSettings settings = m_cloudSettings;

    settings.bottomHeight = bottomHeight;

    setCloudSettings(settings);
}

float Renderer::getCloudBottomHeight() const
{
    return m_cloudSettings.bottomHeight;
}

// 更新雲層頂部高度
void Renderer::setCloudTopHeight(float topHeight)
{
    CloudSettings settings = m_cloudSettings;

    settings.topHeight = topHeight;

    setCloudSettings(settings);
}

float Renderer::getCloudTopHeight() const
{
    return m_cloudSettings.topHeight;
}

// 更新每個雲層 Cell 的世界空間尺寸
void Renderer::setCloudCellSize(float cellSize)
{
    CloudSettings settings = m_cloudSettings;

    settings.cellSize = cellSize;

    setCloudSettings(settings);
}

float Renderer::getCloudCellSize() const
{
    return m_cloudSettings.cellSize;
}

// 更新雲層整體密度縮放值
void Renderer::setCloudDensityScale(float densityScale)
{
    CloudSettings settings = m_cloudSettings;

    settings.densityScale = densityScale;

    setCloudSettings(settings);
}

float Renderer::getCloudDensityScale() const
{
    return m_cloudSettings.densityScale;
}

// 更新雲層移動速度
void Renderer::setCloudSpeed(float speed)
{
    CloudSettings settings = m_cloudSettings;

    settings.speed = speed;

    setCloudSettings(settings);
}

float Renderer::getCloudSpeed() const
{
    return m_cloudSettings.speed;
}

// 更新主要 Ray March 的取樣次數
void Renderer::setCloudRayMarchSteps(std::uint32_t rayMarchSteps)
{
    CloudSettings settings = m_cloudSettings;

    settings.rayMarchSteps = rayMarchSteps;

    setCloudSettings(settings);
}

std::uint32_t Renderer::getCloudRayMarchSteps() const
{
    return m_cloudSettings.rayMarchSteps;
}

// 更新雲層的消光係數
void Renderer::setCloudExtinction(float extinction)
{
    CloudSettings settings = m_cloudSettings;

    settings.extinction = extinction;

    setCloudSettings(settings);
}

float Renderer::getCloudExtinction() const
{
    return m_cloudSettings.extinction;
}

// 更新雲層本身的 RGB 色彩
void Renderer::setCloudColor(const Vector3& color)
{
    CloudSettings settings = m_cloudSettings;

    settings.color = color;

    setCloudSettings(settings);
}

const Vector3& Renderer::getCloudColor() const
{
    return m_cloudSettings.color;
}

// 更新雲層的覆蓋比例
void Renderer::setCloudCoverage(float coverage)
{
    CloudSettings settings = m_cloudSettings;

    settings.coverage = coverage;

    setCloudSettings(settings);
}

float Renderer::getCloudCoverage() const
{
    return m_cloudSettings.coverage;
}

// 更新最終雲密度倍率
void Renderer::setCloudDensityMultiplier(float densityMultiplier)
{
    CloudSettings settings = m_cloudSettings;

    settings.densityMultiplier = densityMultiplier;

    setCloudSettings(settings);
}

float Renderer::getCloudDensityMultiplier() const
{
    return m_cloudSettings.densityMultiplier;
}

// 更新 Base Noise 的取樣尺度
void Renderer::setCloudBaseNoiseScale(float baseNoiseScale)
{
    CloudSettings settings = m_cloudSettings;

    settings.baseNoiseScale = baseNoiseScale;

    setCloudSettings(settings);
}

float Renderer::getCloudBaseNoiseScale() const
{
    return m_cloudSettings.baseNoiseScale;
}

// 更新 Detail Noise 的取樣尺度
void Renderer::setCloudDetailNoiseScale(float detailNoiseScale)
{
    CloudSettings settings = m_cloudSettings;

    settings.detailNoiseScale = detailNoiseScale;

    setCloudSettings(settings);
}

float Renderer::getCloudDetailNoiseScale() const
{
    return m_cloudSettings.detailNoiseScale;
}

// 更新 Detail Noise 對雲朵形狀的侵蝕強度
void Renderer::setCloudDetailErosion(float detailErosion)
{
    CloudSettings settings = m_cloudSettings;

    settings.detailErosion = detailErosion;

    setCloudSettings(settings);
}

float Renderer::getCloudDetailErosion() const
{
    return m_cloudSettings.detailErosion;
}

// 更新 Noise 疊加迭代次數
void Renderer::setCloudNoiseIterations(std::uint32_t noiseIterations)
{
    CloudSettings settings = m_cloudSettings;

    settings.noiseIterations = noiseIterations;

    setCloudSettings(settings);
}

std::uint32_t Renderer::getCloudNoiseIterations() const
{
    return m_cloudSettings.noiseIterations;
}

// 更新雲層光照 Ray March 的取樣次數
void Renderer::setCloudLightMarchSteps(std::uint32_t lightMarchSteps)
{
    CloudSettings settings = m_cloudSettings;

    settings.lightMarchSteps = lightMarchSteps;

    setCloudSettings(settings);
}

std::uint32_t Renderer::getCloudLightMarchSteps() const
{
    return m_cloudSettings.lightMarchSteps;
}
