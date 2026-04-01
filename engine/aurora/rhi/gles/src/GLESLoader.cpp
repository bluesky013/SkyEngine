//
// Created on 2026/04/02.
//

#include <core/platform/Platform.h>

#if SKY_PLATFORM_WINDOWS

#include <GLESLoader.h>
#include <core/logger/Logger.h>

static const char *TAG = "GLESLoader";

// ---------------------------------------------------------------------------
// Define the EGL function-pointer variables (initially null).
// ---------------------------------------------------------------------------
#define AURORA_DEFINE_EGL(TYPE, NAME) TYPE NAME = nullptr;
AURORA_EGL_FUNCTIONS(AURORA_DEFINE_EGL)
#undef AURORA_DEFINE_EGL

// ---------------------------------------------------------------------------
// Define the GL function-pointer variables (initially null).
// ---------------------------------------------------------------------------
#define AURORA_DEFINE_GL(TYPE, NAME) TYPE NAME = nullptr;
AURORA_GL_FUNCTIONS(AURORA_DEFINE_GL)
#undef AURORA_DEFINE_GL

namespace sky::aurora {

    bool LoadEGLFunctions(void *(*getProcAddr)(const char *))
    {
        if (!getProcAddr) {
            return false;
        }

        bool ok = true;

#define AURORA_LOAD_EGL(TYPE, NAME)                                     \
        NAME = reinterpret_cast<TYPE>(getProcAddr(#NAME));              \
        if (!NAME) { LOG_E(TAG, "failed to load %s", #NAME); ok = false; }

        AURORA_EGL_FUNCTIONS(AURORA_LOAD_EGL)
#undef AURORA_LOAD_EGL

        return ok;
    }

    bool LoadGLESFunctions()
    {
        if (!eglGetProcAddress) {
            LOG_E(TAG, "eglGetProcAddress is null; call LoadEGLFunctions first");
            return false;
        }

        bool ok = true;

#define AURORA_LOAD_GL(TYPE, NAME)                                           \
        NAME = reinterpret_cast<TYPE>(eglGetProcAddress(#NAME));             \
        if (!NAME) { LOG_E(TAG, "failed to load %s", #NAME); ok = false; }

        AURORA_GL_FUNCTIONS(AURORA_LOAD_GL)
#undef AURORA_LOAD_GL

        return ok;
    }

} // namespace sky::aurora

#endif // SKY_PLATFORM_WINDOWS
