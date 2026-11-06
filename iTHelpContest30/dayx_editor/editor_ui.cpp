#include <algorithm>

#include "editor_actions.h"
#include "editor_panels.h"

using namespace Skyline;

namespace
{
constexpr float kSplitterThickness = 6.0f;

// 一條可拖曳的分隔線，回傳這個影格拖曳的距離（像素，沿分隔線的法線方向）；沒有拖曳時為 0
float splitter(const char* id, const PanelRect& rect, bool vertical)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowPos(ImVec2(rect.x, rect.y));
    ImGui::SetNextWindowSize(ImVec2(rect.width, rect.height));
    ImGui::Begin(id, nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollWithMouse |
                     ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    ImGui::InvisibleButton("##splitter", ImVec2(rect.width, rect.height));
    float delta = 0.0f;
    if (ImGui::IsItemHovered() || ImGui::IsItemActive())
        ImGui::SetMouseCursor(vertical ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
    if (ImGui::IsItemActive())
        delta = vertical ? ImGui::GetIO().MouseDelta.x : ImGui::GetIO().MouseDelta.y;
    ImGui::End();
    ImGui::PopStyleVar();
    return delta;
}

void drawMenuBar(EditorState& state)
{
    if (!ImGui::BeginMainMenuBar())
        return;

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("Import Model..."))
            ImportModelFromDialog(state);
        ImGui::Separator();
        if (ImGui::MenuItem("Exit"))
            state.quitRequested = true;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit"))
    {
        const bool hasSelection = state.world.GetScene().IsAlive(state.selected);
        if (ImGui::MenuItem("Focus Selected", "F", false, hasSelection))
            FocusOnEntity(state, state.selected);
        if (ImGui::MenuItem("Delete Selected", "Del", false, hasSelection))
            DeleteEntity(state, state.selected);
        ImGui::Separator();
        if (ImGui::MenuItem("Create Empty"))
            CreateEmptyEntity(state, nullEntity);
        if (ImGui::MenuItem("Create Cube"))
            CreatePrimitiveEntity(state, PrimitiveKind::Cube, nullEntity);
        if (ImGui::MenuItem("Create Sphere"))
            CreatePrimitiveEntity(state, PrimitiveKind::Sphere, nullEntity);
        if (ImGui::MenuItem("Create Plane"))
            CreatePrimitiveEntity(state, PrimitiveKind::Plane, nullEntity);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View"))
    {
        if (ImGui::MenuItem("Reset Layout"))
        {
            state.leftFraction = 0.74f;
            state.topFraction = 0.66f;
            state.outlinerFraction = 0.42f;
        }
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}
} // namespace

void DrawEditorUi(EditorState& state)
{
    // 選取的實體可能在其他地方被銷毀了
    if (!state.world.GetScene().IsAlive(state.selected))
        state.selected = nullEntity;

    drawMenuBar(state);

    // Delete 鍵刪除選取（正在輸入文字時不處理）
    if (!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete, false) &&
        state.world.GetScene().IsAlive(state.selected))
        DeleteEntity(state, state.selected);

    // ----- 版面 -----
    const ImGuiViewport* main = ImGui::GetMainViewport();
    const float x = main->WorkPos.x;
    const float y = main->WorkPos.y;
    const float width = main->WorkSize.x;
    const float height = main->WorkSize.y;
    const float t = kSplitterThickness;

    state.leftFraction = std::clamp(state.leftFraction, 0.3f, 0.85f);
    state.topFraction = std::clamp(state.topFraction, 0.25f, 0.85f);
    state.outlinerFraction = std::clamp(state.outlinerFraction, 0.2f, 0.8f);

    const float leftWidth = std::max(width * state.leftFraction - t * 0.5f, 100.0f);
    const float rightX = x + leftWidth + t;
    const float rightWidth = std::max(width - leftWidth - t, 100.0f);
    const float topHeight = std::max(height * state.topFraction - t * 0.5f, 80.0f);
    const float outlinerHeight = std::max(height * state.outlinerFraction - t * 0.5f, 80.0f);

    const PanelRect viewport{x, y, leftWidth, topHeight};
    const PanelRect content{x, y + topHeight + t, leftWidth, std::max(height - topHeight - t, 80.0f)};
    const PanelRect outliner{rightX, y, rightWidth, outlinerHeight};
    const PanelRect details{rightX, y + outlinerHeight + t, rightWidth, std::max(height - outlinerHeight - t, 80.0f)};

    DrawViewportPanel(state, viewport);
    DrawOutlinerPanel(state, outliner);
    DrawDetailsPanel(state, details);
    DrawContentPanel(state, content);

    // ----- 分隔線：放在面板之間的縫隙，拖曳時改變比例 -----
    state.leftFraction += splitter("##splitV", PanelRect{x + leftWidth, y, t, height}, true) / width;
    state.topFraction += splitter("##splitLeft", PanelRect{x, y + topHeight, leftWidth, t}, false) / height;
    state.outlinerFraction += splitter("##splitRight", PanelRect{rightX, y + outlinerHeight, rightWidth, t}, false) / height;
}
