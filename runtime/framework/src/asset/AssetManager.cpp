//
// Created by blues on 2024/6/16.
//

#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetEvent.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/platform/PlatformBase.h>
#include <core/logger/Logger.h>
#include <core/archive/FileArchive.h>
#include <core/profile/Profiler.h>

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

    AssetPtr AssetManager::FindOrCreateAsset(const Uuid &uuid, const Name &type)
    {
        auto hIter = assetHandlers.find(type);
        if (hIter == assetHandlers.end()) {
            LOG_E(TAG, "Asset handler not registered asset %s, type %s", uuid.ToString().c_str(), type.GetStr());
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

    AssetPtr AssetManager::CreateAssetByHeader(const Uuid &uuid, const IStreamArchivePtr &archive)
    {
        // get asset type
        std::string type;
        archive->Load(type);

        // try to find again
        auto asset = FindOrCreateAsset(uuid, Name(type.c_str()));
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

        auto archive = file->ReadAsArchive();
        asset = CreateAssetByHeader(uuid, archive);
        if (!asset) {
            return {};
        }

        // avoid release dep asset
        std::vector<AssetPtr> holder;
        std::vector<AsyncTaskHandle> asyncTasks;
        holder.reserve(asset->dependencies.size());

        for (auto &dep : asset->dependencies) {
            auto depAsset = LoadAsset(dep);
            if (!depAsset) {
                return {};
            }

            holder.emplace_back(depAsset);
            asyncTasks.emplace_back(depAsset->asyncTask.first);
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
                SKY_PROFILE_NAME("LoadAsset")
                auto asset = FindAsset(uuid);
                SKY_ASSERT(asset)
                asset->depAssets.swap(deps);
                auto res = assetHandlers[asset->type]->Load(*archive, asset);
                asset->status.store(res ? AssetBase::Status::LOADED : AssetBase::Status::FAILED);

                AsseEvent::BroadCast(uuid, &IAssetEvent::OnAssetLoaded);
            }, asyncTasks.begin(), asyncTasks.end());

        return asset;
    }

    AssetProductBundle *AssetManager::GetBundle(const ProductBundleKey &target) const
    {
        if (bundles.empty()) {
            return nullptr;
        }

        if (target.empty()) {
            return bundles[0].get();
        }

        for (const auto &bundle : bundles) {
            if (bundle->GetKey() != target) {
                return bundle.get();
            }
        }
        return nullptr;
    }

    AssetPtr AssetManager::LoadAssetFromPath(const std::string &path)
    {
        auto src = AssetDataBase::Get()->FindAsset(path);
        return src ? LoadAsset(src->uuid) : AssetPtr{};
    }

    void AssetManager::SaveAsset(const AssetPtr &asset, const ProductBundleKey &target)
    {
        auto hIter = assetHandlers.find(asset->GetType());
        if (hIter == assetHandlers.end()) {
            return;
        }

        // flush load operation
        asset->BlockUntilLoaded();

        auto *pBundle = GetBundle(target);
        if (pBundle == nullptr) {
            return;
        }

        FilePtr file = pBundle->CreateOrOpenFile(asset->GetUuid());
        if (!file) {
            LOG_E(TAG, "Save Asset %s Failed. Can not create file.", asset->GetUuid().ToString().c_str());
            return;
        }

        auto archive = file->WriteAsArchive();

        auto type = asset->type.GetStr();
        archive->Save(static_cast<uint32_t>(type.size()));
        archive->SaveRaw(type.data(), type.size());
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
        assetHandlers[Name(type.data())].reset(handler);
    }

} // namespace sky