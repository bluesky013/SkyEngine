//
// Created by Zach on 2023/1/30.
//

#include <gles/Device.h>
#include <gles/Swapchain.h>

namespace sky::gles {

    bool Device::Init(const Descriptor &desc)
    {
        {
            Context::Descriptor ctxDesc = {};
            mainContext = std::make_unique<Context>();
            mainContext->Init(ctxDesc);
            pBuffer = std::make_unique<PBuffer>();
            auto cfg = mainContext->QueryConfig(ctxDesc.defaultConfig);
            pBuffer->Init(cfg);
            mainContext->MakeCurrent(*pBuffer);

            graphicsContext = std::make_unique<Context>();
            graphicsContext->Init(ctxDesc, mainContext->GetNativeHandle());
        }

        {
            Context::Descriptor ctxDesc = {};
            ctxDesc.defaultConfig.rgb     = 8;
            ctxDesc.defaultConfig.depth   = 0;
            ctxDesc.defaultConfig.stencil = 0;

            transferContext = std::make_unique<Context>();
            transferContext->Init(ctxDesc, transferContext->GetNativeHandle());
        }

        return true;
    }

    Context *Device::GetMainContext() const
    {
        return mainContext.get();
    }

    Context *Device::GetGraphicsContext() const
    {
        return graphicsContext.get();
    }

    Context *Device::GetTransferContext() const
    {
        return transferContext.get();
    }

}
