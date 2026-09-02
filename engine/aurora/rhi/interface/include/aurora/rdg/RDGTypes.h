//
// Aurora RDG resource descriptors.
//

#pragma once

#include <aurora/rhi/Core.h>

namespace sky::aurora {

    enum class ResourceResidency : uint8_t {
        TRANSIENT = 0,
        PERSISTENT = 1,
    };

    struct RDGTextureDesc {
        PixelFormat       format           = PixelFormat::RGBA8_UNORM;
        Extent3D          extent           = {1, 1, 1};
        uint32_t          mipLevels        = 1;
        uint32_t          arrayLayers      = 1;
        SampleCount       samples          = SampleCount::X1;
        ImageUsageFlags   usage            = ImageUsageFlagBit::NONE;
        ResourceResidency residency        = ResourceResidency::TRANSIENT;
        bool              trackSubresource = false;   // v2 reserved
    };

    struct RDGBufferDesc {
        uint64_t         size      = 0;
        BufferUsageFlags usage     = BufferUsageFlagBit::NONE;
        ResourceResidency residency = ResourceResidency::TRANSIENT;
    };

    // Transient pool reuse statistics (debug / test observability).
    struct TransientPoolStats {
        uint32_t imageHits    = 0;
        uint32_t imageMisses  = 0;
        uint32_t bufferHits   = 0;
        uint32_t bufferMisses = 0;
    };

} // namespace sky::aurora
