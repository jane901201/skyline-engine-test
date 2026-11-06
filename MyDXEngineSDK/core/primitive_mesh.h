#pragma once
#include "skyline_api.h"

#include <cstdint>
#include <vector>

#include "mesh.h"

namespace Skyline
{
/// <summary>
/// 程式碼產生的基本形狀頂點與索引（Triangle List），可交給 Mesh::initFromGeometry 建立 GPU 資源。
/// </summary>
/// <remarks>
/// 所有形狀都以原點為中心、大小為 1（立方體邊長 1、球直徑 1、平面 1x1），
/// 使用時以 Transform 的縮放放大。座標為左手座標，Y 向上。
/// </remarks>
struct PrimitiveGeometry
{
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
};

/// 邊長 1 的立方體，每個面有獨立的法線（24 個頂點）
SKYLINE_API PrimitiveGeometry MakeCubeGeometry();

/// 直徑 1 的球。segments 為經度分段數（至少 3），rings 為緯度分段數（至少 2）；不合法時丟出 std::invalid_argument
SKYLINE_API PrimitiveGeometry MakeSphereGeometry(std::uint32_t segments = 32, std::uint32_t rings = 16);

/// 位於 XZ 平面、法線朝 +Y、大小 1x1 的平面
SKYLINE_API PrimitiveGeometry MakePlaneGeometry();
} // namespace Skyline
