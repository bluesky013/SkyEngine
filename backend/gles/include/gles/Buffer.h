//
// Created by Zach on 2023/1/31.
//

#pragma once

#include <rhi/Buffer.h>
#include <gles/DevObject.h>

namespace sky::gles {

    class Buffer : public rhi::Buffer, public DevObject, public std::enable_shared_from_this<Buffer> {
    public:
        Buffer(Device &dev) : DevObject(dev) {};
        ~Buffer();

        bool Init(const Descriptor &desc);
        rhi::BufferViewPtr CreateView(const rhi::BufferViewDesc &desc) override;
        GLuint GetNativeHandle() const { return buffer; }
        GLenum GetGLTarget() const { return target; }
        GLenum GetGLUsage() const { return usage; }

        uint8_t *Map() override;
        void UnMap() override;

    private:
        GLuint buffer = 0;
        GLenum target = GL_NONE;
        GLenum usage = GL_NONE;
    };
    using BufferPtr = std::shared_ptr<Buffer>;
}
