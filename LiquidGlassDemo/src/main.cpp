#include "App.h"
#include "GlassScene.h"

#include <objbase.h>
#include <shellapi.h>
#include <cstdio>
#include <algorithm>
#include <string>
#include <vector>

namespace {

enum WidgetId {
    kPanel = 1,
    kDropA,
    kDropB,
    kBtnPill,
    kBtnCircle,
    kToggle,
    kToggleKnob,
    kRepel,
    kHint,
};

constexpr int kLabelPx = 16;
constexpr int kHintPx = 14;

std::wstring pillLabel(int clicks) {
    return L"Glass Button - " + std::to_wstring(clicks) + L" clicks";
}

std::wstring circleLabel(int step) {
    switch (step % 3) {
    case 0: return L"+";
    case 1: return L"OK";
    default: return L"R";
    }
}

glass::Widget& addWidget(glass::Scene& scene, int id, float x, float y, float w, float h,
                         float radius, bool circle) {
    glass::Widget widget;
    widget.id = id;
    widget.x = x;
    widget.y = y;
    widget.w = w;
    widget.h = h;
    
    widget.cornerRadius = circle ? 0.5f * std::min(w, h) : radius;
    const double h0 = std::sin(x * 12.9898 + y * 78.233);
    widget.seed = static_cast<float>(h0 - std::floor(h0));
    scene.widgets().push_back(widget);
    return scene.widgets().back();
}

void buildScene(glass::Scene& scene, bool& toggleOn) {
    
    glass::Widget& panel = addWidget(scene, kPanel, 70, 120, 300, 260, 22, false);
    panel.style.moveable = true;
    panel.style.blurRadius = 6;
    panel.style.smoothing = 0.004f;
    panel.label = L"Liquid Glass Panel - draggable";
    panel.labelAlpha = 0.8f;
    
    glass::Widget& dropA = addWidget(scene, kDropA, 450, 160, 110, 110, 0, true);
    dropA.style.moveable = true;
    dropA.style.blurRadius = 5;
    dropA.style.smoothing = 0.012f;
    dropA.style.tintR = 0.55f; dropA.style.tintG = 0.75f; dropA.style.tintB = 1.0f;
    dropA.style.tintA = 0.10f;
    dropA.label = L"Drop A";

    glass::Widget& dropB = addWidget(scene, kDropB, 566, 200, 110, 110, 0, true);
    dropB.style = dropA.style;
    dropB.id = kDropB;
    dropB.x = 566; dropB.y = 200;
    dropB.label = L"Drop B";
    
    glass::Widget& pill = addWidget(scene, kBtnPill, 450, 540, 210, 56, 28, false);
    pill.style.clickable = true;
    pill.style.moveable = true;
    pill.style.blurRadius = 4;
    pill.style.smoothing = 0.004f;
    pill.label = pillLabel(0);
    
    glass::Widget& circle = addWidget(scene, kBtnCircle, 710, 536, 64, 64, 0, true);
    circle.style.clickable = true;
    circle.style.blurRadius = 4;
    circle.style.smoothing = 0.004f;
    circle.label = circleLabel(0);
    
    glass::Widget& track = addWidget(scene, kToggle, 830, 150, 96, 48, 24, false);
    track.style.clickable = true;
    track.style.blurRadius = 4;
    track.style.smoothing = 0.004f;

    glass::Widget& knob = addWidget(scene, kToggleKnob, track.x + 4, track.y + 4, 40, 40, 0, true);
    knob.style.blurRadius = 2;
    knob.style.smoothing = 0.0f;
    knob.label = toggleOn ? L"ON" : L"OFF";
    knob.labelAlpha = 0.0f; 
    
    glass::Widget& repel = addWidget(scene, kRepel, 960, 380, 88, 88, 20, false);
    repel.style.moveable = true;
    repel.style.blurRadius = 5;
    repel.style.smoothing = -0.012f; 
    repel.style.tintR = 1.0f; repel.style.tintG = 0.6f; repel.style.tintB = 0.4f;
    repel.style.tintA = 0.12f;
    repel.label = L"Repel";
    
    glass::Widget& hint = addWidget(scene, kHint, 280, 640, 540, 44, 22, false);
    hint.style.blurRadius = 2;
    hint.style.smoothing = 0.0f;
    hint.label = L"Drag two drops close to see liquid adhesion - ESC / Right-click quits";
    hint.labelAlpha = 0.7f;
    
    for (glass::Widget& w : scene.widgets()) {
        w.style.shadowFactor = 0.0f;
        w.style.shadowA = 0.0f;
        w.style.blurRadius = 12;
    }
}

AppConfig parseCommandLine() {
    AppConfig config;
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return config;
    for (int i = 1; i < argc; ++i) {
        const std::wstring arg = argv[i];
        
        
        const auto value = [&arg](const wchar_t* prefix, std::wstring* out) {
            const size_t n = wcslen(prefix);
            if (arg.size() <= n || arg.compare(0, n, prefix) != 0) return false;
            *out = arg.substr(n);
            return true;
        };
        std::wstring v;
        if (value(L"--shot=", &v)) config.shotPath = v;
        else if (value(L"--frames=", &v)) config.shotFrames = _wtoi(v.c_str());
        else if (value(L"--w=", &v)) config.width = _wtoi(v.c_str());
        else if (value(L"--h=", &v)) config.height = _wtoi(v.c_str());
        else if (value(L"--blur=", &v)) config.blurOverride = _wtoi(v.c_str());
        else if (value(L"--scale=", &v)) config.scaleOverride = static_cast<float>(_wtof(v.c_str()));
        else if (arg == L"--gdi") config.useGdi = true;
        else if (arg == L"--dup") {}
        else if (arg == L"--stats") config.stats = true;
        else if (arg == L"--diag" || arg == L"-v" || arg == L"--verbose") { config.diag = true; config.stats = true; }
        else if (arg == L"--remote") { config.remote = true; }
        else if (arg == L"--keep-capturable") config.keepCapturable = true;
        else if (arg == L"--help") config.helpAndExit = true;
    }
    LocalFree(argv);
    return config;
}

void printHelp() {
    MessageBoxW(nullptr,
        L"Usage: LiquidGlassDemo.exe [options]\n"
        L"  --shot=<path.png>  render N frames, save the frame as PNG and exit\n"
        L"  --frames=<N>       frame count for --shot mode (default 30)\n"
        L"  --w=<N> --h=<N>    window size (default 1100x720)\n"
        L"  --blur=<N>         force every widget's blur radius (0 = crisp)\n"
        L"  --scale=<F>        force the GUI scale (default: DPI, fits window)\n"
        L"  --gdi              force the GDI BitBlt backdrop source\n"
        L"                     (default: DXGI duplication, CPU-converted to SDR)\n"
        L"  --keep-capturable  leave the window visible to screenshots and\n"
        L"                     capture tools.\n"
        L"  --remote           remote control mode: visible over remote desktop / remote control\n"
        L"                     (disables exclusion affinity and uses GDI capture)\n"
        L"  --diag             output startup diagnostics and timing logs to console\n"
        L"  --stats            write per-second timing samples to stats.log\n\n"
        L"Interaction:\n"
        L"  Drag the panel / drops / repel chip    liquid adhesion and separation\n"
        L"  Click buttons and the toggle           focus sweep and click feedback\n"
        L"  ESC or right mouse button              quit",
        L"LiquidGlassDemo", MB_OK | MB_ICONINFORMATION);
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

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    AppConfig config = parseCommandLine();
    if (config.helpAndExit) {
        printHelp();
        return 0;
    }

    if (config.diag) {
        FILE* f = nullptr;
        if (_wfopen_s(&f, L"diag.log", L"w") == 0 && f) fclose(f);
        if (AttachConsole(ATTACH_PARENT_PROCESS)) {
            FILE* fp = nullptr;
            freopen_s(&fp, "CONOUT$", "w", stdout);
            freopen_s(&fp, "CONOUT$", "w", stderr);
        }
        logMsg(L"[LiquidGlassDemo] Starting diagnostic mode...\n");
    }
    
    HANDLE instanceMutex = CreateMutexW(nullptr, TRUE, L"LiquidGlassDemo_Instance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (instanceMutex) { ReleaseMutex(instanceMutex); CloseHandle(instanceMutex); }
        if (config.diag) {
            logMsg(L"[LiquidGlassDemo] ERROR: Another instance is already running.\n");
        }
        MessageBoxW(nullptr, L"LiquidGlassDemo is already running.",
                    L"LiquidGlassDemo", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) {
        if (config.diag) {
            logMsg(L"[LiquidGlassDemo] ERROR: COM initialization failed.\n");
        }
        MessageBoxW(nullptr, L"COM initialization failed.", L"LiquidGlassDemo", MB_ICONERROR);
        return 1;
    }

    App app;
    std::wstring error;
    if (!app.init(instance, config, &error)) {
        if (config.diag) {
            logMsg(L"[LiquidGlassDemo] ERROR: %ls\n", error.c_str());
        }
        FILE* log = nullptr;
        if (_wfopen_s(&log, L"init_error.log", L"w") == 0 && log) {
            fwprintf(log, L"%s\n", error.c_str());
            fclose(log);
        }
        CoUninitialize();
        MessageBoxW(nullptr, error.c_str(), L"LiquidGlassDemo Startup Failed", MB_ICONERROR);
        return 1;
    }

    bool toggleOn = false;
    buildScene(app.scene(), toggleOn);
    
    const float guiScale = app.guiScale();
    for (glass::Widget& w : app.scene().widgets()) {
        w.x *= guiScale;
        w.y *= guiScale;
        w.w *= guiScale;
        w.h *= guiScale;
        w.cornerRadius *= guiScale;
        w.style.shadowOffsetX *= guiScale;
        w.style.shadowOffsetY *= guiScale;
    }

    
    if (config.blurOverride >= 0) {
        const int radius = std::clamp(config.blurOverride, 0, 64);
        for (glass::Widget& w : app.scene().widgets()) {
            w.style.blurRadius = radius;
        }
    }
    
    for (const glass::Widget& w : app.scene().widgets()) {
        if (!w.label.empty()) {
            const int px = (w.id == kHint) ? kHintPx : kLabelPx;
            app.renderer().setLabel(w.id, w.label, px);
        }
    }
    
    app.setFrameHandler([&app, &toggleOn](glass::Scene& scene, double dt) {
        glass::Widget* track = nullptr;
        glass::Widget* knob = nullptr;
        for (glass::Widget& w : scene.widgets()) {
            if (w.id == kToggle) track = &w;
            if (w.id == kToggleKnob) knob = &w;
        }
        if (!track || !knob) return;
        const float margin = 4.0f;
        const float targetX = toggleOn
            ? track->x + track->w - knob->w - margin
            : track->x + margin;
        knob->x = glass::Scene::smoothToward(knob->x, targetX, dt, 0.15f);
        knob->y = track->y + 0.5f * (track->h - knob->h);
        glass::WidgetStyle& s = track->style;
        const float target = toggleOn ? 1.0f : 0.0f;
        s.tintR = glass::Scene::smoothToward(s.tintR, 0.35f * target, dt, 0.15f);
        s.tintG = glass::Scene::smoothToward(s.tintG, 0.80f * target, dt, 0.15f);
        s.tintB = glass::Scene::smoothToward(s.tintB, 0.45f * target, dt, 0.15f);
        s.tintA = glass::Scene::smoothToward(s.tintA, 0.20f * target, dt, 0.15f);
    });
    
    int pillClicks = 0;
    int circleStep = 0;
    app.setClickHandler([&app, &toggleOn, &pillClicks, &circleStep](glass::Widget& w) {
        switch (w.id) {
        case kBtnPill:
            ++pillClicks;
            w.label = pillLabel(pillClicks);
            app.renderer().setLabel(w.id, w.label, kLabelPx);
            break;
        case kBtnCircle:
            ++circleStep;
            w.label = circleLabel(circleStep);
            app.renderer().setLabel(w.id, w.label, kLabelPx);
            break;
        case kToggle:
            toggleOn = !toggleOn;
            break;
        default:
            break;
        }
    });

    const int exitCode = app.run();
    if (instanceMutex) { ReleaseMutex(instanceMutex); CloseHandle(instanceMutex); }
    CoUninitialize();
    return exitCode;
}
