#pragma once
#include <filesystem>
namespace aura {
struct DesktopOptions {
    bool preview = false;
    unsigned duration = 0;
    std::filesystem::path dataDirectory;
    std::filesystem::path log;
};
int RunDesktop(const DesktopOptions& options);
}
