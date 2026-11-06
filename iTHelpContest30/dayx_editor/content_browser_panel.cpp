#include <algorithm>
#include <cctype>
#include <cstring>
#include <system_error>

#include "editor_actions.h"
#include "editor_panels.h"

using namespace Skyline;

namespace
{
namespace fs = std::filesystem;

constexpr double kRefreshInterval = 1.0;
constexpr int kMaxTreeDepth = 6;
constexpr std::size_t kMaxEntriesPerFolder = 500;

std::string lowerCase(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

bool isModelExtension(const std::string& extension)
{
    return extension == ".fbx" || extension == ".obj" || extension == ".gltf" || extension == ".glb";
}

FolderNode buildTree(const fs::path& path, int depth)
{
    FolderNode node;
    node.path = path;
    node.label = Utf8(path.filename());
    if (depth >= kMaxTreeDepth)
        return node;

    std::error_code error;
    for (fs::directory_iterator it(path, error), end; !error && it != end; it.increment(error))
    {
        std::error_code entryError;
        if (it->is_directory(entryError) && node.children.size() < kMaxEntriesPerFolder)
            node.children.push_back(buildTree(it->path(), depth + 1));
    }
    std::sort(node.children.begin(), node.children.end(),
              [](const FolderNode& a, const FolderNode& b) { return lowerCase(a.label) < lowerCase(b.label); });
    return node;
}

// 重新掃描磁碟：資料夾樹與目前資料夾的內容。每秒最多一次，或在資料夾切換、匯入後立刻執行
void refreshCache(EditorState& state)
{
    ContentCache& cache = state.contentCache;
    const double now = ImGui::GetTime();
    if (cache.listedFolder == state.currentFolder && now < cache.nextRefreshTime)
        return;

    std::error_code error;
    fs::create_directories(state.contentRoot, error);
    if (!fs::is_directory(state.currentFolder, error))
        state.currentFolder = state.contentRoot; // 目前的資料夾被刪除了

    cache.tree = buildTree(state.contentRoot, 0);
    cache.tree.label = "Content";
    cache.entries.clear();
    for (fs::directory_iterator it(state.currentFolder, error), end; !error && it != end; it.increment(error))
    {
        if (cache.entries.size() >= kMaxEntriesPerFolder)
            break;
        std::error_code entryError;
        ContentEntry entry;
        entry.path = it->path();
        entry.label = Utf8(it->path().filename());
        entry.isDirectory = it->is_directory(entryError);
        entry.extension = entry.isDirectory ? std::string() : lowerCase(Utf8(it->path().extension()));
        cache.entries.push_back(std::move(entry));
    }
    // 資料夾排在前面，其餘依名稱
    std::sort(cache.entries.begin(), cache.entries.end(), [](const ContentEntry& a, const ContentEntry& b) {
        if (a.isDirectory != b.isDirectory)
            return a.isDirectory;
        return lowerCase(a.label) < lowerCase(b.label);
    });
    cache.listedFolder = state.currentFolder;
    cache.nextRefreshTime = now + kRefreshInterval;
}

void drawFolderNode(EditorState& state, const FolderNode& node)
{
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
                               ImGuiTreeNodeFlags_DefaultOpen;
    if (node.children.empty())
        flags |= ImGuiTreeNodeFlags_Leaf;
    if (node.path == state.currentFolder)
        flags |= ImGuiTreeNodeFlags_Selected;

    const std::string id = Utf8(node.path);
    const bool open = ImGui::TreeNodeEx(id.c_str(), flags, "%s", node.label.c_str());
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
    {
        state.currentFolder = node.path;
        state.contentCache.nextRefreshTime = 0.0;
    }
    if (open)
    {
        for (const FolderNode& child : node.children)
            drawFolderNode(state, child);
        ImGui::TreePop();
    }
}

void drawBreadcrumb(EditorState& state)
{
    std::error_code error;
    const fs::path relative = fs::relative(state.currentFolder, state.contentRoot, error);
    std::string text = "Content";
    if (!error && relative != ".")
        text += " > " + Utf8(relative);
    std::replace(text.begin(), text.end(), '\\', '>');
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(text.c_str());
}

void createFolder(EditorState& state)
{
    std::error_code error;
    for (int i = 1; i < 1000; ++i)
    {
        const fs::path candidate = state.currentFolder / (i == 1 ? "NewFolder" : "NewFolder" + std::to_string(i));
        if (!fs::exists(candidate, error))
        {
            fs::create_directory(candidate, error);
            if (error)
                state.log.Error("Could not create a folder: " + error.message());
            else
                state.log.Info("Created folder '" + Utf8(candidate.filename()) + "'.");
            state.contentCache.nextRefreshTime = 0.0;
            return;
        }
    }
}

void drawAssetGrid(EditorState& state)
{
    const ContentCache& cache = state.contentCache;
    const float tileSize = 96.0f;
    const float labelHeight = 34.0f;
    const float spacing = 10.0f;
    const int columns = std::max(1, static_cast<int>((ImGui::GetContentRegionAvail().x + spacing) / (tileSize + spacing)));

    const std::string filter = lowerCase(cache.filter);
    int column = 0;
    int visibleCount = 0;
    for (std::size_t index = 0; index < cache.entries.size(); ++index)
    {
        const ContentEntry& entry = cache.entries[index];
        if (!filter.empty() && lowerCase(entry.label).find(filter) == std::string::npos)
            continue;
        ++visibleCount;

        ImGui::PushID(static_cast<int>(index));
        const ImVec2 position = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##tile", ImVec2(tileSize, tileSize + labelHeight));
        const bool hovered = ImGui::IsItemHovered();
        const bool selected = entry.path == state.selectedAsset;

        // 方塊與類型文字（還沒有縮圖，用顏色與文字區分類型）
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImU32 tint = IM_COL32(90, 90, 96, 255);
        const char* tag = "FILE";
        std::string upperExtension;
        if (entry.isDirectory)
        {
            tint = IM_COL32(190, 150, 60, 255);
            tag = "DIR";
        }
        else if (isModelExtension(entry.extension))
        {
            tint = IM_COL32(70, 140, 100, 255);
            upperExtension = entry.extension.substr(1);
            std::transform(upperExtension.begin(), upperExtension.end(), upperExtension.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            tag = upperExtension.c_str();
        }
        drawList->AddRectFilled(position, ImVec2(position.x + tileSize, position.y + tileSize), tint, 6.0f);
        if (selected || hovered)
            drawList->AddRect(position, ImVec2(position.x + tileSize, position.y + tileSize),
                              selected ? IM_COL32(255, 200, 60, 255) : IM_COL32(255, 255, 255, 120), 6.0f, 0, 2.0f);
        const ImVec2 tagSize = ImGui::CalcTextSize(tag);
        drawList->AddText(ImVec2(position.x + (tileSize - tagSize.x) * 0.5f, position.y + (tileSize - tagSize.y) * 0.5f),
                          IM_COL32(255, 255, 255, 230), tag);

        // 名稱放在方塊下面，過長時裁切
        drawList->PushClipRect(ImVec2(position.x, position.y + tileSize),
                               ImVec2(position.x + tileSize, position.y + tileSize + labelHeight), true);
        drawList->AddText(ImVec2(position.x + 2.0f, position.y + tileSize + 3.0f), IM_COL32(230, 230, 230, 255),
                          entry.label.c_str());
        drawList->PopClipRect();

        if (hovered)
            ImGui::SetTooltip("%s", entry.label.c_str());
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            state.selectedAsset = entry.path;
        if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            if (entry.isDirectory)
            {
                state.currentFolder = entry.path;
                state.contentCache.nextRefreshTime = 0.0;
            }
            else if (isModelExtension(entry.extension))
            {
                InstantiateAsset(state, entry.path, GroundPointUnderCursor(state, state.viewport.x + state.viewport.width * 0.5f,
                                                                           state.viewport.y + state.viewport.height * 0.6f));
            }
        }

        // 模型可以拖進 Viewport
        if (!entry.isDirectory && isModelExtension(entry.extension) && ImGui::BeginDragDropSource())
        {
            const std::string text = Utf8(entry.path);
            ImGui::SetDragDropPayload("ASSET_PATH", text.c_str(), text.size() + 1);
            ImGui::TextUnformatted(entry.label.c_str());
            ImGui::EndDragDropSource();
        }
        if (!entry.isDirectory && isModelExtension(entry.extension) && ImGui::BeginPopupContextItem("##tileMenu"))
        {
            state.selectedAsset = entry.path;
            if (ImGui::MenuItem("Add to Scene"))
                InstantiateAsset(state, entry.path, GroundPointUnderCursor(state, state.viewport.x + state.viewport.width * 0.5f,
                                                                           state.viewport.y + state.viewport.height * 0.6f));
            ImGui::EndPopup();
        }
        ImGui::PopID();

        if (++column < columns)
            ImGui::SameLine(0.0f, spacing);
        else
            column = 0;
    }

    if (visibleCount == 0)
    {
        ImGui::Dummy(ImVec2(0.0f, 30.0f));
        ImGui::TextDisabled("Drop files here or right click to create content.");
    }
}

void drawContentBrowser(EditorState& state)
{
    refreshCache(state);

    // ----- 工具列 -----
    if (ImGui::Button("Import"))
        ImportModelFromDialog(state);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Import a model file (FBX, OBJ, glTF) into the current folder");
    ImGui::SameLine();
    if (ImGui::Button("New Folder"))
        createFolder(state);
    ImGui::SameLine();
    drawBreadcrumb(state);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200.0f);
    ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - 210.0f);
    ImGui::InputTextWithHint("##filter", "Search Assets", state.contentCache.filter, sizeof(state.contentCache.filter));

    // ----- 左：資料夾樹，右：目前資料夾的內容 -----
    ImGui::BeginChild("##folders", ImVec2(200.0f, 0.0f), ImGuiChildFlags_Borders);
    drawFolderNode(state, state.contentCache.tree);
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##assets", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
    drawAssetGrid(state);
    if (ImGui::BeginPopupContextWindow("##background", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
    {
        if (ImGui::MenuItem("Import..."))
            ImportModelFromDialog(state);
        if (ImGui::MenuItem("New Folder"))
            createFolder(state);
        ImGui::EndPopup();
    }
    ImGui::EndChild();

    ImGui::Text("%zu items", state.contentCache.entries.size());
}

void drawOutputLog(EditorState& state)
{
    if (ImGui::Button("Clear"))
        state.log.Clear();
    ImGui::SameLine();
    ImGui::TextDisabled("%zu lines", state.log.Entries().size());

    ImGui::BeginChild("##log", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
    for (const LogEntry& entry : state.log.Entries())
    {
        ImVec4 color(0.85f, 0.85f, 0.85f, 1.0f);
        if (entry.level == LogLevel::Warning)
            color = ImVec4(1.0f, 0.82f, 0.3f, 1.0f);
        else if (entry.level == LogLevel::Error)
            color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextWrapped("%s", entry.text.c_str());
        ImGui::PopStyleColor();
    }
    // 有新的紀錄且原本就在底部時，跟著捲到底
    static std::uint64_t lastVersion = 0;
    if (lastVersion != state.log.Version())
    {
        lastVersion = state.log.Version();
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
}
} // namespace

void DrawContentPanel(EditorState& state, const PanelRect& rect)
{
    if (BeginPanel("Content", rect))
    {
        if (ImGui::BeginTabBar("##contentTabs"))
        {
            if (ImGui::BeginTabItem("Content Browser"))
            {
                drawContentBrowser(state);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Output Log", nullptr,
                                    state.forceLogTab ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None))
            {
                drawOutputLog(state);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}
