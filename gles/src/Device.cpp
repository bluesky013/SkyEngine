//
// Created by Zach on 2023/1/30.
//

#include <gles/Device.h>

namespace sky::gles {

    bool Device::Init(const Descriptor &desc)
    {
        mainContext = std::make_unique<Context>();
        mainContext->Init();

        graphicsContext = std::make_unique<Context>();
        graphicsContext->Init(mainContext->GetNativeHandle());

        transferContext = std::make_unique<Context>();
        transferContext->Init(transferContext->GetNativeHandle());

        return true;
    }
}
