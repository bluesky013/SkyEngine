//
// Created on 2026/04/02.
//

#pragma once

#include <core/platform/Platform.h>

// GLESLoader: lightweight runtime loader for EGL and GLES 3.2 entry points.
//
// On Win32  all functions come from libEGL.dll / libGLESv2.dll via GetProcAddress
//           or eglGetProcAddress.
// On other  platforms the system linker resolves them directly so the loader
//           is a no-op.

#if SKY_PLATFORM_WINDOWS

#include <GLESForward.h>

namespace sky::aurora {

    // Call once after loading the DynamicModule for libEGL.
    // Resolves all EGL entry points.
    bool LoadEGLFunctions(void *(*getProcAddr)(const char *));

    // Call once after an EGL context has been made current.
    // Resolves all GLES 3.2 entry points via eglGetProcAddress.
    bool LoadGLESFunctions();

} // namespace sky::aurora

// ---------------------------------------------------------------------------
// EGL function pointer typedefs  (egl.h does not provide PFN types)
// ---------------------------------------------------------------------------
typedef EGLDisplay  (EGLAPIENTRY *PFNEGLGETDISPLAYPROC)(EGLNativeDisplayType);
typedef EGLBoolean  (EGLAPIENTRY *PFNEGLINITIALIZEPROC)(EGLDisplay, EGLint *, EGLint *);
typedef EGLBoolean  (EGLAPIENTRY *PFNEGLTERMINATEPROC)(EGLDisplay);
typedef EGLint      (EGLAPIENTRY *PFNEGLGETERRORPROC)(void);
typedef EGLBoolean  (EGLAPIENTRY *PFNEGLBINDAPIPROC)(EGLenum);
typedef EGLBoolean  (EGLAPIENTRY *PFNEGLCHOOSECONFIGPROC)(EGLDisplay, const EGLint *, EGLConfig *, EGLint, EGLint *);
typedef EGLContext  (EGLAPIENTRY *PFNEGLCREATECONTEXTPROC)(EGLDisplay, EGLConfig, EGLContext, const EGLint *);
typedef EGLBoolean  (EGLAPIENTRY *PFNEGLDESTROYCONTEXTPROC)(EGLDisplay, EGLContext);
typedef EGLSurface  (EGLAPIENTRY *PFNEGLCREATEPBUFFERSURFACEPROC)(EGLDisplay, EGLConfig, const EGLint *);
typedef EGLSurface  (EGLAPIENTRY *PFNEGLCREATEWINDOWSURFACEPROC)(EGLDisplay, EGLConfig, EGLNativeWindowType, const EGLint *);
typedef EGLBoolean  (EGLAPIENTRY *PFNEGLDESTROYSURFACEPROC)(EGLDisplay, EGLSurface);
typedef EGLBoolean  (EGLAPIENTRY *PFNEGLMAKECURRENTPROC)(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
typedef EGLBoolean  (EGLAPIENTRY *PFNEGLSWAPBUFFERSPROC)(EGLDisplay, EGLSurface);
typedef EGLBoolean  (EGLAPIENTRY *PFNEGLSWAPINTERVALPROC)(EGLDisplay, EGLint);
typedef EGLBoolean  (EGLAPIENTRY *PFNEGLQUERYSURFACEPROC)(EGLDisplay, EGLSurface, EGLint, EGLint *);
typedef __eglMustCastToProperFunctionPointerType (EGLAPIENTRY *PFNEGLGETPROCADDRESSPROC)(const char *);

// ---------------------------------------------------------------------------
// EGL function pointers
// ---------------------------------------------------------------------------
#define AURORA_EGL_FUNCTIONS(X)                                              \
    X(PFNEGLGETDISPLAYPROC,               eglGetDisplay)                    \
    X(PFNEGLINITIALIZEPROC,               eglInitialize)                    \
    X(PFNEGLTERMINATEPROC,                eglTerminate)                     \
    X(PFNEGLGETERRORPROC,                 eglGetError)                      \
    X(PFNEGLBINDAPIPROC,                  eglBindAPI)                       \
    X(PFNEGLCHOOSECONFIGPROC,             eglChooseConfig)                  \
    X(PFNEGLCREATECONTEXTPROC,            eglCreateContext)                 \
    X(PFNEGLDESTROYCONTEXTPROC,           eglDestroyContext)                \
    X(PFNEGLCREATEPBUFFERSURFACEPROC,     eglCreatePbufferSurface)          \
    X(PFNEGLCREATEWINDOWSURFACEPROC,      eglCreateWindowSurface)          \
    X(PFNEGLDESTROYSURFACEPROC,           eglDestroySurface)               \
    X(PFNEGLMAKECURRENTPROC,              eglMakeCurrent)                   \
    X(PFNEGLSWAPBUFFERSPROC,              eglSwapBuffers)                   \
    X(PFNEGLSWAPINTERVALPROC,             eglSwapInterval)                  \
    X(PFNEGLQUERYSURFACEPROC,             eglQuerySurface)                  \
    X(PFNEGLGETPROCADDRESSPROC,           eglGetProcAddress)

// Declare each EGL symbol as an extern function pointer.
#define AURORA_DECLARE_EGL(TYPE, NAME) extern TYPE NAME;
AURORA_EGL_FUNCTIONS(AURORA_DECLARE_EGL)
#undef AURORA_DECLARE_EGL

// ---------------------------------------------------------------------------
// GLES 3.2 function pointers  (Win32 only)
// ---------------------------------------------------------------------------
#define AURORA_GL_FUNCTIONS(X)                                               \
    X(PFNGLGETSTRINGPROC,                 glGetString)                      \
    X(PFNGLFINISHPROC,                    glFinish)                         \
    X(PFNGLENABLEPROC,                    glEnable)                         \
    X(PFNGLDISABLEPROC,                   glDisable)                        \
    /* buffers */                                                            \
    X(PFNGLGENBUFFERSPROC,                glGenBuffers)                     \
    X(PFNGLDELETEBUFFERSPROC,             glDeleteBuffers)                  \
    X(PFNGLBINDBUFFERPROC,                glBindBuffer)                     \
    X(PFNGLBUFFERDATAPROC,                glBufferData)                     \
    X(PFNGLMAPBUFFERRANGEPROC,            glMapBufferRange)                 \
    X(PFNGLUNMAPBUFFERPROC,               glUnmapBuffer)                   \
    /* textures */                                                           \
    X(PFNGLGENTEXTURESPROC,               glGenTextures)                    \
    X(PFNGLDELETETEXTURESPROC,            glDeleteTextures)                 \
    X(PFNGLBINDTEXTUREPROC,               glBindTexture)                   \
    X(PFNGLTEXSTORAGE2DPROC,              glTexStorage2D)                   \
    X(PFNGLTEXSTORAGE3DPROC,              glTexStorage3D)                   \
    X(PFNGLTEXSTORAGE2DMULTISAMPLEPROC,   glTexStorage2DMultisample)        \
    X(PFNGLTEXSTORAGE3DMULTISAMPLEPROC,   glTexStorage3DMultisample)        \
    /* renderbuffers */                                                      \
    X(PFNGLGENRENDERBUFFERSPROC,          glGenRenderbuffers)               \
    X(PFNGLDELETERENDERBUFFERSPROC,       glDeleteRenderbuffers)            \
    X(PFNGLBINDRENDERBUFFERPROC,          glBindRenderbuffer)               \
    X(PFNGLRENDERBUFFERSTORAGEPROC,       glRenderbufferStorage)            \
    X(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC, glRenderbufferStorageMultisample) \
    /* samplers */                                                           \
    X(PFNGLGENSAMPLERSPROC,               glGenSamplers)                    \
    X(PFNGLDELETESAMPLERSPROC,            glDeleteSamplers)                 \
    X(PFNGLSAMPLERPARAMETERIPROC,         glSamplerParameteri)              \
    X(PFNGLSAMPLERPARAMETERFPROC,         glSamplerParameterf)              \
    /* shaders */                                                            \
    X(PFNGLCREATESHADERPROC,              glCreateShader)                   \
    X(PFNGLDELETESHADERPROC,              glDeleteShader)                   \
    X(PFNGLSHADERSOURCEPROC,              glShaderSource)                   \
    X(PFNGLCOMPILESHADERPROC,             glCompileShader)                  \
    X(PFNGLGETSHADERIVPROC,               glGetShaderiv)                    \
    X(PFNGLGETSHADERINFOLOGPROC,          glGetShaderInfoLog)               \
    /* programs */                                                           \
    X(PFNGLCREATEPROGRAMPROC,             glCreateProgram)                  \
    X(PFNGLDELETEPROGRAMPROC,             glDeleteProgram)                  \
    X(PFNGLATTACHSHADERPROC,              glAttachShader)                   \
    X(PFNGLDETACHSHADERPROC,              glDetachShader)                   \
    X(PFNGLLINKPROGRAMPROC,               glLinkProgram)                    \
    X(PFNGLGETPROGRAMIVPROC,              glGetProgramiv)                   \
    X(PFNGLGETPROGRAMINFOLOGPROC,         glGetProgramInfoLog)              \
    X(PFNGLUSEPROGRAMPROC,                glUseProgram)                     \
    /* state */                                                              \
    X(PFNGLCULLFACEPROC,                  glCullFace)                       \
    X(PFNGLFRONTFACEPROC,                 glFrontFace)                      \
    X(PFNGLDEPTHFUNCPROC,                 glDepthFunc)                      \
    X(PFNGLDEPTHMASKPROC,                 glDepthMask)                      \
    X(PFNGLSTENCILFUNCSEPARATEPROC,       glStencilFuncSeparate)            \
    X(PFNGLSTENCILOPSEPARATEPROC,         glStencilOpSeparate)              \
    X(PFNGLSTENCILMASKSEPARATEPROC,       glStencilMaskSeparate)            \
    X(PFNGLBLENDFUNCSEPARATEPROC,         glBlendFuncSeparate)              \
    X(PFNGLBLENDEQUATIONSEPARATEPROC,     glBlendEquationSeparate)          \
    /* sync */                                                               \
    X(PFNGLFENCESYNCPROC,                 glFenceSync)                      \
    X(PFNGLDELETESYNCPROC,                glDeleteSync)                     \
    X(PFNGLWAITSYNCPROC,                  glWaitSync)                       \
    /* viewport / scissor */                                                 \
    X(PFNGLVIEWPORTPROC,                  glViewport)                       \
    X(PFNGLSCISSORPROC,                   glScissor)                        \
    X(PFNGLCOLORMASKPROC,                 glColorMask)                      \
    X(PFNGLCLEARCOLORPROC,                glClearColor)                     \
    X(PFNGLCLEARDEPTHFPROC,               glClearDepthf)                    \
    X(PFNGLCLEARSTENCILPROC,              glClearStencil)                   \
    X(PFNGLCLEARPROC,                     glClear)                          \
    /* draw */                                                               \
    X(PFNGLDRAWARRAYSPROC,                glDrawArrays)                     \
    X(PFNGLDRAWELEMENTSPROC,              glDrawElements)                   \
    X(PFNGLDRAWARRAYSINSTANCEDPROC,       glDrawArraysInstanced)            \
    X(PFNGLDRAWELEMENTSINSTANCEDPROC,     glDrawElementsInstanced)          \
    X(PFNGLDISPATCHCOMPUTEPROC,           glDispatchCompute)                \
    X(PFNGLDISPATCHCOMPUTEINDIRECTPROC,   glDispatchComputeIndirect)        \
    /* vertex array */                                                       \
    X(PFNGLGENVERTEXARRAYSPROC,           glGenVertexArrays)                \
    X(PFNGLDELETEVERTEXARRAYSPROC,        glDeleteVertexArrays)             \
    X(PFNGLBINDVERTEXARRAYPROC,           glBindVertexArray)                \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC,   glEnableVertexAttribArray)        \
    X(PFNGLVERTEXATTRIBPOINTERPROC,       glVertexAttribPointer)            \
    X(PFNGLBINDBUFFERBASEPROC,            glBindBufferBase)                 \
    X(PFNGLBINDBUFFERRANGEPROC,           glBindBufferRange)                \
    /* framebuffer */                                                        \
    X(PFNGLGENFRAMEBUFFERSPROC,           glGenFramebuffers)                \
    X(PFNGLDELETEFRAMEBUFFERSPROC,        glDeleteFramebuffers)             \
    X(PFNGLBINDFRAMEBUFFERPROC,           glBindFramebuffer)                \
    X(PFNGLFRAMEBUFFERTEXTURE2DPROC,      glFramebufferTexture2D)           \
    X(PFNGLFRAMEBUFFERRENDERBUFFERPROC,   glFramebufferRenderbuffer)        \
    X(PFNGLDRAWBUFFERSPROC,               glDrawBuffers)                    \
    X(PFNGLBLITFRAMEBUFFERPROC,           glBlitFramebuffer)                \
    /* copy */                                                               \
    X(PFNGLCOPYBUFFERSUBDATAPROC,         glCopyBufferSubData)              \
    X(PFNGLTEXSUBIMAGE2DPROC,             glTexSubImage2D)                  \
    X(PFNGLTEXSUBIMAGE3DPROC,             glTexSubImage3D)                  \
    X(PFNGLREADPIXELSPROC,                glReadPixels)                     \
    X(PFNGLMEMORYBARRIERPROC,             glMemoryBarrier)

// Declare each GL symbol as an extern function pointer.
#define AURORA_DECLARE_GL(TYPE, NAME) extern TYPE NAME;
AURORA_GL_FUNCTIONS(AURORA_DECLARE_GL)
#undef AURORA_DECLARE_GL

#endif // SKY_PLATFORM_WINDOWS
