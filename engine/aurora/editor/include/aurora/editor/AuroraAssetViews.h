//
// Aurora editor asset-view data (see asset-viewer-widget / AssetViewData). Registers
// reflected per-asset view structs + their providers for the aurora asset types.
// Read-only MVP: fields render as a reflected form; source write-back/re-cook is later.
//

#pragma once
#include <memory>

namespace sky {
    class SerializationContext;
} // namespace sky

namespace sky::aurora::editor {

    void ReflectAuroraAssetViews(sky::SerializationContext *context);
    void RegisterAuroraAssetViews(std::shared_ptr<const void> lease = {});
    void UnregisterAuroraAssetViews();

} // namespace sky::aurora::editor
