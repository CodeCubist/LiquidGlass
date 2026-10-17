#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#endif
#include <windows.h>
#include "CaptureSource.h"

class DesktopCapture : public ICaptureSource {
public:
    DesktopCapture() = default;
    ~DesktopCapture();
    DesktopCapture(const DesktopCapture&) = delete;
    DesktopCapture& operator=(const DesktopCapture&) = delete;
    
    [[nodiscard]] bool resize(int width, int height) override;
    
    bool captureToTexture(int screenX, int screenY) override;

    float referenceLinearLuminance() const;
    
    void setFrozen(bool frozen) { frozen_ = frozen; }

    int width() const { return width_; }
    int height() const { return height_; }

private:
    void release();
    void generateFallback();

    int width_ = 0, height_ = 0;
    bool frozen_ = false;
    HDC screenDc_ = nullptr;
    HDC memDc_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HGDIOBJ oldBitmap_ = nullptr;
    void* bits_ = nullptr;
};
