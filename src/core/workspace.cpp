#include "aura/workspace.h"
#include <algorithm>
#include <cmath>

namespace aura {
WindowLayer ResolveWindowLayer(bool desktopRequested,bool hostAvailable,bool editing,bool pinned) {
    if (editing || pinned) return WindowLayer::topmost;
    if (desktopRequested && hostAvailable) return WindowLayer::desktop;
    return WindowLayer::floating;
}
std::vector<WidgetPlacement> DefaultWorkspace(float width, float height) {
    float x = std::max(20.f, (width - 1160.f) / 2);
    float y = std::max(24.f, (height - 620.f) / 2 - 16);
    return {
        {WidgetKind::toolbar, {x, y, 1160, 76}},
        {WidgetKind::clock, {x, y + 100, 360, 240}},
        {WidgetKind::launcher, {x + 380, y + 100, 480, 240}},
        {WidgetKind::focus, {x + 880, y + 100, 280, 240}},
        {WidgetKind::notes, {x, y + 360, 360, 260}},
        {WidgetKind::checklist, {x + 380, y + 360, 480, 260}},
        {WidgetKind::appearance, {x + 880, y + 360, 280, 260}}
    };
}
std::vector<ActionArea> WidgetActions(WidgetKind kind, float w, float h) {
    switch (kind) {
    case WidgetKind::toolbar:
        return {{WidgetAction::editLayout,{w-418,20,126,36}},
                {WidgetAction::sendToDesktop,{w-280,20,148,36}},
                {WidgetAction::resetLayout,{w-120,20,96,36}}};
    case WidgetKind::launcher: {
        std::vector<ActionArea> out;
        constexpr WidgetAction actions[] = {WidgetAction::launchExplorer,WidgetAction::launchDownloads,
            WidgetAction::launchDocuments,WidgetAction::launchGithub,WidgetAction::launchSettings,WidgetAction::launchRecycle};
        float cell = (w - 64) / 3;
        for (int i=0;i<6;++i) out.push_back({actions[i],{24+(i%3)*(cell+8),62.f+(i/3)*76,cell,68}});
        return out;
    }
    case WidgetKind::focus: return {{WidgetAction::focus,{24,h-54,w-48,34}}};
    case WidgetKind::notes: return {{WidgetAction::editNote,{w-78,18,54,28}}};
    case WidgetKind::checklist:
        return {{WidgetAction::task0,{24,68,w-48,42}},
                {WidgetAction::task1,{24,120,w-48,42}},
                {WidgetAction::task2,{24,172,w-48,42}}};
    case WidgetKind::appearance:
        return {{WidgetAction::theme0,{24,70,48,44}},{WidgetAction::theme1,{82,70,48,44}},
                {WidgetAction::theme2,{140,70,48,44}},{WidgetAction::theme3,{198,70,48,44}},
                {WidgetAction::clearer,{24,h-58,(w-56)/2,34}},
                {WidgetAction::frostier,{32+(w-56)/2,h-58,(w-56)/2,34}}};
    default: return {};
    }
}
WidgetAction HitWidgetAction(WidgetKind kind, float x, float y, float w, float h) {
    for (const auto& area : WidgetActions(kind,w,h))
        if (x>=area.bounds.x && y>=area.bounds.y && x<area.bounds.x+area.bounds.width && y<area.bounds.y+area.bounds.height)
            return area.action;
    return WidgetAction::none;
}
DipRect MinimumWidgetSize(WidgetKind kind) {
    switch (kind) {
    case WidgetKind::toolbar: return {0,0,880,76};
    case WidgetKind::clock: return {0,0,300,240};
    case WidgetKind::launcher: return {0,0,400,230};
    case WidgetKind::checklist: return {0,0,380,250};
    case WidgetKind::notes: return {0,0,300,220};
    case WidgetKind::appearance: return {0,0,280,260};
    default: return {0,0,280,240};
    }
}
float SnapCoordinate(float value, const std::vector<float>& anchors, float threshold) {
    float result = value, best = threshold;
    for (float anchor : anchors) {
        float distance = std::abs(anchor-value);
        if (distance < best) { best=distance; result=anchor; }
    }
    return result;
}
}
