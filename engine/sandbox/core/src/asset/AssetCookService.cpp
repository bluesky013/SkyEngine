//
// Cook orchestration split out of EditorAssetCatalog (see AssetCookService.h).
//

#include <editor/core/asset/AssetCookService.h>

#include <editor/core/asset/EditorAssetCatalog.h>

#include <framework/asset/AssetBuilderManager.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/CookConfig.h>

namespace sky::editor {

    void AssetCookService::TriggerCook(const Uuid &uuid)
    {
        auto *db = sky::AssetDataBase::Get();
        if (db == nullptr) {
            return;
        }
        auto source = db->FindAsset(uuid);
        if (!source) {
            return;
        }

        const auto &cookCfg = sky::AssetBuilderManager::Get()->GetCookConfig();
        const auto  targets = cookCfg.GetTargets(db->GetCookJson(uuid), cookCfg.GetActivePlatform());

        EditorAssetCatalog::Get()->BeginCook(uuid, targets);
        db->BuildAllTargets(uuid);
    }

    void AssetCookService::TriggerCookAll()
    {
        auto *db = sky::AssetDataBase::Get();
        if (db == nullptr) {
            return;
        }
        auto *builders = sky::AssetBuilderManager::Get();
        db->ForEachSource([&](const sky::AssetSourcePtr &source) {
            if (source != nullptr && builders->HasBuilder(source->ext)) {
                TriggerCook(source->uuid);
            }
        });
    }

} // namespace sky::editor
