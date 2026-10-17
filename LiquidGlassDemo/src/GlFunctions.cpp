#include "GlFunctions.h"
#include "GlContext.h"
#include <string>

namespace gl {

Functions fn;

namespace {
template <typename T>
T proc(const char* name) {
    T p = reinterpret_cast<T>(wglGetProcAddress(name));
    if (!p) p = reinterpret_cast<T>(GetProcAddress(GetModuleHandleW(L"opengl32.dll"), name));
    if (!p) {
        std::string arb = std::string(name) + "ARB";
        p = reinterpret_cast<T>(wglGetProcAddress(arb.c_str()));
    }
    if (!p) {
        std::string ext = std::string(name) + "EXT";
        p = reinterpret_cast<T>(wglGetProcAddress(ext.c_str()));
    }
    return p;
}

template <typename T>
void require(T& slot, const char* name, bool& ok, std::string* missing) {
    slot = proc<T>(name);
    if (!slot) {
        ok = false;
        if (missing) *missing += std::string(name) + " ";
    }
}
} 

bool loadFunctions(std::string* missing) {
    bool ok = true;
#define GL_REQ(name) require(fn.name, "gl" #name, ok, missing)
    GL_REQ(CreateShader);           GL_REQ(ShaderSource);
    GL_REQ(CompileShader);          GL_REQ(GetShaderiv);
    GL_REQ(GetShaderInfoLog);       GL_REQ(CreateProgram);
    GL_REQ(AttachShader);           GL_REQ(LinkProgram);
    GL_REQ(GetProgramiv);           GL_REQ(GetProgramInfoLog);
    GL_REQ(DeleteShader);           GL_REQ(DeleteProgram);
    GL_REQ(UseProgram);             GL_REQ(GetUniformLocation);
    GL_REQ(Uniform1i);              GL_REQ(Uniform1f);
    GL_REQ(Uniform4f);              GL_REQ(UniformBlockBinding);
    GL_REQ(GetUniformBlockIndex);   GL_REQ(BindBufferBase);
    GL_REQ(GenVertexArrays);        GL_REQ(BindVertexArray);
    GL_REQ(DeleteVertexArrays);     GL_REQ(GenBuffers);
    GL_REQ(DeleteBuffers);          GL_REQ(BindBuffer);
    GL_REQ(BufferData);             GL_REQ(GetBufferSubData);
    GL_REQ(MapBuffer);              GL_REQ(UnmapBuffer);             GL_REQ(GenTextures);
    GL_REQ(DeleteTextures);         GL_REQ(BindTexture);
    GL_REQ(TexImage2D);             GL_REQ(TexSubImage2D);
    GL_REQ(TexParameteri);          GL_REQ(ActiveTexture);
    GL_REQ(PixelStorei);            GL_REQ(GenFramebuffers);
    GL_REQ(DeleteFramebuffers);     GL_REQ(BindFramebuffer);
    GL_REQ(FramebufferTexture2D);   GL_REQ(CheckFramebufferStatus);
    GL_REQ(Viewport);               GL_REQ(DrawArrays);
    GL_REQ(Clear);                  GL_REQ(ClearColor);
    GL_REQ(Enable);                 GL_REQ(Disable);
    GL_REQ(BlendFunc);              GL_REQ(ReadPixels);
    GL_REQ(GetIntegerv);            GL_REQ(GetString);
    GL_REQ(GetError);
#undef GL_REQ
    return ok;
}

} 
