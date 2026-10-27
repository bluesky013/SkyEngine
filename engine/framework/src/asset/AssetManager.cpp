//
// Created by blues on 2024/6/16.
//

#include <core/archive/FileArchive.h>
#include <core/archive/MemoryStreamArchive.h>
#include <core/logger/Logger.h>
#include <core/profile/Profiler.h>
#include <core/thread/ThreadDispatcher.h>
#include <framework/asset/AssetBuilderManager.h>
#include <framework/asset/AssetDependencyProvider.h>
#include <framework/asset/AssetEvent.h>
#include <framework/asset/AssetIndexFile.h>
#include <framework/asset/AssetManager.h>
#include <framework/asset/ICookRunner.h>
#include <framework/asset/ISourceCatalog.h>
#include <framework/platform/PlatformBase.h>
#include <framework/serialization/SerializationContext.h>

static const char *TAG = "AssetManager";

namespace sky {

    namespace {

        bool RemainingBytes(const IStreamArchivePtr &archive, uint64_t &size)
        {
            if (!archive) {
                return false;
            }
            auto      &stream = archive->GetStream();
            const auto start  = stream.tellg();
            stream.seekg(0, std::ios::end);
            const auto end = stream.tellg();
            stream.seekg(start);
            if (start < 0 || end < start || !stream) {
                return false;
            }
            size = static_cast<uint64_t>(end - start);
            return true;
        }

        bool ReadHeaderString(IStreamArchive &archive, std::string &out, uint32_t limit)
        {
            uint32_t size = 0;
            if (!archive.Load(size) || size > limit) {
                return false;
            }
            out.resize(size);
            return archive.LoadRaw(out.data(), size) && out.find('\0') == std::string::npos;
        }

        bool ReadProductHeader(const Uuid &uuid, const IStreamArchivePtr &archive, AssetProductInspection &out, std::string &codec)
        {
            uint32_t count = 0;
            if (!archive || !ReadHeaderString(*archive, out.type, 4096) || out.type.empty() || !archive->Load(count) || count > 4096)
                return false;
            out.dependencies.resize(count);
            for (auto &dependency : out.dependencies)
                if (!archive->Load(dependency.word[0]) || !archive->Load(dependency.word[1]) || !dependency || dependency == uuid)
                    return false;
            return ReadHeaderString(*archive, codec, 16);
        }

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
            cookRunner->SetCompletion([this](const AssetBuildResult &result) { OnCookFinished(result); });
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
        if (result.retCode == AssetBuildRetCode::SUCCESS) {
            productChangeSerial.fetch_add(1, std::memory_order_release);
        }
        std::lock_guard<std::recursive_mutex> loadLock(loadMutex);
        std::vector<Uuid>                     members;
        {
            std::lock_guard<std::recursive_mutex> lock(mutex);
            const auto                            pending = pendingOwners.find(result.uuid.ToString() + "#" + result.target);
            if (pending == pendingOwners.end() || pending->second != result.requestId) {
                return;
            }
            pendingOwners.erase(pending);
            for (const auto &[id, job] : pendingJobs) {
                if (job.owner == result.uuid && job.targets.contains(result.target)) {
                    members.push_back(id);
                }
            }
        }
        if (sourceCatalog) {
            sourceCatalog->RefreshOwner(result.uuid);
        }
        for (const auto &member : members) {
            auto completed = result;
            completed.uuid = member;
            CompleteCookMember(completed);
        }
    }

    void AssetManager::CompleteCookMember(const AssetBuildResult &result)
    {
        const bool produced = (!sourceCatalog || sourceCatalog->Exists(result.uuid)) && OpenFile(result.uuid) != nullptr;
        if (produced) {
            RefreshProductIndex();
        }

        TaskNodePtr completion;
        AssetPtr    placeholder;
        Uuid        owner;
        {
            std::lock_guard<std::recursive_mutex> lock(mutex);
            const auto                            iter = pendingJobs.find(result.uuid);
            if (iter == pendingJobs.end()) {
                return; // no pending on-demand load (e.g. an explicit editor cook) or duplicate completion
            }
            if (iter->second.requestId != result.requestId || !iter->second.targets.erase(result.target)) {
                return;
            }
            // Resolve once any target produced a product, or when every requested target has finished.
            if (!(produced || iter->second.targets.empty())) {
                return; // more targets for this asset are still cooking
            }
            owner       = iter->second.owner;
            completion  = iter->second.completion;
            placeholder = iter->second.placeholder;
            // Keep the placeholder discoverable until its load task owns completion.
        }

        const bool scheduled = produced && DeserializeProduct(result.uuid, placeholder, completion);
        if (!scheduled) {
            placeholder->status.store(AssetBase::Status::FAILED);
            ThreadDispatcher::Get()->PostToMain([id = result.uuid]() { AsseEvent::BroadCast(id, &IAssetEvent::OnAssetLoaded); });
        } else {
            completion = nullptr;
        }

        // The original barrier remains attached while deserialization finishes.
        {
            std::lock_guard<std::recursive_mutex> lock(mutex);
            pendingJobs.erase(result.uuid);
        }
        if (completion) {
            AssetExecutor::Get()->CompleteLoadBarrier(completion);
        }
        if (owner != result.uuid) {
            auto childResult = result;
            if (!scheduled) {
                childResult.retCode = AssetBuildRetCode::FAILED;
                childResult.error   = "owner cook did not produce a readable child product";
            }
            ThreadDispatcher::Get()->PostToMain(
                [childResult]() { AsseEvent::BroadCast(childResult.uuid, &IAssetEvent::OnAssetBuildFinished, childResult); });
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
        auto                                  iter = assets.find(uuid);
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
            auto                                 &ref = assets[uuid];
            if (auto res = ref.lock(); res) {
                return res->GetType() == type ? res : AssetPtr{};
            }
            ref = asset = hIter->second->CreateAsset();
        }
        asset->SetUuid(uuid);
        asset->SetType(type);
        return asset;
    }

    AssetPtr AssetManager::CreateAssetByHeader(const Uuid &uuid, const IStreamArchivePtr &archive, std::string &codec, const AssetPtr &destination)
    {
        AssetProductInspection header;
        if (!ReadProductHeader(uuid, archive, header, codec)) {
            return {};
        }
        auto asset = destination ? destination : FindOrCreateAsset(uuid, Name(header.type.c_str()));
        if (asset && asset->GetType() != Name(header.type.c_str())) {
            return {};
        }
        if (asset) {
            asset->dependencies = std::move(header.dependencies);
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

        uint32_t uncompressedSize = 0, compressedSize = 0;
        uint64_t remaining = 0;
        if (!archive->Load(uncompressedSize) || !archive->Load(compressedSize) || !RemainingBytes(archive, remaining) ||
            compressedSize != remaining || compressedSize > 256U * 1024U * 1024U || uncompressedSize == 0 ||
            uncompressedSize > 256U * 1024U * 1024U) {
            return {};
        }
        std::vector<uint8_t> compressed(compressedSize);
        if (!archive->LoadRaw(reinterpret_cast<char *>(compressed.data()), compressedSize)) {
            return {};
        }
        BinaryDataPtr decompressed = new BinaryData(uncompressedSize);
        const auto    res          = compressor->DeCompress({compressed.data(), compressed.size()}, {decompressed->Data(), decompressed->Size()}, 0);
        if (!res.first || res.second != uncompressedSize) {
            LOG_E(TAG, "Product decompression failed or returned the wrong size");
            return {};
        }

        return new IMemoryArchive(decompressed);
    }

    IStreamArchivePtr AssetManager::ReadProductPayload(const IStreamArchivePtr &stream,
                                                       const std::string       &codec,
                                                       const AssetPtr          &asset,
                                                       uint64_t                *declaredBulkSize) const
    {
        uint32_t headerSize = 0;
        uint64_t remaining  = 0;
        if (!stream->Load(headerSize) || headerSize > 256U * 1024U * 1024U || !RemainingBytes(stream, remaining) || remaining < sizeof(uint64_t) ||
            headerSize > remaining - sizeof(uint64_t)) {
            return {};
        }
        BinaryDataPtr headerData = new BinaryData(headerSize);
        if (!stream->LoadRaw(reinterpret_cast<char *>(headerData->Data()), headerSize)) {
            return {};
        }
        const auto payload  = PreparePayload(new IMemoryArchive(headerData), codec);
        uint64_t   bulkSize = 0;
        if (!payload || !stream->Load(bulkSize) || bulkSize != remaining - headerSize - sizeof(uint64_t)) {
            return {};
        }
        if (asset) {
            asset->bulkStart     = stream->Tell();
            asset->productStream = bulkSize ? stream : IStreamArchivePtr{};
        }
        if (declaredBulkSize)
            *declaredBulkSize = bulkSize;
        return payload;
    }

    ProductStatus AssetManager::InspectProduct(const Uuid &uuid, const ProductBundleKey &bundle, AssetProductInspection &out) const
    {
        const auto *source = GetBundle(bundle);
        if (!uuid || !source)
            return {ProductError::NotFound, "product bundle unavailable"};
        AssetProductInspection       inspected;
        FilePtr                      file;
        const AssetProductGroupStore store(source->GetFileSystem());
        if (store.HasOwnerRecord(uuid) && !store.HasLegacyFallback(uuid)) {
            if (const auto status = source->ResolveGroup(uuid, inspected.group); !status)
                return status;
            if (const auto status = source->OpenPinned(*inspected.group, uuid, file); !status)
                return status;
        } else
            file = source->OpenFile(uuid);
        if (!file)
            return {ProductError::NotFound, "product file unavailable"};
        const auto  stream = file->ReadAsArchive();
        std::string codec;
        if (!ReadProductHeader(uuid, stream, inspected, codec) || !(inspected.payload = ReadProductPayload(stream, codec, {}, &inspected.bulkSize)))
            return {ProductError::InvalidData, "invalid product header or payload"};
        out = std::move(inspected);
        return {};
    }

    std::shared_ptr<AssetLoadRequest> AssetManager::LoadAssetAsync(const Uuid &uuid, AssetLoadPolicy policy)
    {
        auto request = std::make_shared<AssetLoadRequest>();
        AssetExecutor::Get()->DependentAsync([this, request, uuid, policy] {
            request->asset = LoadAsset(uuid, policy);
            request->resolved.store(true, std::memory_order_release);
        });
        return request;
    }

    AssetPtr AssetManager::LoadAsset(const Uuid &uuid, AssetLoadPolicy policy) // NOLINT
    {
        std::lock_guard<std::recursive_mutex> loadLock(loadMutex);

        for (const auto &bundle : bundles) {
            if (AssetProductGroupStore(bundle->GetFileSystem()).HasOwnerRecord(uuid)) {
                ProductGroupSnapshotPtr group;
                auto snapshot = bundle->ResolveGroup(uuid, group) ? LoadAssetSnapshot(uuid, bundle->GetKey(), group, policy) : AssetPtr{};
                if (snapshot) {
                    std::lock_guard<std::recursive_mutex> lock(mutex);
                    assets[uuid] = snapshot;
                }
                if (!snapshot && AssetProductGroupStore(bundle->GetFileSystem()).HasLegacyFallback(uuid)) {
                    break;
                }
                if (snapshot) {
                    return snapshot;
                }
                return policy == AssetLoadPolicy::AllowCook && sourceCatalog && sourceCatalog->Exists(uuid) ? LoadAssetOnDemand(uuid) : AssetPtr{};
            }
        }

        GeneratedAssetRecord generated;
        if (policy == AssetLoadPolicy::AllowCook && sourceCatalog && sourceCatalog->GetGenerated(uuid, generated) && !generated.active) {
            return {};
        }

        auto asset = FindAsset(uuid);

        // Another caller may already be loading this asset; reuse its wait handle
        // instead of reading the product header concurrently.
        if (asset && (asset->IsLoaded() || asset->GetStatus() == AssetBase::Status::LOADING) &&
            (policy == AssetLoadPolicy::AllowCook || OpenFile(uuid))) {
            return asset;
        }

        auto file = OpenFile(uuid);
        if (!file) {
            return policy == AssetLoadPolicy::AllowCook ? LoadAssetOnDemand(uuid) : AssetPtr{};
        }

        IStreamArchivePtr stream = file->ReadAsArchive();

        std::string codec;
        asset = CreateAssetByHeader(uuid, stream, codec);
        if (!asset) {
            return {};
        }

        const auto payload = ReadProductPayload(stream, codec, asset);
        if (!payload) {
            asset->status.store(AssetBase::Status::FAILED);
            return {};
        }

        // avoid release dep asset
        std::vector<AssetPtr>    holder;
        std::vector<TaskNodePtr> asyncTasks;
        holder.reserve(asset->dependencies.size());

        for (auto &dep : asset->dependencies) {
            auto depAsset = LoadAsset(dep, policy);
            if (!depAsset) {
                return {};
            }

            holder.emplace_back(depAsset);
            asyncTasks.emplace_back(depAsset->GetAsyncNode());
        }

        asset->status.store(AssetBase::Status::LOADING);
        auto async = AssetExecutor::Get()->DependentAsync(
            [this, weakAsset = std::weak_ptr<AssetBase>(asset), payload, deps = std::move(holder)]() mutable {
                const auto asset = weakAsset.lock();
                if (!asset) {
                    return;
                }
                bool success = true;
                for (const auto &dep : deps) {
                    success &= dep->IsLoaded();
                }

                if (!success) {
                    asset->status.store(AssetBase::Status::FAILED);
                    return;
                }
                SKY_PROFILE_NAME("LoadAsset")
                asset->depAssets.swap(deps);
                LoadInto(asset, payload);
            },
            asyncTasks);
        asset->SetAsyncTask(async.first, async.second.share());

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

        std::string target;
        sourceCatalog->GetTarget(uuid, target);
        Uuid owner;
        if (!sourceCatalog->GetOwner(uuid, owner)) {
            return {};
        }
        bool schedule = false;
        Uuid requestId;

        // Establish the coalescing decision AND the wait handle atomically: a concurrent caller for an
        // in-flight load observes pendingJobs[uuid] and reuses the same handle; it never overwrites it.
        // Scheduling happens after the lock is released (both runners only enqueue).
        AssetPtr loading;
        {
            std::lock_guard<std::recursive_mutex> lock(mutex);

            loading = FindAsset(uuid);
            if (!loading || loading->GetStatus() == AssetBase::Status::FAILED) {
                loading      = CreateBuildAsset(uuid, Name(type.c_str()));
                assets[uuid] = loading;
            }
            if (!loading) {
                return {};
            }

            const bool inFlight    = pendingJobs.find(uuid) != pendingJobs.end();
            const bool alreadyDone = loading->GetStatus() == AssetBase::Status::LOADED;
            if (inFlight || alreadyDone) {
                return loading;
            }

            loading->status.store(AssetBase::Status::LOADING);

            // Establish the wait handle before scheduling the cook; set exactly once per load.
            auto completion = AssetExecutor::Get()->CreateLoadBarrier();
            loading->SetAsyncTask(completion.first, completion.second);
            const auto [pending, inserted] = pendingOwners.try_emplace(owner.ToString() + "#" + target, Uuid::Create());
            requestId                      = pending->second;
            schedule                       = inserted;
            pendingJobs[uuid]              = PendingCook{{target}, {}, completion.first, owner, loading, requestId};
        }

        // Both runners register the pending job above and resolve through OnCookFinished, so in-process
        // and out-of-process cooks behave identically (D5/D7).
        if (!schedule) {
            return loading;
        }
        if (cookRunner != nullptr) {
            std::string sourcePath;
            sourceCatalog->GetSourcePath(owner, sourcePath);
            if (!cookRunner->Request(CookJob{owner, target, sourcePath, requestId})) {
                OnCookFinished({owner, target, AssetBuildRetCode::FAILED, "cook runner rejected request", requestId});
            }
        } else {
            AssetBuilderManager::Get()->BuildRequest(owner, target, [this, requestId](const AssetBuildResult &value) {
                auto result      = value;
                result.requestId = requestId;
                OnCookFinished(result);
            });
        }

        return loading;
    }

    bool AssetManager::LoadInto(const AssetPtr &asset, const IStreamArchivePtr &payload)
    {
        asset->status.store(AssetBase::Status::LOADING);
        auto res = assetHandlers[asset->type]->Load(*payload, asset);
        asset->status.store(res ? AssetBase::Status::LOADED : AssetBase::Status::FAILED);
        // May run on the load worker; deliver OnAssetLoaded on the main role.
        const Uuid uuid = asset->uuid;
        ThreadDispatcher::Get()->PostToMain([uuid]() { AsseEvent::BroadCast(uuid, &IAssetEvent::OnAssetLoaded); });
        return res;
    }

    bool AssetManager::DeserializeProduct(const Uuid &uuid, const AssetPtr &destination, const TaskNodePtr &completion)
    {
        FilePtr                 file;
        ProductGroupSnapshotPtr group;
        ProductBundleKey        groupBundle;
        for (const auto &bundle : bundles) {
            const AssetProductGroupStore store(bundle->GetFileSystem());
            if (store.HasOwnerRecord(uuid) && !store.HasLegacyFallback(uuid)) {
                groupBundle = bundle->GetKey();
                if (!bundle->ResolveGroup(uuid, group) || !bundle->OpenPinned(*group, uuid, file)) {
                    return false;
                }
                break;
            }
            file = bundle->OpenFile(uuid);
            if (file) {
                break;
            }
        }
        if (!file) {
            return false;
        }

        IStreamArchivePtr stream = file->ReadAsArchive();

        std::string codec;
        auto        asset = CreateAssetByHeader(uuid, stream, codec, destination);
        if (!asset) {
            return false;
        }
        asset->productGroup = group;

        const auto payload = ReadProductPayload(stream, codec, asset);
        if (!payload) {
            return false;
        }

        std::vector<AssetPtr>    deps;
        std::vector<TaskNodePtr> tasks;
        deps.reserve(asset->dependencies.size());
        for (auto &dep : asset->dependencies) {
            if (dep == uuid) {
                return false;
            }
            const bool sameGroup =
                group && std::any_of(group->members.begin(), group->members.end(), [&dep](const auto &member) { return member.uuid == dep; });
            const auto depAsset = sameGroup ? LoadAssetSnapshot(dep, groupBundle, group) : LoadAsset(dep);
            if (!depAsset) {
                asset->status.store(AssetBase::Status::FAILED);
                return false;
            }
            tasks.push_back(depAsset->GetAsyncNode());
            deps.emplace_back(depAsset);
        }

        asset->depAssets.swap(deps);
        auto loaded = AssetExecutor::Get()->DependentAsync(
            [this, weak = std::weak_ptr<AssetBase>(asset), payload]() {
                const auto value = weak.lock();
                if (!value) {
                    return;
                }
                for (const auto &dependency : value->depAssets) {
                    if (!dependency->IsLoaded()) {
                        value->status.store(AssetBase::Status::FAILED);
                        return;
                    }
                }
                LoadInto(value, payload);
            },
            tasks);
        AssetExecutor::Get()->DependentAsync([completion]() { AssetExecutor::Get()->CompleteLoadBarrier(completion); }, {loaded.first});
        return true;
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
            if (bundle->GetKey() == target) {
                return bundle.get();
            }
        }
        return nullptr;
    }

    bool AssetManager::HasProduct(const Uuid &uuid, const ProductBundleKey &bundle) const
    {
        auto *productBundle = GetBundle(bundle);
        return productBundle != nullptr && productBundle->OpenFile(uuid) != nullptr;
    }

    AssetPtr AssetManager::LoadAssetFromPath(const std::string &path)
    {
        Uuid       uuid;
        const auto canonical = MakeCanonicalPath(path);
        {
            std::lock_guard<std::recursive_mutex> lock(mutex);
            auto                                  iter = productPathMap.find(canonical);
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

        bool pendingCook = false;
        {
            std::lock_guard<std::recursive_mutex> lock(mutex);
            pendingCook = pendingJobs.contains(asset->GetUuid());
        }
        // A cook must not wait for its own completion barrier.
        if (asset->GetAsyncNode() != nullptr && !pendingCook) {
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

        auto       *compressor = compressProducts ? CompressionManager::Get()->GetCompressor(compressionMethod) : nullptr;
        std::string codec      = compressor != nullptr ? CodecName(compressionMethod) : std::string{};
        archive->Save(static_cast<uint32_t>(codec.size()));
        archive->SaveRaw(codec.data(), codec.size());

        asset->status.store(AssetBase::Status::LOADED);

        // Header block = the handler's DataType payload (compressed when a codec is set).
        OMemoryArchive headerPayload;
        hIter->second->Save(headerPayload, asset);

        std::vector<uint8_t> headerBlock;
        if (compressor == nullptr) {
            const auto *begin = reinterpret_cast<const uint8_t *>(headerPayload.Data());
            headerBlock.assign(begin, begin + headerPayload.Size());
        } else {
            uint32_t             bound = compressor->CompressBound(static_cast<uint32_t>(headerPayload.Size()));
            std::vector<uint8_t> compressed(bound);
            auto                 res = compressor->Compress({reinterpret_cast<const uint8_t *>(headerPayload.Data()), headerPayload.Size()},
                                                            {compressed.data(), compressed.size()}, 0);
            if (!res.first) {
                LOG_E(TAG, "Product compression failed for %s", asset->GetUuid().ToString().c_str());
                return;
            }
            const auto appendU32 = [&headerBlock](uint32_t value) {
                const auto *bytes = reinterpret_cast<const uint8_t *>(&value);
                headerBlock.insert(headerBlock.end(), bytes, bytes + sizeof(uint32_t));
            };
            appendU32(static_cast<uint32_t>(headerPayload.Size()));
            appendU32(res.second);
            headerBlock.insert(headerBlock.end(), compressed.begin(), compressed.begin() + res.second);
        }
        archive->Save(static_cast<uint32_t>(headerBlock.size()));
        if (!headerBlock.empty()) {
            archive->SaveRaw(reinterpret_cast<const char *>(headerBlock.data()), headerBlock.size());
        }

        // Bulk payload (opaque bytes; the per-resource layout lives in the header).
        archive->Save(static_cast<uint64_t>(asset->bulk.size()));
        if (!asset->bulk.empty()) {
            archive->SaveRaw(reinterpret_cast<const char *>(asset->bulk.data()), asset->bulk.size());
        }
        archive->Flush();
        asset->bulk.clear();

        // Update the bundle's product index (path -> uuid) so a later path load resolves.
        // Serialized per AssetManager so concurrent cooks do not lose entries (read-modify-write).
        std::string sourcePath;
        if (sourceCatalog != nullptr && sourceCatalog->GetSourcePath(asset->GetUuid(), sourcePath)) {
            std::lock_guard<std::recursive_mutex> lock(mutex);

            const auto  bundleFs = pBundle->GetFileSystem();
            FileLockPtr indexLock;
            if (const auto status = bundleFs->AcquireFileLock(".groups/index.lock", indexLock); !status) {
                LOG_E(TAG, "Product index lock failed: %s", status.message.c_str());
                return;
            }
            productIndices.Invalidate(bundleFs, FilePath{});
            auto index = productIndices.Get(bundleFs, FilePath{});

            IndexFileEntry entry;
            entry.key = MakeCanonicalPath(sourcePath);
            entry.id  = asset->GetUuid();
            index.Set(entry);
            if (!productIndices.Save(bundleFs, FilePath{}, index)) {
                LOG_E(TAG, "Product index write failed");
                return;
            }

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
        if (auto lease = SerializationContext::Get()->RetainModuleCode())
            moduleCode.emplace_back(std::move(lease));
        assetHandlers[Name(type.data())].reset(handler);
    }

} // namespace sky
