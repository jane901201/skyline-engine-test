#include <algorithm>
#include <cstdio>
#include <cstring>

#include "editor_actions.h"
#include "editor_panels.h"

using namespace Skyline;

void DrawViewportPanel(EditorState& state, const PanelRect& rect)
{
    // 背景透明：3D 場景畫在這個視窗後面，所以視窗本身不能蓋掉它
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 4.0f));
    const bool visible = BeginPanel("Viewport", rect,
                                    ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
                                        ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();

    if (visible)
    {
        // ----- 工具列 -----
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Perspective");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        ImGui::SliderFloat("Speed", &state.fly.moveSpeed, 0.5f, 40.0f, "%.1f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        float fovDegrees = DirectX::XMConvertToDegrees(state.camera.fovY);
        if (ImGui::SliderFloat("FOV", &fovDegrees, 20.0f, 110.0f, "%.0f"))
            state.camera.fovY = DirectX::XMConvertToRadians(fovDegrees);
        ImGui::SameLine();
        if (ImGui::Button("Focus (F)"))
            FocusOnEntity(state, state.selected);

        // ----- 場景區域 -----
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 available = ImGui::GetContentRegionAvail();
        state.viewport = PanelRect{origin.x, origin.y, std::max(available.x, 1.0f), std::max(available.y, 1.0f)};
        state.camera.SetViewport(state.viewport.width, state.viewport.height);

        // 一個透明的按鈕蓋住整個場景區域，用來接收滑鼠與拖放
        ImGui::InvisibleButton("##scene", ImVec2(state.viewport.width, state.viewport.height),
                               ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        const bool hovered = ImGui::IsItemHovered();
        const ImVec2 mouse = ImGui::GetMousePos();

        // 按住右鍵拖曳時，攝影機才接收 WASD 與滑鼠轉動；右鍵必須是在場景區域內按下
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            state.flyActive = true;
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Right))
            state.flyActive = false;
        if (state.flyActive)
            state.fly.Update(state.camera, *state.input, state.deltaTime);

        // 左鍵點擊選取（沒打到任何東西就取消選取）
        if (hovered && !state.flyActive && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            const Ray ray = state.camera.ScreenPointToRay(mouse.x - state.viewport.x, mouse.y - state.viewport.y,
                                                          state.viewport.width, state.viewport.height);
            const auto hit = state.world.GetScene().Raycast(ray);
            state.selected = hit ? hit->entity : nullEntity;
        }
        if (hovered && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F, false))
            FocusOnEntity(state, state.selected);

        // 從 Content Browser 拖進來的模型：放在滑鼠下方的地面
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH"))
            {
                const std::string text(static_cast<const char*>(payload->Data), payload->DataSize);
                InstantiateAsset(state, PathFromUtf8(text.c_str()), GroundPointUnderCursor(state, mouse.x, mouse.y));
            }
            ImGui::EndDragDropTarget();
        }

        // ----- 左上角的統計文字 -----
        char stats[160];
        std::snprintf(stats, sizeof(stats), "%zu entities | %u drawn | %.0f FPS",
                      state.world.GetScene().EntityCount(), state.drawCount, ImGui::GetIO().Framerate);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 textPosition(origin.x + 8.0f, origin.y + 6.0f);
        drawList->AddText(ImVec2(textPosition.x + 1.0f, textPosition.y + 1.0f), IM_COL32(0, 0, 0, 200), stats);
        drawList->AddText(textPosition, IM_COL32(235, 235, 235, 255), stats);
    }
    ImGui::End();
}
