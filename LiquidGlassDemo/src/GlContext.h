#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#endif
#include <windows.h>
#include <string>

class GlContext {
public:
    GlContext() = default;
    ~GlContext();
    GlContext(const GlContext&) = delete;
    GlContext& operator=(const GlContext&) = delete;
    
    bool create(HINSTANCE instance, std::wstring* error);

    void makeCurrent() const;

private:
    static LRESULT CALLBACK helperProc(HWND, UINT, WPARAM, LPARAM);
    HWND   hwnd_ = nullptr;
    HDC    dc_   = nullptr;
    HGLRC  rc_   = nullptr;
};
