//
// Created on 2026/04/01.
//

#include <GLESDevice.h>
#include <GLESInstance.h>
#include <GLESLoader.h>
#include <GLESFence.h>
#include <GLESSemaphore.h>
#include <GLESBuffer.h>
#include <GLESImage.h>
#include <GLESSampler.h>
#include <GLESSwapChain.h>
#include <GLESShader.h>
#include <GLESPipelineState.h>
#include <GLESCommandPool.h>
#include <GLESConversion.h>
#include <core/logger/Logger.h>
#include <core/platform/Platform.h>
#include <cstring>

static const char *TAG = "AuroraGL";

namespace sky::aurora {

    static bool HasExtension(const char *extensions, const char *target)
    {
        if (extensions == nullptr || target == nullptr || target[0] == '\0') {
            return false;
        }

        const size_t targetLength = std::strlen(target);
        const char *cursor = extensions;
        while ((cursor = std::strstr(cursor, target)) != nullptr) {
            const bool atStart = cursor == extensions || cursor[-1] == ' ';
            const bool atEnd = cursor[targetLength] == '\0' || cursor[targetLength] == ' ';
            if (atStart && atEnd) {
                return true;
            }
            cursor += targetLength;
        }
        return false;
    }

    // -----------------------------------------------------------------------
    // GLESContext
    // -----------------------------------------------------------------------
    GLESContext::GLESContext(GLESInstance &inst)
        : instance(inst)
    {
    }

    GLESContext::~GLESContext()
    {
        const auto display = instance.GetEGLDisplay();
        if (eglContext != EGL_NO_CONTEXT) {
            eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroyContext(display, eglContext);
        }
        if (pbuffer != EGL_NO_SURFACE) {
            eglDestroySurface(display, pbuffer);
        }
    }

    void GLESContext::OnAttach(uint32_t threadIndex)
    {
        const auto display = instance.GetEGLDisplay();
        const auto config  = instance.GetEGLConfig();

        const EGLint contextAttribs[] = {
            EGL_CONTEXT_MAJOR_VERSION, 3,
            EGL_CONTEXT_MINOR_VERSION, 2,
            EGL_NONE
        };
        eglContext = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
        if (eglContext == EGL_NO_CONTEXT) {
            LOG_E(TAG, "eglCreateContext failed: 0x%x (thread %u)", eglGetError(), threadIndex);
            return;
        }

        const EGLint pbufferAttribs[] = {
            EGL_WIDTH,  1,
            EGL_HEIGHT, 1,
            EGL_NONE
        };
        pbuffer = eglCreatePbufferSurface(display, config, pbufferAttribs);

        if (eglMakeCurrent(display, pbuffer, pbuffer, eglContext) != EGL_TRUE) {
            LOG_E(TAG, "eglMakeCurrent failed: 0x%x (thread %u)", eglGetError(), threadIndex);
            return;
        }

#if SKY_PLATFORM_WINDOWS
        if (!LoadGLESFunctions()) {
            LOG_E(TAG, "LoadGLESFunctions failed (thread %u)", threadIndex);
            return;
        }
#endif

        LOG_I(TAG, "GLES context attached: thread %u, GL_RENDERER=%s", threadIndex,
              reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    }

    void GLESContext::OnDetach()
    {
        const auto display = instance.GetEGLDisplay();
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }

    // -----------------------------------------------------------------------
    // GLESDevice
    // -----------------------------------------------------------------------
    GLESDevice::GLESDevice(GLESInstance &inst)
        : instance(inst)
    {
    }

    GLESDevice::~GLESDevice()
    {
    }

    bool GLESDevice::OnInit(const DeviceInit &init)
    {
        (void)init;
        capability.maxThreads = 1; // GLES contexts are typically single-threaded
        const auto *extensions = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
        capability.anisotropyEnable = HasExtension(extensions, "GL_EXT_texture_filter_anisotropic") ||
                                      HasExtension(extensions, "GL_ARB_texture_filter_anisotropic");
        LOG_I(TAG, "GLES device initialized");
        return true;
    }

    std::string GLESDevice::GetDeviceInfo() const
    {
        const auto *renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        return renderer ? renderer : "GLES";
    }

    void GLESDevice::WaitIdle() const
    {
        glFinish();
    }

    CommandPool *GLESDevice::CreateCommandPool(QueueType /*type*/)
    {
        auto *pool = new GLESCommandPool(*this);
        if (!pool->Init()) {
            delete pool;
            return nullptr;
        }
        return pool;
    }

    Fence *GLESDevice::CreateFence(const Fence::Descriptor &desc)
    {
        auto *f = new GLESFence();
        if (!f->Init(desc)) {
            delete f;
            return nullptr;
        }
        return f;
    }

    Semaphore *GLESDevice::CreateSema(const Semaphore::Descriptor &desc)
    {
        auto *s = new GLESSemaphore();
        if (!s->Init(desc)) {
            delete s;
            return nullptr;
        }
        return s;
    }

    GraphicsPipeline *GLESDevice::CreatePipelineState(const GraphicsPipeline::Descriptor &desc)
    {
        auto *pipeline = new GLESGraphicsPipeline();
        if (!pipeline->Init(desc)) {
            delete pipeline;
            return nullptr;
        }
        return pipeline;
    }

    Buffer *GLESDevice::CreateBuffer(const Buffer::Descriptor &desc)
    {
        auto *buf = new GLESBuffer(*this);
        if (!buf->Init(desc)) {
            delete buf;
            return nullptr;
        }
        return buf;
    }

    Image *GLESDevice::CreateImage(const Image::Descriptor &desc)
    {
        auto *img = new GLESImage(*this);
        if (!img->Init(desc)) {
            delete img;
            return nullptr;
        }
        return img;
    }

    Sampler *GLESDevice::CreateSampler(const Sampler::Descriptor &desc)
    {
        auto *smp = new GLESSampler(*this);
        if (!smp->Init(desc)) {
            delete smp;
            return nullptr;
        }
        return smp;
    }

    SwapChain *GLESDevice::CreateSwapChain(const SwapChain::Descriptor &desc)
    {
        auto *sc = new GLESSwapChain(*this);
        if (!sc->Init(desc)) {
            delete sc;
            return nullptr;
        }
        return sc;
    }

    ShaderFunction *GLESDevice::CreateShaderFunction(const ShaderFunction::Descriptor &desc)
    {
        auto *fn = new GLESShaderFunction(*this);
        if (!fn->Init(desc)) {
            delete fn;
            return nullptr;
        }
        return fn;
    }

    Shader *GLESDevice::CreateShader(const Shader::Descriptor &desc)
    {
        auto *sh = new GLESShader(*this);
        if (!sh->Init(desc)) {
            delete sh;
            return nullptr;
        }
        return sh;
    }

    PixelFormatFeatureFlags GLESDevice::GetFormatFeatureFlags(PixelFormat format) const
    {
        const auto &glFmt = FromPixelFormat(format);
        if (glFmt.internalFormat == 0) {
            return {};
        }

        const auto &info = GetImageFormatInfo(format);
        PixelFormatFeatureFlags result;

        if (info.isCompressed) {
            // compressed formats: sample + filter only
            result |= PixelFormatFeatureFlagBit::SAMPLE;
            result |= PixelFormatFeatureFlagBit::SAMPLE_FILTER;
            return result;
        }

        if (info.hasDepth || info.hasStencil) {
            result |= PixelFormatFeatureFlagBit::DEPTH_STENCIL;
            result |= PixelFormatFeatureFlagBit::SAMPLE;
            return result;
        }

        // color-renderable check via GLES 3.2 mandatory formats
        const GLenum ifmt = glFmt.internalFormat;
        const bool colorRenderable =
            ifmt == GL_R8       || ifmt == GL_R8UI      || ifmt == GL_R8I       ||
            ifmt == GL_R16F     || ifmt == GL_R32F      || ifmt == GL_R16UI     ||
            ifmt == GL_R16I     || ifmt == GL_R32UI     || ifmt == GL_R32I      ||
            ifmt == GL_RG8      || ifmt == GL_RG8UI     || ifmt == GL_RG8I      ||
            ifmt == GL_RG16F    || ifmt == GL_RG32F     || ifmt == GL_RG16UI    ||
            ifmt == GL_RG16I    || ifmt == GL_RG32UI    || ifmt == GL_RG32I     ||
            ifmt == GL_RGBA8    || ifmt == GL_SRGB8_ALPHA8 ||
            ifmt == GL_RGBA8UI  || ifmt == GL_RGBA8I    ||
            ifmt == GL_RGB10_A2 || ifmt == GL_RGB10_A2UI||
            ifmt == GL_RGBA16F  || ifmt == GL_RGBA32F   ||
            ifmt == GL_RGBA16UI || ifmt == GL_RGBA16I   ||
            ifmt == GL_RGBA32UI || ifmt == GL_RGBA32I;

        if (colorRenderable) {
            result |= PixelFormatFeatureFlagBit::COLOR;
        }

        // blend: all non-integer color-renderable formats
        const bool isInteger =
            glFmt.type == GL_UNSIGNED_INT   || glFmt.type == GL_INT        ||
            glFmt.type == GL_UNSIGNED_SHORT || glFmt.type == GL_SHORT      ||
            glFmt.type == GL_UNSIGNED_BYTE  && glFmt.format == GL_RED_INTEGER;
        const bool integerFormat = (glFmt.format == GL_RED_INTEGER ||
                                    glFmt.format == GL_RG_INTEGER  ||
                                    glFmt.format == GL_RGB_INTEGER ||
                                    glFmt.format == GL_RGBA_INTEGER);

        if (colorRenderable && !integerFormat) {
            result |= PixelFormatFeatureFlagBit::BLEND;
        }

        // all non-compressed non-depth formats are sampleable in GLES 3.2
        result |= PixelFormatFeatureFlagBit::SAMPLE;

        // linear filter: all non-integer formats
        if (!integerFormat) {
            result |= PixelFormatFeatureFlagBit::SAMPLE_FILTER;
        }

        // storage: GLES 3.2 image load/store mandatory formats
        const bool storageCapable =
            ifmt == GL_RGBA32F   || ifmt == GL_RGBA16F   ||
            ifmt == GL_R32F      ||
            ifmt == GL_RGBA32UI  || ifmt == GL_RGBA16UI  ||
            ifmt == GL_RGBA8UI   || ifmt == GL_R32UI     ||
            ifmt == GL_RGBA32I   || ifmt == GL_RGBA16I   ||
            ifmt == GL_RGBA8I    || ifmt == GL_R32I      ||
            ifmt == GL_RGBA8     || ifmt == GL_RGBA8_SNORM;

        if (storageCapable) {
            result |= PixelFormatFeatureFlagBit::STORAGE;
        }

        // atomic: R32UI and R32I
        if (ifmt == GL_R32UI || ifmt == GL_R32I) {
            result |= PixelFormatFeatureFlagBit::STORAGE_ATOMIC;
        }

        return result;
    }

    ThreadContext *GLESDevice::CreateAsyncContext()
    {
        return new GLESContext(instance);
    }

} // namespace sky::aurora
