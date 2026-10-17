#pragma once

#include <common/FrameContext.h>

namespace sky::aurora {
    class MetalDevice;

    class MetalDeviceFrameContext final : public BackendFrameContext {
    public:
        explicit MetalDeviceFrameContext(MetalDevice *) {}
    };

} // namespace sky::aurora
