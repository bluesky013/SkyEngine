//
// Created on 2026/04/01.
//

#include <GLESBuffer.h>
#include <GLESLoader.h>
#include <GLESDevice.h>
#include <core/logger/Logger.h>

static const char *TAG = "AuroraGL";

namespace sky::aurora {

    GLESBuffer::GLESBuffer(GLESDevice &dev)
        : device(dev)
    {
    }

    GLESBuffer::~GLESBuffer()
    {
        if (buffer != 0) {
            glDeleteBuffers(1, &buffer);
        }
    }

    bool GLESBuffer::Init(const Descriptor &desc)
    {
        size = desc.size;

        if (desc.usage & BufferUsageFlagBit::VERTEX) {
            target = GL_ARRAY_BUFFER;
        } else if (desc.usage & BufferUsageFlagBit::INDEX) {
            target = GL_ELEMENT_ARRAY_BUFFER;
        } else if (desc.usage & BufferUsageFlagBit::UNIFORM) {
            target = GL_UNIFORM_BUFFER;
        } else if (desc.usage & BufferUsageFlagBit::STORAGE) {
            target = GL_SHADER_STORAGE_BUFFER;
        } else if (desc.usage & BufferUsageFlagBit::INDIRECT) {
            target = GL_DRAW_INDIRECT_BUFFER;
        } else {
            target = GL_COPY_WRITE_BUFFER;
        }

        usage = (desc.memory == MemoryType::GPU_ONLY) ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW;

        glGenBuffers(1, &buffer);
        if (buffer == 0) {
            LOG_E(TAG, "glGenBuffers failed");
            return false;
        }

        glBindBuffer(target, buffer);
        glBufferData(target, static_cast<GLsizeiptr>(desc.size), nullptr, usage);
        glBindBuffer(target, 0);
        return true;
    }

    uint8_t *GLESBuffer::Map()
    {
        glBindBuffer(target, buffer);
        return static_cast<uint8_t *>(
            glMapBufferRange(target, 0, static_cast<GLsizeiptr>(size),
                             GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
    }

    void GLESBuffer::UnMap()
    {
        glUnmapBuffer(target);
        glBindBuffer(target, 0);
    }

} // namespace sky::aurora