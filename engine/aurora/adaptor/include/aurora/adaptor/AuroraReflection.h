//
// Created on 2026/11/15.
//

#pragma once

namespace sky {
    class SerializationContext;
}
namespace sky::aurora {
    // Aurora.Render owns this definition after Aurora.Metadata has registered persistent schemas.
    void RegisterAuroraRuntimeTypes(SerializationContext *context);
} // namespace sky::aurora
