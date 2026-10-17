// GdiText.h - Rasterizes text into a white-glyph + coverage-alpha texture
// via GDI, for labels composited over the transparent glass.
#pragma once
// GL/gl.h hard-depends on wingdi.h (WINGDIAPI), so windows.h must come first.
#include <windows.h>
#include <GL/gl.h>
#include <string>

class GdiText {
public:
    GdiText() = default;
    ~GdiText();
    GdiText(const GdiText&) = delete;
    GdiText& operator=(const GdiText&) = delete;
    
    [[nodiscard]] bool render(const std::wstring& text, int pixelHeight);

    GLuint texture = 0;
    int width = 0;
    int height = 0;

private:
    void release();
};
