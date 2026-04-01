//
// Created on 2026/04/01.
//

#pragma once

#include <core/platform/Platform.h>

// On Win32, there are no import libraries for the PowerVR GLES/EGL DLLs.
// All entry points are resolved at runtime through GLESLoader.
//
// gl32.h: function prototypes are guarded by GL_GLEXT_PROTOTYPES (not defined).
//         Only PFN typedefs and enum constants are visible.
// egl.h : function prototypes are guarded by !EGL_NO_PROTOTYPES.
//         Define EGL_NO_PROTOTYPES to suppress them on Win32.
#if SKY_PLATFORM_WINDOWS
    #define EGL_NO_PROTOTYPES
    #include <EGL/egl.h>
    #include <GLES3/gl32.h>
#else
    #include <EGL/egl.h>
    #include <GLES3/gl32.h>
    #include <GLES3/gl3ext.h>
#endif
