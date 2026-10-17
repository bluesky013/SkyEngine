//
// Created by blues on 2026/3/29.
//

#pragma once

#include <aurora/rhi/Core.h>
#include <aurora/rhi/Resource.h>
#include <core/template/ReferenceObject.h>

namespace sky::aurora {

    class Device;

    class Image : public RefObject, public IDelayReleaseResource {
    public:
        // Unknown owners cannot enter backend-specific native paths.
        virtual const Device *GetDevice() const noexcept
        {
            return nullptr;
        }

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
#if SKY_ENABLE_RESOURCE_NAME
            const char *name = nullptr;
#endif
        };

        // Mip/layer cells only; zero means unknown storage. Aspects remain a separate declaration.
        virtual uint64_t GetMipLayerCount() const noexcept
        {
            return 0;
        }

        Image()           = default;
        ~Image() override = default;
    };

    // Debug names do not change the storage described by an imported image.
    inline bool IsSameImageStorage(const Image::Descriptor &a, const Image::Descriptor &b) noexcept
    {
        return a.imageType == b.imageType && a.format == b.format && a.extent.width == b.extent.width && a.extent.height == b.extent.height &&
               a.extent.depth == b.extent.depth && a.mipLevels == b.mipLevels && a.arrayLayers == b.arrayLayers && a.samples == b.samples &&
               a.usage == b.usage && a.viewUsage == b.viewUsage && a.memory == b.memory;
    }

    using ImagePtr = CounterPtr<Image>;
} // namespace sky::aurora
