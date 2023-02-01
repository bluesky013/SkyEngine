//
// Created by Zach on 2023/1/31.
//

#include <gles/Buffer.h>
#include <gles/BufferView.h>
#include <gles/Device.h>

namespace sky::gles {

    Buffer::~Buffer()
    {
    }

    bool Buffer::Init(const Descriptor &desc)
    {
        glGenBuffers(1, &buffer);
        return true;
    }

    rhi::BufferViewPtr Buffer::CreateView(const rhi::BufferViewDesc &desc)
    {
        BufferViewPtr ret = std::make_shared<BufferView>(device);
        ret->source      = shared_from_this();
        if (!ret->Init(desc)) {
            ret = nullptr;
        }
        return std::static_pointer_cast<rhi::BufferView>(ret);
    }
}
