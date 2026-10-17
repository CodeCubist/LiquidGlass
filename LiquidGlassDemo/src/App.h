#pragma once
#include "CaptureSource.h"
#include "DesktopCapture.h"
#include "DesktopDup.h"
#include "GlassRenderer.h"
#include "GlassScene.h"
#include "GlContext.h"
#include <functional>
#include <string>
#include <vector>

struct AppConfig {
    int width = 1100;             
    int height = 720;             
    float scaleOverride = 0.0f;   
    int blurOverride = -1;        
    bool useGdi = false;          
    std::wstring shotPath;    
    int shotFrames = 30;      
    bool stats = false;       
    bool keepCapturable = false; 
    bool remote = false;
    bool diag = false;
    bool helpAndExit = false;
};

class App {
public:
    App() = default;
    ~App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    [[nodiscard]] bool init(HINSTANCE instance, const AppConfig& config, std::wstring* error);
    int run();

    glass::Scene& scene() { return scene_; }
    GlassRenderer& renderer() { return renderer_; }
    
    float guiScale() const { return scale_; }
    
    void setFrameHandler(std::function<void(glass::Scene&, double dt)> handler) {
        frameHandler_ = std::move(handler);
    }
    void setClickHandler(std::function<void(glass::Widget&)> handler) {
        clickHandler_ = std::move(handler);
    }

private:
    static LRESULT CALLBACK wndProcThunk(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    void presentFrame();
    void saveShot();

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    AppConfig config_;

    GlContext gl_;
    GlassRenderer renderer_;
    DesktopCapture gdiCapture_;
    DesktopDuplication dupCapture_;
    ICaptureSource* source_ = nullptr;
    glass::Scene scene_;
    
    HBITMAP dib_ = nullptr;
    void* dibBits_ = nullptr;
    HDC screenDc_ = nullptr;
    HDC memDc_ = nullptr;
    std::vector<uint8_t> readback_;
    
    bool mouseInside_ = false;
    bool trackingMouse_ = false;
    float mouseX_ = 0.0f, mouseY_ = 0.0f;
    POINT screenPos_ = {};
    
    LARGE_INTEGER qpcFreq_ = {};
    LARGE_INTEGER lastFrame_ = {};
    long long frameIndex_ = 0;
    bool quitRequested_ = false;
    float scale_ = 1.0f;

    std::function<void(glass::Scene&, double)> frameHandler_;
    std::function<void(glass::Widget&)> clickHandler_;
};
