//
// Created by Zach Lee on 2022/11/10.
//

#pragma once

#include <rhi/Core.h>

namespace sky::rhi {

    class Image {
    public:
        Image()          = default;
        virtual ~Image() = default;

        struct Descriptor {
            ImageType       imageType   = ImageType::IMAGE_2D;
            PixelFormat     format      = PixelFormat::UNDEFINED;
            Extent3D        extent      = {1, 1, 1};
            uint32_t        mipLevels   = 1;
            uint32_t        arrayLayers = 1;
            uint32_t        samples     = 1;
            ImageUsageFlags usage       = ImageUsageFlagBit::NONE;
            MemoryType      memory      = MemoryType::GPU_ONLY;
            bool            allocateMem = true;
        };

        const Descriptor &GetDescriptor() const;

    protected:
        Descriptor imageDesc;
    };

}