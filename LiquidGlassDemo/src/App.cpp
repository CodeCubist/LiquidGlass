#include "App.h"
#include "GlFunctions.h"
#include "PngWriter.h"

#include <algorithm>
#include <chrono>
#include <cwctype>
#include <windowsx.h>
#include <timeapi.h>
#include <tlhelp32.h>

namespace {

constexpr wchar_t kWindowClassName[] = L"LiquidGlassDemo_Window";
constexpr DWORD kWdaExcludeFromCapture = 0x00000011;

bool isRemoteEnvironment() {
    if (GetSystemMetrics(SM_REMOTESESSION) != 0) {
        return true;
    }
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe = {};
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(snapshot, &pe)) {
            const wchar_t* const remoteProcs[] = {
                L"todesk.exe",
                L"anydesk.exe",
                L"teamviewer.exe",
                L"rustdesk.exe",
                L"sunloginclient.exe",
                L"sunloginclient_desktop.exe",
                L"gameviewer.exe",
                L"gameviewerservice.exe",
                L"gameviewerserver.exe",
                L"mstsc.exe",
                L"vncserver.exe",
                L"winvnc.exe",
                L"tv_w32.exe",
                L"tv_x64.exe",
                L"parsec.exe",
                L"sunshine.exe"
            };
            do {
                wchar_t nameLower[MAX_PATH] = {};
                for (size_t i = 0; pe.szExeFile[i] && i + 1 < MAX_PATH; ++i) {
                    nameLower[i] = static_cast<wchar_t>(towlower(pe.szExeFile[i]));
                }
                for (const auto* p : remoteProcs) {
                    if (wcsstr(nameLower, p) != nullptr) {
                        CloseHandle(snapshot);
                        return true;
                    }
                }
            } while (Process32NextW(snapshot, &pe));
        }
        CloseHandle(snapshot);
    }
    return false;
}

void centerInWorkArea(int width, int height, POINT* pos) {
    RECT work = {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    pos->x = work.left + ((work.right - work.left) - width) / 2;
    pos->y = work.top + ((work.bottom - work.top) - height) / 2;
}

void logMsg(const wchar_t* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vwprintf(fmt, args);
    fflush(stdout);
    FILE* f = nullptr;
    if (_wfopen_s(&f, L"diag.log", L"a") == 0 && f) {
        vfwprintf(f, fmt, args);
        fclose(f);
    }
    va_end(args);
}

}

bool App::init(HINSTANCE instance, const AppConfig& config, std::wstring* error) {
    instance_ = instance;
    config_ = config;
    
    if (auto setCtx = reinterpret_cast<decltype(&SetProcessDpiAwarenessContext)>(
            GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext"))) {
        setCtx(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    } else {
        SetProcessDPIAware();
    }
    
    {
        UINT dpi = 96;
        if (auto getDpi = reinterpret_cast<UINT(WINAPI*)()>(
                GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForSystem"))) {
            dpi = getDpi();
        }
        RECT work = {};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
        const float workW = static_cast<float>(work.right - work.left);
        const float workH = static_cast<float>(work.bottom - work.top);
        const float fillScale = std::min(workW / 1100.0f, workH / 720.0f) * 0.85f;
        const float fitScale = std::min(config_.width / 1100.0f,
                                        config_.height / 720.0f);
        float scale = config_.scaleOverride > 0
            ? config_.scaleOverride
            : std::max({static_cast<float>(dpi) / 96.0f, fillScale, fitScale});
        scale = std::clamp(scale, 0.75f, 3.0f);
        const float maxW = workW / static_cast<float>(config_.width);
        const float maxH = workH / static_cast<float>(config_.height);
        scale = std::max(0.5f, std::min({scale, maxW, maxH}));
        scale_ = scale;
        config_.width  = std::max(320, static_cast<int>(config_.width  * scale + 0.5f));
        config_.height = std::max(240, static_cast<int>(config_.height * scale + 0.5f));
    }

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = wndProcThunk;
    wc.hInstance     = instance;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClassName;
    if (!RegisterClassExW(&wc)) {
        if (error) *error = L"Failed to register the window class.";
        return false;
    }

    centerInWorkArea(config_.width, config_.height, &screenPos_);
    const DWORD exStyle = WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
    hwnd_ = CreateWindowExW(exStyle, kWindowClassName, L"LiquidGlassDemo", WS_POPUP,
                            screenPos_.x, screenPos_.y, config_.width, config_.height,
                            nullptr, nullptr, instance, this);
    if (!hwnd_) {
        if (error) *error = L"Failed to create the layered window.";
        return false;
    }
    
    if (config_.remote || config_.keepCapturable || isRemoteEnvironment()) {
        config_.remote = true;
    }
    if (auto setAffinity = reinterpret_cast<BOOL(WINAPI*)(HWND, DWORD)>(
            GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetWindowDisplayAffinity"))) {
        if (!config_.remote) {
            if (!setAffinity(hwnd_, kWdaExcludeFromCapture)) {
                setAffinity(hwnd_, 0x00000001);
            }
        } else {
            setAffinity(hwnd_, 0);
        }
    }
    
    if (config_.diag) {
        logMsg(L"[LiquidGlassDemo] Window rect: %dx%d, Scale: %.2f, Pos: (%ld, %ld)\n",
               config_.width, config_.height, scale_, screenPos_.x, screenPos_.y);
        if (config_.remote) {
            logMsg(L"[LiquidGlassDemo] Remote/Capturable mode active: display affinity disabled.\n");
        }
    }

    std::wstring glError;
    if (!gl_.create(instance, &glError)) {
        if (error) *error = L"OpenGL init failed: " + glError;
        return false;
    }
    gl_.makeCurrent();

    if (config_.diag) {
        const char* vendor = reinterpret_cast<const char*>(gl::GetString(GL_VENDOR));
        const char* renderer = reinterpret_cast<const char*>(gl::GetString(GL_RENDERER));
        const char* version = reinterpret_cast<const char*>(gl::GetString(GL_VERSION));
        logMsg(L"[LiquidGlassDemo] GL Vendor: %hs\n", vendor ? vendor : "unknown");
        logMsg(L"[LiquidGlassDemo] GL Renderer: %hs\n", renderer ? renderer : "unknown");
        logMsg(L"[LiquidGlassDemo] GL Version: %hs\n", version ? version : "unknown");
    }
    
    if (!gdiCapture_.resize(config_.width, config_.height)) {
        if (error) *error = L"Failed to initialize the desktop capture buffer.";
        return false;
    }
    gdiCapture_.setFrozen(config_.remote);
    const bool wantDup = !config_.useGdi && !config_.remote;
    const bool haveDup = wantDup && dupCapture_.resize(config_.width, config_.height);

    if (config_.diag) {
        logMsg(L"[LiquidGlassDemo] Capture mode: %ls\n",
               haveDup ? L"DXGI Desktop Duplication" : L"GDI BitBlt");
    }

    std::string rendererError;
    if (!renderer_.init(config_.width, config_.height, &rendererError)) {
        if (error) {
            *error = L"Renderer init failed: " + std::wstring(rendererError.begin(), rendererError.end());
        }
        return false;
    }
    
    renderer_.setUiScale(scale_);
    
    if (!config_.remote) {
        renderer_.seedBackdrop(gdiCapture_, screenPos_.x, screenPos_.y);
    }
    if (haveDup) {
        dupCapture_.setGdiReference(gdiCapture_.referenceLinearLuminance());
    }
    source_ = haveDup ? static_cast<ICaptureSource*>(&dupCapture_)
                      : static_cast<ICaptureSource*>(&gdiCapture_);
    
    BITMAPINFOHEADER bi = {};
    bi.biSize        = sizeof(bi);
    bi.biWidth       = config_.width;
    bi.biHeight      = config_.height;
    bi.biPlanes      = 1;
    bi.biBitCount    = 32;
    bi.biCompression = BI_RGB;
    dib_ = CreateDIBSection(nullptr, reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS, &dibBits_, nullptr, 0);
    if (!dib_ || !dibBits_) {
        if (error) *error = L"Failed to create the presentation DIB.";
        return false;
    }
    
    screenDc_ = GetDC(nullptr);
    memDc_ = CreateCompatibleDC(screenDc_);
    if (!screenDc_ || !memDc_ || !SelectObject(memDc_, dib_)) {
        if (error) *error = L"Failed to prepare the presentation DCs.";
        return false;
    }

    QueryPerformanceFrequency(&qpcFreq_);
    QueryPerformanceCounter(&lastFrame_);
    return true;
}

App::~App() {
    if (memDc_) DeleteDC(memDc_);
    if (screenDc_) ReleaseDC(nullptr, screenDc_);
    if (dib_) DeleteObject(dib_);
}

LRESULT CALLBACK App::wndProcThunk(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    App* app = nullptr;
    if (msg == WM_NCCREATE) {
        app = reinterpret_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    } else {
        app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    return app ? app->wndProc(hwnd, msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT App::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_MOUSEMOVE: {
        mouseX_ = static_cast<float>(GET_X_LPARAM(lp));
        mouseY_ = static_cast<float>(GET_Y_LPARAM(lp));
        mouseInside_ = true;
        if (!trackingMouse_) {
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            if (TrackMouseEvent(&tme)) trackingMouse_ = true;
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        mouseInside_ = false;
        trackingMouse_ = false;
        return 0;
    case WM_LBUTTONDOWN:
        SetCapture(hwnd);
        scene_.onMouseDown(mouseX_, mouseY_);
        return 0;
    case WM_LBUTTONUP:
        if (GetCapture() == hwnd) ReleaseCapture();
        scene_.onMouseUp(mouseX_, mouseY_);
        return 0;
    case WM_RBUTTONUP:
        quitRequested_ = true;
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) quitRequested_ = true;
        else if (wp == VK_F5 && config_.remote) {
            ShowWindow(hwnd_, SW_HIDE);
            Sleep(30);
            gl_.makeCurrent();
            gdiCapture_.setFrozen(false);
            renderer_.seedBackdrop(gdiCapture_, screenPos_.x, screenPos_.y);
            gdiCapture_.setFrozen(true);
            ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
            presentFrame();
        }
        return 0;
    case WM_SETCURSOR: {
        if (LOWORD(lp) == HTCLIENT) {
            const bool overWidget = scene_.hovered() != nullptr;
            SetCursor(LoadCursorW(nullptr, overWidget ? IDC_HAND : IDC_ARROW));
            return TRUE;
        }
        break;
    }
    case WM_ERASEBKGND:
        return TRUE;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void App::presentFrame() {
    if (!screenDc_ || !memDc_) return;
    renderer_.readbackBgra(readback_);
    if (dibBits_) std::memcpy(dibBits_, readback_.data(), readback_.size());

    SIZE size = { config_.width, config_.height };
    POINT srcOrigin = { 0, 0 };
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(hwnd_, screenDc_, &screenPos_, &size, memDc_, &srcOrigin, 0, &blend, ULW_ALPHA);
}

void App::saveShot() {
    const size_t rowBytes = static_cast<size_t>(config_.width) * 4;
    const int rows = config_.height;
    std::vector<uint8_t> rgba(readback_.size());
    for (int y = 0; y < rows; ++y) {
        const uint8_t* src = readback_.data() + static_cast<size_t>(rows - 1 - y) * rowBytes;
        uint8_t* dst = rgba.data() + static_cast<size_t>(y) * rowBytes;
        for (int x = 0; x < config_.width; ++x) {
            const unsigned a = src[x * 4 + 3];
            dst[x * 4 + 0] = src[x * 4 + 2]; 
            dst[x * 4 + 1] = src[x * 4 + 1]; 
            dst[x * 4 + 2] = src[x * 4 + 0]; 
            dst[x * 4 + 3] = static_cast<uint8_t>(a);
            for (int c = 0; c < 3; ++c) {
                const unsigned p = dst[x * 4 + c];
                dst[x * 4 + c] = static_cast<uint8_t>(a == 0 ? 0 : std::min(255u, (p * 255u + a / 2u) / a));
            }
        }
    }
    png::writeRgba(config_.shotPath, config_.width, config_.height, rgba.data());
}

int App::run() {
    scene_.setClickHandler([this](glass::Widget& w) {
        if (clickHandler_) clickHandler_(w);
    });
    if (config_.remote || config_.keepCapturable) {
        const ULONGLONG settleUntil = config_.shotPath.empty() ? (GetTickCount64() + 200) : GetTickCount64();
        while (!quitRequested_ && GetTickCount64() < settleUntil) {
            MSG msg;
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) quitRequested_ = true;
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            Sleep(15);
        }
        if (quitRequested_) return 0;
        gl_.makeCurrent();
        gdiCapture_.setFrozen(false);
        renderer_.seedBackdrop(gdiCapture_, screenPos_.x, screenPos_.y);
        gdiCapture_.setFrozen(true);
    }
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    
    UINT refreshHz = 60;
    DEVMODEW dm = {};
    dm.dmSize = sizeof(dm);
    MONITORINFOEXW mi = {};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST),
                        &mi) &&
        EnumDisplaySettingsW(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm) &&
        dm.dmDisplayFrequency >= 30) {
        refreshHz = dm.dmDisplayFrequency;
    }
    const DWORD frameWaitMs = std::max<DWORD>(1, 1000u / refreshHz);
    timeBeginPeriod(1);
    
    FILE* statsFile = nullptr;
    if (config_.stats) {
        _wfopen_s(&statsFile, L"stats.log", L"w");
        if (statsFile) {
            fprintf(statsFile, "refreshHz=%u frameWaitMs=%lu window=%dx%d guiScale=%.2f\n",
                    refreshHz, frameWaitMs, config_.width, config_.height, scale_);
            fflush(statsFile);
        }
    }
    double statsWindowStart = 0.0, glAcc = 0.0, presentAcc = 0.0;
    long statsFrames = 0;
    float lastMouseX = -1.0f, lastMouseY = -1.0f;
    bool lastMouseInside = false;

    while (!quitRequested_) {
        const DWORD wait = MsgWaitForMultipleObjects(0, nullptr, FALSE, frameWaitMs, QS_ALLINPUT);
        if (wait == 0xFFFFFFFF) break;
        
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) quitRequested_ = true;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (quitRequested_) break;

        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        const double nowSec = now.QuadPart / static_cast<double>(qpcFreq_.QuadPart);
        double dt = nowSec - lastFrame_.QuadPart / static_cast<double>(qpcFreq_.QuadPart);
        lastFrame_ = now;
        if (dt < 0.0) dt = 0.0;
        if (dt > 0.1) dt = 0.1;

        scene_.update(dt, mouseX_, mouseY_, mouseInside_);
        if (frameHandler_) frameHandler_(scene_, dt);
        
        const bool sceneDirty =
            mouseX_ != lastMouseX || mouseY_ != lastMouseY || mouseInside_ != lastMouseInside ||
            scene_.anyAnimActive();
        lastMouseX = mouseX_;
        lastMouseY = mouseY_;
        lastMouseInside = mouseInside_;
        
        if (source_ == &dupCapture_ && !dupCapture_.healthy()) {
            source_ = &gdiCapture_;
        }

        gl_.makeCurrent();
        LARGE_INTEGER t0, t1, t2;
        QueryPerformanceCounter(&t0);
        const bool backdropNew = renderer_.render(scene_, nowSec, *source_,
                                                  screenPos_.x, screenPos_.y);
        QueryPerformanceCounter(&t1);
        if (backdropNew || sceneDirty) {
            presentFrame();
            QueryPerformanceCounter(&t2);
            presentAcc += (t2.QuadPart - t1.QuadPart) / static_cast<double>(qpcFreq_.QuadPart);
        } else {
            t2 = t1;
        }
        glAcc += (t1.QuadPart - t0.QuadPart) / static_cast<double>(qpcFreq_.QuadPart);

        ++frameIndex_;
        ++statsFrames;
        if ((statsFile || config_.diag) && nowSec - statsWindowStart >= 1.0) {
            const bool dup = source_ == &dupCapture_;
            char statBuf[256];
            snprintf(statBuf, sizeof(statBuf),
                    "t=%.0fs fps=%.0f gl=%.1fms present=%.1fms dup=%d hdr=%d "
                    "dupFrames=%ld white=%.2f nits=%.0f matched=%d calibrated=%d\n",
                    nowSec, statsFrames / (nowSec - statsWindowStart),
                    glAcc * 1000.0 / std::max<long>(1, statsFrames),
                    presentAcc * 1000.0 / std::max<long>(1, statsFrames),
                    static_cast<int>(dup),
                    dup ? static_cast<int>(dupCapture_.hdrActive()) : 0,
                    dup ? dupCapture_.framesGot() : -1,
                    dup ? dupCapture_.whiteScale() : 0.0f,
                    dup ? dupCapture_.sdrWhiteNits() : 0.0f,
                    dup ? static_cast<int>(dupCapture_.scaleMatched()) : 0,
                    dup ? static_cast<int>(dupCapture_.scaleCalibrated()) : 0);
            if (statsFile) { fputs(statBuf, statsFile); fflush(statsFile); }
            if (config_.diag) { fputs(statBuf, stdout); fflush(stdout); }
            statsWindowStart = nowSec;
            statsFrames = 0;
            glAcc = presentAcc = 0.0;
        }
        if (!config_.shotPath.empty() && frameIndex_ >= config_.shotFrames) {
            std::wstring base = config_.shotPath;
            const size_t dot = base.find_last_of(L'.');
            if (dot != std::wstring::npos) base = base.substr(0, dot);
            renderer_.dumpTextures(base);
            saveShot();
            break;
        }
    }

    if (statsFile) fclose(statsFile);
    timeEndPeriod(1);
    return 0;
}