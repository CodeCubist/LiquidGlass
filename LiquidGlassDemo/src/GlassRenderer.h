#pragma once
#include <windows.h>
#include <GL/gl.h>
#include "GlassTypes.h"
#include "CaptureSource.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace glass {
class Scene;
}

class DesktopCapture;

class GlassRenderer {
public:
    GlassRenderer() = default;
    ~GlassRenderer();
    GlassRenderer(const GlassRenderer&) = delete;
    GlassRenderer& operator=(const GlassRenderer&) = delete;
    
    [[nodiscard]] bool init(int width, int height, std::string* error);
    void setUiScale(float scale) { uiScale_ = scale; }
    void setLabel(int widgetId, const std::wstring& text, int pixelHeight);
    
    void seedBackdrop(DesktopCapture& capture, int screenX, int screenY);
    
    [[nodiscard]] bool render(const glass::Scene& scene, double timeSeconds, ICaptureSource& capture,
                              int screenX, int screenY);
    void readbackBgra(std::vector<uint8_t>& out);
    
    void dumpTextures(const std::wstring& base);

    int width() const { return width_; }
    int height() const { return height_; }

private:
    struct Label {
        GLuint texture = 0;
        int width = 0, height = 0;
        std::wstring text;
        int pixelHeight = 0;
    };

    using Params = glass::GlobalParams;

    bool buildTargets(std::string* error);

    void uploadSamplerInfo(float outW, float outH, float inW, float inH);
    void uploadCustomUniforms(double timeSeconds, float mouseX, float mouseY, const Params& p);
    void uploadWidgetInfo(const glass::Scene& scene, int fbW, int fbH);
    void uploadBgConfig(const glass::Scene& scene);
    void uploadBlurConfig(float dx, float dy, int radius);

    void drawFullscreen(GLuint program, float x0, float y0, float x1, float y1,
                        float u0 = 0.0f, float v0 = 0.0f, float u1 = 1.0f, float v1 = 1.0f);
    void drawLabel(const Label& label, float cx, float cy, float alpha);

    int width_ = 0, height_ = 0;

    GLuint vao_ = 0;
    
    GLuint copyProg_ = 0;
    GLuint bgProg_ = 0;
    GLuint blurProg_ = 0;
    GLuint compositeProg_ = 0;
    GLuint presentProg_ = 0;
    GLuint textProg_ = 0;
    
    GLuint screenTex_ = 0;      
    GLuint shadowTex_ = 0;      
    GLuint blurTemp_ = 0;       
    GLuint blurLevels_[5] = {}; 
    GLuint compositeTex_ = 0;   
    GLuint presentTex_ = 0;     
    
    GLuint shadowFbo_ = 0;
    GLuint blurTempFbo_ = 0;
    GLuint blurLevelFbos_[5] = {};
    GLuint compositeFbo_ = 0;
    GLuint presentFbo_ = 0;
    
    GLuint samplerInfoUbo_ = 0;
    GLuint customUniformsUbo_ = 0;
    GLuint widgetInfoUbo_ = 0;
    GLuint bgConfigUbo_ = 0;
    GLuint blurConfigUbo_ = 0;

    std::unordered_map<int, Label> labels_;
    std::vector<int> blurIndexByRadius_;  
    float uiScale_ = 1.0f;                
    
    GLuint pbos_[2] = {};
    int pboIndex_ = 0;
    bool pboReady_[2] = { false, false };
};
