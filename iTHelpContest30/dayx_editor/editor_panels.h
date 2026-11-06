#pragma once

#include <imgui.h>

#include "editor_state.h"

/// 開始一個固定在指定矩形的面板視窗（不能拖曳、縮放，版面由編輯器排）。不論回傳值，都要呼叫 ImGui::End()
inline bool BeginPanel(const char* title, const PanelRect& rect, ImGuiWindowFlags extraFlags = 0)
{
    ImGui::SetNextWindowPos(ImVec2(rect.x, rect.y));
    ImGui::SetNextWindowSize(ImVec2(rect.width, rect.height));
    return ImGui::Begin(title, nullptr,
                        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings | extraFlags);
}

// 各面板。每個影格在 ImGui::NewFrame 與 ImGui::Render 之間各呼叫一次
void DrawViewportPanel(EditorState& state, const PanelRect& rect);
void DrawOutlinerPanel(EditorState& state, const PanelRect& rect);
void DrawDetailsPanel(EditorState& state, const PanelRect& rect);
/// 底部面板：Content Browser 與 Output Log 兩個分頁
void DrawContentPanel(EditorState& state, const PanelRect& rect);

/// 選單列、版面（含可拖曳的分隔線）、快捷鍵，以及上面所有的面板
void DrawEditorUi(EditorState& state);
