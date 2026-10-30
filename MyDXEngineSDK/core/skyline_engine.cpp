#include "skyline_engine.h"
#include "graphics_engine.h"

#include <memory>
#include <stdexcept>

#include "renderer.h"

SkylineEngine* g_skyline_engine = nullptr;

SkylineEngine::~SkylineEngine()
{
    delete m_renderer;
    delete m_graphicsEngine;
}

void SkylineEngine::init(HWND hwnd, UINT frameBufferWidth, UINT frameBufferHeight)
{
    if (m_graphicsEngine != nullptr || m_renderer != nullptr)
        throw std::logic_error("SkylineEngine: The engine is already initialized.");

    auto graphicsEngine = std::make_unique<GraphicsEngine>();
    graphicsEngine->init(hwnd, frameBufferWidth, frameBufferHeight);

    auto renderer = std::make_unique<Renderer>(*graphicsEngine);
    renderer->init();

    m_graphicsEngine = graphicsEngine.release();
    m_renderer = renderer.release();
}

void SkylineEngine::beginFrame()
{
    m_graphicsEngine->beginRender();
}

void SkylineEngine::render(const FrameConstants& frameConstants)
{
    m_renderer->render(frameConstants);
}

void SkylineEngine::endFrame()
{
    m_graphicsEngine->endRender();
}
