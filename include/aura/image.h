#pragma once
#include <cstdint>
#include <vector>

namespace aura {
struct Image {
    unsigned width = 0, height = 0;
    std::vector<uint32_t> pixels;
};
enum class WallpaperFit { fill, fit, stretch, center };
Image MapWallpaper(const Image& source, unsigned width, unsigned height,
                   WallpaperFit fit, uint32_t background,
                   unsigned physicalWidth = 0, unsigned physicalHeight = 0,
                   unsigned sourceWidth = 0, unsigned sourceHeight = 0);
void BoxBlur(Image& image, unsigned radius);
}
