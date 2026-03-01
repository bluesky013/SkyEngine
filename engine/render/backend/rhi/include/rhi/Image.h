//
// Created by Zach Lee on 2022/11/10.
//

#pragma once

#include <rhi/Core.h>
#include <rhi/ImageView.h>

namespace sky::rhi {

    class Image {
    public:
        Image()          = default;
        virtual ~Image() = default;

        struct Descriptor {
            ImageType        imageType   = ImageType::IMAGE_2D;
            PixelFormat      format      = PixelFormat::UNDEFINED;
            Extent3D         extent      = {1, 1, 1};
            uint32_t         mipLevels   = 1;
            uint32_t         arrayLayers = 1;
            rhi::SampleCount samples     = rhi::SampleCount::X1;
            ImageUsageFlags  usage       = ImageUsageFlagBit::NONE;
            MemoryType       memory      = MemoryType::GPU_ONLY;

            bool cubeCompatible = false;
        };

        const Descriptor &GetDescriptor() const { return imageDesc; };
        const ImageFormatInfo &GetFormatInfo() const { return formatInfo; };

        virtual ImageViewPtr CreateView(const ImageViewDesc &desc) = 0;

    protected:
        Descriptor imageDesc;
        ImageFormatInfo formatInfo;
    };
    using ImagePtr = std::shared_ptr<Image>;

}
