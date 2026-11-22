#include "aura/image.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace aura {
Image MapWallpaper(const Image& source, unsigned width, unsigned height,
                   WallpaperFit fit, uint32_t background, unsigned physicalWidth,
                   unsigned physicalHeight, unsigned sourceWidth, unsigned sourceHeight) {
    if (!width || !height || width > 2048 || height > 2048 ||
        !source.width || !source.height ||
        source.pixels.size() != size_t(source.width) * source.height)
        throw std::invalid_argument("Invalid bounded image dimensions");
    Image out{width, height, std::vector<uint32_t>(size_t(width) * height, background)};
    double sw = sourceWidth ? sourceWidth : source.width;
    double sh = sourceHeight ? sourceHeight : source.height;
    double mw = physicalWidth ? physicalWidth : width;
    double mh = physicalHeight ? physicalHeight : height;
    double sx = mw / sw, sy = mh / sh;
    if (fit == WallpaperFit::fill) sx = sy = std::max(sx, sy);
    if (fit == WallpaperFit::fit) sx = sy = std::min(sx, sy);
    if (fit == WallpaperFit::center) sx = sy = 1;
    double ox = (mw - sw * sx) / 2, oy = (mh - sh * sy) / 2;
    for (unsigned y = 0; y < height; ++y) {
        for (unsigned x = 0; x < width; ++x) {
            double u = ((x + .5) * mw / width - ox) / sx / sw;
            double v = ((y + .5) * mh / height - oy) / sy / sh;
            if (u >= 0 && u < 1 && v >= 0 && v < 1) {
                auto ix = std::min(source.width - 1, unsigned(u * source.width));
                auto iy = std::min(source.height - 1, unsigned(v * source.height));
                out.pixels[size_t(y) * width + x] = source.pixels[size_t(iy) * source.width + ix] | 0xff000000u;
            }
        }
    }
    return out;
}
void BoxBlur(Image& image, unsigned radius) {
    if (!image.width || !image.height || image.pixels.size() != size_t(image.width) * image.height)
        throw std::invalid_argument("Invalid image");
    radius = std::min(radius, 16u);
    if (!radius) return;
    std::vector<uint32_t> temp(image.pixels.size());
    for (int pass = 0; pass < 2; ++pass) {
        const auto& input = pass ? temp : image.pixels;
        auto& output = pass ? image.pixels : temp;
        int length = int(pass ? image.height : image.width);
        int rows = int(pass ? image.width : image.height);
        auto index = [&](int row, int pos) {
            return pass ? size_t(pos) * image.width + row : size_t(row) * image.width + pos;
        };
        for (int row = 0; row < rows; ++row) {
            unsigned sum[3]{};
            auto add = [&](int pos, int sign) {
                uint32_t p = input[index(row, std::clamp(pos, 0, length - 1))];
                for (int c = 0; c < 3; ++c) {
                    unsigned component = (p >> (c * 8)) & 255;
                    if (sign > 0) sum[c] += component; else sum[c] -= component;
                }
            };
            for (int i = -int(radius); i <= int(radius); ++i) add(i, 1);
            unsigned n = 2 * radius + 1;
            for (int p = 0; p < length; ++p) {
                output[index(row, p)] = 0xff000000u | (sum[0] / n) |
                    ((sum[1] / n) << 8) | ((sum[2] / n) << 16);
                add(p - int(radius), -1);
                add(p + int(radius) + 1, 1);
            }
        }
    }
}
}
