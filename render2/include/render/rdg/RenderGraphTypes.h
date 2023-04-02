//
// Created by Zach Lee on 2023/4/2.
//

#pragma once

#include <rhi/Core.h>

namespace sky::rdg {

    enum class ResourceResidency {
        TRANSIENT,
        PERSISTENT,
    };

    enum class ResourceAccess {
        READ,
        WRITE,
        READ_WRITE
    };

    struct TextureAttachmentDesc {
        rhi::Extent3D extent = {1, 1, 1};
        uint32_t mipLevel = 1;
        uint32_t arrayLayer = 1;

        rhi::SampleCount sample = rhi::SampleCount::X1;
        rhi::PixelFormat format = rhi::PixelFormat::RGBA8_UNORM;
        rhi::ImageUsageFlags usage = rhi::ImageUsageFlagBit::NONE;
        ResourceResidency residency = ResourceResidency::PERSISTENT;
    };

    struct BufferAttachmentDesc {
        uint64_t size = 0;
        rhi::BufferUsageFlags usage = rhi::BufferUsageFlagBit::NONE;
        ResourceResidency residency = ResourceResidency::PERSISTENT;
    };

} // namespace sky