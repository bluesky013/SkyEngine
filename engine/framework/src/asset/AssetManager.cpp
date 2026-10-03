//
// Created by blues on 2024/6/16.
//

#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetEvent.h>
#include <framework/asset/ICookRunner.h>
#include <framework/asset/ISourceCatalog.h>
#include <framework/asset/AssetBuilderManager.h>
#include <framework/asset/AssetIndexFile.h>
#include <framework/asset/AssetDependencyProvider.h>
#include <framework/platform/PlatformBase.h>
#include <core/logger/Logger.h>
#include <core/archive/FileArchive.h>
#include <core/profile/Profiler.h>
#include <core/archive/MemoryStreamArchive.h>

static const char* TAG = "AssetManager";

namespace sky {

    namespace {

        const char *CodecName(CompressionMethod method)
        {
            switch (method) {
            case CompressionMethod::LZ4: return "lz4";
            case CompressionMethod::ZLIB: return "zlib";
            }
            return "";
        }

        bool CodecFromName(const std::string &name, CompressionMethod &out)
        {
            if (name == "lz4") {
                out = CompressionMethod::LZ4;
                return true;
            }
            if (name == "zlib") {
                out = CompressionMethod::ZLIB;
                return true;
            }
            return false;
        }

    } // namespace
    void AssetManager::SetWorkFileSystem(const FileSystemPtr &fs)
    {
        workSpace = fs;
    }

    void AssetManager::SetCookRunner(ICookRunner *runner)
    {
        cookRunner = runner;
        if (cookRunner != nullptr) {
            cookRunner->SetCompletion([this](const AssetBuildResult &result) {
                OnCookFinished(result);
            });
        }
    }

    void AssetManager::RefreshProductIndex()
    {
        std::lock_guard<std::recursive_mutex> lock(mutex);

        productPathMap.clear();
        for (auto &bundle : bundles) {
            const auto &bundleFs = bundle->GetFileSystem();
            productIndices.Invalidate(bundleFs, FilePath{});
            const auto &index = productIndices.Get(bundleFs, FilePath{});
            for (const auto &entry : index.Entries()) {
                productPathMap[entry.key] = entry.id;
            }
        }
    }

    void AssetManager::OnCookFinished(const AssetBuildResult &result)
    {
        std::shared_ptr<std::promise<void>> promise;
        {
            std::lock_guard<std::recursive_mutex> lock(mutex);
            const auto iter = pendingJobs.find(result.uuid);
            if (iter == pendingJobs.end()) {
                return; // unknown or duplicate completion
            }
            promise = iter->second.promise;
            pendingJobs.erase(iter);
            pendingCooks.erase(result.uuid);
        }

        const bool produced = OpenFile(result.uuid) != nullptr;
        if (produced) {
            RefreshProductIndex();
            DeserializeProduct(result.uuid);
        } else if (auto asset = FindAsset(result.uuid); asset) {
            asset->status.store(AssetBase::Status::FAILED);
        }

        if (promise) {
            promise->set_value();
        }
    }

    void AssetManager::AddAssetProductBundle(AssetProductBundle *bundle)
    {
        bundles.emplace_back(bundle);

        const auto &index = productIndices.Get(bundle->GetFileSystem(), FilePath{});

        std::lock_guard<std::recursive_mutex> lock(mutex);
        for (const auto &entry : index.Entries()) {
            productPathMap[entry.key] = entry.id;
        }
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
            LOG_E(TAG, "Asset handler not registered asset %s", uuid.ToString().c_str());
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

    AssetPtr AssetManager::CreateAssetByHeader(const Uuid &uuid, const IStreamArchivePtr &archive, std::string &codec)
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

            archive->Load(codec);
        }
        return asset;
    }

    IStreamArchivePtr AssetManager::PreparePayload(const IStreamArchivePtr &archive, const std::string &codec) const
    {
        if (codec.empty()) {
            return archive;
        }

        CompressionMethod method;
        if (!CodecFromName(codec, method)) {
            LOG_E(TAG, "Unknown product codec %s", codec.c_str());
            return {};
        }

        auto *compressor = CompressionManager::Get()->GetCompressor(method);
        if (compressor == nullptr) {
            LOG_E(TAG, "No compressor registered for codec %s", codec.c_str());
            return {};
        }

        uint32_t uncompressedSize = 0;
        archive->Load(uncompressedSize);
        uint32_t compressedSize = 0;
        archive->Load(compressedSize);

        std::vector<uint8_t> compressed(compressedSize);
        archive->LoadRaw(reinterpret_cast<char *>(compressed.data()), compressedSize);

        BinaryDataPtr decompressed = new BinaryData(uncompressedSize);
        auto res = compressor->DeCompress(
            {compressed.data(), compressed.size()},
            {decompressed->Data(), decompressed->Size()}, 0);
        if (!res.first) {
            LOG_E(TAG, "Product decompression failed");
            return {};
        }

        return new IMemoryArchive(decompressed);
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
            return LoadAssetOnDemand(uuid);
        }

        auto  bin  = file->ReadBin();
        IStreamArchivePtr archive = new IMemoryArchive(bin);

        std::string codec;
        asset = CreateAssetByHeader(uuid, archive, codec);
        if (!asset) {
            return {};
        }

        auto payload = PreparePayload(archive, codec);
        if (!payload) {
            return {};
        }


        // avoid release dep asset
        std::vector<AssetPtr> holder;
        std::vector<TaskNodePtr> asyncTasks;
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
        asset->asyncTask = AssetExecutor::Get()->DependentAsync(
            [this, uuid, payload, deps = std::move(holder)]() mutable {
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

                if (!asset)
                {
                    LOG_E(TAG, "Asset %s not found while loading. Maybe deleted?", uuid.ToString().c_str());
                }

                SKY_ASSERT(asset)
                asset->depAssets.swap(deps);
                LoadInto(asset, payload);
            }, asyncTasks);

        return asset;
    }

    AssetPtr AssetManager::LoadAssetOnDemand(const Uuid &uuid)
    {
        if (sourceCatalog == nullptr || !sourceCatalog->Exists(uuid)) {
            LOG_E(TAG, "Asset file missing %s", uuid.ToString().c_str());
            return {};
        }

        std::string type;
        if (!sourceCatalog->GetType(uuid, type)) {
            LOG_E(TAG, "No handler type for asset %s", uuid.ToString().c_str());
            return {};
        }

        // Create the LOADING asset first so a coalescing caller always gets a valid handle.
        auto loading = FindOrCreateAsset(uuid, Name(type.c_str()));
        if (!loading) {
            return {};
        }

        // Coalesce concurrent on-demand cooks for the same asset.
        {
            std::lock_guard<std::recursive_mutex> lock(mutex);
            if (!pendingCooks.insert(uuid).second) {
                return loading;
            }
        }

        std::string target;
        sourceCatalog->GetTarget(uuid, target);

        loading->status.store(AssetBase::Status::LOADING);

        // Establish the wait handle before scheduling the cook so BlockUntilLoaded unblocks on completion.
        auto promise = std::make_shared<std::promise<void>>();
        loading->asyncTask.second = promise->get_future();

        if (cookRunner != nullptr) {
            // Mode-agnostic path (D5): the runner raises the build-finished event and calls
            // OnCookFinished on completion, which refreshes the index and resolves the load.
            std::string sourcePath;
            sourceCatalog->GetSourcePath(uuid, sourcePath);
            {
                std::lock_guard<std::recursive_mutex> lock(mutex);
                pendingJobs[uuid] = PendingCook{target, sourcePath, promise};
            }
            cookRunner->Request(CookJob{uuid, target, sourcePath});
        } else {
            // Built-in inline in-process path (default; unchanged behavior).
            AssetExecutor::Get()->SubmitCook([this, uuid, target, promise]() {
                if (auto *builderManager = AssetBuilderManager::Get(); builderManager != nullptr) {
                    builderManager->BuildRequestSync(uuid, target);
                }

                const bool produced = OpenFile(uuid) != nullptr;
                {
                    std::lock_guard<std::recursive_mutex> lock(mutex);
                    pendingCooks.erase(uuid);
                }

                if (produced) {
                    DeserializeProduct(uuid);
                } else if (auto asset = FindAsset(uuid); asset) {
                    asset->status.store(AssetBase::Status::FAILED);
                }

                promise->set_value();
            });
        }

        return loading;
    }

    bool AssetManager::LoadInto(const AssetPtr &asset, const IStreamArchivePtr &payload)
    {
        asset->status.store(AssetBase::Status::LOADING);
        auto res = assetHandlers[asset->type]->Load(*payload, asset);
        asset->status.store(res ? AssetBase::Status::LOADED : AssetBase::Status::FAILED);
        AsseEvent::BroadCast(asset->uuid, &IAssetEvent::OnAssetLoaded);
        return res;
    }

    void AssetManager::DeserializeProduct(const Uuid &uuid)
    {
        // Deserialize from the freshly produced product without touching asyncTask
        // (a caller may be waiting on the on-demand wait handle).
        auto file = OpenFile(uuid);
        if (!file) {
            return;
        }

        IStreamArchivePtr archive = new IMemoryArchive(file->ReadBin());

        std::string codec;
        auto asset = CreateAssetByHeader(uuid, archive, codec);
        if (!asset) {
            return;
        }

        auto payload = PreparePayload(archive, codec);
        if (!payload) {
            asset->status.store(AssetBase::Status::FAILED);
            return;
        }

        std::vector<AssetPtr> deps;
        deps.reserve(asset->dependencies.size());
        for (auto &dep : asset->dependencies) {
            auto depAsset = LoadAsset(dep);
            if (!depAsset) {
                asset->status.store(AssetBase::Status::FAILED);
                return;
            }
            depAsset->BlockUntilLoaded();
            deps.emplace_back(depAsset);
        }

        asset->depAssets.swap(deps);
        LoadInto(asset, payload);
    }

    AssetProductBundle *AssetManager::GetBundle(const ProductBundleKey &target) const    {
        if (bundles.empty()) {
            return nullptr;
        }

        if (target.empty()) {
            return bundles[0].get();
        }

        for (const auto &bundle : bundles) {
            if (bundle->GetKey() == target) {
                return bundle.get();
            }
        }
        return nullptr;
    }

    AssetPtr AssetManager::LoadAssetFromPath(const std::string &path)
    {
        Uuid uuid;
        const auto canonical = MakeCanonicalPath(path);
        {
            std::lock_guard<std::recursive_mutex> lock(mutex);
            auto iter = productPathMap.find(canonical);
            if (iter != productPathMap.end()) {
                uuid = iter->second;
            }
        }

        if (!uuid) {
            // Level 2: editor source-catalog fallback for not-yet-cooked sources (empty at runtime).
            if (sourceCatalog != nullptr) {
                Uuid resolved;
                if (sourceCatalog->ResolvePath(path, resolved)) {
                    uuid = resolved;
                }
            }
        }

        return uuid ? LoadAsset(uuid) : AssetPtr{};
    }

    void AssetManager::SaveAsset(const AssetPtr &asset, const ProductBundleKey &target)
    {
        auto hIter = assetHandlers.find(asset->GetType());
        if (hIter == assetHandlers.end()) {
            return;
        }

        // Flush an in-flight load task; the on-demand placeholder has no task node, so skip it
        // (waiting on it would deadlock the cooking task itself).
        if (asset->asyncTask.first != nullptr) {
            asset->BlockUntilLoaded();
        }

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

        auto *compressor = compressProducts ? CompressionManager::Get()->GetCompressor(compressionMethod) : nullptr;
        std::string codec = compressor != nullptr ? CodecName(compressionMethod) : std::string{};
        archive->Save(static_cast<uint32_t>(codec.size()));
        archive->SaveRaw(codec.data(), codec.size());

        asset->status.store(AssetBase::Status::LOADED);
        if (compressor == nullptr) {
            hIter->second->Save(*archive, asset);
        } else {
            OMemoryArchive payload;
            hIter->second->Save(payload, asset);

            uint32_t bound = compressor->CompressBound(static_cast<uint32_t>(payload.Size()));
            std::vector<uint8_t> compressed(bound);
            auto res = compressor->Compress(
                {reinterpret_cast<const uint8_t *>(payload.Data()), payload.Size()},
                {compressed.data(), compressed.size()}, 0);
            if (res.first) {
                archive->Save(static_cast<uint32_t>(payload.Size()));
                archive->Save(res.second);
                archive->SaveRaw(reinterpret_cast<const char *>(compressed.data()), res.second);
            } else {
                LOG_E(TAG, "Product compression failed for %s", asset->GetUuid().ToString().c_str());
            }
        }

        // Update the bundle's product index (path -> uuid) so a later path load resolves.
        // Serialized per AssetManager so concurrent cooks do not lose entries (read-modify-write).
        std::string sourcePath;
        if (sourceCatalog != nullptr && sourceCatalog->GetSourcePath(asset->GetUuid(), sourcePath)) {
            std::lock_guard<std::recursive_mutex> lock(mutex);

            auto bundleFs = pBundle->GetFileSystem();
            auto index = productIndices.Get(bundleFs, FilePath{});

            IndexFileEntry entry;
            entry.key = MakeCanonicalPath(sourcePath);
            entry.id = asset->GetUuid();
            index.Set(entry);
            productIndices.Save(bundleFs, FilePath{}, index);

            productPathMap[entry.key] = entry.id;
        }
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

    void AssetManager::BuildDependencyGraph(AssetDependencyGraph &graph) const
    {
        graph.Clear();

        std::lock_guard<std::recursive_mutex> lock(mutex);
        for (const auto &[id, weak] : assets) {
            if (auto asset = weak.lock()) {
                graph.Add(id, asset->dependencies);
            }
        }
    }

    void AssetManager::RegisterAssetHandler(const std::string_view &type, AssetHandlerBase *handler)
    {
        assetHandlers[Name(type.data())].reset(handler);
    }

} // namespace sky
