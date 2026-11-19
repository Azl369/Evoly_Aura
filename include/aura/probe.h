#pragma once
#include "aura/render.h"
#include <fstream>

namespace aura {
struct Options {
    Backend backend = Backend::software;
    Material material = Material::wallpaper;
    unsigned panels = 6, duration = 0;
    bool desktop = false, hidden = false, exercise = false;
    std::wstring log;
};
Options ParseOptions(const std::vector<std::wstring>& arguments);
int RunProbe(const Options& options);
}
