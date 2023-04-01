//
// Created by Zach on 2023/1/31.
//

#pragma once

#include <rhi/ImageView.h>
#include <gles/DevObject.h>
#include <gles/Image.h>

namespace sky::gles {

    class ImageView : public rhi::ImageView, public DevObject {
    public:
        ImageView(Device &dev) : DevObject(dev) {}
        ~ImageView() = default;

        bool Init(const rhi::ImageViewDesc &desc);
        const ImagePtr &GetImage() const { return source; }
        GLuint GetNativeHandle() const { return source->GetNativeHandle(); }

    private:
        friend class Image;
        ImagePtr source;
    };

    using ImageViewPtr = std::shared_ptr<ImageView>;

}
