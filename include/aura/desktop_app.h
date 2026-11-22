#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace aura {
struct DesktopOptions {
    bool preview = false;
    unsigned duration = 0;
    std::filesystem::path dataDirectory;
    std::filesystem::path log;
    std::vector<std::wstring> shortcutImports;
};
int RunDesktop(const DesktopOptions& options);
}
