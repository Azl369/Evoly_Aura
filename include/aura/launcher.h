#pragma once
#include "aura/image.h"
#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace aura {
constexpr size_t launcherPageSize=6,launcherEntryLimit=256;
struct LauncherEntry {
    std::wstring id,name,target;
    int builtin=-1;
};
struct LauncherTile {
    std::wstring name;
    std::shared_ptr<const Image> image;
    int builtin=-1;
};
std::vector<LauncherEntry> DefaultLaunchers();
bool ValidLauncher(const LauncherEntry& entry);
size_t LauncherPages(size_t count);
class LauncherStore {
public:
    explicit LauncherStore(std::filesystem::path directory);
    std::vector<LauncherEntry> Load() const;
    void Save(const std::vector<LauncherEntry>& entries) const;
private:
    std::filesystem::path file_;
};
}
