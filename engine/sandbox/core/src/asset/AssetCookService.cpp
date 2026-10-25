//
// Cook orchestration split out of EditorAssetCatalog (see AssetCookService.h).
//

#include <editor/core/asset/AssetCookService.h>

#include <editor/core/asset/EditorAssetCatalog.h>

#include <framework/asset/AssetBuilderManager.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/CookConfig.h>

namespace sky::editor {

    void AssetCookService::RequestCook(const Uuid &uuid)
    {
        std::lock_guard<std::mutex> lock(mutex);
        pending.insert(uuid);
    }

    void AssetCookService::RequestCookAll()
    {
        std::lock_guard<std::mutex> lock(mutex);
        cookAllPending = true;
    }

    void AssetCookService::Drain()
    {
        std::unordered_set<Uuid> batch;
        bool                     all = false;
        {
            std::lock_guard<std::mutex> lock(mutex);
            batch.swap(pending);
            all            = cookAllPending;
            cookAllPending = false;
        }

        for (const auto &uuid : batch) {
            CookOne(uuid);
        }
        if (all) {
            CookAll();
        }
    }

    void AssetCookService::CookOne(const Uuid &uuid)
    {
        auto *db = sky::AssetDataBase::Get();
        if (db == nullptr) {
            return;
        }
        Uuid owner;
        if (!db->GetOwner(uuid, owner))
            return;
        auto source = db->FindAsset(owner);
        if (!source) {
            return;
        }

        const auto &cookCfg = sky::AssetBuilderManager::Get()->GetCookConfig();
        const auto  targets = cookCfg.GetTargets(db->GetCookJson(owner), cookCfg.GetActivePlatform());

        EditorAssetCatalog::Get()->BeginCook(uuid, targets);
        db->BuildAllTargets(uuid);
    }

    void AssetCookService::CookAll()
    {
        auto *db = sky::AssetDataBase::Get();
        if (db == nullptr) {
            return;
        }
        auto *builders = sky::AssetBuilderManager::Get();
        db->ForEachSource([&](const sky::AssetSourcePtr &source) {
            if (source != nullptr && builders->HasBuilder(source->ext)) {
                CookOne(source->uuid);
            }
        });
    }

} // namespace sky::editor
