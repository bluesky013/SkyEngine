#pragma once

#include <aurora/renderer/RendererTypes.h>
#include <core/name/Name.h>
#include <core/util/Uuid.h>
#include <cstdint>
#include <vector>

namespace sky::aurora {

    struct RenderResourceKey {
        Uuid     product;
        uint64_t revision                                    = 0;
        uint64_t usageVariant                                = 0;
        uint64_t deviceEpoch                                 = 0;
        bool     operator==(const RenderResourceKey &) const = default;
    };

    struct RenderResourceBinding {
        RenderResourceKey resource;
        uint64_t          bindingRevision = 0;
    };

    // Pointer-free owned bytes use the provider's format schema; retain until CPU readers/copies finish.
    struct RenderResourcePayload {
        RenderResourceKey    resource;
        Name                 format;
        std::vector<uint8_t> bytes;
    };

    enum class RenderResourceState : uint8_t { CPU_READY, UPLOAD_QUEUED, UPLOAD_SUBMITTED, RESIDENT, FAILED, CANCELLED };

    struct ResourceStateChanged {
        RenderResourceKey   resource;
        RenderResourceState state            = RenderResourceState::CPU_READY;
        uint64_t            submissionSerial = 0;
        RendererError       error; // meaningful for FAILED
    };

} // namespace sky::aurora
