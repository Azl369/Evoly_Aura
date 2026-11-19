#include "aura/desktop_app.h"
#include <windows.h>
#include <shellapi.h>
#include <vector>
#include <string>
#include <stdexcept>
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int) {
    int count{};
    auto raw=CommandLineToArgvW(GetCommandLineW(),&count);
    if (!raw) return 2;
    std::vector<std::wstring> args;
    for (int i=1;i<count;++i) args.emplace_back(raw[i]);
    LocalFree(raw);
    aura::DesktopOptions options;
    try {
        for (size_t i=0;i<args.size();++i) {
            auto next=[&]() { if (++i>=args.size()) throw std::invalid_argument("Missing option value"); return args[i]; };
            if (args[i]==L"--preview") options.preview=true;
            else if (args[i]==L"--duration") {
                auto text=next(); size_t used{}; auto value=std::stoul(text,&used);
                if (used!=text.size() || value>86400) throw std::invalid_argument("Invalid duration");
                options.duration=unsigned(value);
            } else if (args[i]==L"--data-dir") options.dataDirectory=next();
            else if (args[i]==L"--log") options.log=next();
            else throw std::invalid_argument("Unknown option");
        }
        return aura::RunDesktop(options);
    } catch (const std::exception& error) {
        if (!options.duration) MessageBoxA(nullptr,error.what(),"Evoly Aura",MB_OK|MB_ICONERROR);
        OutputDebugStringA(error.what()); return 1;
    }
}
