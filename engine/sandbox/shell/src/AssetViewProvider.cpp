//
// Asset view flag derivation (see AssetViewProvider.h).
//

#include <editor/shell/AssetViewProvider.h>

#include <editor/core/asset/AssetViewData.h>
#include <framework/asset/AssetBuilderManager.h>

namespace sky::editor {

    AssetViewFlags DeriveAssetViewFlags(const std::string &type, const std::string &ext)
    {
        if (type.empty()) {
            // Unregistered / unknown type: default to the cook-settings fallback (BUNDLE).
            return AssetViewFlags::BUNDLE;
        }

        AssetViewFlags flags = AssetViewFlags::NONE;

        auto *builders = sky::AssetBuilderManager::Get();
        if (builders != nullptr && !ext.empty() && builders->GetBuilderSettingsType(ext) != nullptr) {
            flags = flags | AssetViewFlags::BUNDLE;
        }
        if (AssetViewProviderRegistry::Get()->Find(type) != nullptr || AssetViewDataRegistry::Get()->Has(type)) {
            flags = flags | AssetViewFlags::DATA;
        }
        return flags; // NONE -> HIDDEN
    }

} // namespace sky::editor
