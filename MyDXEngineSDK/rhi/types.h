#pragma once

#include <cstdint>

namespace Skyline::RHI
{
enum class Format
{
    Unknown,
    Rgba8Unorm,
    Bgra8Unorm,
    Rgba16Float,
    R32Float,
    Rg32Float,
    Rgb32Float,
    Rgba32Float,
    D32Float,
    D24UnormS8Uint
};

enum class PrimitiveType { Point, Line, Triangle };
enum class ShaderStage { Unknown, Vertex, Pixel, Compute };
enum class CullMode { None, Front, Back };
enum class FillMode { Solid, Wireframe };
enum class CompareOp { Never, Less, Equal, LessEqual, Greater, NotEqual, GreaterEqual, Always };
enum class BlendFactor { Zero, One, SourceAlpha, InverseSourceAlpha, DestinationAlpha, InverseDestinationAlpha };
enum class BlendOp { Add, Subtract, ReverseSubtract, Minimum, Maximum };
enum class InputRate { Vertex, Instance };

struct SampleDesc
{
    std::uint32_t count = 1;
    std::uint32_t quality = 0;
};
}
