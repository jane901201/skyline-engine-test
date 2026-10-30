#pragma once

#include <cstdint>

#include "math/vector.h"

struct CloudSettings
{
    std::uint32_t cloudCount = 128;
    float bottomHeight = 5.0f;
    float topHeight = 16.0f;
    float cellSize = 4.5f;
    float densityScale = 0.18f;
    float speed = 0.7f;
    std::uint32_t rayMarchSteps = 72;
    float extinction = 0.9f;
    Vector3 color{1.0f, 0.98f, 0.94f};
    float coverage = 0.72f;
    float densityMultiplier = 3.5f;
    float baseNoiseScale = 0.12f;
    float detailNoiseScale = 0.55f;
    float detailErosion = 0.22f;
    float bottomFadeDistance = 1.5f;
    float topFadeDistance = 4.0f;
    float lightAbsorption = 0.85f;
    std::uint32_t noiseIterations = 5;
    std::uint32_t lightMarchSteps = 6;
    float lightStepSize = 1.75f;
    Vector3 sunDirection{-0.65f, 0.65f, 0.38f};
    float sunIntensity = 1.25f;
    Vector3 sunColor{1.0f, 0.76f, 0.60f};
};
