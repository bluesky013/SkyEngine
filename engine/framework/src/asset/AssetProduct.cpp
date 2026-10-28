#include <framework/asset/AssetManager.h>

#include <algorithm>
#include <array>
#include <core/archive/MemoryStreamArchive.h>
#include <framework/asset/ISourceCatalog.h>
#include <unordered_set>

namespace sky {
    namespace {
        uint64_t HashBytes(std::span<const uint8_t> bytes, uint64_t hash = 14695981039346656037ULL)
        {
            for (const auto byte : bytes)
                hash = (hash ^ byte) * 1099511628211ULL;
            return hash;
        }

        ProductStatus FileStatus(const FileOperationStatus &status)
        {
            return status ? ProductStatus{}
                          : ProductStatus{status.error == FileOperationError::Unsupported ? ProductError::Unsupported
                                          : status.error == FileOperationError::Busy      ? ProductError::Busy
                                                                                          : ProductError::IOError,
                                          status.message};
        }

        bool ReadString(IStreamArchive &archive, std::string &out, uint32_t limit)
        {
            uint32_t length = 0;
            if (!archive.Load(length) || length > limit)
                return false;
            out.resize(length);
            return archive.LoadRaw(out.data(), length) && out.find('\0') == std::string::npos;
        }
    } // namespace

    FilePath ProductFilePath(const Uuid &uuid)
    {
        const auto name = uuid.ToString();
        return FilePath(name.substr(0, 2) + "/" + name + ".bin");
    }

    AssetPtr AssetManager::CreateBuildAsset(const Uuid &uuid, const Name &type) const
    {
        const auto handler = assetHandlers.find(type);
        if (!uuid || handler == assetHandlers.end())
            return {};
        auto asset = handler->second->CreateAsset();
        asset->SetUuid(uuid);
        asset->SetType(type);
        return asset;
    }

    ProductStatus
    AssetManager::ReadProductHeader(const Uuid &uuid, const IStreamArchivePtr &input, AssetProductInspection &out, std::string &codec) const
    {
        if (!input || !input->IsOpen() || !uuid)
            return {ProductError::InvalidData, "product stream or identity is invalid"};
        auto &stream = input->GetStream();
        stream.clear();
        stream.seekg(0, std::ios::end);
        const auto length = stream.tellg();
        stream.seekg(0);
        if (!stream || length < static_cast<std::streamoff>(ASSET_PRODUCT_HEADER_BYTES))
            return {ProductError::InvalidData, "product header is truncated"};
        uint32_t magic = 0, version = 0;
        Uuid     identity;
        uint64_t size = 0, checksum = 0;
        if (!input->Load(magic) || !input->Load(version) || !input->LoadRaw(reinterpret_cast<char *>(identity.data), sizeof(Uuid)) ||
            !input->Load(size) || !input->Load(checksum) || magic != ASSET_PRODUCT_MAGIC || identity != uuid ||
            size != static_cast<uint64_t>(length) - ASSET_PRODUCT_HEADER_BYTES)
            return {ProductError::InvalidData, "product framing, identity or length is invalid"};
        if (version != ASSET_PRODUCT_VERSION)
            return {ProductError::Unsupported, "unsupported asset product format version"};
        uint64_t                   hash      = 14695981039346656037ULL;
        uint64_t                   remaining = size;
        std::array<uint8_t, 16384> buffer{};
        while (remaining) {
            const auto count = static_cast<size_t>(std::min<uint64_t>(remaining, buffer.size()));
            if (!input->LoadRaw(reinterpret_cast<char *>(buffer.data()), count))
                return {ProductError::IOError, "product body read failed"};
            hash = HashBytes(std::span(buffer.data(), count), hash);
            remaining -= count;
        }
        if (hash != checksum)
            return {ProductError::InvalidData, "product checksum mismatch"};
        stream.seekg(static_cast<std::streamoff>(ASSET_PRODUCT_HEADER_BYTES));
        uint32_t count = 0;
        if (!stream || !ReadString(*input, out.type, 4096) || out.type.empty() || !input->Load(count) || count > 4096)
            return {ProductError::InvalidData, "product type or dependency count is invalid"};
        std::unordered_set<Uuid> unique;
        out.dependencies.resize(count);
        for (auto &dependency : out.dependencies) {
            if (!input->LoadRaw(reinterpret_cast<char *>(dependency.data), sizeof(Uuid)) || !dependency || dependency == uuid ||
                !unique.insert(dependency).second)
                return {ProductError::InvalidData, "product dependency identity is invalid"};
        }
        if (!ReadString(*input, codec, 16))
            return {ProductError::InvalidData, "product codec is invalid"};
        out.formatVersion = version;
        out.checksum      = checksum;
        return {};
    }

    ProductStatus AssetManager::EncodeProduct(const AssetPtr &asset, std::vector<uint8_t> &out) const
    {
        if (!asset || !asset->GetUuid() || asset->GetStatus() != AssetBase::Status::INITIAL)
            return {ProductError::InvalidData, "product publication requires a fresh build asset"};
        const auto handler = assetHandlers.find(asset->GetType());
        if (handler == assetHandlers.end())
            return {ProductError::InvalidData, "product handler is unavailable"};
        OMemoryArchive header;
        if (!handler->second->SaveChecked(header, asset) || header.Size() == 0 || header.Size() > 256U * 1024U * 1024U)
            return {ProductError::InvalidData, "product handler failed checked serialization"};
        IMemoryArchive validationInput(header.Data(), header.Size());
        const auto     validationAsset = handler->second->CreateAsset();
        if (!handler->second->Load(validationInput, validationAsset))
            return {ProductError::InvalidData, "encoded product failed handler validation"};
        return EncodePayload(asset->uuid, asset->type.GetStr(), asset->dependencies,
                             {reinterpret_cast<const uint8_t *>(header.Data()), header.Size()}, asset->bulk, out);
    }

    ProductStatus AssetManager::EncodePayload(const Uuid              &uuid,
                                              std::string_view         type,
                                              std::span<const Uuid>    dependencies,
                                              std::span<const uint8_t> payload,
                                              std::span<const uint8_t> bulk,
                                              std::vector<uint8_t>    &out) const
    {
        if (!uuid || type.empty() || type.size() > 4096 || type.find('\0') != std::string_view::npos || dependencies.size() > 4096 ||
            payload.empty() || payload.size() > 256U * 1024U * 1024U)
            return {ProductError::InvalidData, "invalid encoded product"};
        std::unordered_set<Uuid> unique;
        for (const auto &dependency : dependencies)
            if (!dependency || dependency == uuid || !unique.insert(dependency).second)
                return {ProductError::InvalidData, "invalid or duplicate product dependency"};
        auto *compressor = compressProducts ? CompressionManager::Get()->GetCompressor(compressionMethod) : nullptr;
        if (compressProducts && !compressor)
            return {ProductError::Unsupported, "requested product compressor is unavailable"};
        OMemoryArchive block;
        std::string    codec;
        if (compressor) {
            codec            = compressionMethod == CompressionMethod::LZ4 ? "lz4" : "zlib";
            const auto bound = compressor->CompressBound(static_cast<uint32_t>(payload.size()));
            if (!bound)
                return {ProductError::InvalidData, "product compressor returned an invalid bound"};
            std::vector<uint8_t> compressed(bound);
            const auto           result = compressor->Compress(payload, compressed, 0);
            if (!result.first || !result.second || result.second > bound || result.second > UINT32_MAX - 8U ||
                !block.Save(static_cast<uint32_t>(payload.size())) || !block.Save(result.second) ||
                !block.SaveRaw(reinterpret_cast<const char *>(compressed.data()), result.second))
                return {ProductError::IOError, "product compression failed"};
        } else if (!block.SaveRaw(reinterpret_cast<const char *>(payload.data()), payload.size()))
            return {ProductError::IOError, "product header encoding failed"};
        OMemoryArchive      body;
        BinaryOutputArchive archive(body);
        archive.SaveValue(type);
        archive.SaveValue(static_cast<uint32_t>(dependencies.size()));
        for (const auto &dependency : dependencies)
            archive.SaveValue(dependency);
        archive.SaveValue(codec);
        archive.SaveValue(static_cast<uint32_t>(block.Size()));
        archive.SaveValue(block.Data(), block.Size());
        archive.SaveValue(static_cast<uint64_t>(bulk.size()));
        if (!bulk.empty())
            archive.SaveValue(reinterpret_cast<const char *>(bulk.data()), bulk.size());
        if (!archive.IsValid())
            return {ProductError::IOError, "product body serialization failed"};
        const auto     bytes = std::span(reinterpret_cast<const uint8_t *>(body.Data()), body.Size());
        OMemoryArchive encoded;
        if (!encoded.Save(ASSET_PRODUCT_MAGIC) || !encoded.Save(ASSET_PRODUCT_VERSION) ||
            !encoded.SaveRaw(reinterpret_cast<const char *>(uuid.data), sizeof(Uuid)) || !encoded.Save(static_cast<uint64_t>(body.Size())) ||
            !encoded.Save(HashBytes(bytes)) || !encoded.SaveRaw(body.Data(), body.Size()))
            return {ProductError::IOError, "product framing serialization failed"};
        const auto *begin = reinterpret_cast<const uint8_t *>(encoded.Data());
        out.assign(begin, begin + encoded.Size());
        return {};
    }

    void AssetManager::InvalidateProducts(std::span<const Uuid> products)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex);
        for (const auto &product : products)
            if (!pendingJobs.contains(product))
                assets.erase(product);
        productChangeSerial.fetch_add(1, std::memory_order_release);
    }

    ProductStatus AssetManager::SaveAsset(const AssetPtr &asset, const ProductBundleKey &bundle)
    {
        return SaveProducts(std::array<AssetPtr, 1>{asset}, bundle);
    }

    ProductStatus AssetManager::SaveProducts(std::span<const AssetPtr> products, const ProductBundleKey &bundle)
    {
        std::lock_guard<std::recursive_mutex> loadLock(loadMutex);
        if (products.empty() || products.size() > 4096)
            return {ProductError::InvalidData, "product publication requires products within the batch limit"};
        std::vector<EncodedProduct> encoded;
        encoded.reserve(products.size());
        for (const auto &product : products) {
            if (!product)
                return {ProductError::InvalidData, "missing product"};
            EncodedProduct output;
            output.uuid = product->GetUuid();
            if (const auto status = EncodeProduct(product, output.bytes); !status)
                return status;
            encoded.emplace_back(std::move(output));
        }
        return PublishProducts(encoded, bundle);
    }

    ProductStatus AssetManager::SaveProductData(std::span<const AssetProductData> products, const ProductBundleKey &bundle)
    {
        std::lock_guard<std::recursive_mutex> loadLock(loadMutex);
        if (products.empty() || products.size() > 4096)
            return {ProductError::InvalidData, "product publication requires products within the batch limit"};
        std::vector<EncodedProduct> encoded;
        encoded.reserve(products.size());
        for (const auto &product : products) {
            EncodedProduct output;
            output.uuid = product.uuid;
            if (const auto status = EncodePayload(product.uuid, product.type, product.dependencies, product.payload, {}, output.bytes); !status)
                return status;
            encoded.emplace_back(std::move(output));
        }
        return PublishProducts(encoded, bundle);
    }

    ProductStatus AssetManager::PublishProducts(std::span<const EncodedProduct> products, const ProductBundleKey &bundle)
    {
        const auto *target = GetBundle(bundle);
        if (!target || products.empty() || products.size() > 4096 || target->GetFileSystem()->IsReadOnly())
            return {ProductError::InvalidData, "product publication requires a writable bundle and products"};
        const auto &filesystem = target->GetFileSystem();
        FileLockPtr lock;
        if (const auto status = filesystem->AcquireFileLock("product.lock", lock); !status)
            return FileStatus(status);
        struct Output {
            const EncodedProduct *product = nullptr;
            FilePath              staging;
        };
        std::vector<Output>      outputs;
        std::unordered_set<Uuid> identities;
        std::vector<Uuid>        published;
        for (const auto &product : products) {
            if (!product.uuid || !identities.insert(product.uuid).second)
                return {ProductError::InvalidData, "duplicate or missing product"};
            Output output;
            output.product = &product;
            output.staging = FilePath(ProductFilePath(product.uuid).GetStr() + "." + Uuid::Create().ToString() + ".tmp");
            outputs.emplace_back(std::move(output));
            published.emplace_back(product.uuid);
        }
        const auto cleanup = [&](ProductStatus status, bool invalidate) {
            for (const auto &output : outputs) {
                const auto remove = [&](const FilePath &path) {
                    if (const auto removed = filesystem->RemoveFile(path); !removed) {
                        status.error = ProductError::IOError;
                        status.message += "; cleanup failed: " + removed.message;
                    }
                };
                remove(output.staging);
                if (invalidate)
                    remove(ProductFilePath(output.product->uuid));
            }
            if (invalidate)
                InvalidateProducts(published);
            return status;
        };
        for (const auto &output : outputs) {
            if (const auto status = filesystem->WriteFileExclusive(output.staging, output.product->bytes); !status)
                return cleanup(FileStatus(status), false);
            const auto           file = filesystem->OpenFile(output.staging);
            std::vector<uint8_t> readback;
            if (!file || !file->ReadBin(readback) || readback != output.product->bytes)
                return cleanup({ProductError::IOError, "product readback differs from encoded bytes"}, false);
        }
        uint32_t replaced = 0;
        for (const auto &output : outputs) {
            if (const auto status = filesystem->ReplaceFileAtomic(output.staging, ProductFilePath(output.product->uuid)); !status)
                return cleanup(FileStatus(status), replaced != 0);
            ++replaced;
        }
        if (sourceCatalog) {
            std::lock_guard<std::recursive_mutex> guard(mutex);
            productIndices.Invalidate(filesystem, FilePath{});
            auto index = productIndices.Get(filesystem, FilePath{});
            for (const auto &product : products) {
                std::string path;
                Uuid        owner;
                if (sourceCatalog->GetOwner(product.uuid, owner) && owner == product.uuid && sourceCatalog->GetSourcePath(product.uuid, path))
                    index.Set({MakeCanonicalPath(path), product.uuid, {}});
            }
            if (!productIndices.Save(filesystem, FilePath{}, index))
                return cleanup({ProductError::IOError, "product index publication failed"}, true);
        }
        InvalidateProducts(published);
        return cleanup({}, false);
    }
} // namespace sky
