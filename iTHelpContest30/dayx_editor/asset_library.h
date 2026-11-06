#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/model_importer.h"
#include "gameplay/game_world.h"
#include "render_pass/scene_renderer.h"

/// 已匯入並上傳到 GPU 的模型：匯入的資料，加上它在 SceneRenderer 裡對應的 mesh / material id
struct LoadedModel
{
    Skyline::ImportedModel model;
    std::vector<std::uint32_t> meshIds;     ///< 與 model.meshes 一一對應
    std::vector<std::uint32_t> materialIds; ///< 與 model.materials 一一對應
};

/// <summary>
/// 管理匯入的模型資產：同一個檔案只解析與上傳一次，之後可以重複放進場景。
/// </summary>
/// <remarks>
/// 這是最簡版本：模型保存在記憶體，沒有自己的資產檔與 GUID，編輯器重開就要重新匯入。
/// SceneRenderer 的 mesh 與 material 目前沒有釋放機制，所以載入過的資源會一直存在。
/// </remarks>
class AssetLibrary
{
  public:
    explicit AssetLibrary(Skyline::SceneRenderer& renderer) : m_renderer(&renderer) {}

    /// 解析並上傳模型；已載入過的路徑直接回傳。失敗時丟出 std::runtime_error（或 std::invalid_argument）
    const LoadedModel& Load(const std::filesystem::path& path);

    [[nodiscard]] bool IsLoaded(const std::filesystem::path& path) const;

    /// <summary>
    /// 把模型放進場景：建立一個以檔名命名的根實體，底下依匯入的階層建立所有節點。
    /// </summary>
    /// <returns>根實體</returns>
    Skyline::EntityId Instantiate(Skyline::GameWorld& world, const std::filesystem::path& path,
                                  const Vector3& position);

    /// 登記 mesh id 的顯示名稱，供屬性面板使用
    void NameMesh(std::uint32_t meshId, std::string name)
    {
        m_meshNames[meshId] = std::move(name);
    }
    [[nodiscard]] std::string MeshName(std::uint32_t meshId) const;

  private:
    static std::string Key(const std::filesystem::path& path);

    Skyline::SceneRenderer* m_renderer;
    std::map<std::string, LoadedModel> m_models;
    std::unordered_map<std::uint32_t, std::string> m_meshNames;
};
