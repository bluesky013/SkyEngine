//
// Created on 2026/04/01.
//

#pragma once

#include <aurora/rhi/Image.h>
#include <GLESForward.h>

namespace sky::aurora {

    class GLESDevice;

    class GLESImage : public Image {
    public:
        explicit GLESImage(GLESDevice &dev);
        ~GLESImage() override;

        bool Init(const Descriptor &desc);

        GLuint GetNativeHandle() const { return texId; }
        GLenum GetTarget() const { return target; }
        bool   IsRenderBuffer() const { return renderBuffer; }

    private:
        GLESDevice &device;
        GLuint texId        = 0;
        GLenum target       = GL_TEXTURE_2D;
        bool   renderBuffer = false;
    };

} // namespace sky::aurora
