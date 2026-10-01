// ZEHRA KINIK - GL soyutlamasi
// Android: dogrudan OpenGL ES 3.0. Masaustu: OpenGL 3.3 core, fonksiyonlar calisma aninda
// yuklenir (SDL_GL_GetProcAddress). Iki tarafta da ayni GLES3 alt kumesi ve ayni shader'lar kullanilir.
#pragma once

#if defined(__ANDROID__)
#include <GLES3/gl3.h>
#define ZK_GLSL_VERSION "#version 300 es\n"
inline bool zkLoadGL(void* (*)(const char*)) { return true; }
#else
#define GL_GLES_PROTOTYPES 0
#include <GLES3/gl3.h>
#define ZK_GLSL_VERSION "#version 330 core\n"

#define ZK_GL_FUNCS(X)                                                                                         \
    X(PFNGLCREATESHADERPROC, glCreateShader) X(PFNGLSHADERSOURCEPROC, glShaderSource)                          \
    X(PFNGLCOMPILESHADERPROC, glCompileShader) X(PFNGLGETSHADERIVPROC, glGetShaderiv)                          \
    X(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog) X(PFNGLCREATEPROGRAMPROC, glCreateProgram)                \
    X(PFNGLATTACHSHADERPROC, glAttachShader) X(PFNGLLINKPROGRAMPROC, glLinkProgram)                            \
    X(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation) X(PFNGLUSEPROGRAMPROC, glUseProgram)                  \
    X(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv) X(PFNGLGENTEXTURESPROC, glGenTextures)                    \
    X(PFNGLBINDTEXTUREPROC, glBindTexture) X(PFNGLTEXIMAGE2DPROC, glTexImage2D)                                \
    X(PFNGLTEXPARAMETERIPROC, glTexParameteri) X(PFNGLGENRENDERBUFFERSPROC, glGenRenderbuffers)                \
    X(PFNGLBINDRENDERBUFFERPROC, glBindRenderbuffer) X(PFNGLRENDERBUFFERSTORAGEPROC, glRenderbufferStorage)    \
    X(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers) X(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer)              \
    X(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D)                                                   \
    X(PFNGLFRAMEBUFFERRENDERBUFFERPROC, glFramebufferRenderbuffer)                                             \
    X(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays) X(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray)              \
    X(PFNGLGENBUFFERSPROC, glGenBuffers) X(PFNGLBINDBUFFERPROC, glBindBuffer)                                  \
    X(PFNGLBUFFERDATAPROC, glBufferData) X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray)        \
    X(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer) X(PFNGLVIEWPORTPROC, glViewport)                    \
    X(PFNGLCLEARCOLORPROC, glClearColor) X(PFNGLCLEARPROC, glClear) X(PFNGLENABLEPROC, glEnable)               \
    X(PFNGLDISABLEPROC, glDisable) X(PFNGLDRAWARRAYSPROC, glDrawArrays) X(PFNGLACTIVETEXTUREPROC, glActiveTexture) \
    X(PFNGLREADPIXELSPROC, glReadPixels) X(PFNGLPIXELSTOREIPROC, glPixelStorei)                             \
    X(PFNGLUNIFORM2FPROC, glUniform2f) X(PFNGLBLENDFUNCPROC, glBlendFunc)                                      \
    X(PFNGLUNIFORM1FPROC, glUniform1f) X(PFNGLUNIFORM3FPROC, glUniform3f) X(PFNGLDEPTHMASKPROC, glDepthMask)  \
    X(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers) X(PFNGLDELETETEXTURESPROC, glDeleteTextures)          \
    X(PFNGLDELETERENDERBUFFERSPROC, glDeleteRenderbuffers)

// Isim cakismasini onlemek icin (opengl32 / libGL disa aktarimlari) zk_ onekli isaretciler
#define ZK_GL_DECLARE(type, name) extern type zk_##name;
ZK_GL_FUNCS(ZK_GL_DECLARE)
#undef ZK_GL_DECLARE
#define glCreateShader zk_glCreateShader
#define glShaderSource zk_glShaderSource
#define glCompileShader zk_glCompileShader
#define glGetShaderiv zk_glGetShaderiv
#define glGetShaderInfoLog zk_glGetShaderInfoLog
#define glCreateProgram zk_glCreateProgram
#define glAttachShader zk_glAttachShader
#define glLinkProgram zk_glLinkProgram
#define glGetUniformLocation zk_glGetUniformLocation
#define glUseProgram zk_glUseProgram
#define glUniformMatrix4fv zk_glUniformMatrix4fv
#define glGenTextures zk_glGenTextures
#define glBindTexture zk_glBindTexture
#define glTexImage2D zk_glTexImage2D
#define glTexParameteri zk_glTexParameteri
#define glGenRenderbuffers zk_glGenRenderbuffers
#define glBindRenderbuffer zk_glBindRenderbuffer
#define glRenderbufferStorage zk_glRenderbufferStorage
#define glGenFramebuffers zk_glGenFramebuffers
#define glBindFramebuffer zk_glBindFramebuffer
#define glFramebufferTexture2D zk_glFramebufferTexture2D
#define glFramebufferRenderbuffer zk_glFramebufferRenderbuffer
#define glGenVertexArrays zk_glGenVertexArrays
#define glBindVertexArray zk_glBindVertexArray
#define glGenBuffers zk_glGenBuffers
#define glBindBuffer zk_glBindBuffer
#define glBufferData zk_glBufferData
#define glEnableVertexAttribArray zk_glEnableVertexAttribArray
#define glVertexAttribPointer zk_glVertexAttribPointer
#define glViewport zk_glViewport
#define glClearColor zk_glClearColor
#define glClear zk_glClear
#define glEnable zk_glEnable
#define glDisable zk_glDisable
#define glDrawArrays zk_glDrawArrays
#define glActiveTexture zk_glActiveTexture
#define glReadPixels zk_glReadPixels
#define glPixelStorei zk_glPixelStorei
#define glUniform2f zk_glUniform2f
#define glBlendFunc zk_glBlendFunc
#define glUniform1f zk_glUniform1f
#define glUniform3f zk_glUniform3f
#define glDepthMask zk_glDepthMask
#define glDeleteFramebuffers zk_glDeleteFramebuffers
#define glDeleteTextures zk_glDeleteTextures
#define glDeleteRenderbuffers zk_glDeleteRenderbuffers

bool zkLoadGL(void* (*getProc)(const char*));
#endif
