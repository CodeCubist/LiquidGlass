#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_5.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include "CaptureSource.h"

class DesktopDuplication : public ICaptureSource {
public:
    DesktopDuplication() = default;
    ~DesktopDuplication();
    DesktopDuplication(const DesktopDuplication&) = delete;
    DesktopDuplication& operator=(const DesktopDuplication&) = delete;
    
    [[nodiscard]] bool resize(int width, int height) override;
    
    bool captureToTexture(int screenX, int screenY) override;
    
    bool healthy() const { return failCount_ < 90; }
    
    void setGdiReference(float refLinearLuminance) {
        gdiRefLum_ = refLinearLuminance;
        gdiRefSet_ = refLinearLuminance > 0.002f;
    }
    
    bool hdrActive() const { return mode_ == Mode::Fp16; }
    float whiteScale() const { return whiteScale_; }
    float sdrWhiteNits() const { return sdrWhiteNits_; }
    int outputColorSpace() const { return outputColorSpace_; }
    bool scaleMatched() const { return scaleMatched_; }
    bool scaleCalibrated() const { return scaleCalibrated_; }
    long framesGot() const { return framesGot_; }

private:
    enum class Mode { Unknown, Bgra8, Fp16 };

    void reset();
    void convertAndUpload(void* mapped, UINT rowPitch);

    int width_ = 0, height_ = 0;
    Mode mode_ = Mode::Unknown;
    RECT outputRect_ = {};

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGIOutputDuplication> duplication_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_;
    
    float whiteScale_ = 1.0f;       
    float sdrWhiteNits_ = 80.0f;    
    int outputColorSpace_ = -1;     
    bool scaleMatched_ = false;     
    bool scaleCalibrated_ = false;  
    float gdiRefLum_ = 0.0f;
    bool gdiRefSet_ = false;
    float lutScale_ = -1.0f;        
    float lutNits_ = -1.0f;         
    std::vector<float> srgbLut_;    
    std::vector<uint8_t> scratch_;  

    bool everCaptured_ = false;     
    int failCount_ = 0;
    long framesGot_ = 0;            
};
