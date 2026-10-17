#include "DesktopCapture.h"
#include "GlFunctions.h"
#include <algorithm>
#include <cmath>
#include <vector>

DesktopCapture::~DesktopCapture() { release(); }

void DesktopCapture::release() {
    if (oldBitmap_ && memDc_) SelectObject(memDc_, oldBitmap_);
    if (bitmap_) DeleteObject(bitmap_);
    if (memDc_) DeleteDC(memDc_);
    if (screenDc_) ReleaseDC(nullptr, screenDc_);
    bitmap_ = nullptr;
    memDc_ = nullptr;
    screenDc_ = nullptr;
    oldBitmap_ = nullptr;
    bits_ = nullptr;
    width_ = height_ = 0;
}

bool DesktopCapture::resize(int width, int height) {
    if (width <= 0 || height <= 0) return false;
    if (width == width_ && height == height_ && bitmap_) return true;
    release();

    BITMAPINFOHEADER bi = {};
    bi.biSize        = sizeof(bi);
    bi.biWidth       = width;
    bi.biHeight      = height;
    bi.biPlanes      = 1;
    bi.biBitCount    = 32;
    bi.biCompression = BI_RGB;

    screenDc_ = GetDC(nullptr);
    memDc_    = CreateCompatibleDC(screenDc_);
    bitmap_   = CreateDIBSection(nullptr, reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS, &bits_, nullptr, 0);
    if (!screenDc_ || !memDc_ || !bitmap_ || !bits_) { release(); return false; }
    oldBitmap_ = SelectObject(memDc_, bitmap_);

    width_  = width;
    height_ = height;
    generateFallback();
    return true;
}

void DesktopCapture::generateFallback() {
    if (!bits_ || width_ <= 0 || height_ <= 0) return;
    auto* dst = static_cast<uint8_t*>(bits_);
    for (int y = 0; y < height_; ++y) {
        float fy = static_cast<float>(y) / static_cast<float>(height_);
        for (int x = 0; x < width_; ++x) {
            float fx = static_cast<float>(x) / static_cast<float>(width_);
            float f = (fx + (1.0f - fy)) * 0.5f;
            uint8_t r = static_cast<uint8_t>(std::clamp(30.0f + 50.0f * (1.0f - f), 0.0f, 255.0f));
            uint8_t g = static_cast<uint8_t>(std::clamp(45.0f + 85.0f * (1.0f - fy) + 40.0f * fx, 0.0f, 255.0f));
            uint8_t b = static_cast<uint8_t>(std::clamp(80.0f + 140.0f * (1.0f - fy) + 60.0f * fx, 0.0f, 255.0f));
            size_t idx = (static_cast<size_t>(y) * width_ + x) * 4;
            dst[idx + 0] = b;
            dst[idx + 1] = g;
            dst[idx + 2] = r;
            dst[idx + 3] = 255;
        }
    }
}

float DesktopCapture::referenceLinearLuminance() const {
    if (!bits_ || width_ <= 0 || height_ <= 0) return 0.0f;
    const auto* px = static_cast<const uint8_t*>(bits_);
    std::vector<float> lums;
    lums.reserve(static_cast<size_t>(width_ / 8) * (height_ / 8));
    auto toLinear = [](float c) {
        return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
    };
    for (int y = 0; y < height_; y += 8) {
        for (int x = 0; x < width_; x += 8) {
            const size_t i = (static_cast<size_t>(y) * width_ + x) * 4;
            const float b = toLinear(px[i + 0] / 255.0f);
            const float g = toLinear(px[i + 1] / 255.0f);
            const float r = toLinear(px[i + 2] / 255.0f);
            lums.push_back(0.2126f * r + 0.7152f * g + 0.0722f * b);
        }
    }
    if (lums.empty()) return 0.0f;
    const size_t idx = static_cast<size_t>(lums.size() * 0.9);
    std::nth_element(lums.begin(), lums.begin() + std::min(idx, lums.size() - 1), lums.end());
    return lums[std::min(idx, lums.size() - 1)];
}

bool DesktopCapture::captureToTexture(int screenX, int screenY) {
    if (!bitmap_ || frozen_) return false;
    if (!screenDc_) screenDc_ = GetDC(nullptr);
    BOOL ok = screenDc_ ? BitBlt(memDc_, 0, 0, width_, height_, screenDc_, screenX, screenY, SRCCOPY) : FALSE;
    if (!ok) {
        if (screenDc_) { ReleaseDC(nullptr, screenDc_); screenDc_ = nullptr; }
        screenDc_ = GetDC(nullptr);
        if (screenDc_) {
            ok = BitBlt(memDc_, 0, 0, width_, height_, screenDc_, screenX, screenY, SRCCOPY);
        }
    }
    if (!ok) {
        generateFallback();
    }

    gl::PixelStorei(GL_UNPACK_ALIGNMENT, 4);
    gl::TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_, height_, gl::kBgra, GL_UNSIGNED_BYTE, bits_);
    return true;
}
