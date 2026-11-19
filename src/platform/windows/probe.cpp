#include "aura/probe.h"
#include <shellapi.h>
#include <dwmapi.h>
#include <windowsx.h>
#include <filesystem>
#include <chrono>
#include <stdexcept>
#include <algorithm>

namespace aura {
namespace {
constexpr UINT kTray = WM_APP + 1, kCommand = WM_APP + 2;
constexpr UINT kShow = 100, kHide = 101, kDesktop = 102, kHud = 103,
    kSolid = 104, kWallpaper = 105, kAcrylic = 106, kExit = 107;
struct Panel { HWND window{}; unsigned index{}; bool acrylic = false; };
class Probe {
public:
    Options options;
    std::unique_ptr<Renderer> renderer;
    HWND controller{};
    HANDLE instance{};
    std::vector<Panel> panels;
    std::ofstream log;
    bool visible = true, desktop = false, failed = false;
    unsigned exerciseStep = 0;
    UINT taskbarCreated = 0;
    DWORD started = GetTickCount();
    ~Probe() {
        if (controller) {
            NOTIFYICONDATAW data{sizeof(data)}; data.hWnd = controller; data.uID = 1;
            Shell_NotifyIconW(NIM_DELETE, &data);
        }
        for (auto& panel : panels) if (IsWindow(panel.window)) DestroyWindow(panel.window);
        if (controller && IsWindow(controller)) DestroyWindow(controller);
        if (instance) CloseHandle(instance);
    }
    void Event(const std::string& name, long long value = 0) {
        if (log) {
            log << "{\"elapsed_ms\":" << DWORD(GetTickCount() - started)
                << ",\"event\":\"" << name << "\",\"value\":" << value << "}\n";
            log.flush();
        }
    }
    static LRESULT CALLBACK ControllerProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        auto* self = reinterpret_cast<Probe*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (msg == WM_NCCREATE) {
            self = static_cast<Probe*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (!self) return DefWindowProcW(hwnd, msg, wp, lp);
        try { return self->ControllerMessage(hwnd, msg, wp, lp); }
        catch (const std::exception&) {
            self->failed = true; self->Event("controller_error", msg); PostQuitMessage(1); return 0;
        }
    }
    static LRESULT CALLBACK PanelProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        auto* self = reinterpret_cast<Probe*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (msg == WM_NCCREATE) {
            self = static_cast<Probe*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (!self) return DefWindowProcW(hwnd, msg, wp, lp);
        try {
            auto found = std::find_if(self->panels.begin(), self->panels.end(), [&](const auto& p) { return p.window == hwnd; });
            switch (msg) {
            case WM_ERASEBKGND: return 1;
            case WM_PAINT: {
                PAINTSTRUCT paint{}; BeginPaint(hwnd, &paint); EndPaint(hwnd, &paint);
                if (found != self->panels.end() && self->visible)
                    self->renderer->Paint(hwnd, found->index, self->options.material, found->acrylic);
                return 0;
            }
            case WM_NCHITTEST:
                if (!self->desktop && GET_Y_LPARAM(lp) - self->Top(hwnd) < MulDiv(52, GetDpiForWindow(hwnd), 96))
                    return HTCAPTION;
                break;
            case WM_EXITSIZEMOVE:
                InvalidateRect(hwnd, nullptr, FALSE); self->Event("move_end"); return 0;
            case WM_DPICHANGED: {
                auto* rect = reinterpret_cast<RECT*>(lp);
                SetWindowPos(hwnd, nullptr, rect->left, rect->top, rect->right - rect->left,
                    rect->bottom - rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
                InvalidateRect(hwnd, nullptr, FALSE); return 0;
            }
            case WM_SIZE: InvalidateRect(hwnd, nullptr, FALSE); return 0;
            case WM_CLOSE: self->Show(false); return 0;
            case WM_CONTEXTMENU: self->Menu(hwnd); return 0;
            }
        } catch (const std::exception& error) {
            self->failed = true; self->Event(std::string("paint_error: ") + error.what(), msg); PostQuitMessage(1); return 0;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    static int Top(HWND hwnd) { RECT r{}; GetWindowRect(hwnd, &r); return r.top; }
    void AddTray() {
        NOTIFYICONDATAW data{sizeof(data)};
        data.hWnd = controller; data.uID = 1;
        data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        data.uCallbackMessage = kTray;
        data.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        wcscpy_s(data.szTip, L"Evoly Aura P0 · Ctrl+Alt+F10 显示/隐藏");
        Event("tray_added", Shell_NotifyIconW(NIM_ADD, &data));
    }
    void Menu(HWND owner) {
        HMENU menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING, kShow, L"显示面板");
        AppendMenuW(menu, MF_STRING, kHide, L"隐藏并释放表面");
        AppendMenuW(menu, MF_STRING, kDesktop, L"桌面挂载（实验）");
        AppendMenuW(menu, MF_STRING, kHud, L"悬浮 HUD");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, kSolid, L"纯色");
        AppendMenuW(menu, MF_STRING, kWallpaper, L"壁纸缓存玻璃");
        AppendMenuW(menu, MF_STRING, kAcrylic, L"系统 Acrylic（GPU 实验）");
        AppendMenuW(menu, MF_STRING, kExit, L"退出 P0");
        POINT cursor{}; GetCursorPos(&cursor);
        SetForegroundWindow(owner);
        UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
            cursor.x, cursor.y, 0, owner, nullptr);
        DestroyMenu(menu);
        if (command) SendMessageW(controller, kCommand, command, 0);
        PostMessageW(owner, WM_NULL, 0, 0);
    }
    static HWND DesktopWorker() {
        HWND progman = FindWindowW(L"Progman", nullptr);
        if (!progman) return nullptr;
        DWORD_PTR result{};
        SendMessageTimeoutW(progman, 0x052C, 0, 0, SMTO_ABORTIFHUNG, 1000, &result);
        HWND worker = nullptr;
        EnumWindows([](HWND top, LPARAM value) -> BOOL {
            wchar_t className[64]{};
            GetClassNameW(top, className, 64);
            if (wcscmp(className, L"Progman") != 0 && wcscmp(className, L"WorkerW") != 0) return TRUE;
            if (FindWindowExW(top, nullptr, L"SHELLDLL_DefView", nullptr)) {
                HWND candidate = FindWindowExW(nullptr, top, L"WorkerW", nullptr);
                if (candidate && IsWindowVisible(candidate)) {
                    *reinterpret_cast<HWND*>(value) = candidate;
                    return FALSE;
                }
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&worker));
        // Windows 11 may host the wallpaper WorkerW as a Progman child.
        if (!worker) {
            HWND candidate = FindWindowExW(progman, nullptr, L"WorkerW", nullptr);
            if (candidate && IsWindowVisible(candidate)) worker = candidate;
        }
        return worker;
    }
    void ApplyMaterial(Panel& panel) {
        int type = options.material == Material::acrylic && options.backend == Backend::composition ? 3 : 1;
        HRESULT hr = DwmSetWindowAttribute(panel.window, DWMWA_SYSTEMBACKDROP_TYPE, &type, sizeof(type));
        panel.acrylic = type == 3 && SUCCEEDED(hr) && !desktop;
        if (type == 3) Event("acrylic_api_hresult", static_cast<long>(hr));
        if (options.material == Material::acrylic && !panel.acrylic) Event("acrylic_solid_fallback");
        InvalidateRect(panel.window, nullptr, FALSE);
    }
    void Mode(bool requestedDesktop) {
        HWND parent = requestedDesktop ? DesktopWorker() : nullptr;
        desktop = requestedDesktop && parent;
        Event(desktop ? "desktop_host_found" : requestedDesktop ? "desktop_fallback_hud" : "hud");
        for (auto& panel : panels) {
            if (!IsWindow(panel.window)) continue;
            RECT screen{}; GetWindowRect(panel.window, &screen);
            if (!desktop) SetParent(panel.window, nullptr);
            auto style = GetWindowLongPtrW(panel.window, GWL_STYLE);
            style &= ~(WS_POPUP | WS_CHILD);
            style |= desktop ? WS_CHILD : WS_POPUP;
            SetWindowLongPtrW(panel.window, GWL_STYLE, style);
            if (desktop) {
                SetLastError(0);
                HWND old = SetParent(panel.window, parent);
                if (!old && GetLastError() != 0) {
                    Event("desktop_setparent_error", GetLastError());
                    desktop = false;
                    SetParent(panel.window, nullptr);
                    SetWindowLongPtrW(panel.window, GWL_STYLE, (style & ~WS_CHILD) | WS_POPUP);
                }
            }
            POINT point{screen.left, screen.top};
            if (desktop) ScreenToClient(parent, &point);
            SetWindowPos(panel.window, desktop ? HWND_BOTTOM : HWND_TOPMOST,
                point.x, point.y, screen.right - screen.left, screen.bottom - screen.top,
                SWP_NOACTIVATE | SWP_FRAMECHANGED);
            ApplyMaterial(panel);
        }
    }
    void Show(bool show) {
        visible = show;
        for (auto& panel : panels) ShowWindow(panel.window, show ? SW_SHOWNOACTIVATE : SW_HIDE);
        if (show) {
            if (options.material == Material::wallpaper && !panels.empty()) renderer->LoadBackground(panels[0].window);
            for (auto& panel : panels) InvalidateRect(panel.window, nullptr, FALSE);
            ScheduleClock();
        } else {
            KillTimer(controller, 1);
            renderer->ReleaseAll();
        }
        Event(show ? "shown" : "hidden_surfaces_released", renderer->draws);
    }
    void ScheduleClock() {
        SYSTEMTIME now{}; GetLocalTime(&now);
        SetTimer(controller, 1, 60000 - now.wSecond * 1000 - now.wMilliseconds, nullptr);
    }
    void CreatePanels() {
        panels.clear();
        UINT dpi = GetDpiForSystem();
        int width = MulDiv(400, dpi, 96), height = MulDiv(240, dpi, 96);
        RECT work{}; SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
        int columns = int(std::max(LONG(1), (work.right - work.left - 48) / (width + 16)));
        panels.resize(options.panels);
        for (unsigned i = 0; i < options.panels; ++i) {
            auto& panel = panels[i]; panel.index = i;
            DWORD ex = WS_EX_TOOLWINDOW | (options.backend == Backend::software ?
                WS_EX_LAYERED : WS_EX_NOREDIRECTIONBITMAP);
            panel.window = CreateWindowExW(ex, L"EvolyAuraP0Panel", L"Evoly Aura P0", WS_POPUP,
                work.left + 24 + int(i % columns) * (width + 16),
                work.top + 24 + int(i / columns) * (height + 16), width, height,
                nullptr, nullptr, GetModuleHandleW(nullptr), this);
            if (!panel.window) throw std::runtime_error("Panel window creation failed");
            int corner = DWMWCP_ROUND;
            DwmSetWindowAttribute(panel.window, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
        }
        if (options.material == Material::wallpaper) renderer->LoadBackground(panels[0].window);
        Mode(options.desktop);
    }
    void Exercise() {
        ++exerciseStep;
        if (exerciseStep <= 100) {
            Material sequence[] = {Material::solid, Material::wallpaper, Material::acrylic};
            options.material = sequence[exerciseStep % 3];
            renderer->ReleaseAll();
            if (options.material == Material::wallpaper) renderer->LoadBackground(panels[0].window);
            for (auto& panel : panels) {
                RECT r{}; GetWindowRect(panel.window, &r);
                SetWindowPos(panel.window, nullptr, r.left + (exerciseStep % 2 ? 8 : -8), r.top,
                    r.right - r.left + (exerciseStep % 2 ? 8 : -8), r.bottom - r.top,
                    SWP_NOZORDER | SWP_NOACTIVATE);
                ApplyMaterial(panel);
                UpdateWindow(panel.window);
            }
            Event("exercise_cycle", exerciseStep);
        } else if (exerciseStep == 101) Show(false);
        else if (exerciseStep == 102) Show(true);
        else { KillTimer(controller, 3); Event("exercise_complete", renderer->draws); }
    }
    LRESULT ControllerMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        if (msg == taskbarCreated && taskbarCreated) {
            AddTray();
            bool destroyed = std::any_of(panels.begin(), panels.end(), [](const auto& p) { return !IsWindow(p.window); });
            if (destroyed) {
                for (auto& p : panels) if (IsWindow(p.window)) DestroyWindow(p.window);
                renderer->ReleaseAll(); CreatePanels();
            } else Mode(options.desktop);
            Show(visible); Event("explorer_recovered"); return 0;
        }
        switch (msg) {
        case kTray:
            if (lp == WM_RBUTTONUP) Menu(hwnd);
            if (lp == WM_LBUTTONUP) Show(!visible);
            return 0;
        case WM_HOTKEY:
            if (wp == 1) Show(!visible);
            if (wp == 2) { options.desktop = !desktop; Mode(options.desktop); }
            if (wp == 3) PostQuitMessage(0);
            return 0;
        case kCommand:
            if (wp == kExit) PostQuitMessage(0);
            if (wp == kShow || wp == kHide) Show(wp == kShow);
            if (wp == kDesktop || wp == kHud) { options.desktop = wp == kDesktop; Mode(options.desktop); }
            if (wp >= kSolid && wp <= kAcrylic) {
                options.material = wp == kSolid ? Material::solid : wp == kWallpaper ? Material::wallpaper : Material::acrylic;
                renderer->ReleaseAll();
                if (options.material == Material::wallpaper) renderer->LoadBackground(panels[0].window);
                for (auto& panel : panels) ApplyMaterial(panel);
            }
            return 0;
        case WM_TIMER:
            if (wp == 1 && visible) {
                for (auto& panel : panels) InvalidateRect(panel.window, nullptr, FALSE);
                ScheduleClock();
            }
            if (wp == 2) PostQuitMessage(0);
            if (wp == 3) Exercise();
            return 0;
        case WM_SETTINGCHANGE:
        case WM_DISPLAYCHANGE:
            if (visible && options.material == Material::wallpaper) {
                renderer->LoadBackground(panels[0].window);
                for (auto& panel : panels) InvalidateRect(panel.window, nullptr, FALSE);
            }
            return 0;
        case WM_CLOSE: PostQuitMessage(0); return 0;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    int Run() {
        instance = CreateMutexW(nullptr, TRUE, L"Local\\EvolyAuraP0Probe");
        if (!instance) throw std::runtime_error("Single-instance mutex failed");
        if (GetLastError() == ERROR_ALREADY_EXISTS) return 3;
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        if (!options.log.empty()) {
            log.open(std::filesystem::path(options.log));
            if (!log) throw std::runtime_error("Cannot open probe event log");
        }
        renderer = std::make_unique<Renderer>(options.backend);
        WNDCLASSW cls{};
        cls.hInstance = GetModuleHandleW(nullptr); cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        cls.lpfnWndProc = ControllerProc; cls.lpszClassName = L"EvolyAuraP0Controller";
        if (!RegisterClassW(&cls)) throw std::runtime_error("Controller class registration failed");
        cls.lpfnWndProc = PanelProc; cls.lpszClassName = L"EvolyAuraP0Panel";
        if (!RegisterClassW(&cls)) throw std::runtime_error("Panel class registration failed");
        controller = CreateWindowExW(WS_EX_TOOLWINDOW, L"EvolyAuraP0Controller", L"Aura P0 Controller",
            WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, cls.hInstance, this);
        if (!controller) throw std::runtime_error("Controller window failed");
        taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
        AddTray();
        Event("hotkey_show_registered", RegisterHotKey(controller, 1, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_F10));
        Event("hotkey_mode_registered", RegisterHotKey(controller, 2, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_F11));
        Event("hotkey_exit_registered", RegisterHotKey(controller, 3, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_F12));
        CreatePanels();
        Event("warp_fallback", renderer->warp());
        Event(renderer->WallpaperStatus(), renderer->WallpaperBytes());
        Show(!options.hidden);
        if (options.duration) SetTimer(controller, 2, options.duration * 1000, nullptr);
        if (options.exercise) SetTimer(controller, 3, 120, nullptr);
        Event("ready", options.panels);
        Event("panel_dpi", GetDpiForWindow(panels[0].window));
        Event("screen_width", GetSystemMetrics(SM_CXSCREEN));
        Event("screen_height", GetSystemMetrics(SM_CYSCREEN));
        Event("monitor_count", GetSystemMetrics(SM_CMONITORS));
        MSG message{};
        BOOL result{};
        while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
            TranslateMessage(&message); DispatchMessageW(&message);
        }
        Event("draw_count", renderer->draws);
        Event("exit", failed || result == -1 ? 1 : 0);
        return failed || result == -1 ? 1 : int(message.wParam);
    }
};
}
Options ParseOptions(const std::vector<std::wstring>& arguments) {
    Options options;
    for (size_t i = 0; i < arguments.size(); ++i) {
        auto arg = arguments[i];
        auto next = [&]() {
            if (++i >= arguments.size()) throw std::invalid_argument("Missing option value");
            return arguments[i];
        };
        if (arg == L"--backend") {
            auto value = next();
            if (value != L"software" && value != L"composition") throw std::invalid_argument("Unknown backend");
            options.backend = value == L"software" ? Backend::software : Backend::composition;
        } else if (arg == L"--material") {
            auto value = next();
            if (value != L"solid" && value != L"wallpaper" && value != L"acrylic") throw std::invalid_argument("Unknown material");
            options.material = value == L"solid" ? Material::solid : value == L"wallpaper" ? Material::wallpaper : Material::acrylic;
        } else if (arg == L"--panels" || arg == L"--duration") {
            auto raw = next(); size_t consumed{};
            unsigned long value = std::stoul(raw, &consumed);
            if (consumed != raw.size()) throw std::invalid_argument("Invalid numeric option");
            if (arg == L"--panels") {
                if (!value || value > 12) throw std::invalid_argument("Panels must be 1..12");
                options.panels = unsigned(value);
            } else {
                if (value > 86400) throw std::invalid_argument("Duration must be <=86400 seconds");
                options.duration = unsigned(value);
            }
        } else if (arg == L"--log") options.log = next();
        else if (arg == L"--desktop") options.desktop = true;
        else if (arg == L"--hidden") options.hidden = true;
        else if (arg == L"--exercise") options.exercise = true;
        else throw std::invalid_argument("Unknown argument");
    }
    return options;
}
int RunProbe(const Options& options) {
    Check(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED), "COM apartment");
    int result = 1;
    try {
        Probe probe; probe.options = options; result = probe.Run();
    } catch (...) { CoUninitialize(); throw; }
    CoUninitialize();
    return result;
}
}
