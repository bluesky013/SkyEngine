//
// Created by Zach on 2023/1/31.
//

#pragma once

#include <rhi/Image.h>
#include <gles/DevObject.h>
#include <gles/Conversion.h>
#include <gles/egl/Surface.h>

namespace sky::gles {

    class Image : public rhi::Image, public DevObject, public std::enable_shared_from_this<Image> {
    public:
        Image(Device &dev) : DevObject(dev) {}
        ~Image();

        bool Init(const Descriptor &desc);
        rhi::ImageViewPtr CreateView(const rhi::ImageViewDesc &desc) override;

        const SurfacePtr &GetSurface() const { return surface; }
        GLuint GetNativeHandle() const { return texId; }
        bool IsRenderBuffer() const { return renderBuffer; }

    private:
        friend class SwapChain;

        GLuint texId = 0;
        bool renderBuffer = false;
        SurfacePtr surface;
    };

    using ImagePtr = std::shared_ptr<Image>;
}
