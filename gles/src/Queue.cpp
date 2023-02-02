//
// Created by Zach on 2023/1/31.
//

#include <gles/Queue.h>
#include <gles/CommandBuffer.h>

namespace sky::gles {

    bool Queue::Init(const Context::Descriptor &cfg)
    {
        context = std::make_unique<Context>();
        CreateTask([this, cfg]() {
            context->Init(cfg);
        });

        return true;
    }

    void Queue::Submit(const rhi::CommandBufferPtr &cmd)
    {
        CreateTask([cmd]() {
            auto commandBuffer = std::static_pointer_cast<CommandBuffer>(cmd);
            commandBuffer->Execute();
        });
    }
}

