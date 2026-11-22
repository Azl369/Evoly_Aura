#include "aura/probe.h"
#include <shellapi.h>
#include <filesystem>
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int count{};
    LPWSTR* raw = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!raw) return 2;
    std::vector<std::wstring> arguments;
    for (int i = 1; i < count; ++i) arguments.emplace_back(raw[i]);
    LocalFree(raw);
    aura::Options options;
    try {
        options = aura::ParseOptions(arguments);
        return aura::RunProbe(options);
    } catch (const std::exception& e) {
        OutputDebugStringA(e.what());
        if (!options.log.empty()) {
            std::ofstream file(std::filesystem::path(options.log), std::ios::app);
            file << "{\"event\":\"startup_error\",\"detail\":\"" << e.what() << "\"}\n";
        }
        if (!options.duration) MessageBoxA(nullptr, e.what(), "Aura P0 startup error", MB_OK | MB_ICONERROR);
        return 1;
    }
}
