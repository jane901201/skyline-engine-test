#pragma once

#include "resource_bank.h"
#include "shader.h"

class GraphicsEngine;
class Renderer;
struct FrameConstants;

class SkylineEngine
{
public:
    ~SkylineEngine();

    void beginFrame();
    void render(const FrameConstants& frameConstants);
    void endFrame();

    void init(HWND hwnd, UINT frameBufferWidth, UINT frameBufferHeight);

private:
    GraphicsEngine* m_graphicsEngine = nullptr;
    Renderer* m_renderer = nullptr;
    TResourceBank<Shader> m_shaderBank;

};

extern SkylineEngine* g_skyline_engine;
