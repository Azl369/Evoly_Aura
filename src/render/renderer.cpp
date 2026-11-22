#include "aura/render.h"
#include <dxgi1_2.h>
#include <wincodec.h>
#include <stdexcept>
#include <sstream>
#include <algorithm>

namespace aura {
void Check(HRESULT result, const char* operation) {
    if (FAILED(result)) {
        std::ostringstream message;
        message << operation << " failed: 0x" << std::hex << unsigned(result);
        throw std::runtime_error(message.str());
    }
}
struct Renderer::Surface {
    HWND window{};
    unsigned width{}, height{};
    HDC dc{};
    HBITMAP dib{};
    HGDIOBJ previous{};
    ComPtr<IDCompositionTarget> target;
    ComPtr<IDCompositionVisual2> visual;
    ComPtr<IDCompositionSurface> surface;
    ~Surface() {
        if (dc && previous) SelectObject(dc, previous);
        if (dib) DeleteObject(dib);
        if (dc) DeleteDC(dc);
    }
};
Renderer::Renderer(Backend backend) : backend_(backend) {
    Check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_PPV_ARGS(&factory_)), "D2D factory");
    Check(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(text_.GetAddressOf())), "DWrite factory");
    auto format = [&](float size, DWRITE_FONT_WEIGHT weight, ComPtr<IDWriteTextFormat>& out) {
        Check(text_->CreateTextFormat(L"Microsoft YaHei UI", nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, size, L"zh-CN", &out), "Text format");
    };
    format(18, DWRITE_FONT_WEIGHT_SEMI_BOLD, title_);
    format(13, DWRITE_FONT_WEIGHT_NORMAL, body_);
    format(38, DWRITE_FONT_WEIGHT_LIGHT, clock_);
    if (backend == Backend::software) {
        auto props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96, 96);
        Check(factory_->CreateDCRenderTarget(&props, &software_), "Software target");
    } else {
        HRESULT result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &d3d_, nullptr, nullptr);
        if (FAILED(result)) {
            warp_ = true;
            Check(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &d3d_, nullptr, nullptr), "WARP device");
        }
        ComPtr<IDXGIDevice> dxgi;
        Check(d3d_.As(&dxgi), "DXGI device");
        Check(factory_->CreateDevice(dxgi.Get(), &d2d_), "Shared D2D device");
        ComPtr<IDCompositionDesktopDevice> desktop;
        Check(DCompositionCreateDevice2(d2d_.Get(), IID_PPV_ARGS(&desktop)), "Composition device");
        Check(desktop.As(&composition_), "Composition v2 interface");
    }
}
Renderer::~Renderer() = default;
void Renderer::LoadBackground(HWND window) {
    wallpaperBitmap_.Reset();
    wallpaper_ = LoadWallpaper(window);
}
void Renderer::Release(HWND window) {
    std::erase_if(surfaces_, [&](const auto& item) { return item->window == window; });
    if (composition_) composition_->Commit();
}
void Renderer::ReleaseAll() {
    surfaces_.clear();
    wallpaperBitmap_.Reset();
    icons_.clear();
    ReleaseLauncherIcons();
    if (composition_) composition_->Commit();
}
void Renderer::ReleaseLauncherIcons(){launcherBitmaps_.clear();}
size_t Renderer::LauncherBitmapBytes() const {
    size_t bytes=0;
    for(const auto& item:launcherBitmaps_)bytes+=item.source->pixels.size()*4;
    return bytes;
}
void Renderer::Paint(HWND window, unsigned index, Material material, bool acrylicAccepted, const WidgetView* view) {
    RECT rect{};
    GetClientRect(window, &rect);
    unsigned width = unsigned(rect.right), height = unsigned(rect.bottom);
    if (!width || !height || width > 4096 || height > 4096) return;
    auto found = std::find_if(surfaces_.begin(), surfaces_.end(), [&](const auto& item) { return item->window == window; });
    if (found != surfaces_.end() && ((*found)->width != width || (*found)->height != height)) {
        Release(window);
        found = surfaces_.end();
    }
    if (found == surfaces_.end()) {
        auto item = std::make_unique<Surface>();
        item->window = window; item->width = width; item->height = height;
        if (backend_ == Backend::software) {
            item->dc = CreateCompatibleDC(nullptr);
            BITMAPINFO info{};
            info.bmiHeader = {sizeof(BITMAPINFOHEADER), LONG(width), -LONG(height), 1, 32, BI_RGB};
            void* bits = nullptr;
            item->dib = CreateDIBSection(item->dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
            if (!item->dc || !item->dib) throw std::runtime_error("DIB allocation failed");
            item->previous = SelectObject(item->dc, item->dib);
        } else {
            ComPtr<IDCompositionDesktopDevice> desktop;
            Check(composition_.As(&desktop), "Desktop composition");
            Check(desktop->CreateTargetForHwnd(window, TRUE, &item->target), "Composition target");
            Check(composition_->CreateVisual(&item->visual), "Composition visual");
            Check(composition_->CreateSurface(width, height, DXGI_FORMAT_B8G8R8A8_UNORM,
                DXGI_ALPHA_MODE_PREMULTIPLIED, &item->surface), "Composition surface");
            Check(item->visual->SetContent(item->surface.Get()), "Visual content");
            Check(item->target->SetRoot(item->visual.Get()), "Visual root");
        }
        surfaces_.push_back(std::move(item));
        found = std::prev(surfaces_.end());
    }
    auto& item = **found;
    float scale = float(GetDpiForWindow(window)) / 96;
    if (backend_ == Backend::software) {
        Check(software_->BindDC(item.dc, &rect), "Bind DIB");
        software_->BeginDraw();
        software_->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale));
        Draw(software_.Get(), window, index, material, false, scale, view);
        Check(software_->EndDraw(), "Software draw");
        RECT screen{};
        GetWindowRect(window, &screen);
        POINT destination{screen.left, screen.top}, source{};
        SIZE size{LONG(width), LONG(height)};
        BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        if (!UpdateLayeredWindow(window, nullptr, &destination, &size, item.dc, &source, 0, &blend, ULW_ALPHA))
            throw std::runtime_error("UpdateLayeredWindow failed");
    } else {
        ComPtr<ID2D1DeviceContext> context;
        POINT offset{};
        Check(item.surface->BeginDraw(nullptr, IID_PPV_ARGS(&context), &offset), "Composition BeginDraw");
        context->SetDpi(96, 96);
        context->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale) *
            D2D1::Matrix3x2F::Translation(float(offset.x), float(offset.y)));
        try {
            Draw(context.Get(), window, index, material, acrylicAccepted, scale, view);
        } catch (...) {
            item.surface->EndDraw();
            throw;
        }
        Check(item.surface->EndDraw(), "Composition EndDraw");
        Check(composition_->Commit(), "Composition commit");
    }
    ++draws;
}
void Renderer::Draw(ID2D1RenderTarget* target, HWND window, unsigned index,
                    Material material, bool acrylicAccepted, float scale, const WidgetView* view) {
    if (view) { DrawWidget(target,window,*view,scale); return; }
    RECT client{}, screen{};
    GetClientRect(window, &client);
    GetWindowRect(window, &screen);
    float w = float(client.right) / scale, h = float(client.bottom) / scale;
    target->Clear(D2D1::ColorF(0, 0));
    target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    ComPtr<ID2D1SolidColorBrush> brush;
    Check(target->CreateSolidColorBrush(D2D1::ColorF(0x172431, 1), &brush), "Brush");
    auto panel = D2D1::RoundedRect(D2D1::RectF(1, 1, w - 1, h - 1), 18, 18);
    bool sampled = material == Material::wallpaper && !wallpaper_.image.pixels.empty();
    if (sampled) {
        if (!wallpaperBitmap_) {
            const auto& image = wallpaper_.image;
            Check(target->CreateBitmap(D2D1::SizeU(image.width, image.height), image.pixels.data(), image.width * 4,
                D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                    D2D1_ALPHA_MODE_PREMULTIPLIED)), &wallpaperBitmap_), "Wallpaper bitmap");
        }
        ComPtr<ID2D1BitmapBrush> imageBrush;
        Check(target->CreateBitmapBrush(wallpaperBitmap_.Get(), &imageBrush), "Wallpaper brush");
        float mw = float(wallpaper_.monitor.right - wallpaper_.monitor.left);
        float mh = float(wallpaper_.monitor.bottom - wallpaper_.monitor.top);
        imageBrush->SetTransform(D2D1::Matrix3x2F::Scale(
            mw / wallpaper_.image.width / scale, mh / wallpaper_.image.height / scale) *
            D2D1::Matrix3x2F::Translation(
                float(wallpaper_.monitor.left - screen.left) / scale,
                float(wallpaper_.monitor.top - screen.top) / scale));
        target->FillRoundedRectangle(panel, imageBrush.Get());
        brush->SetColor(D2D1::ColorF(0x102235, .64f));
    } else {
        brush->SetColor(D2D1::ColorF(0x142336,
            material == Material::acrylic && acrylicAccepted ? .18f : 1.f));
    }
    target->FillRoundedRectangle(panel, brush.Get());
    brush->SetColor(D2D1::ColorF(0xd9efff, .3f));
    target->DrawRoundedRectangle(panel, brush.Get(), 1);
    brush->SetColor(D2D1::ColorF(0xf1f7fc));
    std::wstring title = L"Evoly Aura  /  浮光 " + std::to_wstring(index + 1);
    target->DrawTextW(title.c_str(), UINT32(title.size()), title_.Get(), D2D1::RectF(22, 20, w - 16, 52), brush.Get());
    WCHAR time[32]{};
    GetTimeFormatEx(LOCALE_NAME_INVARIANT, TIME_NOSECONDS, nullptr, L"HH:mm", time, 32);
    target->DrawTextW(time, UINT32(wcslen(time)), clock_.Get(), D2D1::RectF(20, 60, w - 20, 115), brush.Get());
    brush->SetColor(D2D1::ColorF(0xc3dae9));
    const wchar_t* detail = L"原生窗口 · 分钟刷新 · 共享资源";
    target->DrawTextW(detail, UINT32(wcslen(detail)), body_.Get(), D2D1::RectF(22, 126, w - 15, 151), brush.Get());
    std::wstring status = backend_ == Backend::software ? L"Software / " : L"DirectComposition / ";
    status += sampled ? L"壁纸缓存玻璃" : material == Material::acrylic && acrylicAccepted ?
        L"Acrylic 请求已接受" : material == Material::acrylic ? L"Acrylic 回退" : L"纯色";
    target->DrawTextW(status.c_str(), UINT32(status.size()), body_.Get(), D2D1::RectF(22, h - 36, w - 15, h - 12), brush.Get());
    if (icons_.empty()) {
        ComPtr<IWICImagingFactory> wic;
        Check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic)), "Icon WIC");
        const wchar_t* ids[] = {IDI_APPLICATION, IDI_INFORMATION, IDI_WARNING, IDI_SHIELD};
        for (auto id : ids) {
            ComPtr<IWICBitmap> image;
            Check(wic->CreateBitmapFromHICON(LoadIconW(nullptr, id), &image), "Stock icon");
            ComPtr<IWICFormatConverter> converter;
            Check(wic->CreateFormatConverter(&converter), "Icon converter");
            Check(converter->Initialize(image.Get(), GUID_WICPixelFormat32bppPBGRA,
                WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom), "Icon PBGRA");
            ComPtr<ID2D1Bitmap> bitmap;
            Check(target->CreateBitmapFromWicBitmap(converter.Get(), &bitmap), "Stock icon bitmap");
            icons_.push_back(bitmap);
        }
    }
    for (size_t i = 0; i < icons_.size(); ++i) {
        float x = 22 + float(i) * 42;
        target->DrawBitmap(icons_[i].Get(), D2D1::RectF(x, 163, x + 24, 187), .9f);
    }
}
}
