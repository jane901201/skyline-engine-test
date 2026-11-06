#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <optional>
#include <utility>
#include <vector>

#include "editor_actions.h"
#include "editor_panels.h"

using namespace Skyline;

namespace
{
// 這個影格要執行的操作：走訪途中不能修改場景，所以先記下來，走訪完再執行
struct PendingOps
{
    std::vector<EntityId> destroy;
    std::optional<std::pair<EntityId, EntityId>> reparent; // 實體、新父節點
    std::optional<EntityId> createChildOf;
    std::optional<PrimitiveKind> createPrimitive;
    EntityId primitiveParent = nullEntity;
};

bool containsInsensitive(const std::string& text, const char* needle)
{
    const std::size_t length = std::strlen(needle);
    if (length == 0)
        return true;
    if (text.size() < length)
        return false;
    for (std::size_t i = 0; i + length <= text.size(); ++i)
    {
        std::size_t j = 0;
        while (j < length && std::tolower(static_cast<unsigned char>(text[i + j])) ==
                                 std::tolower(static_cast<unsigned char>(needle[j])))
            ++j;
        if (j == length)
            return true;
    }
    return false;
}

// 自己或任何子孫的名稱符合搜尋字串
bool matchesFilter(const Scene& scene, EntityId id, const char* filter)
{
    if (containsInsensitive(scene.Name(id), filter))
        return true;
    for (const EntityId child : scene.Children(id))
        if (matchesFilter(scene, child, filter))
            return true;
    return false;
}

std::uint64_t entityKey(EntityId id)
{
    return (static_cast<std::uint64_t>(id.generation) << 32) | id.index;
}

void drawNode(EditorState& state, EntityId id, const char* filter, PendingOps& ops, int depth)
{
    Scene& scene = state.world.GetScene();
    if (!matchesFilter(scene, id, filter))
        return;

    const std::vector<EntityId>& children = scene.Children(id);
    const std::optional<Renderable>& renderable = scene.GetRenderable(id);

    ImGui::TableNextRow();
    ImGui::PushID(static_cast<int>(id.index));

    // 欄 0：顯示 / 隱藏
    ImGui::TableSetColumnIndex(0);
    if (renderable)
    {
        // 上層節點的 TreePush 讓縮排累積到每一欄，第 0 欄很窄，會把核取方塊擠出去，所以先把縮排拿掉
        const float indent = static_cast<float>(depth) * ImGui::GetStyle().IndentSpacing;
        if (indent > 0.0f)
            ImGui::Unindent(indent);
        bool visible = renderable->visible;
        if (ImGui::Checkbox("##visible", &visible))
        {
            Renderable changed = *renderable;
            changed.visible = visible;
            scene.SetRenderable(id, changed);
        }
        if (indent > 0.0f)
            ImGui::Indent(indent);
    }

    // 欄 1：名稱與階層
    ImGui::TableSetColumnIndex(1);
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_OpenOnArrow |
                               ImGuiTreeNodeFlags_DefaultOpen;
    if (children.empty())
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (id == state.selected)
        flags |= ImGuiTreeNodeFlags_Selected;

    const bool open = ImGui::TreeNodeEx(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(entityKey(id) + 1)),
                                        flags, "%s", scene.Name(id).c_str());
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
        state.selected = id;

    // 拖曳到另一個項目上，就變成它的子節點
    if (ImGui::BeginDragDropSource())
    {
        ImGui::SetDragDropPayload("ENTITY", &id, sizeof(id));
        ImGui::TextUnformatted(scene.Name(id).c_str());
        ImGui::EndDragDropSource();
    }
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY"))
        {
            const EntityId dragged = *static_cast<const EntityId*>(payload->Data);
            if (!(dragged == id))
                ops.reparent = std::make_pair(dragged, id);
        }
        ImGui::EndDragDropTarget();
    }

    if (ImGui::BeginPopupContextItem("##context"))
    {
        state.selected = id;
        if (ImGui::MenuItem("Focus", "F"))
            FocusOnEntity(state, id);
        if (ImGui::MenuItem("Create Empty Child"))
            ops.createChildOf = id;
        if (ImGui::BeginMenu("Create Child"))
        {
            if (ImGui::MenuItem("Cube"))
            {
                ops.createPrimitive = PrimitiveKind::Cube;
                ops.primitiveParent = id;
            }
            if (ImGui::MenuItem("Sphere"))
            {
                ops.createPrimitive = PrimitiveKind::Sphere;
                ops.primitiveParent = id;
            }
            if (ImGui::MenuItem("Plane"))
            {
                ops.createPrimitive = PrimitiveKind::Plane;
                ops.primitiveParent = id;
            }
            ImGui::EndMenu();
        }
        if (!scene.Parent(id).IsNull() && ImGui::MenuItem("Unparent"))
            ops.reparent = std::make_pair(id, nullEntity);
        ImGui::Separator();
        if (ImGui::MenuItem("Delete", "Del"))
            ops.destroy.push_back(id);
        ImGui::EndPopup();
    }

    // 欄 2：類型
    ImGui::TableSetColumnIndex(2);
    ImGui::TextDisabled("%s", renderable ? "Mesh" : "Empty");

    if (open && !children.empty())
    {
        for (const EntityId child : children)
            drawNode(state, child, filter, ops, depth + 1);
        ImGui::TreePop();
    }
    ImGui::PopID();
}
} // namespace

void DrawOutlinerPanel(EditorState& state, const PanelRect& rect)
{
    if (BeginPanel("Outliner", rect))
    {
        Scene& scene = state.world.GetScene();
        PendingOps ops;
        static char filter[128] = {};

        // ----- 工具列：搜尋與新增 -----
        ImGui::SetNextItemWidth(-90.0f);
        ImGui::InputTextWithHint("##search", "Search...", filter, sizeof(filter));
        ImGui::SameLine();
        if (ImGui::Button("+ Add"))
            ImGui::OpenPopup("##add");
        if (ImGui::BeginPopup("##add"))
        {
            if (ImGui::MenuItem("Empty"))
                ops.createChildOf = nullEntity;
            ImGui::Separator();
            if (ImGui::MenuItem("Cube"))
                ops.createPrimitive = PrimitiveKind::Cube;
            if (ImGui::MenuItem("Sphere"))
                ops.createPrimitive = PrimitiveKind::Sphere;
            if (ImGui::MenuItem("Plane"))
                ops.createPrimitive = PrimitiveKind::Plane;
            ImGui::EndPopup();
        }

        // ----- 階層表格 -----
        const float footer = ImGui::GetTextLineHeightWithSpacing() + 6.0f;
        if (ImGui::BeginTable("##outliner", 3,
                              ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_BordersInnerV,
                              ImVec2(0.0f, -footer)))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("##visible", ImGuiTableColumnFlags_WidthFixed, 28.0f);
            ImGui::TableSetupColumn("Item Label", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 64.0f);
            ImGui::TableHeadersRow();

            std::vector<EntityId> roots;
            scene.ForEach([&](EntityId id) {
                if (scene.Parent(id).IsNull())
                    roots.push_back(id);
            });
            for (const EntityId root : roots)
                drawNode(state, root, filter, ops, 0);

            // 表格的空白處按一下取消選取
            if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered())
                state.selected = nullEntity;
            ImGui::EndTable();
        }

        // ----- 底部：數量 -----
        ImGui::Separator();
        ImGui::Text("%zu entities", scene.EntityCount());

        // ----- 執行這個影格累積的操作 -----
        if (ops.reparent)
            ReparentEntity(state, ops.reparent->first, ops.reparent->second);
        if (ops.createChildOf)
            CreateEmptyEntity(state, *ops.createChildOf);
        if (ops.createPrimitive)
            CreatePrimitiveEntity(state, *ops.createPrimitive, ops.primitiveParent);
        for (const EntityId id : ops.destroy)
            DeleteEntity(state, id);
    }
    ImGui::End();
}
