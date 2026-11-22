#include "aura/render.h"
#include <shobjidl.h>
#include <wincodec.h>
#include <algorithm>

namespace aura {
Wallpaper LoadWallpaper(HWND window) {
    Wallpaper result;
    MONITORINFO mi{sizeof(mi)};
    if (!GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY), &mi))
        throw std::runtime_error("Monitor info failed");
    result.monitor = mi.rcMonitor;
    ComPtr<IDesktopWallpaper> desktop;
    if (FAILED(CoCreateInstance(CLSID_DesktopWallpaper, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&desktop)))) {
        result.status = "desktop_wallpaper_api_unavailable"; return result;
    }
    COLORREF background{};
    desktop->GetBackgroundColor(&background);
    uint32_t bg = 0xff000000u | (GetRValue(background) << 16) | (GetGValue(background) << 8) | GetBValue(background);
    DESKTOP_WALLPAPER_POSITION position{};
    desktop->GetPosition(&position);
    if (position == DWPOS_TILE || position == DWPOS_SPAN) {
        result.status = "tile_or_span_solid_fallback"; return result;
    }
    UINT count = 0;
    desktop->GetMonitorDevicePathCount(&count);
    std::wstring path;
    for (UINT i = 0; i < count; ++i) {
        LPWSTR device = nullptr;
        if (FAILED(desktop->GetMonitorDevicePathAt(i, &device))) continue;
        RECT rect{};
        desktop->GetMonitorRECT(device, &rect);
        if (EqualRect(&rect, &result.monitor)) {
            LPWSTR raw = nullptr;
            if (SUCCEEDED(desktop->GetWallpaper(device, &raw)) && raw) path = raw;
            CoTaskMemFree(raw);
        }
        CoTaskMemFree(device);
    }
    if (path.empty()) { result.status = "no_static_wallpaper_solid_fallback"; return result; }
    ComPtr<IWICImagingFactory> wic;
    Check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic)), "WIC factory");
    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = wic->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr)) { result.status = "wallpaper_decode_solid_fallback"; return result; }
    ComPtr<IWICBitmapFrameDecode> frame;
    Check(decoder->GetFrame(0, &frame), "Wallpaper frame");
    UINT originalW{}, originalH{};
    Check(frame->GetSize(&originalW, &originalH), "Wallpaper dimensions");
    if (!originalW || !originalH || uint64_t(originalW) * originalH > 100000000) {
        result.status = "wallpaper_source_limit_solid_fallback"; return result;
    }
    double ratio = std::min(1., 1024. / std::max(originalW, originalH));
    UINT w = std::max(1u, UINT(originalW * ratio)), h = std::max(1u, UINT(originalH * ratio));
    ComPtr<IWICBitmapScaler> scaler;
    Check(wic->CreateBitmapScaler(&scaler), "Wallpaper scaler");
    Check(scaler->Initialize(frame.Get(), w, h, WICBitmapInterpolationModeFant), "Wallpaper scale");
    ComPtr<IWICFormatConverter> converter;
    Check(wic->CreateFormatConverter(&converter), "Wallpaper converter");
    Check(converter->Initialize(scaler.Get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0,
        WICBitmapPaletteTypeCustom), "Wallpaper BGRA");
    Image decoded{w, h, std::vector<uint32_t>(size_t(w) * h)};
    Check(converter->CopyPixels(nullptr, w * 4, UINT(decoded.pixels.size() * 4),
        reinterpret_cast<BYTE*>(decoded.pixels.data())), "Wallpaper pixels");
    unsigned mw = unsigned(result.monitor.right - result.monitor.left);
    unsigned mh = unsigned(result.monitor.bottom - result.monitor.top);
    double cacheRatio = std::min(1., 640. / std::max(mw, mh));
    WallpaperFit fit = position == DWPOS_FIT ? WallpaperFit::fit :
        position == DWPOS_STRETCH ? WallpaperFit::stretch :
        position == DWPOS_CENTER ? WallpaperFit::center : WallpaperFit::fill;
    result.image = MapWallpaper(decoded, std::max(1u, unsigned(mw * cacheRatio)),
        std::max(1u, unsigned(mh * cacheRatio)), fit, bg, mw, mh, originalW, originalH);
    BoxBlur(result.image, 5);
    result.status = "static_wallpaper_cached";
    return result;
}
}
