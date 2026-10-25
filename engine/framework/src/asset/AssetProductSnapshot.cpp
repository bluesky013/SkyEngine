#include <framework/asset/AssetManager.h>

#include <algorithm>
#include <core/archive/MemoryStreamArchive.h>
#include <core/thread/ThreadDispatcher.h>
#include <framework/asset/AssetEvent.h>
#include <framework/asset/ISourceCatalog.h>

namespace sky {
    namespace {
        bool ReadString(IStreamArchive &stream, std::string &out, uint32_t limit)
        {
            uint32_t length = 0;
            if (!stream.Load(length) || length > limit) {
                return false;
            }
            out.resize(length);
            return stream.LoadRaw(out.data(), length) && out.find('\0') == std::string::npos;
        }
    } // namespace

    AssetPtr AssetManager::CreateBuildAsset(const Uuid &uuid, const Name &type) const
    {
        const auto handler = assetHandlers.find(type);
        if (!uuid || handler == assetHandlers.end()) {
            return {};
        }
        auto asset = handler->second->CreateAsset();
        asset->SetUuid(uuid);
        asset->SetType(type);
        return asset;
    }

    ProductStatus AssetManager::BeginProductGroup(const Uuid              &owner,
                                                  const ProductBundleKey  &bundle,
                                                  std::span<const Uuid>    members,
                                                  ProductGroupTransaction &out) const
    {
        const auto *target = GetBundle(bundle);
        return target ? AssetProductGroupStore(target->GetFileSystem()).Begin(owner, members, out)
                      : ProductStatus{ProductError::NotFound, "product bundle is unavailable"};
    }

    ProductStatus AssetManager::EncodeProduct(const AssetPtr &asset, std::vector<uint8_t> &out) const
    {
        if (!asset || !asset->GetUuid() || asset->GetStatus() != AssetBase::Status::INITIAL || asset->GetProductGroup()) {
            return {ProductError::InvalidData, "product publication requires a fresh build asset"};
        }
        const auto handler = assetHandlers.find(asset->GetType());
        if (handler == assetHandlers.end() || asset->dependencies.size() > 4096) {
            return {ProductError::InvalidData, "product handler or dependency list is invalid"};
        }
        OMemoryArchive header;
        if (!handler->second->SaveChecked(header, asset) || header.Size() == 0 || header.Size() > UINT32_MAX - 8U) {
            return {ProductError::InvalidData, "product handler failed checked serialization"};
        }
        IMemoryArchive validationInput(header.Data(), header.Size());
        const auto     validationAsset = handler->second->CreateAsset();
        if (!handler->second->Load(validationInput, validationAsset)) {
            return {ProductError::InvalidData, "encoded product failed handler validation"};
        }
        auto *compressor = compressProducts ? CompressionManager::Get()->GetCompressor(compressionMethod) : nullptr;
        if (compressProducts && !compressor) {
            return {ProductError::Unsupported, "requested product compressor is unavailable"};
        }
        std::string          codec;
        std::vector<uint8_t> compressed;
        OMemoryArchive       block;
        if (compressor) {
            codec            = compressionMethod == CompressionMethod::LZ4 ? "lz4" : "zlib";
            const auto bound = compressor->CompressBound(static_cast<uint32_t>(header.Size()));
            if (bound == 0) {
                return {ProductError::InvalidData, "product compressor returned an invalid bound"};
            }
            compressed.resize(bound);
            const auto result = compressor->Compress({reinterpret_cast<const uint8_t *>(header.Data()), header.Size()}, compressed, 0);
            if (!result.first || result.second == 0 || result.second > bound || result.second > UINT32_MAX - 8U) {
                return {ProductError::IOError, "product compression failed"};
            }
            if (!block.Save(static_cast<uint32_t>(header.Size())) || !block.Save(result.second) ||
                !block.SaveRaw(reinterpret_cast<const char *>(compressed.data()), result.second)) {
                return {ProductError::IOError, "compressed product encoding failed"};
            }
        } else if (!block.SaveRaw(header.Data(), header.Size())) {
            return {ProductError::IOError, "product encoding failed"};
        }
        OMemoryArchive      product;
        BinaryOutputArchive archive(product);
        archive.SaveValue(asset->type.GetStr());
        archive.SaveValue(static_cast<uint32_t>(asset->dependencies.size()));
        for (const auto &dependency : asset->dependencies) {
            archive.SaveValue(dependency);
        }
        archive.SaveValue(codec);
        archive.SaveValue(static_cast<uint32_t>(block.Size()));
        archive.SaveValue(block.Data(), block.Size());
        archive.SaveValue(static_cast<uint64_t>(asset->bulk.size()));
        if (!asset->bulk.empty()) {
            archive.SaveValue(reinterpret_cast<const char *>(asset->bulk.data()), asset->bulk.size());
        }
        if (!archive.IsValid()) {
            return {ProductError::IOError, "product serialization failed"};
        }
        const auto *begin = reinterpret_cast<const uint8_t *>(product.Data());
        out.assign(begin, begin + product.Size());
        return {};
    }

    ProductStatus AssetManager::SaveProductGroup(ProductGroupTransaction  &transaction,
                                                 const ProductBundleKey   &bundle,
                                                 std::span<const AssetPtr> members,
                                                 ProductGroupSnapshotPtr  &out)
    {
        const auto *target = GetBundle(bundle);
        if (!target || members.empty() || members.size() > 4096) {
            return {ProductError::InvalidData, "product group requires a bundle and members"};
        }
        const AssetProductGroupStore store(target->GetFileSystem());
        for (const auto &member : members) {
            std::vector<uint8_t> bytes;
            if (const auto status = EncodeProduct(member, bytes); !status) {
                return status;
            }
            if (const auto status = store.Stage(transaction, member->GetUuid(), bytes); !status) {
                return status;
            }
        }
        if (sourceCatalog) {
            std::lock_guard<std::recursive_mutex> guard(mutex);
            const auto                           &filesystem = target->GetFileSystem();
            FileLockPtr                           indexLock;
            if (const auto status = filesystem->AcquireFileLock(".groups/index.lock", indexLock); !status) {
                return {ProductError::IOError, status.message};
            }
            productIndices.Invalidate(filesystem, FilePath{});
            auto index = productIndices.Get(filesystem, FilePath{});
            for (const auto &member : members) {
                std::string path;
                if (member->GetUuid() == transaction.GetOwner() && sourceCatalog->GetSourcePath(member->GetUuid(), path)) {
                    index.Set({MakeCanonicalPath(path), member->GetUuid(), {}});
                }
            }
            if (!productIndices.Save(filesystem, FilePath{}, index)) {
                return {ProductError::IOError, "product path registration failed"};
            }
            for (const auto &entry : index.Entries()) {
                productPathMap[entry.key] = entry.id;
            }
        }
        const auto status = store.Commit(transaction, out);
        if (status) { productChangeSerial.fetch_add(1, std::memory_order_release); }
        return status;
    }

    AssetPtr AssetManager::LoadAssetSnapshot(const Uuid &uuid, const ProductBundleKey &bundle, const ProductGroupSnapshotPtr &snapshot,
                                            AssetLoadPolicy policy)
    {
        std::lock_guard<std::recursive_mutex> lock(loadMutex);
        const auto                           *target = GetBundle(bundle);
        if (!target || !snapshot) {
            return {};
        }
        auto &cached = snapshots[snapshot->generation][uuid];
        if (const auto asset = cached.lock(); asset && asset->GetStatus() != AssetBase::Status::FAILED) {
            return asset;
        }
        FilePtr file;
        if (!target->OpenPinned(*snapshot, uuid, file)) {
            return {};
        }
        const auto  input = file->ReadAsArchive();
        std::string type, codec;
        uint32_t    count = 0;
        if (!input || !ReadString(*input, type, 4096) || !input->Load(count) || count > 4096) {
            return {};
        }
        auto asset = CreateBuildAsset(uuid, Name(type.c_str()));
        if (!asset) {
            return {};
        }
        asset->dependencies.resize(count);
        for (auto &dependency : asset->dependencies) {
            if (!input->Load(dependency.word[0]) || !input->Load(dependency.word[1]) || !dependency || dependency == uuid) {
                return {};
            }
        }
        uint32_t size = 0;
        if (!ReadString(*input, codec, 16) || !input->Load(size)) {
            return {};
        }
        const auto member =
            std::find_if(snapshot->members.begin(), snapshot->members.end(), [&uuid](const auto &value) { return value.uuid == uuid; });
        const auto offset = input->Tell();
        if (member == snapshot->members.end() || size > 256U * 1024U * 1024U || offset > member->size || size > member->size - offset ||
            member->size - offset - size < sizeof(uint64_t)) {
            return {};
        }
        BinaryDataPtr data = new BinaryData(size);
        if (!input->LoadRaw(reinterpret_cast<char *>(data->Data()), size)) {
            return {};
        }
        IStreamArchivePtr payload = new IMemoryArchive(data);
        if (!codec.empty()) {
            // Check envelope bounds before the shared decompressor allocates either buffer.
            uint32_t unpacked = 0, packed = 0;
            if (!payload->Load(unpacked) || !payload->Load(packed) || size < 8 || packed != size - 8 || unpacked == 0 ||
                unpacked > 256U * 1024U * 1024U) {
                return {};
            }
            payload->GetStream().seekg(0);
            payload = PreparePayload(payload, codec);
            if (!payload) {
                return {};
            }
        }
        uint64_t bulkSize = 0;
        if (!input->Load(bulkSize) || bulkSize != member->size - offset - size - sizeof(uint64_t)) {
            return {};
        }
        asset->bulkStart     = input->Tell();
        asset->productStream = bulkSize ? input : IStreamArchivePtr{};
        asset->productGroup  = snapshot;
        asset->status.store(AssetBase::Status::LOADING);
        // Register before dependency traversal to detect malformed dependency cycles.
        cached = asset;
        std::vector<TaskNodePtr> tasks;
        for (const auto &dependency : asset->dependencies) {
            const bool sameGroup = std::any_of(snapshot->members.begin(), snapshot->members.end(),
                                               [&dependency](const auto &value) { return value.uuid == dependency; });
            auto child = sameGroup ? LoadAssetSnapshot(dependency, bundle, snapshot, policy) : LoadAsset(dependency, policy);
            if (!child || (child->GetStatus() == AssetBase::Status::LOADING && !child->GetAsyncNode() && !child->GetWaitHandle().valid())) {
                asset->status.store(AssetBase::Status::FAILED);
                return asset;
            }
            tasks.emplace_back(child->GetAsyncNode());
            asset->depAssets.emplace_back(std::move(child));
        }
        auto task = AssetExecutor::Get()->DependentAsync(
            [this, weakAsset = std::weak_ptr<AssetBase>(asset), payload]() {
                const auto asset = weakAsset.lock();
                if (!asset) {
                    return;
                }
                for (const auto &dependency : asset->depAssets) {
                    if (!dependency->IsLoaded()) {
                        asset->status.store(AssetBase::Status::FAILED);
                        return;
                    }
                }
                LoadInto(asset, payload);
            },
            tasks);
        asset->SetAsyncTask(task.first, task.second.share());
        return asset;
    }
} // namespace sky
