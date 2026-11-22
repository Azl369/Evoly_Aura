#include "aura/desktop_app.h"
#include "aura/render.h"
#include "aura/workspace.h"
#include "aura/system_sampler.h"
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace aura {
namespace {
constexpr UINT trayMessage=WM_APP+1,controlMessage=WM_APP+2;
constexpr UINT revealMessage=WM_APP+4,systemSampleMessage=WM_APP+5;
constexpr UINT minuteTimer=1,focusTimer=2,durationTimer=3;
struct DesktopPanel {
    HWND window{};
    WidgetPlacement placement;
    WidgetAction hovered=WidgetAction::none;
    bool attached=false;
};
HWND FindDesktopSurface() {
    HWND progman=FindWindowW(L"Progman",nullptr);
    if (!progman) return nullptr;
    DWORD_PTR result{};
    SendMessageTimeoutW(progman,0x052C,0,0,SMTO_ABORTIFHUNG,1000,&result);
    HWND worker=nullptr;
    EnumWindows([](HWND top,LPARAM value)->BOOL {
        wchar_t cls[64]{}; GetClassNameW(top,cls,64);
        if (wcscmp(cls,L"Progman") && wcscmp(cls,L"WorkerW")) return TRUE;
        if (FindWindowExW(top,nullptr,L"SHELLDLL_DefView",nullptr)) {
            HWND next=FindWindowExW(nullptr,top,L"WorkerW",nullptr);
            if (next && IsWindowVisible(next)) { *reinterpret_cast<HWND*>(value)=next; return FALSE; }
        }
        return TRUE;
    },reinterpret_cast<LPARAM>(&worker));
    if (!worker) {
        auto candidate=FindWindowExW(progman,nullptr,L"WorkerW",nullptr);
        if (candidate && IsWindowVisible(candidate)) worker=candidate;
    }
    return worker;
}
std::filesystem::path UserDataDirectory() {
    PWSTR value=nullptr;
    Check(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&value),"Local app data");
    std::filesystem::path result(value);
    CoTaskMemFree(value);
    return result/L"EvolyAura";
}
class DesktopApp {
public:
    DesktopOptions options;
    std::unique_ptr<Renderer> renderer;
    std::unique_ptr<WorkspaceStore> store;
    SystemSampler systemSampler;
    std::vector<DesktopPanel> panels;
    HWND controller{},noteEditor{},noteInput{},desktopHost{};
    HANDLE instance{};
    bool editing=false,desktop=true,visible=true,focusRunning=false,failed=false,warned=false;
    bool temporaryReveal=false,changingLayer=false;
    bool trayVersion4=false,revealQueued=false;
    bool systemSampleLogged=false;
    WPARAM revealGeneration=0;
    int theme=0,checkedTasks=0,remaining=1500;
    float opacity=.54f;
    std::wstring note;
    RECT work{};
    UINT systemDpi=96,taskbarCreated=0;
    std::ofstream log;
    HWND dragging{};
    bool resizing=false;
    POINT dragStart{};
    RECT dragBounds{};
    std::chrono::steady_clock::time_point focusEnd;
    ~DesktopApp() {
        systemSampler.Stop();
        if (noteEditor && IsWindow(noteEditor)) DestroyWindow(noteEditor);
        NOTIFYICONDATAW data{sizeof(data)}; data.hWnd=controller; data.uID=1;
        if (controller) Shell_NotifyIconW(NIM_DELETE,&data);
        for (auto& panel:panels) if (IsWindow(panel.window)) DestroyWindow(panel.window);
        if (controller && IsWindow(controller)) DestroyWindow(controller);
        if (instance) CloseHandle(instance);
    }
    void Log(const char* event) { if (log) { log<<event<<'\n'; log.flush(); } }
    float Scale() const { return systemDpi/96.f; }
    bool DesktopRequested() const { return desktop && !editing && !temporaryReveal; }
    bool OnDesktop() const { return DesktopRequested() && IsWindow(desktopHost) && IsWindowVisible(desktopHost); }
    DesktopPanel* Panel(HWND hwnd) {
        for (auto& panel:panels) if (panel.window==hwnd) return &panel;
        return nullptr;
    }
    std::vector<WidgetPlacement> Placements() const {
        std::vector<WidgetPlacement> out;
        for (const auto& panel:panels) out.push_back(panel.placement);
        return out;
    }
    void Save() {
        for (auto& panel:panels) {
            RECT r{};
            if (!IsWindow(panel.window) || IsIconic(panel.window) || !GetWindowRect(panel.window,&r)) continue;
            auto& b=panel.placement.bounds;
            b={float(r.left-work.left)/Scale(),float(r.top-work.top)/Scale(),
                float(r.right-r.left)/Scale(),float(r.bottom-r.top)/Scale()};
        }
        try { store->Save(Placements(),theme,opacity,checkedTasks); warned=false; }
        catch (const std::exception&) {
            Log("save_failed");
            if (!warned) {
                warned=true;
                MessageBoxW(controller,L"本次布局无法保存。请确认数据目录可写；当前窗口仍然可用。",L"Evoly Aura",MB_OK|MB_ICONWARNING|MB_TOPMOST);
            }
        }
    }
    WidgetView View(const DesktopPanel& panel) {
        WidgetView view;
        view.kind=panel.placement.kind; view.editing=editing; view.desktop=OnDesktop();
        view.sampleWallpaper=panel.attached; view.temporaryReveal=temporaryReveal;
        view.theme=theme; view.opacity=opacity; view.note=note; view.hovered=panel.hovered;
        view.focusRunning=focusRunning; view.focusSeconds=remaining; view.checkedTasks=checkedTasks;
        if (panel.placement.kind==WidgetKind::systemMonitor) view.system=systemSampler.Read();
        return view;
    }
    void Paint(DesktopPanel& panel) {
        auto view=View(panel);
        renderer->Paint(panel.window,0,Material::wallpaper,false,&view);
    }
    void InvalidateAll() {
        for (auto& panel:panels) if (IsWindow(panel.window)) InvalidateRect(panel.window,nullptr,FALSE);
    }
    void AddTray() {
        NOTIFYICONDATAW data{sizeof(data)};
        data.hWnd=controller; data.uID=1; data.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP|NIF_SHOWTIP;
        data.uCallbackMessage=trayMessage; data.hIcon=LoadIconW(nullptr,IDI_APPLICATION);
        wcscpy_s(data.szTip,L"Evoly Aura · 浮光桌面");
        trayVersion4=false;
        if (Shell_NotifyIconW(NIM_ADD,&data)) {
            data.uVersion=NOTIFYICON_VERSION_4;
            trayVersion4=Shell_NotifyIconW(NIM_SETVERSION,&data)!=FALSE;
        }
    }
    void SetMode() {
        changingLayer=true;
        struct TransitionGuard { bool& flag; ~TransitionGuard() { flag=false; } } guard{changingLayer};
        desktopHost=DesktopRequested()?FindDesktopSurface():nullptr;
        if (DesktopRequested() && !desktopHost) Log("desktop_fallback_normal");
        for (auto& panel:panels) {
            if (!IsWindow(panel.window)) continue;
            auto layer=ResolveWindowLayer(DesktopRequested(),desktopHost!=nullptr,editing,panel.placement.pinned);
            bool child=layer==WindowLayer::desktop;
            if (IsIconic(panel.window)) ShowWindow(panel.window,SW_RESTORE);
            // Repeated reveal/hide must not detach and reattach an unchanged
            // window. In particular, revealed cards stay outside wallpaper hosts.
            const HWND wantedParent=child?desktopHost:nullptr;
            if (GetParent(panel.window)==wantedParent) {
                panel.attached=child;
                SetWindowPos(panel.window,layer==WindowLayer::topmost?HWND_TOPMOST:HWND_NOTOPMOST,
                    0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
                if (child) SetWindowPos(panel.window,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
                continue;
            }
            RECT r{}; GetWindowRect(panel.window,&r);
            ShowWindow(panel.window,SW_HIDE);
            SetParent(panel.window,nullptr);
            auto style=GetWindowLongPtrW(panel.window,GWL_STYLE)&~(WS_CHILD|WS_POPUP);
            SetWindowLongPtrW(panel.window,GWL_STYLE,style|WS_POPUP);
            // Explicitly clear the old topmost band before reparenting.
            SetWindowPos(panel.window,HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
            if (child) {
                SetWindowLongPtrW(panel.window,GWL_STYLE,style|WS_CHILD);
                SetLastError(0);
                HWND prior=SetParent(panel.window,desktopHost);
                if (!prior && GetLastError()!=0) {
                    child=false; Log("panel_attach_fallback");
                    SetParent(panel.window,nullptr);
                    SetWindowLongPtrW(panel.window,GWL_STYLE,style|WS_POPUP);
                }
            }
            panel.attached=child;
            POINT p{r.left,r.top};
            if (child) ScreenToClient(desktopHost,&p);
            HWND order=child?HWND_TOP:layer==WindowLayer::topmost?HWND_TOPMOST:HWND_NOTOPMOST;
            SetWindowPos(panel.window,order,p.x,p.y,r.right-r.left,r.bottom-r.top,
                SWP_NOACTIVATE|SWP_FRAMECHANGED|(visible?SWP_SHOWWINDOW:0));
        }
        InvalidateAll();
        Log(OnDesktop()?"desktop_mode":"floating_mode");
    }
    void EnsurePanels() {
        if (std::all_of(panels.begin(),panels.end(),[](const auto& p){return IsWindow(p.window)!=FALSE;})) return;
        auto placements=Placements();
        for (auto& p:panels) if (IsWindow(p.window)) DestroyWindow(p.window);
        renderer->ReleaseAll(); CreatePanels(placements); Log("panels_recreated");
    }
    void Show(bool show) {
        if (!show) { systemSampler.Stop(); systemSampleLogged=false; }
        visible=show;
        if (!show) { ++revealGeneration; temporaryReveal=false; revealQueued=false; }
        if (show) EnsurePanels();
        if (show && DesktopRequested() && !panels.empty()) renderer->LoadBackground(panels.front().window);
        if (show) SetMode();
        for (auto& panel:panels) ShowWindow(panel.window,show?SW_SHOWNOACTIVATE:SW_HIDE);
        if (show) {
            InvalidateAll();
            for (auto& panel:panels) UpdateWindow(panel.window);
            ScheduleMinute();
            systemSampler.Start(controller,systemSampleMessage);
        }
        else { KillTimer(controller,minuteTimer); renderer->ReleaseAll(); }
        Log(show?"shown":"hidden");
    }
    void RaiseFloating() {
        // Acquire foreground permission before raising all sibling tool windows.
        if (!panels.empty() && !GetParent(panels.front().window)) SetForegroundWindow(panels.front().window);
        for (auto& p:panels) if (IsWindow(p.window) && !GetParent(p.window))
            SetWindowPos(p.window,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);
    }
    void Reveal() {
        ++revealGeneration;
        revealQueued=false;
        temporaryReveal=desktop && !editing;
        Show(true); RaiseFloating(); Log("workspace_revealed");
    }
    void QueueReveal() {
        // Leave the Shell callback before activating/reparenting windows. A tray
        // selection always reveals; opening its overflow can already deactivate us.
        if (!revealQueued) revealQueued=PostMessageW(controller,revealMessage,++revealGeneration,0)!=FALSE;
    }
    void ToggleVisibility() {
        if (visible && (temporaryReveal || !desktop || editing)) Show(false);
        else Reveal();
    }
    void ChangeMode(bool wantDesktop) {
        ++revealGeneration; revealQueued=false;
        desktop=wantDesktop; editing=false; temporaryReveal=false;
        Show(true);
        if (!wantDesktop) RaiseFloating();
        Save();
    }
    void TogglePin(DesktopPanel& panel) {
        panel.placement.pinned=!panel.placement.pinned; SetMode(); Save();
    }
    void ScheduleMinute() {
        SYSTEMTIME time{}; GetLocalTime(&time);
        SetTimer(controller,minuteTimer,60000-time.wSecond*1000-time.wMilliseconds,nullptr);
    }
    void SetEditing(bool edit) {
        temporaryReveal=false;
        editing=edit;
        if (!edit) { checkedTasks|=1; Save(); }
        Show(true);
        Log(edit?"edit_started":"edit_finished");
    }
    void Reset() {
        auto defaults=DefaultWorkspace((work.right-work.left)/Scale(),(work.bottom-work.top)/Scale());
        for (size_t i=0;i<panels.size();++i) {
            panels[i].placement=defaults[i];
            MoveTo(panels[i],defaults[i].bounds);
        }
        SetMode(); Save(); InvalidateAll(); Log("layout_reset");
    }
    void MoveTo(DesktopPanel& panel,DipRect b) {
        POINT p{work.left+LONG(std::lround(b.x*Scale())),work.top+LONG(std::lround(b.y*Scale()))};
        HWND parent=GetParent(panel.window);
        if (parent) ScreenToClient(parent,&p);
        SetWindowPos(panel.window,nullptr,p.x,p.y,int(std::lround(b.width*Scale())),int(std::lround(b.height*Scale())),
            SWP_NOZORDER|SWP_NOACTIVATE);
    }
    void OpenKnown(REFKNOWNFOLDERID id) {
        PWSTR value=nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(id,0,nullptr,&value))) {
            ShellExecuteW(controller,L"open",value,nullptr,nullptr,SW_SHOWNORMAL);
            CoTaskMemFree(value);
        }
    }
    void Action(WidgetAction action) {
        switch (action) {
        case WidgetAction::editLayout: SetEditing(!editing); break;
        case WidgetAction::resetLayout: Reset(); break;
        case WidgetAction::editNote: EditNote(); break;
        case WidgetAction::focus:
            if (focusRunning) {
                remaining=std::max(0,int(std::chrono::ceil<std::chrono::seconds>(focusEnd-std::chrono::steady_clock::now()).count()));
                focusRunning=false; KillTimer(controller,focusTimer);
            } else {
                if (remaining<=0) remaining=1500;
                focusEnd=std::chrono::steady_clock::now()+std::chrono::seconds(remaining);
                focusRunning=true; SetTimer(controller,focusTimer,1000,nullptr); checkedTasks|=4; Save();
            }
            InvalidateAll(); Log(focusRunning?"focus_started":"focus_paused"); break;
        case WidgetAction::launchExplorer: ShellExecuteW(controller,L"open",L"explorer.exe",nullptr,nullptr,SW_SHOWNORMAL); break;
        case WidgetAction::launchDownloads: OpenKnown(FOLDERID_Downloads); break;
        case WidgetAction::launchDocuments: OpenKnown(FOLDERID_Documents); break;
        case WidgetAction::launchGithub: ShellExecuteW(controller,L"open",L"https://github.com/Azl369/Evoly_Aura",nullptr,nullptr,SW_SHOWNORMAL); break;
        case WidgetAction::launchSettings: ShellExecuteW(controller,L"open",L"ms-settings:",nullptr,nullptr,SW_SHOWNORMAL); break;
        case WidgetAction::launchRecycle: ShellExecuteW(controller,L"open",L"shell:RecycleBinFolder",nullptr,nullptr,SW_SHOWNORMAL); break;
        default:
            if (action>=WidgetAction::task0 && action<=WidgetAction::task2) {
                checkedTasks^=1<<(int(action)-int(WidgetAction::task0)); Save(); InvalidateAll();
            }
            if (action>=WidgetAction::theme0 && action<=WidgetAction::theme3) {
                theme=int(action)-int(WidgetAction::theme0); Save(); InvalidateAll(); Log("theme_changed");
            }
            if (action==WidgetAction::clearer || action==WidgetAction::frostier) {
                opacity=std::clamp(opacity+(action==WidgetAction::clearer?-.08f:.08f),.30f,.85f);
                Save(); InvalidateAll(); Log("opacity_changed");
            }
        }
    }
    void Menu(HWND owner) {
        HMENU menu=CreatePopupMenu();
        AppendMenuW(menu,MF_STRING|(editing?MF_CHECKED:0),104,L"编辑布局");
        if (auto* panel=Panel(owner); panel && panel->placement.kind!=WidgetKind::toolbar)
            AppendMenuW(menu,MF_STRING|(panel->placement.pinned?MF_CHECKED:0),108,L"单独置顶此卡片");
        AppendMenuW(menu,MF_STRING,105,L"重置布局");
        AppendMenuW(menu,MF_STRING,100,L"唤起卡片");
        AppendMenuW(menu,MF_STRING|(visible?0:MF_GRAYED),101,L"隐藏卡片");
        AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
        AppendMenuW(menu,MF_STRING,107,L"退出 Evoly Aura");
        POINT point{}; GetCursorPos(&point); SetForegroundWindow(controller);
        UINT command=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,controller,nullptr);
        DestroyMenu(menu);
        if (command==108) {
            if (auto* panel=Panel(owner)) TogglePin(*panel);
        } else if (command==100) QueueReveal();
        else if (command) PostMessageW(controller,controlMessage,command,0);
        PostMessageW(controller,WM_NULL,0,0);
    }
    static LRESULT CALLBACK WindowProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
        auto* self=reinterpret_cast<DesktopApp*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
        if (msg==WM_NCCREATE) {
            self=static_cast<DesktopApp*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
            SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
        }
        if (!self) return DefWindowProcW(hwnd,msg,wp,lp);
        try { return self->Message(hwnd,msg,wp,lp); }
        catch (const std::exception& error) {
            self->Log(error.what()); self->failed=true; PostQuitMessage(1); return 0;
        }
    }
    void Drag(HWND hwnd,POINT point) {
        auto* panel=Panel(hwnd);
        if (!panel) return;
        int dx=point.x-dragStart.x,dy=point.y-dragStart.y;
        RECT r=dragBounds;
        if (resizing) {
            auto minimum=MinimumWidgetSize(panel->placement.kind);
            int minW=int(minimum.width*Scale()),minH=int(minimum.height*Scale());
            int maxW=std::max(minW,std::min(int(1800*Scale()),int(work.right-r.left)));
            int maxH=std::max(minH,std::min(int(1000*Scale()),int(work.bottom-r.top)));
            r.right=r.left+std::clamp(int(r.right-r.left)+dx,minW,maxW);
            if (panel->placement.kind!=WidgetKind::toolbar)
                r.bottom=r.top+std::clamp(int(r.bottom-r.top)+dy,minH,maxH);
        } else {
            int width=r.right-r.left,height=r.bottom-r.top;
            float x=float(r.left+dx),y=float(r.top+dy);
            std::vector<float> xs{float(work.left+16),float(work.right-width-16)};
            std::vector<float> ys{float(work.top+16),float(work.bottom-height-16)};
            for (const auto& other:panels) if (other.window!=hwnd) {
                RECT o{}; GetWindowRect(other.window,&o);
                float gap=20*Scale();
                xs.insert(xs.end(),{float(o.left),o.right+gap,o.left-width-gap});
                ys.insert(ys.end(),{float(o.top),o.bottom+gap,o.top-height-gap});
            }
            x=SnapCoordinate(x,xs,10*Scale()); y=SnapCoordinate(y,ys,10*Scale());
            r.left=std::clamp(int(x),int(work.left),std::max(int(work.left),int(work.right)-width));
            r.top=std::clamp(int(y),int(work.top),std::max(int(work.top),int(work.bottom)-height));
            r.right=r.left+width; r.bottom=r.top+height;
        }
        SetWindowPos(hwnd,nullptr,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_NOZORDER|SWP_NOACTIVATE);
        InvalidateRect(hwnd,nullptr,FALSE); UpdateWindow(hwnd);
    }
    LRESULT Message(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
        if (msg==WM_KEYDOWN && wp==VK_ESCAPE) {
            if (temporaryReveal) Show(false);
            else if (editing) SetEditing(false);
            return 0;
        }
        if (hwnd==controller) {
            if (msg==taskbarCreated && taskbarCreated) {
                AddTray();
                bool lost=std::any_of(panels.begin(),panels.end(),[](const auto& p){return !IsWindow(p.window);});
                if (lost) {
                    auto placements=Placements();
                    for (auto& p:panels) if (IsWindow(p.window)) DestroyWindow(p.window);
                    renderer->ReleaseAll(); CreatePanels(placements);
                }
                Show(visible); Log("shell_recovered"); return 0;
            }
            switch (msg) {
            case systemSampleMessage:
                if (visible) {
                    const auto snapshot=systemSampler.Read();
                    if (log && !systemSampleLogged && snapshot.cpuValid && snapshot.memoryValid) {
                        systemSampleLogged=true;
                        log<<"system_sample cpu="<<(snapshot.cpuValid?snapshot.cpuPercent:-1)
                        <<" gpu="<<(snapshot.gpuValid?snapshot.gpuPercent:-1)
                        <<" available="<<snapshot.availableMemory<<" total="<<snapshot.totalMemory<<'\n'; log.flush(); }
                    for (auto& p:panels) if (p.placement.kind==WidgetKind::systemMonitor) InvalidateRect(p.window,nullptr,FALSE);
                }
                return 0;
            case trayMessage: {
                const UINT event=trayVersion4?LOWORD(lp):UINT(lp);
                const UINT icon=trayVersion4?HIWORD(lp):UINT(wp);
                if (icon!=1) return 0;
                if (log && (event==NIN_SELECT || event==NIN_KEYSELECT || event==WM_CONTEXTMENU || event==WM_RBUTTONUP)) {
                    log<<"tray_event "<<event<<'\n'; log.flush();
                }
                if (trayVersion4?(event==NIN_SELECT || event==NIN_KEYSELECT):event==WM_LBUTTONUP) QueueReveal();
                if (trayVersion4?event==WM_CONTEXTMENU:event==WM_RBUTTONUP) Menu(hwnd);
                return 0;
            }
            case revealMessage:
                if (wp==revealGeneration) { revealQueued=false; Reveal(); }
                return 0;
            case controlMessage:
                if (wp==100) Reveal();
                else if (wp==101) Show(false);
                else if (wp==102 || wp==103) ChangeMode(wp==102);
                else if (wp==104) SetEditing(!editing);
                else if (wp==105) Reset();
                else if (wp==106) Action(WidgetAction(int(WidgetAction::theme0)+(theme+1)%4));
                else if (wp==107) { Save(); PostQuitMessage(0); }
                else if (wp==109) Action(WidgetAction::focus);
                else if (wp==110) EditNote();
                else if (wp>=120 && wp<120+panels.size()) TogglePin(panels[size_t(wp-120)]);
                return 0;
            case WM_HOTKEY:
                if (wp==1) ToggleVisibility();
                else if (wp==3) { Save(); PostQuitMessage(0); }
                else if (wp==4) SetEditing(!editing);
                return 0;
            case WM_TIMER:
                if (wp==minuteTimer && visible) {
                    for (auto& p:panels) if (p.placement.kind==WidgetKind::clock) InvalidateRect(p.window,nullptr,FALSE);
                    ScheduleMinute();
                }
                if (wp==focusTimer && focusRunning) {
                    remaining=std::max(0,int(std::chrono::ceil<std::chrono::seconds>(focusEnd-std::chrono::steady_clock::now()).count()));
                    if (!remaining) { focusRunning=false; KillTimer(hwnd,focusTimer); Log("focus_finished"); }
                    if (visible) for (auto& p:panels) if (p.placement.kind==WidgetKind::focus) InvalidateRect(p.window,nullptr,FALSE);
                }
                if (wp==durationTimer) { Save(); PostQuitMessage(0); }
                return 0;
            case WM_SETTINGCHANGE:
            case WM_DISPLAYCHANGE:
                SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
                if (visible && !panels.empty()) { renderer->LoadBackground(panels[0].window); InvalidateAll(); }
                return 0;
            case WM_CLOSE: Save(); PostQuitMessage(0); return 0;
            }
            return DefWindowProcW(hwnd,msg,wp,lp);
        }
        auto* panel=Panel(hwnd);
        switch (msg) {
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint{}; BeginPaint(hwnd,&paint); EndPaint(hwnd,&paint);
            if (panel && visible) Paint(*panel);
            return 0;
        }
        case WM_SIZE: InvalidateRect(hwnd,nullptr,FALSE); return 0;
        case WM_NCHITTEST: return HTCLIENT;
        case WM_MOUSEMOVE: {
            POINT cursor{}; GetCursorPos(&cursor);
            if (dragging==hwnd && GetCapture()==hwnd) { Drag(hwnd,cursor); return 0; }
            if (panel) {
                RECT r{}; GetClientRect(hwnd,&r);
                float s=GetDpiForWindow(hwnd)/96.f,x=GET_X_LPARAM(lp)/s,y=GET_Y_LPARAM(lp)/s;
                auto hovered=HitWidgetAction(panel->placement.kind,x,y,r.right/s,r.bottom/s);
                if (hovered!=panel->hovered) {
                    panel->hovered=hovered; InvalidateRect(hwnd,nullptr,FALSE);
                    TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,hwnd,0}; TrackMouseEvent(&track);
                }
                auto cursorId=hovered!=WidgetAction::none?IDC_HAND:
                    editing && x>r.right/s-24 && y>r.bottom/s-24?IDC_SIZENWSE:
                    editing && y<54?IDC_SIZEALL:IDC_ARROW;
                SetCursor(LoadCursorW(nullptr,cursorId));
            }
            return 0;
        }
        case WM_MOUSELEAVE:
            if (panel) { panel->hovered=WidgetAction::none; InvalidateRect(hwnd,nullptr,FALSE); }
            return 0;
        case WM_LBUTTONDOWN:
            if (panel) {
                RECT r{}; GetClientRect(hwnd,&r);
                float s=GetDpiForWindow(hwnd)/96.f,x=GET_X_LPARAM(lp)/s,y=GET_Y_LPARAM(lp)/s;
                auto action=HitWidgetAction(panel->placement.kind,x,y,r.right/s,r.bottom/s);
                if (action!=WidgetAction::none) { Action(action); return 0; }
                if (editing && (y<54 || (x>r.right/s-26 && y>r.bottom/s-26))) {
                    dragging=hwnd; resizing=y>=54;
                    GetCursorPos(&dragStart); GetWindowRect(hwnd,&dragBounds); SetCapture(hwnd);
                }
            }
            return 0;
        case WM_LBUTTONUP:
            if (dragging==hwnd) { dragging=nullptr; ReleaseCapture(); Save(); Log(resizing?"panel_resized":"panel_moved"); }
            return 0;
        case WM_CAPTURECHANGED: dragging=nullptr; return 0;
        case WM_LBUTTONDBLCLK:
            if (panel && panel->placement.kind==WidgetKind::notes) EditNote();
            return 0;
        case WM_CONTEXTMENU: Menu(hwnd); return 0;
        case WM_CLOSE: Show(false); return 0;
        }
        return DefWindowProcW(hwnd,msg,wp,lp);
    }
    static LRESULT CALLBACK NoteProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
        auto* self=reinterpret_cast<DesktopApp*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
        if (msg==WM_NCCREATE) {
            self=static_cast<DesktopApp*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
            SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
        }
        if (!self) return DefWindowProcW(hwnd,msg,wp,lp);
        if (msg==WM_COMMAND) {
            if (LOWORD(wp)==1) {
                int length=GetWindowTextLengthW(self->noteInput);
                std::wstring value(size_t(length)+1,L'\0');
                GetWindowTextW(self->noteInput,value.data(),length+1); value.resize(size_t(length));
                try {
                    self->store->SaveNote(value); self->note=value; self->checkedTasks|=2;
                    self->Save(); self->InvalidateAll(); self->Log("note_saved"); DestroyWindow(hwnd);
                } catch (const std::exception&) {
                    MessageBoxW(hwnd,L"保存失败，内容仍保留在编辑框中。",L"Evoly Aura",MB_OK|MB_ICONWARNING);
                }
            } else if (LOWORD(wp)==2) DestroyWindow(hwnd);
            return 0;
        }
        if (msg==WM_SIZE && self->noteInput) {
            int w=LOWORD(lp),h=HIWORD(lp);
            MoveWindow(self->noteInput,16,16,std::max(80,w-32),std::max(60,h-82),TRUE);
            MoveWindow(GetDlgItem(hwnd,1),std::max(16,w-194),h-50,82,32,TRUE);
            MoveWindow(GetDlgItem(hwnd,2),std::max(104,w-98),h-50,82,32,TRUE);
            return 0;
        }
        if (msg==WM_DESTROY) { self->noteEditor=nullptr; self->noteInput=nullptr; return 0; }
        return DefWindowProcW(hwnd,msg,wp,lp);
    }
    void EditNote() {
        if (noteEditor) { SetForegroundWindow(noteEditor); return; }
        noteEditor=CreateWindowExW(WS_EX_TOOLWINDOW|(editing?WS_EX_TOPMOST:0),L"AuraNoteEditor",L"编辑便签 · Evoly Aura",
            WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_THICKFRAME,work.left+280,work.top+180,560,380,
            nullptr,nullptr,GetModuleHandleW(nullptr),this);
        if (!noteEditor) throw std::runtime_error("Note editor creation failed");
        noteInput=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",note.c_str(),
            WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN|WS_VSCROLL,
            16,16,510,240,noteEditor,reinterpret_cast<HMENU>(3),GetModuleHandleW(nullptr),nullptr);
        SendMessageW(noteInput,EM_LIMITTEXT,8192,0);
        HFONT font=static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        SendMessageW(noteInput,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
        for (int i=1;i<=2;++i) {
            HWND button=CreateWindowW(L"BUTTON",i==1?L"保存":L"取消",WS_CHILD|WS_VISIBLE|WS_TABSTOP,
                330+(i-1)*96,280,82,32,noteEditor,reinterpret_cast<HMENU>(INT_PTR(i)),GetModuleHandleW(nullptr),nullptr);
            SendMessageW(button,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
        }
        RECT r{}; GetClientRect(noteEditor,&r); SendMessageW(noteEditor,WM_SIZE,0,MAKELPARAM(r.right,r.bottom));
        ShowWindow(noteEditor,SW_SHOW); SetForegroundWindow(noteEditor); SetFocus(noteInput);
    }
    void CreatePanels(const std::vector<WidgetPlacement>& placements) {
        panels.clear(); panels.resize(placements.size());
        for (size_t i=0;i<placements.size();++i) {
            auto& panel=panels[i]; panel.placement=placements[i];
            auto b=placements[i].bounds;
            int width=std::min(int(std::lround(b.width*Scale())),int(work.right-work.left));
            int height=std::min(int(std::lround(b.height*Scale())),int(work.bottom-work.top));
            int x=std::clamp(int(work.left)+int(std::lround(b.x*Scale())),int(work.left),std::max(int(work.left),int(work.right)-width));
            int y=std::clamp(int(work.top)+int(std::lround(b.y*Scale())),int(work.top),std::max(int(work.top),int(work.bottom)-height));
            panel.window=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOOLWINDOW,L"AuraWorkspacePanel",L"Evoly Aura",
                WS_POPUP,x,y,width,height,nullptr,nullptr,GetModuleHandleW(nullptr),this);
            if (!panel.window) throw std::runtime_error("Workspace panel creation failed");
        }
    }
    int Run() {
        instance=CreateMutexW(nullptr,TRUE,L"Local\\EvolyAuraDesktop");
        if (!instance) throw std::runtime_error("Workspace mutex failed");
        if (GetLastError()==ERROR_ALREADY_EXISTS) {
            HWND existing=FindWindowW(L"AuraWorkspaceController",nullptr);
            if (existing) {
                DWORD process=0; GetWindowThreadProcessId(existing,&process);
                if (process) AllowSetForegroundWindow(process);
                DWORD_PTR result{};
                SendMessageTimeoutW(existing,controlMessage,100,0,SMTO_ABORTIFHUNG,2000,&result);
            }
            return 0;
        }
        if (!options.log.empty()) log.open(options.log);
        desktop=!options.preview;
        systemDpi=GetDpiForSystem();
        SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
        store=std::make_unique<WorkspaceStore>(options.dataDirectory.empty()?UserDataDirectory():options.dataDirectory);
        auto placements=DefaultWorkspace((work.right-work.left)/Scale(),(work.bottom-work.top)/Scale());
        store->Load(placements,theme,opacity,checkedTasks); note=store->LoadNote();
        renderer=std::make_unique<Renderer>(Backend::software);
        WNDCLASSW cls{}; cls.hInstance=GetModuleHandleW(nullptr); cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);
        cls.lpfnWndProc=WindowProc; cls.lpszClassName=L"AuraWorkspaceController";
        if (!RegisterClassW(&cls)) throw std::runtime_error("Workspace controller class failed");
        cls.style=CS_DBLCLKS; cls.lpszClassName=L"AuraWorkspacePanel";
        if (!RegisterClassW(&cls)) throw std::runtime_error("Workspace panel class failed");
        cls.style=0; cls.lpfnWndProc=NoteProc; cls.lpszClassName=L"AuraNoteEditor";
        cls.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
        if (!RegisterClassW(&cls)) throw std::runtime_error("Note editor class failed");
        controller=CreateWindowExW(WS_EX_TOOLWINDOW,L"AuraWorkspaceController",L"Evoly Aura Controller",WS_POPUP,
            0,0,0,0,nullptr,nullptr,cls.hInstance,this);
        if (!controller) throw std::runtime_error("Workspace controller creation failed");
        taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");
        AddTray();
        RegisterHotKey(controller,1,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,VK_F10);
        RegisterHotKey(controller,3,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,VK_F12);
        RegisterHotKey(controller,4,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,VK_F9);
        CreatePanels(placements);
        renderer->LoadBackground(panels.front().window);
        Show(true);
        if (options.preview) RaiseFloating();
        if (options.duration) SetTimer(controller,durationTimer,options.duration*1000,nullptr);
        Log("workspace_ready");
        MSG message{}; BOOL result{};
        while ((result=GetMessageW(&message,nullptr,0,0))>0) {
            if (noteEditor && IsDialogMessageW(noteEditor,&message)) continue;
            TranslateMessage(&message); DispatchMessageW(&message);
        }
        Log(failed?"workspace_failed":"workspace_exit");
        return failed || result==-1?1:int(message.wParam);
    }
};
}
int RunDesktop(const DesktopOptions& options) {
    Check(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED),"Workspace COM");
    int result;
    try { DesktopApp app; app.options=options; result=app.Run(); }
    catch (...) { CoUninitialize(); throw; }
    CoUninitialize(); return result;
}
}
