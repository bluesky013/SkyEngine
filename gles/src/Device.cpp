//
// Created by Zach on 2023/1/30.
//

#include <gles/Device.h>
#include <gles/Swapchain.h>

namespace sky::gles {

    Device::~Device()
    {
        graphicsQueue->Shutdown();
        transferQueue->Shutdown();
    }

    bool Device::Init(const Descriptor &desc)
    {
        Context::Descriptor ctxDesc = {};
        mainContext = std::make_unique<Context>();
        mainContext->Init(ctxDesc);

        ctxDesc.sharedContext = mainContext->GetNativeHandle();
        graphicsQueue = std::make_unique<Queue>(*this);
        graphicsQueue->StartThread();
        graphicsQueue->Init(ctxDesc);

        ctxDesc.defaultConfig.rgb     = 8;
        ctxDesc.defaultConfig.depth   = 0;
        ctxDesc.defaultConfig.stencil = 0;
        transferQueue = std::make_unique<Queue>(*this);
        transferQueue->StartThread();
        transferQueue->Init(ctxDesc);
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

}
