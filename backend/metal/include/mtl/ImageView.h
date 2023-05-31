//
// Created by Zach Lee on 2022/11/4.
//

#pragma once

#include <rhi/ImageView.h>
#include <mtl/DevObject.h>
#include <mtl/Image.h>

namespace sky::mtl {

    class ImageView : public rhi::ImageView, public DevObject {
    public:
        ImageView(Device &dev) : DevObject(dev) {}
        ~ImageView() = default;

        id<MTLTexture> GetNativeHandle() const { return source->GetNativeHandle(); }
    private:
        bool Init(const rhi::ImageViewDesc &desc);
        std::shared_ptr<rhi::ImageView> CreateView(const rhi::ImageViewDesc &desc) const override;

        friend class Image;
        ImagePtr source;
    };
    using ImageViewPtr = std::shared_ptr<ImageView>;
}
