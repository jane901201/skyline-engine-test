#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "asset_library.h"
#include "editor_log.h"
#include "gameplay/game_world.h"
#include "input/input_state.h"
#include "render_pass/scene_renderer.h"
#include "scene/camera.h"
#include "scene/fly_camera.h"

/// 螢幕（Client 區）上的一塊矩形，單位是像素
struct PanelRect
{
    float x = 0.0f;
    float y = 0.0f;
    float width = 1.0f;
    float height = 1.0f;
};

/// Content Browser 資料夾樹的一個節點
struct FolderNode
{
    std::filesystem::path path;
    std::string label;
    std::vector<FolderNode> children;
};

/// Content Browser 資料夾裡的一個項目
struct ContentEntry
{
    std::filesystem::path path;
    std::string label;      ///< 顯示用的 UTF-8 名稱
    std::string extension;  ///< 小寫副檔名（含點），資料夾為空
    bool isDirectory = false;
};

/// Content Browser 的快取：避免每個影格都掃描磁碟
struct ContentCache
{
    std::filesystem::path listedFolder;
    std::vector<ContentEntry> entries;
    FolderNode tree;
    double nextRefreshTime = 0.0;
    char filter[128] = {};
};

/// <summary>
/// 編輯器的所有狀態。面板函式讀寫它，沒有其他全域狀態。
/// </summary>
/// <remarks>
/// 編輯模式下 GameWorld 不會被 Tick：場景只是資料，沒有行為在跑。
/// 之後的 Play 模式要在場景的複本上執行，不能直接動這份資料。
/// </remarks>
struct EditorState
{
    EditorState(Skyline::SceneRenderer& renderer, std::filesystem::path contentRoot)
        : renderer(&renderer), assets(renderer), contentRoot(std::move(contentRoot))
    {
        currentFolder = this->contentRoot;
    }

    Skyline::SceneRenderer* renderer;
    Skyline::GameWorld world;
    Skyline::Camera camera;
    Skyline::FlyCameraController fly;
    EditorLog log;
    AssetLibrary assets;

    const Skyline::Input::State* input = nullptr;
    float deltaTime = 0.0f;
    bool quitRequested = false;

    // ----- 選取 -----
    Skyline::EntityId selected = Skyline::nullEntity;
    std::filesystem::path selectedAsset;

    // ----- Viewport -----
    PanelRect viewport;
    bool flyActive = false; ///< 按住右鍵拖曳中，攝影機接收移動輸入

    // ----- 世界設定 -----
    Skyline::SceneLighting lighting;
    float sunYawDegrees = 40.0f;
    float sunElevationDegrees = 46.0f;

    // ----- Content Browser -----
    std::filesystem::path contentRoot;
    std::filesystem::path currentFolder;
    ContentCache contentCache;

    // ----- 版面（比例）-----
    float leftFraction = 0.74f;   ///< 左欄佔整體寬度
    float topFraction = 0.66f;    ///< 左欄中 Viewport 佔高度
    float outlinerFraction = 0.42f; ///< 右欄中 Outliner 佔高度

    // 自動化測試用：強制切到指定分頁（命令列 --tab=log / --tab=world）
    bool forceLogTab = false;
    bool forceWorldTab = false;

    // ----- 統計 -----
    std::uint32_t drawCount = 0;
};
