//
// Cook orchestration split out of EditorAssetCatalog: resolve an asset's targets, mark them Cooking, and
// trigger the framework cook. Process-wide (cross-DLL) service.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/util/Uuid.h>

namespace sky::editor {

    class AssetCookService : public sky::Singleton<AssetCookService> {
        friend class sky::Singleton<AssetCookService>;

    public:
        // Cook one asset across its resolved targets (asset override, else project targets, else the active
        // platform's preset bundles). Marks it Cooking so the tree/detail views update immediately.
        void TriggerCook(const Uuid &uuid);
    };

} // namespace sky::editor
