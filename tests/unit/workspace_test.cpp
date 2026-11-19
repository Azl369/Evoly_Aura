#include "aura/workspace.h"
#include <windows.h>
#include <iostream>
#include <stdexcept>
#include <cmath>
void Require(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
int wmain(int argc,wchar_t** argv) {
    try {
        if (argc!=2) throw std::runtime_error("Pass an out-of-tree test directory");
        auto widgets=aura::DefaultWorkspace(1600,1000);
        Require(widgets.size()==7,"Seven workspace panels");
        for (const auto& widget:widgets) {
            auto size=aura::MinimumWidgetSize(widget.kind);
            auto areas=aura::WidgetActions(widget.kind,size.width,size.height);
            for (const auto& area:areas) {
                const auto& r=area.bounds;
                Require(r.x>=0 && r.y>=0 && r.x+r.width<=size.width && r.y+r.height<=size.height,"Action stays inside minimum panel");
            }
        }
        for (size_t i=1;i<widgets.size();++i) for (size_t j=0;j<i;++j) {
            auto a=widgets[i].bounds,b=widgets[j].bounds;
            bool overlap=a.x<b.x+b.width && a.x+a.width>b.x && a.y<b.y+b.height && a.y+a.height>b.y;
            Require(!overlap,"Default panels must not overlap");
        }
        Require(aura::HitWidgetAction(aura::WidgetKind::notes,300,25,360,260)==aura::WidgetAction::editNote,"Note edit hit target");
        Require(aura::HitWidgetAction(aura::WidgetKind::focus,100,200,280,240)==aura::WidgetAction::focus,"Focus hit target");
        Require(aura::HitWidgetAction(aura::WidgetKind::clock,100,100,360,240)==aura::WidgetAction::none,"Clock has no accidental action");
        Require(aura::SnapCoordinate(103,{100,200},8)==100,"Nearest snap");
        Require(aura::SnapCoordinate(130,{100,200},8)==130,"No distant snap");
        auto path=std::filesystem::path(argv[1])/(L"workspace-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
        aura::WorkspaceStore store(path);
        widgets[2].bounds.x=321.6f; widgets[3].pinned=true;
        store.Save(widgets,2,.62f,5);
        auto loaded=aura::DefaultWorkspace(1600,1000);
        int theme=0,tasks=0; float opacity=0;
        store.Load(loaded,theme,opacity,tasks);
        Require(theme==2 && tasks==5 && std::abs(opacity-.62f)<.001,"Appearance roundtrip");
        Require(std::abs(loaded[2].bounds.x-321.6f)<.001f && loaded[3].pinned,"Fractional-DPI placement and pinned roundtrip");
        std::wstring note=L"浮光桌面\n第二行：保留中文。";
        store.SaveNote(note);
        Require(store.LoadNote()==note,"UTF-8 note roundtrip");
        store.SaveNote(L"");
        Require(store.LoadNote().empty(),"Empty note preserved");
        bool limited=false;
        try { store.SaveNote(std::wstring(8193,L'x')); } catch (const std::runtime_error&) { limited=true; }
        Require(limited,"Bound note allocation");
        std::filesystem::remove(path/L"workspace.ini");
        std::filesystem::remove(path/L"note.txt");
        std::filesystem::remove(path);
        std::cout<<"Workspace layout, hit targets, snapping and UTF-8 persistence passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
