//
// Created by blues on 2026/3/29.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <aurora/rhi/Core.h>
#include <aurora/rhi/Resource.h>

namespace sky::aurora {

    class Image
        : public RefObject
        , public IDelayReleaseResource {
    public:
        struct Descriptor {
            ImageType           imageType   = ImageType::IMAGE_2D;
            PixelFormat         format      = PixelFormat::UNDEFINED;
            Extent3D            extent      = {1, 1, 1};
            uint32_t            mipLevels   = 1;
            uint32_t            arrayLayers = 1;
            SampleCount         samples     = SampleCount::X1;
            ImageUsageFlags     usage       = ImageUsageFlagBit::NONE;
            ImageViewUsageFlags viewUsage   = ImageViewUsageFlagBit::NONE;
            MemoryType          memory      = MemoryType::GPU_ONLY;
        };

        Image() = default;
        ~Image() override = default;
    };

    using ImagePtr = CounterPtr<Image>;
} // namespace sky::aurora
