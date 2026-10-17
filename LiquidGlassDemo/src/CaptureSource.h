#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#endif

class ICaptureSource {
public:
    virtual ~ICaptureSource() = default;
    
    virtual bool resize(int width, int height) = 0;
    
    virtual bool captureToTexture(int screenX, int screenY) = 0;
};
