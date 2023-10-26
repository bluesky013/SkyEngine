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
        bufferDesc = desc;
        CHECK(glGenBuffers(1, &buffer));

        // general helper target
        if (bufferDesc.usage & rhi::BufferUsageFlagBit::VERTEX) {
            target = GL_ARRAY_BUFFER;
        } else if (bufferDesc.usage & rhi::BufferUsageFlagBit::INDEX) {
            target = GL_ELEMENT_ARRAY_BUFFER;
        } else if (bufferDesc.usage & rhi::BufferUsageFlagBit::UNIFORM) {
            target = GL_UNIFORM_BUFFER;
        } else if (bufferDesc.usage & rhi::BufferUsageFlagBit::STORAGE) {
            target = GL_SHADER_STORAGE_BUFFER;
        } else if (bufferDesc.usage & rhi::BufferUsageFlagBit::INDIRECT) {
            target = GL_DRAW_INDIRECT_BUFFER;
        }

        if (desc.memory == rhi::MemoryType::GPU_ONLY) {
            usage = GL_STATIC_DRAW;
        } else {
            usage = GL_DYNAMIC_DRAW;
        }

        CHECK(glBindBuffer(target, buffer));
        CHECK(glBufferData(target, desc.size, nullptr, usage));
        CHECK(glBindBuffer(target, 0));
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

    uint8_t *Buffer::Map()
    {
        CHECK(glBindBuffer(target, buffer));
        return static_cast<uint8_t *>(glMapBufferRange(target, 0, bufferDesc.size, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
    }

    void Buffer::UnMap()
    {
        CHECK(glUnmapBuffer(target));
        CHECK(glBindBuffer(target, 0));
    }
}
