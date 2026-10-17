#include "GlContext.h"
#include "GlFunctions.h"
#include <string>

namespace {
constexpr wchar_t kHelperClassName[] = L"LiquidGlassDemo_GlHelper";

#ifndef WGL_ARB_create_context
#define WGL_ARB_create_context 1
#define WGL_CONTEXT_MAJOR_VERSION_ARB  0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB  0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB   0x209D
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#endif


template <typename T>
T wglProc(const char* name) {
    return reinterpret_cast<T>(wglGetProcAddress(name));
}

int describePixelFormat(HDC dc) {
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize    = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags  = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;
    return ChoosePixelFormat(dc, &pfd);
}
} 

GlContext::~GlContext() {
    if (rc_) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(rc_);
    }
    if (dc_) ReleaseDC(hwnd_, dc_);
    if (hwnd_) DestroyWindow(hwnd_);
}

LRESULT CALLBACK GlContext::helperProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    return DefWindowProcW(h, msg, wp, lp);
}

bool GlContext::create(HINSTANCE instance, std::wstring* error) {
    auto fail = [&](const std::wstring& what) {
        if (error) *error = what;
        return false;
    };

    WNDCLASSEXW wc  = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = helperProc;
    wc.hInstance     = instance;
    wc.lpszClassName = kHelperClassName;
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(WS_EX_NOACTIVATE, kHelperClassName, L"", WS_OVERLAPPEDWINDOW,
                            0, 0, 8, 8, nullptr, nullptr, instance, nullptr);
    if (!hwnd_) return fail(L"Failed to create the OpenGL helper window.");

    dc_ = GetDC(hwnd_);
    const int pf = describePixelFormat(dc_);
    PIXELFORMATDESCRIPTOR have = {};
    have.nSize = sizeof(have);
    if (pf == 0 || !DescribePixelFormat(dc_, pf, sizeof(have), &have) ||
        !SetPixelFormat(dc_, pf, &have)) {
        return fail(L"Failed to set the pixel format for the OpenGL helper window.");
    }
    
    HGLRC legacy = wglCreateContext(dc_);
    if (!legacy || !wglMakeCurrent(dc_, legacy)) return fail(L"Failed to initialize OpenGL.");

    using CreateContextAttribsFn = HGLRC(WINAPI*)(HDC, HGLRC, const int*);
    auto createAttribs = wglProc<CreateContextAttribsFn>("wglCreateContextAttribsARB");
    
    struct Attempt { int major; int minor; bool coreProfile; };
    const Attempt attempts[] = {
        { 4, 6, false },
        { 4, 5, false },
        { 4, 3, false },
        { 4, 0, false },
        { 3, 3, true  },
        { 3, 3, false },
        { 3, 2, false },
        { 3, 1, false },
        { 3, 0, false },
    };

    DWORD lastError = 0;
    if (createAttribs) {
        for (const Attempt& attempt : attempts) {
            int attribs[7];
            int n = 0;
            attribs[n++] = WGL_CONTEXT_MAJOR_VERSION_ARB; attribs[n++] = attempt.major;
            attribs[n++] = WGL_CONTEXT_MINOR_VERSION_ARB; attribs[n++] = attempt.minor;
            if (attempt.coreProfile) {
                attribs[n++] = WGL_CONTEXT_PROFILE_MASK_ARB;
                attribs[n++] = WGL_CONTEXT_CORE_PROFILE_BIT_ARB;
            }
            attribs[n] = 0;

            SetLastError(0);
            rc_ = createAttribs(dc_, nullptr, attribs);
            if (rc_ && wglMakeCurrent(dc_, rc_)) {
                std::string missing;
                if (gl::loadFunctions(&missing)) {
                    wglDeleteContext(legacy);
                    return true;
                }
            }
            if (rc_) { wglDeleteContext(rc_); rc_ = nullptr; }
            lastError = GetLastError();
        }

        int emptyAttribs[] = { 0 };
        SetLastError(0);
        rc_ = createAttribs(dc_, nullptr, emptyAttribs);
        if (rc_ && wglMakeCurrent(dc_, rc_)) {
            std::string missing;
            if (gl::loadFunctions(&missing)) {
                wglDeleteContext(legacy);
                return true;
            }
        }
        if (rc_) { wglDeleteContext(rc_); rc_ = nullptr; }
    }

    if (wglMakeCurrent(dc_, legacy)) {
        std::string missing;
        if (gl::loadFunctions(&missing)) {
            rc_ = legacy;
            return true;
        }
    }

    wchar_t detail[128];
    swprintf_s(detail, L"(WGL error 0x%08lX)", static_cast<unsigned long>(lastError));
    std::wstring msg = std::wstring(L"The driver does not support a compatible OpenGL context ") + detail;
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(legacy);
    return fail(msg);
}

void GlContext::makeCurrent() const {
    wglMakeCurrent(dc_, rc_);
}
