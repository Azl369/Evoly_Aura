#include "aura/launcher.h"
#include "aura/launcher_icons.h"
#include <shlobj.h>
#include <wrl/client.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int wmain(int argc,wchar_t** argv){
 try{
    Require(argc==2,"Pass an isolated test directory");
    auto root=std::filesystem::path(argv[1])/(L"launcher-"+std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(root);
    aura::LauncherStore store(root);
    auto entries=store.Load();Require(entries.size()==6,"Missing config migrates original six entries");
    entries.push_back({L"custom",L"测试应用 \"一\"",L"D:\\Program Files\\测试.lnk",-1});
    store.Save(entries);auto loaded=store.Load();
    Require(loaded.size()==7 && loaded.back().name==entries.back().name && loaded.back().target==entries.back().target,"Chinese quoted names and paths roundtrip");
    std::swap(loaded[0],loaded[6]);store.Save(loaded);
    Require(store.Load().front().id==L"custom","Reorder persists");
    bool invalid=false;loaded.push_back(loaded.front());
    try{store.Save(loaded);}catch(const std::runtime_error&){invalid=true;}
    Require(invalid && store.Load().size()==7,"Invalid mutation preserves old config");
    store.Save({});Require(store.Load().empty(),"Empty launcher survives restart");
    Require(aura::LauncherPages(0)==1 && aura::LauncherPages(6)==1 && aura::LauncherPages(7)==2 && aura::LauncherPages(256)==43,"Page boundaries");
    Require(SUCCEEDED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)),"COM initialization");
    wchar_t system[4096]{};GetSystemDirectoryW(system,4096);
    const auto iconFile=std::filesystem::path(system)/L"shell32.dll";
    auto window=CreateWindowExW(0,L"STATIC",L"Launcher icon test",0,0,0,0,0,HWND_MESSAGE,nullptr,nullptr,nullptr);
    Require(window!=nullptr,"Icon receiver");
    aura::LauncherIcons icons;icons.Start(window,WM_APP+6,root/L"icon-cache");icons.Resume();
    auto icon=[&](const std::wstring& source,unsigned size){
        auto end=GetTickCount64()+5000;std::shared_ptr<const aura::Image> image;
        while(GetTickCount64()<end && !(image=icons.Get(source,size))){MSG msg{};while(PeekMessageW(&msg,window,WM_APP+6,WM_APP+6,PM_REMOVE)){}Sleep(10);}
        Require(image!=nullptr,"Actual Windows icon extraction");return image;
    };
    auto first=icon(iconFile.wstring(),64);
    Require(first->width==64 && first->height==64 && std::any_of(first->pixels.begin(),first->pixels.end(),[](uint32_t pixel){return (pixel>>24)>0;}),"Icon has actual pixels");
    Require(icons.Get(iconFile.wstring(),64)==first,"Cache shares icon pixels");
    for(int i=0;i<64;++i){
        auto path=root/(L"test"+std::to_wstring(i)+L".lnk");
        Microsoft::WRL::ComPtr<IShellLinkW> link;Microsoft::WRL::ComPtr<IPersistFile> persist;
        Require(SUCCEEDED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&link))),"Create test shortcut");
        link->SetPath((std::filesystem::path(system)/L"notepad.exe").c_str());link->SetIconLocation(iconFile.c_str(),i%16);
        Require(SUCCEEDED(link.As(&persist)) && SUCCEEDED(persist->Save(path.c_str(),TRUE)),"Save test shortcut");
        auto image=icon(path.wstring(),128);
        auto stats=icons.ReadStats();Require(stats.bytes<=aura::LauncherIcons::memoryLimit && stats.entries<=aura::LauncherIcons::entryLimit,"LRU cache bound");
    }
    auto stats=icons.ReadStats();Require(stats.bytes>2*1024*1024,"LRU test exercised a substantial cache");
    std::cout<<"Bounded CPU icon bytes="<<stats.bytes<<" entries="<<stats.entries<<'\n';
    icons.Pause();Require(icons.ReadStats().bytes==0 && icons.ReadStats().queued==0,"Hide releases cache and queue");
    icons.Resume();auto restored=icon(iconFile.wstring(),64);Require(restored->pixels==first->pixels,"Disk cache roundtrip");
    {std::ofstream keep(root/L"icon-cache"/L"keep.txt");keep<<"not owned";}
    icons.Refresh();icon(iconFile.wstring(),64);
    Require(std::filesystem::exists(root/L"icon-cache"/L"keep.txt"),"Refresh preserves unrelated files");
    icons.Stop();DestroyWindow(window);CoUninitialize();
    std::cout<<"Launcher persistence, migration, paging, actual icons, cache eviction and hide/resume passed\n";
    return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
