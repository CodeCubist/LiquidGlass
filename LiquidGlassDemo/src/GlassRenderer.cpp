#include "GlassRenderer.h"
#include "DesktopCapture.h"
#include "GlassScene.h"
#include "GdiText.h"
#include "GlFunctions.h"
#include "PngWriter.h"
#include "Shaders.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr GLuint kBindingSamplerInfo     = 0;
constexpr GLuint kBindingCustomUniforms  = 1;
constexpr GLuint kBindingWidgetInfo      = 2;
constexpr GLuint kBindingBgConfig        = 3;
constexpr GLuint kBindingBlurConfig      = 4;

constexpr int kMaxWidgets = 64;          
constexpr int kMaxBlurLevels = 5;        
constexpr int kMaxBlurRadius = 64;       

GLuint compileShader(GLenum type, const char* src, std::string* error, const char* name) {
    GLuint shader = gl::CreateShader(type);
    gl::ShaderSource(shader, 1, &src, nullptr);
    gl::CompileShader(shader);
    GLint ok = GL_FALSE;
    gl::GetShaderiv(shader, gl::kCompileStatus, &ok);
    if (!ok) {
        GLint len = 0;
        gl::GetShaderiv(shader, gl::kInfoLogLength, &len);
        std::string log(static_cast<size_t>(std::max(len, 1)), '\0');
        gl::GetShaderInfoLog(shader, len, nullptr, log.data());
        if (error) {
            *error = std::string("shader compile failed [") + name + "]: " +
                     (len > 0 ? log.c_str() : "(empty driver log)");
        }
        gl::DeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint linkProgram(GLuint vs, GLuint fs, std::string* error, const char* name) {
    GLuint program = gl::CreateProgram();
    gl::AttachShader(program, vs);
    gl::AttachShader(program, fs);
    gl::LinkProgram(program);
    GLint ok = GL_FALSE;
    gl::GetProgramiv(program, gl::kLinkStatus, &ok);
    if (!ok) {
        GLint len = 0;
        gl::GetProgramiv(program, gl::kInfoLogLength, &len);
        std::string log(static_cast<size_t>(std::max(len, 1)), '\0');
        gl::GetProgramInfoLog(program, len, nullptr, log.data());
        if (error) {
            *error = std::string("program link failed [") + name + "]: " +
                     (len > 0 ? log.c_str() : "(empty driver log)");
        }
        gl::DeleteProgram(program);
        return 0;
    }
    return program;
}

GLuint makeTexture(int w, int h, GLint internalFormat) {
    GLuint tex = 0;
    gl::GenTextures(1, &tex);
    gl::BindTexture(GL_TEXTURE_2D, tex);
    gl::TexImage2D(GL_TEXTURE_2D, 0, internalFormat, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl::kClampToEdge);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl::kClampToEdge);
    return tex;
}

bool checkFbo(std::string* error, const char* name) {
    if (gl::CheckFramebufferStatus(gl::kFramebuffer) != gl::kFramebufferComplete) {
        if (error) *error = std::string("framebuffer incomplete [") + name + "]";
        return false;
    }
    return true;
}
    
constexpr int kWidgetInfoBytes = 16 + kMaxWidgets * 11 * 16;
constexpr int kBlurConfigBytes = 16 + 65 * 16;
    
void gaussianWeights(int radius, float* kernel /* 65 */) {
    std::memset(kernel, 0, 65 * sizeof(float));
    radius = std::clamp(radius, 0, kMaxBlurRadius);
    if (radius == 0) { kernel[0] = 1.0f; return; }
    const float sigma = static_cast<float>(radius) / 3.0f;
    float sum = 0.0f;
    for (int i = 0; i <= radius; ++i) {
        const float w = std::exp(-0.5f * static_cast<float>(i * i) / (sigma * sigma));
        kernel[i] = w;
        sum += (i == 0) ? w : 2.0f * w;
    }
    for (int i = 0; i <= radius; ++i) kernel[i] /= sum;
}

} 

GlassRenderer::~GlassRenderer() {
    auto delTex = [](GLuint& t) { if (t) { gl::DeleteTextures(1, &t); t = 0; } };
    auto delFbo = [](GLuint& f) { if (f) { gl::DeleteFramebuffers(1, &f); f = 0; } };
    auto delUbo = [](GLuint& u) { if (u) { gl::DeleteBuffers(1, &u); u = 0; } };
    delTex(screenTex_);   delTex(shadowTex_);   delTex(blurTemp_);
    delTex(compositeTex_); delTex(presentTex_);
    for (GLuint& t : blurLevels_) delTex(t);
    delFbo(shadowFbo_);   delFbo(blurTempFbo_);
    delFbo(compositeFbo_); delFbo(presentFbo_);
    for (GLuint& f : blurLevelFbos_) delFbo(f);
    delUbo(samplerInfoUbo_); delUbo(customUniformsUbo_); delUbo(widgetInfoUbo_);
    delUbo(bgConfigUbo_);    delUbo(blurConfigUbo_);
    if (pbos_[0] || pbos_[1]) gl::DeleteBuffers(2, pbos_);
    auto delProg = [](GLuint& p) { if (p) { gl::DeleteProgram(p); p = 0; } };
    delProg(copyProg_); delProg(bgProg_); delProg(blurProg_);
    delProg(compositeProg_); delProg(presentProg_); delProg(textProg_);
    for (auto& kv : labels_) {
        if (kv.second.texture) gl::DeleteTextures(1, &kv.second.texture);
    }
    if (vao_) gl::DeleteVertexArrays(1, &vao_);
}

bool GlassRenderer::init(int width, int height, std::string* error) {
    width_ = width;
    height_ = height;

    gl::GenVertexArrays(1, &vao_);
    gl::BindVertexArray(vao_);

    auto build = [&](const char* vsSrc, const char* fsSrc, const char* name) {
        const GLuint vs = compileShader(gl::kVertexShader, vsSrc, error, name);
        if (!vs) return 0u;
        const GLuint fs = compileShader(gl::kFragmentShader, fsSrc, error, name);
        if (!fs) { gl::DeleteShader(vs); return 0u; }
        const GLuint program = linkProgram(vs, fs, error, name);
        gl::DeleteShader(vs);
        gl::DeleteShader(fs);
        return program;
    };

    copyProg_      = build(shaders::kVsQuad, shaders::kFsCopy,      "copy");
    bgProg_        = build(shaders::kVsQuad, shaders::kFsBg,        "bg");
    blurProg_      = build(shaders::kVsQuad, shaders::kFsBlur,      "blur");
    compositeProg_ = build(shaders::kVsQuad, shaders::kFsComposite, "composite");
    presentProg_   = build(shaders::kVsQuad, shaders::kFsPresent,   "present");
    textProg_      = build(shaders::kVsQuad, shaders::kFsText,      "text");
    if (!copyProg_ || !bgProg_ || !blurProg_ || !compositeProg_ || !presentProg_ ||
        !textProg_) {
        return false;
    }
    
    gl::UseProgram(bgProg_);
    gl::Uniform1i(gl::GetUniformLocation(bgProg_, "Sampler0"), 0);
    gl::UseProgram(blurProg_);
    gl::Uniform1i(gl::GetUniformLocation(blurProg_, "DiffuseSampler"), 0);
    gl::UseProgram(compositeProg_);
    for (int i = 0; i < 6; ++i) {
        gl::Uniform1i(gl::GetUniformLocation(compositeProg_,
            ("Sampler" + std::to_string(i)).c_str()), i);
    }
    gl::UseProgram(presentProg_);
    gl::Uniform1i(gl::GetUniformLocation(presentProg_, "Sampler0"), 0);
    gl::UseProgram(textProg_);
    gl::Uniform1i(gl::GetUniformLocation(textProg_, "Sampler0"), 0);
    
    auto bindBlocks = [&](GLuint program) {
        auto bind = [&](const char* block, GLuint binding) {
            const GLuint idx = gl::GetUniformBlockIndex(program, block);
            if (idx != gl::kInvalidIndex) gl::UniformBlockBinding(program, idx, binding);
        };
        bind("SamplerInfo", kBindingSamplerInfo);
        bind("CustomUniforms", kBindingCustomUniforms);
        bind("WidgetInfo", kBindingWidgetInfo);
        bind("BgConfig", kBindingBgConfig);
        bind("BlurConfig", kBindingBlurConfig);
    };
    bindBlocks(bgProg_);
    bindBlocks(blurProg_);
    bindBlocks(compositeProg_);

    
    auto makeUbo = [](gl::GLsizeiptr bytes) {
        GLuint ubo = 0;
        gl::GenBuffers(1, &ubo);
        gl::BindBuffer(gl::kUniformBuffer, ubo);
        gl::BufferData(gl::kUniformBuffer, bytes, nullptr, gl::kDynamicDraw);
        return ubo;
    };
    samplerInfoUbo_    = makeUbo(16);
    customUniformsUbo_ = makeUbo(128);
    widgetInfoUbo_     = makeUbo(kWidgetInfoBytes);
    bgConfigUbo_       = makeUbo(16);
    blurConfigUbo_     = makeUbo(kBlurConfigBytes);
    
    const gl::GLsizeiptr frameBytes = static_cast<gl::GLsizeiptr>(width_) * height_ * 4;
    gl::GenBuffers(2, pbos_);
    for (GLuint pbo : pbos_) {
        gl::BindBuffer(gl::kPixelPackBuffer, pbo);
        gl::BufferData(gl::kPixelPackBuffer, frameBytes, nullptr, gl::kStreamRead);
    }
    gl::BindBuffer(gl::kPixelPackBuffer, 0);
    gl::BindBufferBase(gl::kUniformBuffer, kBindingSamplerInfo,    samplerInfoUbo_);
    gl::BindBufferBase(gl::kUniformBuffer, kBindingCustomUniforms, customUniformsUbo_);
    gl::BindBufferBase(gl::kUniformBuffer, kBindingWidgetInfo,     widgetInfoUbo_);
    gl::BindBufferBase(gl::kUniformBuffer, kBindingBgConfig,       bgConfigUbo_);
    gl::BindBufferBase(gl::kUniformBuffer, kBindingBlurConfig,     blurConfigUbo_);

    if (!buildTargets(error)) return false;
    return true;
}

bool GlassRenderer::buildTargets(std::string* error) {
    
    std::vector<uint32_t> initBg(static_cast<size_t>(width_) * height_);
    for (int y = 0; y < height_; ++y) {
        float fy = static_cast<float>(y) / static_cast<float>(height_);
        for (int x = 0; x < width_; ++x) {
            float fx = static_cast<float>(x) / static_cast<float>(width_);
            float f = (fx + (1.0f - fy)) * 0.5f;
            uint8_t r = static_cast<uint8_t>(std::clamp(30.0f + 50.0f * (1.0f - f), 0.0f, 255.0f));
            uint8_t g = static_cast<uint8_t>(std::clamp(45.0f + 85.0f * (1.0f - fy) + 40.0f * fx, 0.0f, 255.0f));
            uint8_t b = static_cast<uint8_t>(std::clamp(80.0f + 140.0f * (1.0f - fy) + 60.0f * fx, 0.0f, 255.0f));
            initBg[static_cast<size_t>(y) * width_ + x] = (255u << 24) | (b << 16) | (g << 8) | r;
        }
    }
    gl::GenTextures(1, &screenTex_);
    gl::BindTexture(GL_TEXTURE_2D, screenTex_);
    gl::TexImage2D(GL_TEXTURE_2D, 0, gl::kRgba8, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, initBg.data());
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl::kClampToEdge);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl::kClampToEdge);

    auto attach = [](GLuint fbo, GLuint tex, const char* name, std::string* error) {
        gl::BindFramebuffer(gl::kFramebuffer, fbo);
        gl::FramebufferTexture2D(gl::kFramebuffer, gl::kColorAttachment0, GL_TEXTURE_2D, tex, 0);
        return checkFbo(error, name);
    };

    auto makeFbo = [](GLuint* fbo) { gl::GenFramebuffers(1, fbo); };

    shadowTex_ = makeTexture(width_, height_, gl::kRgba8);
    blurTemp_  = makeTexture(width_, height_, gl::kRgba8);
    for (GLuint& t : blurLevels_) t = makeTexture(width_, height_, gl::kRgba8);
    compositeTex_ = makeTexture(width_, height_, gl::kRgba8);
    presentTex_   = makeTexture(width_, height_, gl::kRgba8);

    makeFbo(&shadowFbo_);
    makeFbo(&blurTempFbo_);
    for (GLuint& f : blurLevelFbos_) makeFbo(&f);
    makeFbo(&compositeFbo_);
    makeFbo(&presentFbo_);

    bool ok = attach(shadowFbo_, shadowTex_, "shadow", error);
    ok = ok && attach(blurTempFbo_, blurTemp_, "blurTemp", error);
    for (int i = 0; i < kMaxBlurLevels && ok; ++i) {
        ok = attach(blurLevelFbos_[i], blurLevels_[i], "blurLevel", error);
    }
    ok = ok && attach(compositeFbo_, compositeTex_, "composite", error);
    ok = ok && attach(presentFbo_, presentTex_, "present", error);
    gl::BindFramebuffer(gl::kFramebuffer, 0);
    return ok;
}

void GlassRenderer::setLabel(int widgetId, const std::wstring& text, int pixelHeight) {
    const int px = std::max(6, static_cast<int>(pixelHeight * uiScale_ + 0.5f));
    Label& label = labels_[widgetId];
    if (label.texture && label.text == text && label.pixelHeight == px) return;
    GdiText gdi;
    if (!gdi.render(text, px)) return;
    if (label.texture) gl::DeleteTextures(1, &label.texture);
    label.texture = gdi.texture;
    label.width = gdi.width;
    label.height = gdi.height;
    label.text = text;
    label.pixelHeight = px;
    gdi.texture = 0; 
}

void GlassRenderer::seedBackdrop(DesktopCapture& capture, int screenX, int screenY) {
    gl::ActiveTexture(gl::kTexture0);
    gl::BindTexture(GL_TEXTURE_2D, screenTex_);
    capture.captureToTexture(screenX, screenY);
}

void GlassRenderer::uploadSamplerInfo(float outW, float outH, float inW, float inH) {
    const float data[4] = { outW, outH, inW, inH };
    gl::BindBuffer(gl::kUniformBuffer, samplerInfoUbo_);
    gl::BufferData(gl::kUniformBuffer, sizeof(data), data, gl::kDynamicDraw);
}

void GlassRenderer::uploadCustomUniforms(double timeSeconds, float mouseX, float mouseY, const Params& p) {
    uint8_t buf[128] = {};
    auto putF = [&](size_t offset, float v) { std::memcpy(buf + offset, &v, 4); };
    auto putV4 = [&](size_t offset, float a, float b, float c, float d) {
        putF(offset, a); putF(offset + 4, b); putF(offset + 8, c); putF(offset + 12, d);
    };
    putF(0, static_cast<float>(timeSeconds));          
    putV4(16, mouseX, static_cast<float>(height_) - mouseY, 0.0f, 0.0f); 
    putF(32, 0.0f);                                    
    putV4(48, -0.7071f, 0.7071f, 0.0f, 0.0f);          
    putV4(64, 1.0f, 1.0f, 1.0f, 0.1f);                 
    putF(80, 2.0f);                                    
    putF(84, p.debugStep);                             
    putF(88, 0.0f);                                    
    putF(92, p.pixelGridSize);
    putF(96, p.hoverScalePx);
    putF(100, p.focusScalePx);
    putF(104, p.focusBorderWidthPx);
    putF(108, p.focusBorderIntensity);
    putF(112, p.focusBorderSpeed);
    gl::BindBuffer(gl::kUniformBuffer, customUniformsUbo_);
    gl::BufferData(gl::kUniformBuffer, sizeof(buf), buf, gl::kDynamicDraw);
}

void GlassRenderer::uploadWidgetInfo(const glass::Scene& scene, int fbW, int fbH) {
    const auto& widgets = scene.widgets();
    const int count = std::min<int>(static_cast<int>(widgets.size()), kMaxWidgets);
    
    std::vector<int> radii;
    for (const glass::Widget& w : widgets) radii.push_back(std::max(0, w.style.blurRadius));
    std::sort(radii.begin(), radii.end());
    radii.erase(std::unique(radii.begin(), radii.end()), radii.end());
    if (radii.size() > static_cast<size_t>(kMaxBlurLevels)) radii.resize(kMaxBlurLevels);
    blurIndexByRadius_.assign(radii.begin(), radii.end());

    std::vector<uint8_t> buf(kWidgetInfoBytes, 0);
    auto putV4 = [&](int arrayIdx, int slot, float a, float b, float c, float d) {
        const size_t offset = 16 + (static_cast<size_t>(arrayIdx) * kMaxWidgets + slot) * 16;
        std::memcpy(buf.data() + offset + 0, &a, 4);
        std::memcpy(buf.data() + offset + 4, &b, 4);
        std::memcpy(buf.data() + offset + 8, &c, 4);
        std::memcpy(buf.data() + offset + 12, &d, 4);
    };
    const float countF = static_cast<float>(count);
    std::memcpy(buf.data(), &countF, 4);

    for (int i = 0; i < count; ++i) {
        const glass::Widget& w = widgets[static_cast<size_t>(i)];
        
        
        putV4(0, i, w.x, static_cast<float>(fbH) - w.y - w.h, w.w, w.h);                  
        const float rad = w.cornerRadius;                                                  
        putV4(1, i, rad, rad, rad, rad);
        putV4(2, i, w.style.tintR, w.style.tintG, w.style.tintB, w.style.tintA);           
        putV4(3, i, w.style.refThickness, w.style.refFactor, w.style.refDispersion,
              w.style.refFresnelRange);                                                    
        putV4(4, i, w.style.refFresnelHardness, w.style.refFresnelFactor,
              w.style.glareRange, w.style.glareHardness);                                  
        putV4(5, i, 0.0f, 0.0f, 0.0f, 0.0f);                                               
        putV4(6, i, w.style.smoothing, 0.0f, 0.0f, 0.0f);                                  
        putV4(7, i, 0.0f, 0.0f, static_cast<float>(fbW), static_cast<float>(fbH));         
        putV4(8, i, w.style.shadowExpand, w.style.shadowFactor,
              w.style.shadowOffsetX, w.style.shadowOffsetY);                               
        putV4(9, i, w.style.shadowR, w.style.shadowG, w.style.shadowB, w.style.shadowA);   
        int blurIdx = 0;
        for (size_t k = 0; k < blurIndexByRadius_.size(); ++k) {
            if (blurIndexByRadius_[k] == std::max(0, w.style.blurRadius)) { blurIdx = static_cast<int>(k); break; }
        }
        putV4(10, i, static_cast<float>(blurIdx), w.hoverK, w.focusK, w.seed);             
    }
    gl::BindBuffer(gl::kUniformBuffer, widgetInfoUbo_);
    gl::BufferData(gl::kUniformBuffer, buf.size(), buf.data(), gl::kDynamicDraw);
}

void GlassRenderer::uploadBgConfig(const glass::Scene& scene) {
    
    
    const glass::WidgetStyle& s = scene.widgets().empty()
        ? glass::WidgetStyle{} : scene.widgets().front().style;
    const float data[4] = { s.shadowExpand, s.shadowFactor, s.shadowOffsetX, s.shadowOffsetY };
    gl::BindBuffer(gl::kUniformBuffer, bgConfigUbo_);
    gl::BufferData(gl::kUniformBuffer, sizeof(data), data, gl::kDynamicDraw);
}

void GlassRenderer::uploadBlurConfig(float dx, float dy, int radius) {
    float kernel[65];
    gaussianWeights(radius, kernel);
    std::vector<uint8_t> buf(kBlurConfigBytes, 0);
    std::memcpy(buf.data(), &dx, 4);
    std::memcpy(buf.data() + 4, &dy, 4);
    
    const float radiusF = static_cast<float>(radius);
    std::memcpy(buf.data() + 8, &radiusF, 4);
    for (int i = 0; i < 65; ++i) {
        std::memcpy(buf.data() + 16 + i * 16, &kernel[i], 4); 
    }
    gl::BindBuffer(gl::kUniformBuffer, blurConfigUbo_);
    gl::BufferData(gl::kUniformBuffer, buf.size(), buf.data(), gl::kDynamicDraw);
}

void GlassRenderer::drawFullscreen(GLuint program, float x0, float y0, float x1, float y1,
                                   float u0, float v0, float u1, float v1) {
    gl::BindVertexArray(vao_);
    gl::UseProgram(program);
    gl::Uniform4f(gl::GetUniformLocation(program, "uRectNdc"), x0, y0, x1, y1);
    gl::Uniform4f(gl::GetUniformLocation(program, "uUvRange"), u0, v0, u1, v1);
    gl::DrawArrays(GL_TRIANGLES, 0, 3);
}

void GlassRenderer::drawLabel(const Label& label, float cx, float cy, float alpha) {
    if (!label.texture) return;
    
    
    const float l = std::round(cx - 0.5f * label.width);
    const float t = std::round(cy - 0.5f * label.height);
    
    const float x0 = l / static_cast<float>(width_) * 2.0f - 1.0f;
    const float x1 = (l + label.width) / static_cast<float>(width_) * 2.0f - 1.0f;
    const float y1 = 1.0f - t / static_cast<float>(height_) * 2.0f;
    const float y0 = 1.0f - (t + label.height) / static_cast<float>(height_) * 2.0f;
    gl::Uniform4f(gl::GetUniformLocation(textProg_, "uTint"), 1.0f, 1.0f, 1.0f, alpha);
    drawFullscreen(textProg_, x0, y0, x1, y1);
}

bool GlassRenderer::render(const glass::Scene& scene, double timeSeconds, ICaptureSource& capture,
                           int screenX, int screenY) {
    const float W = static_cast<float>(width_);
    const float H = static_cast<float>(height_);
    gl::BindVertexArray(vao_);
    gl::Disable(GL_BLEND);
    gl::Viewport(0, 0, width_, height_);
    
    gl::ActiveTexture(gl::kTexture0);
    gl::BindTexture(GL_TEXTURE_2D, screenTex_);
    const bool backdropNew = capture.captureToTexture(screenX, screenY);
    
    uploadSamplerInfo(W, H, W, H);
    uploadBgConfig(scene);
    uploadWidgetInfo(scene, width_, height_);
    uploadCustomUniforms(timeSeconds, 0.0f, 0.0f, scene.params());
    gl::ActiveTexture(gl::kTexture0);
    gl::BindTexture(GL_TEXTURE_2D, screenTex_);
    gl::BindFramebuffer(gl::kFramebuffer, shadowFbo_);
    drawFullscreen(bgProg_, -1.0f, -1.0f, 1.0f, 1.0f);
    
    uploadSamplerInfo(W, H, W, H);
    for (int k = 0; k < static_cast<int>(blurIndexByRadius_.size()); ++k) {
        const int radius = blurIndexByRadius_[static_cast<size_t>(k)];
        if (radius <= 0) {
            
            gl::BindFramebuffer(gl::kFramebuffer, blurLevelFbos_[k]);
            gl::BindTexture(GL_TEXTURE_2D, shadowTex_);
            drawFullscreen(copyProg_, -1.0f, -1.0f, 1.0f, 1.0f);
            continue;
        }
        uploadBlurConfig(1.0f, 0.0f, radius);
        gl::BindFramebuffer(gl::kFramebuffer, blurTempFbo_);
        gl::BindTexture(GL_TEXTURE_2D, shadowTex_);
        drawFullscreen(blurProg_, -1.0f, -1.0f, 1.0f, 1.0f);
        uploadBlurConfig(0.0f, 1.0f, radius);
        gl::BindFramebuffer(gl::kFramebuffer, blurLevelFbos_[k]);
        gl::BindTexture(GL_TEXTURE_2D, blurTemp_);
        drawFullscreen(blurProg_, -1.0f, -1.0f, 1.0f, 1.0f);
    }

    
    uploadSamplerInfo(W, H, W, H);
    uploadCustomUniforms(timeSeconds, 0.0f, 0.0f, scene.params());
    gl::BindFramebuffer(gl::kFramebuffer, compositeFbo_);
    gl::ActiveTexture(gl::kTexture0);
    gl::BindTexture(GL_TEXTURE_2D, shadowTex_);
    for (int k = 0; k < kMaxBlurLevels; ++k) {
        gl::ActiveTexture(gl::kTexture0 + 1 + k);
        gl::BindTexture(GL_TEXTURE_2D, blurLevels_[k]);
    }
    drawFullscreen(compositeProg_, -1.0f, -1.0f, 1.0f, 1.0f);
    
    gl::BindFramebuffer(gl::kFramebuffer, presentFbo_);
    gl::ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    gl::Clear(GL_COLOR_BUFFER_BIT);
    gl::ActiveTexture(gl::kTexture0);
    gl::BindTexture(GL_TEXTURE_2D, compositeTex_);
    drawFullscreen(presentProg_, -1.0f, -1.0f, 1.0f, 1.0f);
    
    gl::Enable(GL_BLEND);
    gl::BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    for (const glass::Widget& w : scene.widgets()) {
        const auto it = labels_.find(w.id);
        if (it == labels_.end()) continue;
        gl::ActiveTexture(gl::kTexture0);
        gl::BindTexture(GL_TEXTURE_2D, it->second.texture);
        drawLabel(it->second, w.x + 0.5f * w.w, w.y + 0.5f * w.h, w.labelAlpha);
    }
    gl::Disable(GL_BLEND);
    gl::BindFramebuffer(gl::kFramebuffer, 0);
    return backdropNew;
}

void GlassRenderer::dumpTextures(const std::wstring& base) {
    auto save = [this](const std::wstring& path, GLuint fbo, int w, int h) {
        std::vector<uint8_t> rgba(static_cast<size_t>(w) * h * 4);
        gl::BindFramebuffer(gl::kFramebuffer, fbo);
        gl::ReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        const size_t row = static_cast<size_t>(w) * 4;
        std::vector<uint8_t> flipped(rgba.size());
        for (int y = 0; y < h; ++y) {
            std::memcpy(&flipped[static_cast<size_t>(y) * row],
                        &rgba[static_cast<size_t>(h - 1 - y) * row], row);
        }
        png::writeRgba(path, w, h, flipped.data());
    };
    auto saveTexture = [&](GLuint texture, int w, int h, const std::wstring& path) {
        GLuint fbo = 0;
        gl::GenFramebuffers(1, &fbo);
        gl::BindFramebuffer(gl::kFramebuffer, fbo);
        gl::FramebufferTexture2D(gl::kFramebuffer, gl::kColorAttachment0, GL_TEXTURE_2D, texture, 0);
        save(path, fbo, w, h);
        gl::DeleteFramebuffers(1, &fbo);
    };
    saveTexture(screenTex_, width_, height_, base + L"_capture.png");
    save(base + L"_shadow.png", shadowFbo_, width_, height_);
    save(base + L"_blur0.png", blurLevelFbos_[0], width_, height_);
    save(base + L"_blur1.png", blurLevelFbos_[1], width_, height_);
    save(base + L"_blur2.png", blurLevelFbos_[2], width_, height_);
    save(base + L"_blur3.png", blurLevelFbos_[3], width_, height_);
    save(base + L"_composite.png", compositeFbo_, width_, height_);
    save(base + L"_present.png", presentFbo_, width_, height_);
    
    for (const auto& entry : labels_) {
        const Label& label = entry.second;
        if (!label.texture || label.width <= 0 || label.height <= 0) continue;
        saveTexture(label.texture, label.width, label.height,
                    base + L"_label_" + std::to_wstring(entry.first) + L".png");
    }
    gl::BindFramebuffer(gl::kFramebuffer, 0);
    
    {
        FILE* f = nullptr;
        _wfopen_s(&f, (base + L"_ubo.txt").c_str(), L"w");
        if (f) {
            const size_t offset = 16 + 10 * kMaxWidgets * 16;
            std::vector<uint8_t> buf(64 * 16);
            gl::BindBuffer(gl::kUniformBuffer, widgetInfoUbo_);
            gl::GetBufferSubData(gl::kUniformBuffer, static_cast<gl::GLintptr>(offset),
                                 static_cast<gl::GLsizeiptr>(buf.size()), buf.data());
            fprintf(f, "Extra0[i] = (blurIdx, hoverK, focusK, seed)\n");
            for (int i = 0; i < 9; ++i) {
                float v[4];
                std::memcpy(v, buf.data() + i * 16, 16);
                fprintf(f, "[%d] %.3f %.3f %.3f %.3f\n", i, v[0], v[1], v[2], v[3]);
            }
            fclose(f);
        }
    }
    gl::BindFramebuffer(gl::kFramebuffer, 0);
}

void GlassRenderer::readbackBgra(std::vector<uint8_t>& out) {
    const size_t bytes = static_cast<size_t>(width_) * height_ * 4;
    out.resize(bytes);
    gl::BindFramebuffer(gl::kFramebuffer, presentFbo_);
    
    const int write = pboIndex_;
    const int read = 1 - write;
    gl::BindBuffer(gl::kPixelPackBuffer, pbos_[write]);
    gl::ReadPixels(0, 0, width_, height_, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    auto convertRow = [](const uint8_t* src, uint8_t* dst, int w) {
        for (int x = 0; x < w; ++x) {
            const uint8_t r = src[x * 4 + 0];
            const uint8_t g = src[x * 4 + 1];
            const uint8_t b = src[x * 4 + 2];
            const uint8_t a = src[x * 4 + 3];
            dst[x * 4 + 0] = b;
            dst[x * 4 + 1] = g;
            dst[x * 4 + 2] = r;
            dst[x * 4 + 3] = a;
        }
    };
    bool consumed = false;
    if (pboReady_[read]) {
        gl::BindBuffer(gl::kPixelPackBuffer, pbos_[read]);
        void* mapped = gl::MapBuffer(gl::kPixelPackBuffer, gl::kReadOnly);
        if (mapped) {
            const auto* src = static_cast<const uint8_t*>(mapped);
            for (int y = 0; y < height_; ++y) {
                convertRow(src + static_cast<size_t>(y) * width_ * 4,
                           out.data() + static_cast<size_t>(y) * width_ * 4, width_);
            }
            gl::UnmapBuffer(gl::kPixelPackBuffer);
            consumed = true;
        }
    }
    gl::BindBuffer(gl::kPixelPackBuffer, 0);
    if (!consumed) {
        gl::ReadPixels(0, 0, width_, height_, GL_RGBA, GL_UNSIGNED_BYTE, out.data());
        for (int y = 0; y < height_; ++y) {
            convertRow(out.data() + static_cast<size_t>(y) * width_ * 4,
                       out.data() + static_cast<size_t>(y) * width_ * 4, width_);
        }
    }
    pboReady_[write] = true;
    pboIndex_ = read;
}
