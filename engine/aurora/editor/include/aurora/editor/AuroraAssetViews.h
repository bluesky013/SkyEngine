//
// Aurora editor asset-view data (see asset-viewer-widget / AssetViewData). Registers
// reflected per-asset view structs + their providers for the aurora asset types.
// Read-only MVP: fields render as a reflected form; source write-back/re-cook is later.
//

#pragma once

namespace sky {
    class SerializationContext;
} // namespace sky

namespace sky::aurora::editor {

    // Idempotent: registers the aurora view structs + AssetViewDataRegistry providers.
    void RegisterAuroraAssetViews(sky::SerializationContext *context);

} // namespace sky::aurora::editor
