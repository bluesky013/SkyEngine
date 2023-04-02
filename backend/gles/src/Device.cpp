//
// Created by Zach on 2023/1/30.
//

#include <gles/Device.h>
#include <gles/Swapchain.h>
#include <core/util/String.h>
#include <core/logger/Logger.h>
#include <core/util/DynamicModule.h>
#include <algorithm>

static const char *TAG = "GLES";
std::unique_ptr<sky::DynamicModule> g_Gles;

namespace sky::gles {

    static bool CheckExtension(const std::vector<std::string> &extensions, const char* ext)
    {
        return std::any_of(extensions.begin(), extensions.end(), [ext](const std::string &extension) {
            return extension.find(ext) != std::string::npos;
        });
    }

    void * GladLoadProcAddress(const char *procName) {
        return g_Gles->GetAddress(procName);
    }

    static bool InitGL()
    {
        if (g_Gles && g_Gles->IsLoaded()) {
            return true;
        }

        g_Gles = std::make_unique<DynamicModule>("libGLESV2.dll");
        g_Gles->Load();
        gladLoadGLES2Loader(GladLoadProcAddress);
        return g_Gles->IsLoaded();
    }

    Device::~Device()
    {
        if (graphicsQueue) {
            graphicsQueue->Shutdown();
        }
        if (transferQueue) {
            transferQueue->Shutdown();
        }
    }

    bool Device::Init(const Descriptor &desc)
    {
        Context::Descriptor ctxDesc = {};
        mainContext = std::make_unique<Context>();
        mainContext->Init(ctxDesc);

        if (!InitGL()) {
            return false;
        }

        std::string extStr = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
        extensions = Split(extStr, ' ');
        LOG_I(TAG, "*******************extensions****************");
        for (auto &ext : extensions) {
            LOG_I(TAG, "%s", ext.c_str());
        }
        InitLimitation();
        InitDeviceFeature();

        ctxDesc.sharedContext = mainContext->GetNativeHandle();
        graphicsQueue = std::make_unique<Queue>(*this);
        graphicsQueue->StartThread();
        graphicsQueue->Init(ctxDesc, rhi::QueueType::GRAPHICS);

        ctxDesc.defaultConfig.rgb     = 8;
        ctxDesc.defaultConfig.depth   = 0;
        ctxDesc.defaultConfig.stencil = 0;
        transferQueue = std::make_unique<Queue>(*this);
        transferQueue->StartThread();
        transferQueue->Init(ctxDesc, rhi::QueueType::TRANSFER);
        return true;
    }

    Context *Device::GetMainContext() const
    {
        return mainContext.get();
    }

    Queue *Device::GetGraphicsQueue() const
    {
        return graphicsQueue.get();
    }

    Queue *Device::GetTransferQueue() const
    {
        return transferQueue.get();
    }

    Queue* Device::GetQueue(rhi::QueueType type) const
    {
        return type == rhi::QueueType::GRAPHICS ? graphicsQueue.get() : transferQueue.get();
    }

    void Device::InitLimitation()
    {
        glGetIntegerv(GL_MAX_SHADER_PIXEL_LOCAL_STORAGE_FAST_SIZE_EXT, reinterpret_cast<GLint*>(&limitation.maxFastShaderPixelStorage));
        glGetIntegerv(GL_MAX_SHADER_PIXEL_LOCAL_STORAGE_SIZE_EXT, reinterpret_cast<GLint*>(&limitation.maxShaderPixelStorage));
    }

    void Device::InitDeviceFeature()
    {
        enabledFeature.framebufferFetch = enabledFeature.framebufferFetch && CheckExtension(extensions, "shader_framebuffer_fetch");
        enabledFeature.pixelLocalStorage = enabledFeature.pixelLocalStorage && CheckExtension(extensions, "shader_pixel_local_storage");
    }

}
