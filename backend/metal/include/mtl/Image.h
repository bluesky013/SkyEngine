//
// Created by Zach Lee on 2022/11/4.
//

#pragma once

#include <rhi/Image.h>
#include <mtl/DevObject.h>
#import <Metal/MTLTexture.h>
#import <QuartzCore/CAMetalLayer.h>

namespace sky::mtl {

    class Image : public rhi::Image, public DevObject, public std::enable_shared_from_this<Image> {
    public:
        Image(Device &);
        ~Image();

        rhi::ImageViewPtr CreateView(const rhi::ImageViewDesc &desc);

        void SetDrawable(id<CAMetalDrawable> drawable);
        void ResetDrawable();

    private:
        friend class Device;
        friend class SwapChain;
        bool Init(const Descriptor &);

        id<MTLTexture> texture = nil;
        id<CAMetalDrawable> currentDrawable = nil;
        MTLTextureDescriptor *textureDesc = nil;
    };
    using ImagePtr = std::shared_ptr<Image>;
}
