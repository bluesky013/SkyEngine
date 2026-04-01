//
// Created on 2026/04/01.
//

#pragma once

#include <aurora/rhi/Buffer.h>
#include <GLESForward.h>

namespace sky::aurora {

    class GLESDevice;

    class GLESBuffer : public Buffer {
    public:
        explicit GLESBuffer(GLESDevice &dev);
        ~GLESBuffer() override;

        bool Init(const Descriptor &desc);

        GLuint GetNativeHandle() const { return buffer; }
        GLenum GetTarget() const { return target; }

        uint8_t *Map();
        void UnMap();

    private:
        GLESDevice &device;
        GLuint buffer = 0;
        GLenum target = GL_NONE;
        GLenum usage  = GL_STATIC_DRAW;
        uint64_t size = 0;
    };

} // namespace sky::aurora
