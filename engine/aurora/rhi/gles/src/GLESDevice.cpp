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
#include <core/logger/Logger.h>

static const char *TAG = "AuroraGL";

namespace sky::aurora {

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

#ifdef WIN32
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
        capability.maxThreads = 1; // GLES contexts are typically single-threaded
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

    ThreadContext *GLESDevice::CreateAsyncContext()
    {
        return new GLESContext(instance);
    }

} // namespace sky::aurora
