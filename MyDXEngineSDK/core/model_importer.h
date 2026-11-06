#pragma once
#include "skyline_api.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "math/aabb.h"
#include "primitive_mesh.h"
#include "scene/transform.h"

namespace Skyline
{
/// 匯入的材質（只取基礎色、金屬度、粗糙度，貼圖尚未支援）
struct ImportedMaterial
{
    std::string name;
    Vector3 baseColor{0.8f, 0.8f, 0.8f};
    float metallic = 0.0f;
    float roughness = 0.6f;
};

/// 匯入的網格，頂點在所屬節點的本地空間
struct ImportedMesh
{
    std::string name;
    PrimitiveGeometry geometry;
    AABB bounds;                  ///< 本地空間的包圍盒
    std::uint32_t material = 0;   ///< ImportedModel::materials 的索引；沒有材質時為 invalidMaterial
};

/// 匯入的節點。父節點一定排在子節點之前，所以逐一建立就能保證父節點已存在
struct ImportedNode
{
    std::string name;
    int parent = -1;       ///< ImportedModel::nodes 的索引；根節點為 -1
    Transform local;       ///< 相對於父節點
    int mesh = -1;         ///< ImportedModel::meshes 的索引；沒有網格時為 -1
};

/// 匯入的模型：節點階層、網格與材質。座標已轉成左手座標（Y 向上）
struct ImportedModel
{
    static constexpr std::uint32_t invalidMaterial = 0xFFFFFFFFu;

    std::vector<ImportedNode> nodes;
    std::vector<ImportedMesh> meshes;
    std::vector<ImportedMaterial> materials;
    /// 匯入時略過或修正的項目（例如沒有三角形的網格、無法分解的節點矩陣），給使用者看
    std::vector<std::string> warnings;
};

/// <summary>
/// 以 Assimp 匯入模型檔（FBX、OBJ 等 Assimp 支援的格式）。
/// </summary>
/// <remarks>
/// - path 為空時丟出 std::invalid_argument；檔案讀取失敗或不含任何網格時丟出 std::runtime_error。
/// - 一個節點有多個網格時，每個網格會成為該節點底下獨立的子節點，所以每個節點最多一個網格。
/// - 只保留三角形；點與線會被移除。法線不存在時由 Assimp 產生。
/// - FBX 依檔案內的 UnitScaleFactor 轉成公尺（匯入後 1 單位 = 1 公尺），縮放放在根節點；其他格式維持檔案原本的單位。
/// - 只讀取檔案，不碰 GPU，可以在沒有裝置時使用。
/// </remarks>
SKYLINE_API ImportedModel ImportModelFile(const std::filesystem::path& path);
} // namespace Skyline
