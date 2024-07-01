//
// Created by blues on 2024/6/16.
//

#include <framework/asset/AssetManager.h>
#include <framework/platform/PlatformBase.h>
#include <core/logger/Logger.h>
#include <core/archive/FileArchive.h>

static const char* TAG = "AssetManager";

namespace sky {
    void AssetManager::SetWorkFileSystem(const FileSystemPtr &fs)
    {
        workSpace = fs;
    }

    void AssetManager::AddAssetProductBundle(AssetProductBundle *bundle)
    {
        bundles.emplace_back(bundle);
    }

    AssetPtr AssetManager::FindAsset(const Uuid &uuid) const
    {
        // check asset exists
        std::lock_guard<std::recursive_mutex> lock(mutex);
        auto iter = assets.find(uuid);
        if (iter != assets.end()) {
            if (auto res = iter->second.lock(); res) {
                return res;
            }
        }
        return {};
    }

    AssetPtr AssetManager::FindOrCreateAsset(const Uuid &uuid, const std::string &type)
    {
        auto hIter = assetHandlers.find(type);
        if (hIter == assetHandlers.end()) {
            LOG_E(TAG, "Asset handler not registered asset %s, type %s", uuid.ToString().c_str(), type.c_str());
            return {};
        }

        std::shared_ptr<AssetBase> asset;
        {
            std::lock_guard<std::recursive_mutex> lock(mutex);
            auto &ref = assets[uuid];
            if (auto res = ref.lock(); res) {
                return res;
            }
            ref = asset = hIter->second->CreateAsset();
        }
        asset->SetUuid(uuid);
        asset->SetType(type);
        return asset;
    }

    AssetPtr AssetManager::CreateAssetByHeader(const Uuid &uuid, const IArchivePtr &archive)
    {
        // get asset type
        std::string type;
        archive->Load(type);

        // try to find again
        auto asset = FindOrCreateAsset(uuid, type);
        if (asset) {
            uint32_t depCount = 0;
            archive->Load(depCount);

            asset->dependencies.resize(depCount);
            for (uint32_t i = 0; i < depCount; ++i) {
                auto &dep = asset->dependencies[i];
                archive->Load(dep.word[0]);
                archive->Load(dep.word[1]);
            }
        }
        return asset;
    }

    AssetPtr AssetManager::LoadAsset(const Uuid &uuid) // NOLINT
    {
        auto asset = FindAsset(uuid);

        // check loaded
        if (asset && asset->IsLoaded()) {
            return asset;
        }

        auto file = OpenFile(uuid);
        if (!file) {
            LOG_E(TAG, "Asset file missing %s", uuid.ToString().c_str());
            return {};
        }

        IArchivePtr archive = file->ReadAsArchive();
        asset = CreateAssetByHeader(uuid, archive);
        if (!asset) {
            return {};
        }

        // avoid release dep asset
        std::vector<AssetPtr> holder;
        std::vector<tf::AsyncTask> asyncTasks;
        holder.reserve(asset->dependencies.size());

        for (auto &dep : asset->dependencies) {
            holder.emplace_back(LoadAsset(dep));
            asyncTasks.emplace_back(holder.back()->asyncTask.first);
        }

        asset->status.store(AssetBase::Status::LOADING);
        asset->asyncTask = AssetExecutor::Get()->DependentAsyncRange(
            [this, uuid, archive, deps = std::move(holder)]() mutable {
                bool success = true;
                for (const auto &dep : deps) {
                    SKY_ASSERT(dep->status.load() >= AssetBase::Status::LOADED)
                    success &= dep->IsLoaded();
                }

                if (!success) {
                    return;
                }

                auto asset = FindAsset(uuid);
                SKY_ASSERT(asset)
                asset->depAssets.swap(deps);
                auto res = assetHandlers[asset->type]->Load(*archive, asset);
                asset->status.store(res ? AssetBase::Status::LOADED : AssetBase::Status::FAILED);
            }, asyncTasks.begin(), asyncTasks.end());

        return asset;
    }

    void AssetManager::SaveAsset(const AssetPtr &asset, const ProductBundleKey &target)
    {
        auto hIter = assetHandlers.find(asset->GetType());
        if (hIter == assetHandlers.end()) {
            return;
        }

        // flush load operation
        asset->BlockUntilLoaded();

        FilePtr file;
        for (const auto &bundle : bundles) {
            if (bundle->GetKey() != target || bundle->IsPacked()) {
                continue;
            }
            file = bundle->CreateOrOpenFile(asset->GetUuid());
            if (file) {
                break;
            }
        }
        if (!file) {
            LOG_E(TAG, "Save Asset %s Failed. Can not create file.", asset->GetUuid().ToString().c_str());
            return;
        }

        auto archive = file->WriteAsArchive();

        archive->Save(asset->type);
        archive->Save(static_cast<uint32_t>(asset->dependencies.size()));
        for (auto &dep : asset->dependencies) {
            archive->Save(dep.word[0]);
            archive->Save(dep.word[1]);
        }

        asset->status.store(AssetBase::Status::LOADED);
        hIter->second->Save(*archive, asset);
    }

    FilePtr AssetManager::OpenFile(const Uuid &uuid) const
    {
        for (const auto &bundle : bundles) {
            if (auto file = bundle->OpenFile(uuid); file) {
                return file;
            }
        }
        return {};
    }

    void AssetManager::RegisterAssetHandler(const std::string_view &type, AssetHandlerBase *handler)
    {
        assetHandlers[type.data()].reset(handler);
    }

} // namespace sky