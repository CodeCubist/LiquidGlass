#include "GdiText.h"
#include "GlFunctions.h"
#include <algorithm>
#include <cstdint>
#include <vector>

GdiText::~GdiText() { release(); }

void GdiText::release() {
    if (texture) { gl::DeleteTextures(1, &texture); texture = 0; }
}

bool GdiText::render(const std::wstring& text, int pixelHeight) {
    release();
    if (text.empty() || pixelHeight <= 0) return false;
    
    constexpr int kSuper = 3;
    const int drawHeight = pixelHeight * kSuper;
    
    HFONT font = CreateFontW(
        -drawHeight, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    if (!font) font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

    HDC hdc = CreateCompatibleDC(nullptr);
    HGDIOBJ oldFont = SelectObject(hdc, font);
    bool ok = false;

    do {
        RECT rc = {0, 0, 0, 0};
        DrawTextW(hdc, text.c_str(), -1, &rc, DT_CALCRECT | DT_NOPREFIX | DT_SINGLELINE);
        const int pad = std::max(2, pixelHeight / 4) * kSuper;
        const int textW = std::max(1, static_cast<int>(rc.right - rc.left));
        const int textH = std::max(1, static_cast<int>(rc.bottom - rc.top));
        const int bigW = textW + pad * 2;
        const int bigH = textH + pad * 2;

        BITMAPINFOHEADER bi = {};
        bi.biSize = sizeof(bi);
        bi.biWidth = bigW;
        bi.biHeight = bigH;
        bi.biPlanes = 1;
        bi.biBitCount = 32;
        bi.biCompression = BI_RGB;

        HDC memDc = CreateCompatibleDC(hdc);
        if (!memDc) break;
        HBITMAP bmp = CreateDIBSection(hdc, reinterpret_cast<BITMAPINFO*>(&bi),
                                       DIB_RGB_COLORS, nullptr, nullptr, 0);
        if (!bmp) { DeleteDC(memDc); break; }
        HGDIOBJ oldBmp = SelectObject(memDc, bmp);
        HGDIOBJ oldMemFont = SelectObject(memDc, font);

        HBRUSH black = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        RECT full = {0, 0, bigW, bigH};
        FillRect(memDc, &full, black);
        SetBkMode(memDc, TRANSPARENT);
        SetTextColor(memDc, RGB(255, 255, 255));
        RECT drawRc = {pad, pad, pad + textW, pad + textH};
        DrawTextW(memDc, text.c_str(), -1, &drawRc,
                  DT_NOPREFIX | DT_SINGLELINE | DT_CENTER | DT_VCENTER);

        std::vector<uint8_t> raw(static_cast<size_t>(bigW) * bigH * 4);
        ok = GetDIBits(memDc, bmp, 0, bigH, raw.data(),
                       reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS) != 0;
        SelectObject(memDc, oldMemFont);
        SelectObject(memDc, oldBmp);
        DeleteObject(bmp);
        DeleteDC(memDc);
        if (!ok) break;
        
        width  = bigW / kSuper;
        height = bigH / kSuper;
        std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);
        auto luminance = [&raw, bigW](int x, int y) {
            const size_t i = (static_cast<size_t>(y) * bigW + x) * 4;
            const float b = static_cast<float>(raw[i + 0]);
            const float g = static_cast<float>(raw[i + 1]);
            const float r = static_cast<float>(raw[i + 2]);
            return (0.114f * b + 0.587f * g + 0.299f * r) / 255.0f;
        };
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float acc = 0.0f;
                for (int sy = 0; sy < kSuper; ++sy) {
                    for (int sx = 0; sx < kSuper; ++sx) {
                        const int srcY = std::min(y * kSuper + sy, bigH - 1);
                        const int srcX = std::min(x * kSuper + sx, bigW - 1);
                        acc += luminance(srcX, srcY);
                    }
                }
                const float cov = acc / (kSuper * kSuper);
                const size_t i = (static_cast<size_t>(y) * width + x) * 4;
                pixels[i + 0] = 255;
                pixels[i + 1] = 255;
                pixels[i + 2] = 255;
                pixels[i + 3] = static_cast<uint8_t>(std::min(cov, 1.0f) * 255.0f + 0.5f);
            }
        }

        gl::GenTextures(1, &texture);
        gl::BindTexture(GL_TEXTURE_2D, texture);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl::kClampToEdge);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl::kClampToEdge);
        gl::PixelStorei(GL_UNPACK_ALIGNMENT, 4);
        gl::TexImage2D(GL_TEXTURE_2D, 0, gl::kRgba8, width, height, 0,
                       GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        ok = texture != 0;
    } while (false);

    SelectObject(hdc, oldFont);
    if (font) DeleteObject(font);
    DeleteDC(hdc);
    return ok;
}
