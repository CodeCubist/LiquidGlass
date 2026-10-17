#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#endif
#include <windows.h>
#include <GL/gl.h>
#include <string>

namespace gl {

using GLchar_    = char;
using GLsizeiptr = ptrdiff_t;
using GLintptr   = ptrdiff_t;
using GLubyte_   = unsigned char;
    
constexpr GLenum kFragmentShader      = 0x8B30;
constexpr GLenum kVertexShader        = 0x8B31;
constexpr GLenum kCompileStatus       = 0x8B81;
constexpr GLenum kLinkStatus          = 0x8B82;
constexpr GLenum kInfoLogLength       = 0x8B84;
constexpr unsigned kInvalidIndex      = 0xFFFFFFFFu;
constexpr GLenum kUniformBuffer       = 0x8A11;
constexpr GLenum kFramebuffer         = 0x8D40;
constexpr GLenum kDynamicDraw         = 0x88E8;
constexpr GLenum kColorAttachment0    = 0x8CE0;
constexpr GLenum kFramebufferComplete = 0x8CD5;
constexpr GLenum kRgba8               = 0x8058;
constexpr GLenum kBgra                = 0x80E1;   
constexpr GLenum kClampToEdge         = 0x812F;
constexpr GLenum kTexture0            = 0x84C0;   
constexpr GLenum kStreamRead          = 0x88E9;
constexpr GLenum kPixelPackBuffer     = 0x88EB;
constexpr GLenum kReadOnly            = 0x88B8;
    
extern "C" {
typedef GLuint        (APIENTRY* PFNGLCREATESHADERPROC)(GLenum type);
typedef void          (APIENTRY* PFNGLSHADERSOURCEPROC)(GLuint shader, GLsizei count, const GLchar_ *const* string, const GLint* length);
typedef void          (APIENTRY* PFNGLCOMPILESHADERPROC)(GLuint shader);
typedef void          (APIENTRY* PFNGLGETSHADERIVPROC)(GLuint shader, GLenum pname, GLint* params);
typedef void          (APIENTRY* PFNGLGETSHADERINFOLOGPROC)(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar_* infoLog);
typedef GLuint        (APIENTRY* PFNGLCREATEPROGRAMPROC)(void);
typedef void          (APIENTRY* PFNGLATTACHSHADERPROC)(GLuint program, GLuint shader);
typedef void          (APIENTRY* PFNGLLINKPROGRAMPROC)(GLuint program);
typedef void          (APIENTRY* PFNGLGETPROGRAMIVPROC)(GLuint program, GLenum pname, GLint* params);
typedef void          (APIENTRY* PFNGLGETPROGRAMINFOLOGPROC)(GLuint program, GLsizei bufSize, GLsizei* length, GLchar_* infoLog);
typedef void          (APIENTRY* PFNGLDELETESHADERPROC)(GLuint shader);
typedef void          (APIENTRY* PFNGLDELETEPROGRAMPROC)(GLuint program);
typedef void          (APIENTRY* PFNGLUSEPROGRAMPROC)(GLuint program);
typedef GLint         (APIENTRY* PFNGLGETUNIFORMLOCATIONPROC)(GLuint program, const GLchar_* name);
typedef void          (APIENTRY* PFNGLUNIFORM1IPROC)(GLint location, GLint v0);
typedef void          (APIENTRY* PFNGLUNIFORM1FPROC)(GLint location, GLfloat v0);
typedef void          (APIENTRY* PFNGLUNIFORM4FPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void          (APIENTRY* PFNGLUNIFORMBLOCKBINDINGPROC)(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding);
typedef GLuint        (APIENTRY* PFNGLGETUNIFORMBLOCKINDEXPROC)(GLuint program, const GLchar_* uniformBlockName);
typedef void          (APIENTRY* PFNGLBINDBUFFERBASEPROC)(GLenum target, GLuint index, GLuint buffer);
typedef void          (APIENTRY* PFNGLGENVERTEXARRAYSPROC)(GLsizei n, GLuint* arrays);
typedef void          (APIENTRY* PFNGLBINDVERTEXARRAYPROC)(GLuint array);
typedef void          (APIENTRY* PFNGLDELETEVERTEXARRAYSPROC)(GLsizei n, const GLuint* arrays);
typedef void          (APIENTRY* PFNGLGENBUFFERSPROC)(GLsizei n, GLuint* buffers);
typedef void          (APIENTRY* PFNGLDELETEBUFFERSPROC)(GLsizei n, const GLuint* buffers);
typedef void          (APIENTRY* PFNGLBINDBUFFERPROC)(GLenum target, GLuint buffer);
typedef void          (APIENTRY* PFNGLBUFFERDATAPROC)(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
typedef void          (APIENTRY* PFNGLGETBUFFERSUBDATAPROC)(GLenum target, GLintptr offset, GLsizeiptr size, void* data);
typedef void*         (APIENTRY* PFNGLMAPBUFFERPROC)(GLenum target, GLenum access);
typedef GLboolean     (APIENTRY* PFNGLUNMAPBUFFERPROC)(GLenum target);
typedef void          (APIENTRY* PFNGLGENTEXTURESPROC)(GLsizei n, GLuint* textures);
typedef void          (APIENTRY* PFNGLDELETETEXTURESPROC)(GLsizei n, const GLuint* textures);
typedef void          (APIENTRY* PFNGLBINDTEXTUREPROC)(GLenum target, GLuint texture);
typedef void          (APIENTRY* PFNGLTEXIMAGE2DPROC)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels);
typedef void          (APIENTRY* PFNGLTEXSUBIMAGE2DPROC)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels);
typedef void          (APIENTRY* PFNGLTEXPARAMETERIPROC)(GLenum target, GLenum pname, GLint param);
typedef void          (APIENTRY* PFNGLACTIVETEXTUREPROC)(GLenum texture);
typedef void          (APIENTRY* PFNGLPIXELSTOREIPROC)(GLenum pname, GLint param);
typedef void          (APIENTRY* PFNGLGENFRAMEBUFFERSPROC)(GLsizei n, GLuint* framebuffers);
typedef void          (APIENTRY* PFNGLDELETEFRAMEBUFFERSPROC)(GLsizei n, const GLuint* framebuffers);
typedef void          (APIENTRY* PFNGLBINDFRAMEBUFFERPROC)(GLenum target, GLuint framebuffer);
typedef void          (APIENTRY* PFNGLFRAMEBUFFERTEXTURE2DPROC)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef GLenum        (APIENTRY* PFNGLCHECKFRAMEBUFFERSTATUSPROC)(GLenum target);
typedef void          (APIENTRY* PFNGLVIEWPORTPROC)(GLint x, GLint y, GLsizei width, GLsizei height);
typedef void          (APIENTRY* PFNGLDRAWARRAYSPROC)(GLenum mode, GLint first, GLsizei count);
typedef void          (APIENTRY* PFNGLCLEARPROC)(GLbitfield mask);
typedef void          (APIENTRY* PFNGLCLEARCOLORPROC)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void          (APIENTRY* PFNGLENABLEPROC)(GLenum cap);
typedef void          (APIENTRY* PFNGLDISABLEPROC)(GLenum cap);
typedef void          (APIENTRY* PFNGLBLENDFUNCPROC)(GLenum sfactor, GLenum dfactor);
typedef void          (APIENTRY* PFNGLREADPIXELSPROC)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels);
typedef void          (APIENTRY* PFNGLGETINTEGERVPROC)(GLenum pname, GLint* data);
typedef const GLubyte_*(APIENTRY* PFNGLGETSTRINGPROC)(GLenum name);
typedef GLenum        (APIENTRY* PFNGLGETERRORPROC)(void);
}
    
struct Functions {
    
    PFNGLCREATESHADERPROC           CreateShader{};
    PFNGLSHADERSOURCEPROC           ShaderSource{};
    PFNGLCOMPILESHADERPROC          CompileShader{};
    PFNGLGETSHADERIVPROC            GetShaderiv{};
    PFNGLGETSHADERINFOLOGPROC       GetShaderInfoLog{};
    PFNGLCREATEPROGRAMPROC          CreateProgram{};
    PFNGLATTACHSHADERPROC           AttachShader{};
    PFNGLLINKPROGRAMPROC            LinkProgram{};
    PFNGLGETPROGRAMIVPROC           GetProgramiv{};
    PFNGLGETPROGRAMINFOLOGPROC      GetProgramInfoLog{};
    PFNGLDELETESHADERPROC           DeleteShader{};
    PFNGLDELETEPROGRAMPROC          DeleteProgram{};
    PFNGLUSEPROGRAMPROC             UseProgram{};
    PFNGLGETUNIFORMLOCATIONPROC     GetUniformLocation{};
    PFNGLUNIFORM1IPROC              Uniform1i{};
    PFNGLUNIFORM1FPROC              Uniform1f{};
    PFNGLUNIFORM4FPROC              Uniform4f{};
    PFNGLUNIFORMBLOCKBINDINGPROC    UniformBlockBinding{};
    PFNGLGETUNIFORMBLOCKINDEXPROC   GetUniformBlockIndex{};
    PFNGLBINDBUFFERBASEPROC         BindBufferBase{};
    
    PFNGLGENVERTEXARRAYSPROC        GenVertexArrays{};
    PFNGLBINDVERTEXARRAYPROC        BindVertexArray{};
    PFNGLDELETEVERTEXARRAYSPROC     DeleteVertexArrays{};
    PFNGLGENBUFFERSPROC             GenBuffers{};
    PFNGLDELETEBUFFERSPROC          DeleteBuffers{};
    PFNGLBINDBUFFERPROC             BindBuffer{};
    PFNGLBUFFERDATAPROC             BufferData{};
    PFNGLGETBUFFERSUBDATAPROC       GetBufferSubData{};
    PFNGLMAPBUFFERPROC              MapBuffer{};
    PFNGLUNMAPBUFFERPROC            UnmapBuffer{};
    
    PFNGLGENTEXTURESPROC            GenTextures{};
    PFNGLDELETETEXTURESPROC         DeleteTextures{};
    PFNGLBINDTEXTUREPROC            BindTexture{};
    PFNGLTEXIMAGE2DPROC             TexImage2D{};
    PFNGLTEXSUBIMAGE2DPROC          TexSubImage2D{};
    PFNGLTEXPARAMETERIPROC          TexParameteri{};
    PFNGLACTIVETEXTUREPROC          ActiveTexture{};
    PFNGLPIXELSTOREIPROC            PixelStorei{};
    
    PFNGLGENFRAMEBUFFERSPROC        GenFramebuffers{};
    PFNGLDELETEFRAMEBUFFERSPROC     DeleteFramebuffers{};
    PFNGLBINDFRAMEBUFFERPROC        BindFramebuffer{};
    PFNGLFRAMEBUFFERTEXTURE2DPROC   FramebufferTexture2D{};
    PFNGLCHECKFRAMEBUFFERSTATUSPROC CheckFramebufferStatus{};
    
    PFNGLVIEWPORTPROC               Viewport{};
    PFNGLDRAWARRAYSPROC             DrawArrays{};
    PFNGLCLEARPROC                  Clear{};
    PFNGLCLEARCOLORPROC             ClearColor{};
    PFNGLENABLEPROC                 Enable{};
    PFNGLDISABLEPROC                Disable{};
    PFNGLBLENDFUNCPROC              BlendFunc{};
    PFNGLREADPIXELSPROC             ReadPixels{};
    PFNGLGETINTEGERVPROC            GetIntegerv{};
    PFNGLGETSTRINGPROC              GetString{};
    PFNGLGETERRORPROC               GetError{};
};
    
extern Functions fn;

inline auto& CreateShader            = fn.CreateShader;
inline auto& ShaderSource            = fn.ShaderSource;
inline auto& CompileShader           = fn.CompileShader;
inline auto& GetShaderiv             = fn.GetShaderiv;
inline auto& GetShaderInfoLog        = fn.GetShaderInfoLog;
inline auto& CreateProgram           = fn.CreateProgram;
inline auto& AttachShader            = fn.AttachShader;
inline auto& LinkProgram             = fn.LinkProgram;
inline auto& GetProgramiv            = fn.GetProgramiv;
inline auto& GetProgramInfoLog       = fn.GetProgramInfoLog;
inline auto& DeleteShader            = fn.DeleteShader;
inline auto& DeleteProgram           = fn.DeleteProgram;
inline auto& UseProgram              = fn.UseProgram;
inline auto& GetUniformLocation      = fn.GetUniformLocation;
inline auto& Uniform1i               = fn.Uniform1i;
inline auto& Uniform1f               = fn.Uniform1f;
inline auto& Uniform4f               = fn.Uniform4f;
inline auto& UniformBlockBinding     = fn.UniformBlockBinding;
inline auto& GetUniformBlockIndex    = fn.GetUniformBlockIndex;
inline auto& BindBufferBase          = fn.BindBufferBase;
inline auto& GenVertexArrays         = fn.GenVertexArrays;
inline auto& BindVertexArray         = fn.BindVertexArray;
inline auto& DeleteVertexArrays      = fn.DeleteVertexArrays;
inline auto& GenBuffers              = fn.GenBuffers;
inline auto& DeleteBuffers           = fn.DeleteBuffers;
inline auto& BindBuffer              = fn.BindBuffer;
inline auto& BufferData              = fn.BufferData;
inline auto& GetBufferSubData        = fn.GetBufferSubData;
inline auto& MapBuffer               = fn.MapBuffer;
inline auto& UnmapBuffer             = fn.UnmapBuffer;
inline auto& GenTextures             = fn.GenTextures;
inline auto& DeleteTextures          = fn.DeleteTextures;
inline auto& BindTexture             = fn.BindTexture;
inline auto& TexImage2D              = fn.TexImage2D;
inline auto& TexSubImage2D           = fn.TexSubImage2D;
inline auto& TexParameteri           = fn.TexParameteri;
inline auto& ActiveTexture           = fn.ActiveTexture;
inline auto& PixelStorei             = fn.PixelStorei;
inline auto& GenFramebuffers         = fn.GenFramebuffers;
inline auto& DeleteFramebuffers      = fn.DeleteFramebuffers;
inline auto& BindFramebuffer         = fn.BindFramebuffer;
inline auto& FramebufferTexture2D    = fn.FramebufferTexture2D;
inline auto& CheckFramebufferStatus  = fn.CheckFramebufferStatus;
inline auto& Viewport                = fn.Viewport;
inline auto& DrawArrays              = fn.DrawArrays;
inline auto& Clear                   = fn.Clear;
inline auto& ClearColor              = fn.ClearColor;
inline auto& Enable                  = fn.Enable;
inline auto& Disable                 = fn.Disable;
inline auto& BlendFunc               = fn.BlendFunc;
inline auto& ReadPixels              = fn.ReadPixels;
inline auto& GetIntegerv             = fn.GetIntegerv;
inline auto& GetString               = fn.GetString;
inline auto& GetError                = fn.GetError;

bool loadFunctions(std::string* missing = nullptr);

} 
