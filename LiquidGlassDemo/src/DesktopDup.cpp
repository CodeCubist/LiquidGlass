#include "DesktopDup.h"
#include "GlFunctions.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <dxgi1_5.h>

using Microsoft::WRL::ComPtr;

namespace {

constexpr int kCsPq = 11;
    
float halfToFloat(unsigned short h) {
    const unsigned int sign = (h & 0x8000u) << 16;
    const unsigned int exp  = (h & 0x7C00u) >> 10;
    unsigned int man        = h & 0x03FFu;
    unsigned int bits;
    if (exp == 0) {
        if (man == 0) {
            bits = sign;
        } else {
            unsigned int e = 127 - 15 + 1;
            while ((man & 0x0400u) == 0) { man <<= 1; --e; }
            man &= 0x03FFu;
            bits = sign | (e << 23) | (man << 13);
        }
    } else if (exp == 0x1F) {
        bits = sign | 0x7F800000u | (man << 13);
    } else {
        bits = sign | ((exp - 15 + 127) << 23) | (man << 13);
    }
    float out = 0.0f;
    std::memcpy(&out, &bits, sizeof(out));
    return out;
}

float linearToSrgb(float v) {
    return v <= 0.0031308f ? v * 12.92f : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
}
    
float pqEotf(float e) {
    if (e <= 0.0f) return 0.0f;
    if (e >= 1.0f) return 10000.0f;
    constexpr float m1 = 0.1593017578125f;
    constexpr float m2 = 78.84375f;
    constexpr float c1 = 0.8359375f;
    constexpr float c2 = 18.84765625f;
    constexpr float c3 = 18.6875f;
    const float ep = std::pow(e, 1.0f / m2);
    const float num = std::max(ep - c1, 0.0f);
    const float den = c2 - c3 * ep;
    return 10000.0f * std::pow(num / std::max(den, 1e-6f), 1.0f / m1);
}
    
unsigned int querySdrWhiteLevel(const WCHAR* gdiDeviceName) {
    UINT numPaths = 0, numModes = 0;
    if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &numPaths, &numModes) != ERROR_SUCCESS) {
        return 0;
    }
    std::vector<DISPLAYCONFIG_PATH_INFO> paths(numPaths);
    std::vector<DISPLAYCONFIG_MODE_INFO> modes(numModes);
    if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &numPaths, paths.data(),
                           &numModes, modes.data(), nullptr) != ERROR_SUCCESS) {
        return 0;
    }
    for (const DISPLAYCONFIG_PATH_INFO& path : paths) {
        DISPLAYCONFIG_SOURCE_DEVICE_NAME source = {};
        source.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
        source.header.size = sizeof(source);
        source.header.adapterId = path.sourceInfo.adapterId;
        source.header.id = path.sourceInfo.id;
        if (DisplayConfigGetDeviceInfo(&source.header) != ERROR_SUCCESS) continue;
        if (wcscmp(source.viewGdiDeviceName, gdiDeviceName) != 0) continue;

        DISPLAYCONFIG_SDR_WHITE_LEVEL white = {};
        white.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL;
        white.header.size = sizeof(white);
        white.header.adapterId = path.sourceInfo.adapterId;
        white.header.id = path.sourceInfo.id;
        if (DisplayConfigGetDeviceInfo(&white.header) != ERROR_SUCCESS) continue;
        return white.SDRWhiteLevel;
    }
    return 0;
}

}

DesktopDuplication::~DesktopDuplication() { reset(); }

void DesktopDuplication::reset() {
    if (context_) context_->ClearState();
    staging_.Reset();
    duplication_.Reset();
    context_.Reset();
    device_.Reset();
    mode_ = Mode::Unknown;
}

bool DesktopDuplication::resize(int width, int height) {
    if (width <= 0 || height <= 0) return false;
    reset();

    ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(factory.GetAddressOf())))) return false;

    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<IDXGIOutput> output;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    DXGI_OUTPUT_DESC outputDesc = {};
    bool found = false;

    for (UINT a = 0; factory->EnumAdapters1(a, adapter.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++a) {
        DXGI_ADAPTER_DESC1 ad = {};
        adapter->GetDesc1(&ad);
        if (ad.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

        for (UINT o = 0; adapter->EnumOutputs(o, output.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++o) {
            device.Reset();
            context.Reset();
            HRESULT hr = D3D11CreateDevice(
                adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, nullptr, 0,
                D3D11_SDK_VERSION, device.GetAddressOf(), nullptr, context.GetAddressOf());
            if (FAILED(hr)) continue;

            D3D11_TEXTURE2D_DESC stagingDesc = {};
            stagingDesc.Width            = static_cast<UINT>(width);
            stagingDesc.Height           = static_cast<UINT>(height);
            stagingDesc.MipLevels        = 1;
            stagingDesc.ArraySize        = 1;
            stagingDesc.SampleDesc.Count = 1;
            stagingDesc.Usage            = D3D11_USAGE_STAGING;
            stagingDesc.CPUAccessFlags   = D3D11_CPU_ACCESS_READ;

            Mode candidateMode = Mode::Unknown;
            ComPtr<IDXGIOutputDuplication> candidateDup;

            ComPtr<IDXGIOutput5> output5;
            if (SUCCEEDED(output.As(&output5))) {
                const DXGI_FORMAT fp16[] = { DXGI_FORMAT_R16G16B16A16_FLOAT };
                if (SUCCEEDED(output5->DuplicateOutput1(device.Get(), 0, 1, fp16, candidateDup.GetAddressOf()))) {
                    stagingDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
                    candidateMode = Mode::Fp16;
                }
            }
            if (candidateMode == Mode::Unknown) {
                ComPtr<IDXGIOutput1> output1;
                if (SUCCEEDED(output.As(&output1)) &&
                    SUCCEEDED(output1->DuplicateOutput(device.Get(), candidateDup.GetAddressOf()))) {
                    stagingDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
                    candidateMode = Mode::Bgra8;
                }
            }
            if (candidateMode == Mode::Unknown || !candidateDup) continue;

            ComPtr<ID3D11Texture2D> candidateStaging;
            if (FAILED(device->CreateTexture2D(&stagingDesc, nullptr, candidateStaging.GetAddressOf()))) continue;

            device_ = device;
            context_ = context;
            duplication_ = candidateDup;
            staging_ = candidateStaging;
            mode_ = candidateMode;

            output->GetDesc(&outputDesc);
            outputRect_ = outputDesc.DesktopCoordinates;

            if (ComPtr<IDXGIOutput6> output6; SUCCEEDED(output.As(&output6))) {
                DXGI_OUTPUT_DESC1 desc1 = {};
                if (SUCCEEDED(output6->GetDesc1(&desc1))) {
                    outputColorSpace_ = static_cast<int>(desc1.ColorSpace);
                }
            }

            const unsigned int level = querySdrWhiteLevel(outputDesc.DeviceName);
            if (level != 0) {
                scaleMatched_ = true;
                sdrWhiteNits_  = static_cast<float>(level) * 0.08f;
                whiteScale_    = static_cast<float>(level) / 1000.0f;
            }

            found = true;
            break;
        }
        if (found) break;
    }

    if (!found) {
        reset();
        return false;
    }

    width_  = width;
    height_ = height;
    scratch_.assign(static_cast<size_t>(width_) * height_ * 4, 0);
    return true;
}

bool DesktopDuplication::captureToTexture(int screenX, int screenY) {
    if (!device_ || duplication_ == nullptr) return false;
    
    DXGI_OUTDUPL_FRAME_INFO info = {};
    ComPtr<IDXGIResource> resource;
    const UINT timeout = everCaptured_ ? 0u : 100u;
    HRESULT hr = duplication_->AcquireNextFrame(timeout, &info, resource.GetAddressOf());
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        return false;
    }
    if (FAILED(hr)) {
        duplication_.Reset();
        if (++failCount_ > 90) return false;
        return false;
    }

    ComPtr<ID3D11Texture2D> desktopTex;
    hr = resource.As(&desktopTex);
    if (SUCCEEDED(hr)) {
        D3D11_TEXTURE2D_DESC desktopDesc = {};
        desktopTex->GetDesc(&desktopDesc);
        int localX = screenX - outputRect_.left;
        int localY = screenY - outputRect_.top;
        D3D11_BOX box;
        box.left   = static_cast<UINT>(std::clamp(localX, 0, static_cast<int>(desktopDesc.Width)));
        box.top    = static_cast<UINT>(std::clamp(localY, 0, static_cast<int>(desktopDesc.Height)));
        box.right  = std::min<UINT>(box.left + static_cast<UINT>(width_), desktopDesc.Width);
        box.bottom = std::min<UINT>(box.top + static_cast<UINT>(height_), desktopDesc.Height);
        box.front  = 0;
        box.back   = 1;
        if (box.right > box.left && box.bottom > box.top) {
            context_->CopySubresourceRegion(staging_.Get(), 0, 0, 0, 0,
                                            desktopTex.Get(), 0, &box);
        }
    }
    duplication_->ReleaseFrame();
    if (FAILED(hr)) {
        if (++failCount_ > 90) return false;
        return false;
    }

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(context_->Map(staging_.Get(), 0, D3D11_MAP_READ, 0, &mapped))) {
        if (++failCount_ > 90) return false;
        return false;
    }

    convertAndUpload(mapped.pData, mapped.RowPitch);
    context_->Unmap(staging_.Get(), 0);

    failCount_ = 0;
    everCaptured_ = true;
    ++framesGot_;
    return true;
}

void DesktopDuplication::convertAndUpload(void* mapped, UINT rowPitch) {
    const auto* src = static_cast<const uint8_t*>(mapped);
    const int w = width_;
    const int h = height_;

    if (mode_ == Mode::Bgra8) {
        for (int y = 0; y < h; ++y) {
            std::memcpy(scratch_.data() + static_cast<size_t>(y) * w * 4,
                        src + static_cast<size_t>(h - 1 - y) * rowPitch,
                        static_cast<size_t>(w) * 4);
        }
        gl::PixelStorei(GL_UNPACK_ALIGNMENT, 4);
        gl::TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h,
                          gl::kBgra, GL_UNSIGNED_BYTE, scratch_.data());
        return;
    }
    
    const auto* halves = reinterpret_cast<const unsigned short*>(mapped);
    const size_t halvesPerRow = rowPitch / 2;
    
    if (!scaleMatched_ && !scaleCalibrated_ && gdiRefSet_) {
        std::vector<float> sig;
        sig.reserve(static_cast<size_t>(h) * (w / 32));
        for (int y = 0; y < h; y += 4) {
            const unsigned short* row = halves + static_cast<size_t>(y) * halvesPerRow;
            for (int x = 0; x < w; x += 32) {
                const unsigned short* px = row + x * 4;
                sig.push_back(std::max({ halfToFloat(px[0]), halfToFloat(px[1]),
                                         halfToFloat(px[2]) }));
            }
        }
        if (!sig.empty()) {
            const size_t idx = static_cast<size_t>(sig.size() * 0.9);
            std::nth_element(sig.begin(), sig.begin() + std::min(idx, sig.size() - 1), sig.end());
            const float p90 = sig[std::min(idx, sig.size() - 1)];
            if (p90 > 0.0005f) {
                if (outputColorSpace_ == kCsPq) {
                    sdrWhiteNits_ = std::clamp(pqEotf(p90) * 10000.0f /
                                                   std::max(gdiRefLum_, 0.002f),
                                               40.0f, 4000.0f);
                } else {
                    whiteScale_ = std::clamp(p90 / std::max(gdiRefLum_, 0.002f), 1.0f, 12.0f);
                }
                scaleCalibrated_ = true;
            }
        }
    }
    
    const bool pq = outputColorSpace_ == kCsPq;
    const bool needRebuild =
        srgbLut_.empty() ||
        (pq ? std::abs(sdrWhiteNits_ - lutNits_) > lutNits_ * 0.04f
            : std::abs(whiteScale_ - lutScale_) > lutScale_ * 0.04f);
    if (needRebuild) {
        srgbLut_.resize(65536);
        for (unsigned int bits = 0; bits < 65536; ++bits) {
            const float v = halfToFloat(static_cast<unsigned short>(bits));
            float sdr = 0.0f;
            if (pq) {
                sdr = std::clamp(pqEotf(v) / sdrWhiteNits_, 0.0f, 1.0f);
            } else {
                sdr = std::clamp(v / whiteScale_, 0.0f, 1.0f);
            }
            srgbLut_[bits] = linearToSrgb(sdr);
        }
        lutScale_ = whiteScale_;
        lutNits_ = sdrWhiteNits_;
    }

    for (int y = 0; y < h; ++y) {
        const unsigned short* row = halves + static_cast<size_t>(h - 1 - y) * halvesPerRow;
        uint8_t* dst = scratch_.data() + static_cast<size_t>(y) * w * 4;
        for (int x = 0; x < w; ++x) {
            const unsigned short* px = row + x * 4; 
            dst[x * 4 + 0] = static_cast<uint8_t>(srgbLut_[px[2]] * 255.0f + 0.5f); 
            dst[x * 4 + 1] = static_cast<uint8_t>(srgbLut_[px[1]] * 255.0f + 0.5f); 
            dst[x * 4 + 2] = static_cast<uint8_t>(srgbLut_[px[0]] * 255.0f + 0.5f); 
            dst[x * 4 + 3] = 255;
        }
    }
    gl::PixelStorei(GL_UNPACK_ALIGNMENT, 4);
    gl::TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h,
                      gl::kBgra, GL_UNSIGNED_BYTE, scratch_.data());
}
