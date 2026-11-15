//
// Created on 2026/11/15.
//

#pragma once

namespace sky {
    class SerializationContext;
}
namespace sky::aurora {
    // Owned by the Aurora.Metadata process definition; runtime and cook only consume it.
    void RegisterAuroraAssetTypes(SerializationContext *context);
} // namespace sky::aurora
