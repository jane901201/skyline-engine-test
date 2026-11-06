#include "primitive_mesh.h"

#include <cmath>
#include <stdexcept>

namespace Skyline
{
PrimitiveGeometry MakeCubeGeometry()
{
    PrimitiveGeometry geometry;

    // 每個面：法線、面上的兩個軸向（u 向右、v 向上，u x v = 法線，與左手座標的外側一致）
    struct Face
    {
        DirectX::XMFLOAT3 normal, u, v;
    };
    const Face faces[6] = {
        {{0, 0, -1}, {1, 0, 0}, {0, 1, 0}},  // -Z
        {{0, 0, 1}, {-1, 0, 0}, {0, 1, 0}},  // +Z
        {{-1, 0, 0}, {0, 0, -1}, {0, 1, 0}}, // -X（u x v 在左手座標下指向外）
        {{1, 0, 0}, {0, 0, 1}, {0, 1, 0}},   // +X
        {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}},  // +Y
        {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}},  // -Y
    };
    const float corners[4][2] = {{-0.5f, -0.5f}, {-0.5f, 0.5f}, {0.5f, 0.5f}, {0.5f, -0.5f}};
    const float uvs[4][2] = {{0, 1}, {0, 0}, {1, 0}, {1, 1}};

    for (const Face& face : faces)
    {
        const std::uint32_t first = static_cast<std::uint32_t>(geometry.vertices.size());
        for (int i = 0; i < 4; ++i)
        {
            MeshVertex vertex{};
            vertex.position = {0.5f * face.normal.x + corners[i][0] * face.u.x + corners[i][1] * face.v.x,
                               0.5f * face.normal.y + corners[i][0] * face.u.y + corners[i][1] * face.v.y,
                               0.5f * face.normal.z + corners[i][0] * face.u.z + corners[i][1] * face.v.z};
            vertex.normal = face.normal;
            vertex.uv = {uvs[i][0], uvs[i][1]};
            geometry.vertices.push_back(vertex);
        }
        geometry.indices.insert(geometry.indices.end(), {first, first + 1, first + 2, first, first + 2, first + 3});
    }
    return geometry;
}

PrimitiveGeometry MakeSphereGeometry(std::uint32_t segments, std::uint32_t rings)
{
    if (segments < 3 || rings < 2)
        throw std::invalid_argument("MakeSphereGeometry: segments must be at least 3 and rings at least 2.");

    constexpr float pi = 3.14159265358979f;
    PrimitiveGeometry geometry;

    // 由上（+Y）到下（-Y）共 rings + 1 圈頂點，每圈 segments + 1 個（最後一個與第一個重合，用來接 UV）
    for (std::uint32_t ring = 0; ring <= rings; ++ring)
    {
        const float latitude = pi * static_cast<float>(ring) / static_cast<float>(rings);
        const float y = std::cos(latitude);
        const float radius = std::sin(latitude);
        for (std::uint32_t segment = 0; segment <= segments; ++segment)
        {
            const float longitude = 2.0f * pi * static_cast<float>(segment) / static_cast<float>(segments);
            const DirectX::XMFLOAT3 normal{radius * std::cos(longitude), y, radius * std::sin(longitude)};
            MeshVertex vertex{};
            vertex.normal = normal;
            vertex.position = {normal.x * 0.5f, normal.y * 0.5f, normal.z * 0.5f};
            vertex.uv = {static_cast<float>(segment) / static_cast<float>(segments),
                         static_cast<float>(ring) / static_cast<float>(rings)};
            geometry.vertices.push_back(vertex);
        }
    }

    const std::uint32_t stride = segments + 1;
    for (std::uint32_t ring = 0; ring < rings; ++ring)
    {
        for (std::uint32_t segment = 0; segment < segments; ++segment)
        {
            const std::uint32_t a = ring * stride + segment;
            const std::uint32_t b = a + stride;
            geometry.indices.insert(geometry.indices.end(), {a, a + 1, b, a + 1, b + 1, b});
        }
    }
    return geometry;
}

PrimitiveGeometry MakePlaneGeometry()
{
    PrimitiveGeometry geometry;
    const DirectX::XMFLOAT3 up{0, 1, 0};
    geometry.vertices = {
        {{-0.5f, 0, -0.5f}, up, {0, 1}},
        {{-0.5f, 0, 0.5f}, up, {0, 0}},
        {{0.5f, 0, 0.5f}, up, {1, 0}},
        {{0.5f, 0, -0.5f}, up, {1, 1}},
    };
    geometry.indices = {0, 1, 2, 0, 2, 3};
    return geometry;
}
} // namespace Skyline
