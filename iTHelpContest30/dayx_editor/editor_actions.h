#pragma once

#include <filesystem>
#include <string>

#include "editor_state.h"

// 編輯器的操作。面板與選單共用；使用者操作可能失敗（檔案壞掉、會造成階層循環等），
// 這一層負責接住例外並寫進 Output Log，不會讓編輯器因此結束。

enum class PrimitiveKind
{
    Cube,
    Sphere,
    Plane
};

/// UTF-8 與檔案路徑互轉（ImGui 與 std::filesystem 之間）
std::string Utf8(const std::filesystem::path& path);
std::filesystem::path PathFromUtf8(const std::string& text);

/// 建立沒有任何元件的空實體
Skyline::EntityId CreateEmptyEntity(EditorState& state, Skyline::EntityId parent);

/// 建立一個基本形狀的實體（每個實體有自己的材質）
Skyline::EntityId CreatePrimitiveEntity(EditorState& state, PrimitiveKind kind, Skyline::EntityId parent);

/// 銷毀實體與它的子孫；被選取的實體被銷毀時清除選取
void DeleteEntity(EditorState& state, Skyline::EntityId entity);

/// 把攝影機移到能看見該實體的位置（沿目前的視線方向後退）
void FocusOnEntity(EditorState& state, Skyline::EntityId entity);

/// 變更父節點並保持世界位置；會造成循環時只記錄錯誤
void ReparentEntity(EditorState& state, Skyline::EntityId entity, Skyline::EntityId newParent);

/// 把模型資產放進場景，回傳根實體；失敗時回傳 nullEntity
Skyline::EntityId InstantiateAsset(EditorState& state, const std::filesystem::path& file,
                                   const Vector3& position);

/// 開啟檔案對話框選一個模型檔，複製到目前資料夾並匯入
void ImportModelFromDialog(EditorState& state);

/// 滑鼠在 Viewport 內的位置（螢幕座標）對應到地面（y = 0）的點；射線沒打到地面時回傳攝影機前方 8 單位
Vector3 GroundPointUnderCursor(const EditorState& state, float screenX, float screenY);

/// 套用 state.sunYawDegrees / sunElevationDegrees 到 state.lighting.direction
void UpdateSunDirection(EditorState& state);
