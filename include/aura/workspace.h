#pragma once
#include <array>
#include <string>
#include <vector>
#include <filesystem>

namespace aura {
enum class WidgetKind { toolbar, clock, launcher, focus, notes, checklist, appearance };
enum class WindowLayer { desktop, floating, topmost };
WindowLayer ResolveWindowLayer(bool desktopRequested, bool hostAvailable, bool editing, bool pinned);
enum class WidgetAction {
    none, editLayout, sendToDesktop, resetLayout, editNote, focus,
    launchExplorer, launchDownloads, launchDocuments, launchGithub, launchSettings, launchRecycle,
    task0, task1, task2, theme0, theme1, theme2, theme3, clearer, frostier
};
struct DipRect { float x{}, y{}, width{}, height{}; };
struct WidgetPlacement { WidgetKind kind{}; DipRect bounds; bool pinned = false; };
struct WidgetView {
    WidgetKind kind{};
    bool editing = false, desktop = true, focusRunning = false;
    bool sampleWallpaper = true, temporaryReveal = false;
    int theme = 0, focusSeconds = 1500, checkedTasks = 0;
    float opacity = .54f;
    WidgetAction hovered = WidgetAction::none;
    std::wstring note = L"把想法留在这里。\n双击便签，或点击右上角「编辑」。";
};
struct ActionArea { WidgetAction action; DipRect bounds; };
std::vector<WidgetPlacement> DefaultWorkspace(float availableWidth, float availableHeight);
std::vector<ActionArea> WidgetActions(WidgetKind kind, float width, float height);
WidgetAction HitWidgetAction(WidgetKind kind, float x, float y, float width, float height);
DipRect MinimumWidgetSize(WidgetKind kind);
float SnapCoordinate(float value, const std::vector<float>& anchors, float threshold);
class WorkspaceStore {
public:
    explicit WorkspaceStore(std::filesystem::path directory);
    void Load(std::vector<WidgetPlacement>& widgets, int& theme, float& opacity, int& checkedTasks);
    void Save(const std::vector<WidgetPlacement>& widgets, int theme, float opacity, int checkedTasks);
    std::wstring LoadNote() const;
    void SaveNote(const std::wstring& note);
    const std::filesystem::path& Directory() const { return directory_; }
private:
    std::filesystem::path directory_;
};
}
