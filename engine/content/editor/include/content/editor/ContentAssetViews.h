//
// Content editor asset-view data: registers the animation asset view structs +
// their providers (see AssetViewData). Read-only MVP.
//

#pragma once

namespace sky {
    class SerializationContext;
} // namespace sky

namespace sky::content::editor {

    // Idempotent: registers the content view structs + AssetViewDataRegistry providers.
    void RegisterContentAssetViews(sky::SerializationContext *context);

} // namespace sky::content::editor
