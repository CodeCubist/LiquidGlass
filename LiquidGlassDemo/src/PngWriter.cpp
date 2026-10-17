#include "PngWriter.h"
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <vector>

namespace png {

bool writeRgba(const std::wstring& path, int width, int height, const uint8_t* rgba) {
    if (!rgba || width <= 0 || height <= 0) return false;

    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(factory.GetAddressOf())))) {
        return false;
    }

    Microsoft::WRL::ComPtr<IWICStream> stream;
    if (FAILED(factory->CreateStream(stream.GetAddressOf())) ||
        FAILED(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE))) {
        return false;
    }

    Microsoft::WRL::ComPtr<IWICBitmapEncoder> encoder;
    if (FAILED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.GetAddressOf())) ||
        FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache))) {
        return false;
    }

    Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;
    Microsoft::WRL::ComPtr<IPropertyBag2> props;
    if (FAILED(encoder->CreateNewFrame(frame.GetAddressOf(), props.GetAddressOf())) ||
        FAILED(frame->Initialize(props.Get())) ||
        FAILED(frame->SetSize(static_cast<UINT>(width), static_cast<UINT>(height)))) {
        return false;
    }

    
    std::vector<uint8_t> bgra(static_cast<size_t>(width) * height * 4);
    for (size_t i = 0; i < bgra.size(); i += 4) {
        bgra[i + 0] = rgba[i + 2];
        bgra[i + 1] = rgba[i + 1];
        bgra[i + 2] = rgba[i + 0];
        bgra[i + 3] = rgba[i + 3];
    }

    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    if (FAILED(frame->SetPixelFormat(&format)) ||
        FAILED(frame->WritePixels(height, static_cast<UINT>(width) * 4,
                                  static_cast<UINT>(bgra.size()), bgra.data())) ||
        FAILED(frame->Commit()) ||
        FAILED(encoder->Commit())) {
        return false;
    }
    return true;
}

} 
