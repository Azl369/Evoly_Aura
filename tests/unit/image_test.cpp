#include "aura/image.h"
#include <iostream>
#include <stdexcept>
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
int main() {
    try {
        aura::Image image{3, 2, std::vector<uint32_t>(6, 0xff123456)};
        aura::BoxBlur(image, 4);
        for (auto pixel : image.pixels) Require(pixel == 0xff123456, "Blur must preserve constant edges");
        aura::Image split{2, 1, {0xff000000, 0xffffffff}};
        auto stretch = aura::MapWallpaper(split, 4, 2, aura::WallpaperFit::stretch, 0xff112233);
        Require(stretch.pixels[0] == 0xff000000 && stretch.pixels[3] == 0xffffffff, "Stretch sampling");
        auto fit = aura::MapWallpaper(split, 2, 4, aura::WallpaperFit::fit, 0xff112233);
        Require(fit.pixels[0] == 0xff112233, "Fit letterbox");
        aura::Image impulse{3, 1, {0xff000000, 0xffffffff, 0xff000000}};
        aura::BoxBlur(impulse, 1);
        for (auto pixel : impulse.pixels) Require(pixel == 0xff555555, "Blur clamped edge average");
        bool rejected = false;
        try { aura::MapWallpaper(split, 4096, 1, aura::WallpaperFit::fill, 0); }
        catch (const std::invalid_argument&) { rejected = true; }
        Require(rejected, "Bounded output dimensions");
        std::cout << "Image mapping, bounded allocation and blur tests passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
