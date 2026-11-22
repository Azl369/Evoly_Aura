#include "aura/system_sampler.h"
#include "aura/render.h"
#include <iostream>
#include <stdexcept>

void Require(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
LRESULT CALLBACK PreviewProc(HWND window,UINT message,WPARAM wp,LPARAM lp) {
    if(message==WM_NCHITTEST) return HTCLIENT;
    if(message==WM_ERASEBKGND) return 1;
    if(message==WM_PAINT) { PAINTSTRUCT paint{}; BeginPaint(window,&paint); EndPaint(window,&paint); return 0; }
    return DefWindowProcW(window,message,wp,lp);
}
int Preview() {
    const float scale=GetDpiForSystem()/96.f;
    RECT work{}; SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
    WNDCLASSW cls{}; cls.lpfnWndProc=PreviewProc; cls.hInstance=GetModuleHandleW(nullptr);
    cls.lpszClassName=L"AuraSystemPreview"; RegisterClassW(&cls);
    auto background=CreateSolidBrush(RGB(28,43,48));
    WNDCLASSW backingClass{}; backingClass.hInstance=cls.hInstance; backingClass.lpfnWndProc=DefWindowProcW;
    backingClass.lpszClassName=L"AuraSystemPreviewBacking"; backingClass.hbrBackground=background;
    RegisterClassW(&backingClass);
    auto backing=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST,backingClass.lpszClassName,L"",WS_POPUP,
        work.right-int(388*scale),work.top+int(28*scale),int(328*scale),int(204*scale),nullptr,nullptr,cls.hInstance,nullptr);
    Require(backing!=nullptr,"Preview backing creation");
    ShowWindow(backing,SW_SHOWNOACTIVATE);
    SetWindowPos(backing,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);
    auto window=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOOLWINDOW|WS_EX_TOPMOST,cls.lpszClassName,L"Aura system preview",WS_POPUP,
        work.right-int(388*scale),work.top+int(28*scale),int(328*scale),int(204*scale),backing,nullptr,cls.hInstance,nullptr);
    Require(window!=nullptr,"Preview creation");
    aura::Renderer renderer(aura::Backend::software);
    aura::SystemSampler sampler;
    aura::WidgetView view; view.kind=aura::WidgetKind::systemMonitor; view.sampleWallpaper=false; view.theme=2;
    ShowWindow(window,SW_SHOWNOACTIVATE);
    SetWindowPos(window,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);
    renderer.Paint(window,0,aura::Material::wallpaper,false,&view);
    SetForegroundWindow(window);
    sampler.Start(window,WM_APP+5);
    const auto end=GetTickCount64()+12000;
    while(GetTickCount64()<end) {
        MSG event{};
        while(GetTickCount64()<end && PeekMessageW(&event,nullptr,0,0,PM_REMOVE)) {
            if(event.message==WM_APP+5) {
                view.system=sampler.Read(); renderer.Paint(window,0,aura::Material::wallpaper,false,&view);
            } else { TranslateMessage(&event); DispatchMessageW(&event); }
        }
        Sleep(10);
    }
    sampler.Stop(); DestroyWindow(window); DestroyWindow(backing); DeleteObject(background); return 0;
}
int wmain(int argc,wchar_t** argv) {
    try {
        if(argc==2 && std::wstring_view(argv[1])==L"--preview") return Preview();
        auto window=CreateWindowExW(0,L"STATIC",L"Aura sampler test",0,0,0,0,0,HWND_MESSAGE,nullptr,nullptr,nullptr);
        Require(window!=nullptr,"Test receiver creation");
        aura::SystemSampler sampler;
        constexpr UINT message=WM_APP+5;
        sampler.Start(window,message);
        auto wait=[&](DWORD duration) {
            auto end=GetTickCount64()+duration;
            MSG event{}; unsigned count=0;
            while (GetTickCount64()<end) {
                while (PeekMessageW(&event,window,message,message,PM_REMOVE)) ++count;
                Sleep(10);
            }
            return count;
        };
        Require(wait(4700)>=2,"Timer publishes actual samples");
        auto snapshot=sampler.Read();
        Require(snapshot.cpuValid && snapshot.cpuPercent>=0 && snapshot.cpuPercent<=100,"CPU sample range after warmup");
        Require(snapshot.memoryValid && snapshot.totalMemory>0 && snapshot.availableMemory<=snapshot.totalMemory,"Physical memory sample");
        MEMORYSTATUSEX memory{sizeof(memory)}; GlobalMemoryStatusEx(&memory);
        Require(memory.ullTotalPhys==snapshot.totalMemory,"System physical memory total matches Windows");
        Require(!snapshot.gpuValid || (snapshot.gpuPercent>=0 && snapshot.gpuPercent<=100),"GPU unavailable or bounded");
        std::cout<<"CPU="<<snapshot.cpuPercent<<" GPU="<<(snapshot.gpuValid?snapshot.gpuPercent:-1)
            <<" available="<<snapshot.availableMemory<<" total="<<snapshot.totalMemory<<'\n';
        sampler.Stop();
        wait(100); // Drain notifications already delivered before Stop.
        Require(wait(2200)==0,"Hidden sampler stops notifications");
        Require(!sampler.Read().memoryValid,"Hidden sampler clears stale values");
        sampler.Start(window,message);
        Require(wait(2600)>=1,"Sampler resumes after hide");
        Require(sampler.Read().memoryValid,"Resumed physical memory sample");
        sampler.Stop(); DestroyWindow(window);
        return 0;
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
