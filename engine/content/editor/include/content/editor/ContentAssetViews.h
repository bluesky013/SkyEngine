//
// Content editor asset-view data: registers the animation asset view structs +
// their providers (see AssetViewData). Read-only MVP.
//

#pragma once

namespace sky {
    class SerializationContext;
} // namespace sky

namespace sky::content::editor {

    void ReflectContentAssetViews(sky::SerializationContext *context);
    void RegisterContentAssetViews();
    void UnregisterContentAssetViews();

} // namespace sky::content::editor
