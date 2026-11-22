#pragma once
#include <windows.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <dcomp.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include "aura/image.h"
#include "aura/workspace.h"

namespace aura {
using Microsoft::WRL::ComPtr;
void Check(HRESULT result, const char* operation);
enum class Backend { software, composition };
enum class Material { solid, wallpaper, acrylic };
struct Wallpaper {
    Image image;
    RECT monitor{};
    std::string status = "not_loaded";
};
Wallpaper LoadWallpaper(HWND window);
class Renderer {
public:
    explicit Renderer(Backend backend);
    ~Renderer();
    Backend backend() const { return backend_; }
    bool warp() const { return warp_; }
    void LoadBackground(HWND window);
    void Paint(HWND window, unsigned index, Material material, bool acrylicAccepted, const WidgetView* view = nullptr);
    void Release(HWND window);
    void ReleaseAll();
    void ReleaseLauncherIcons();
    size_t LauncherBitmapBytes() const;
    size_t WallpaperBytes() const { return wallpaper_.image.pixels.size() * 4; }
    const std::string& WallpaperStatus() const { return wallpaper_.status; }
    uint64_t draws = 0;
private:
    struct Surface;
    void Draw(ID2D1RenderTarget* target, HWND window, unsigned index,
              Material material, bool acrylicAccepted, float scale, const WidgetView* view);
    void DrawWidget(ID2D1RenderTarget* target, HWND window, const WidgetView& view, float scale);
    std::array<ComPtr<IDWriteTextFormat>,6> widgetFonts_;
    ComPtr<IDWriteInlineObject> launcherEllipsis_;
    Backend backend_;
    bool warp_ = false;
    ComPtr<ID2D1Factory1> factory_;
    ComPtr<IDWriteFactory> text_;
    ComPtr<IDWriteTextFormat> title_, body_, clock_;
    ComPtr<ID3D11Device> d3d_;
    ComPtr<ID2D1Device> d2d_;
    ComPtr<IDCompositionDevice2> composition_;
    ComPtr<ID2D1DCRenderTarget> software_;
    ComPtr<ID2D1Bitmap> wallpaperBitmap_;
    std::vector<ComPtr<ID2D1Bitmap>> icons_;
    struct LauncherBitmap { std::shared_ptr<const Image> source; ComPtr<ID2D1Bitmap> bitmap; };
    std::vector<LauncherBitmap> launcherBitmaps_;
    Wallpaper wallpaper_;
    std::vector<std::unique_ptr<Surface>> surfaces_;
};
}
