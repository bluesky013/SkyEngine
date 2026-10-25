#include <framework/asset/AssetDataBase.h>

#include <algorithm>
#include <core/logger/Logger.h>

namespace sky {

    ProductStatus AssetDataBase::CheckGeneratedCatalogConflicts(const Uuid &owner, const GeneratedAssetManifest &manifest) const
    {
        for (const auto &record : manifest.records) {
            const auto existing = generatedAssets.find(record.uuid);
            if (idMap.contains(record.uuid) ||
                (existing != generatedAssets.end() &&
                 (existing->second.owner != owner || existing->second.key != record.key || existing->second.type != record.type))) {
                return {ProductError::OwnerConflict, "generated UUID conflicts with another source or child"};
            }
        }
        return {};
    }

    ProductStatus AssetDataBase::PublishGeneratedCatalog(const Uuid &owner, const GeneratedAssetManifest &manifest)
    {
        if (const auto status = CheckGeneratedCatalogConflicts(owner, manifest); !status) {
            return status;
        }
        std::erase_if(generatedAssets, [&owner](const auto &entry) { return entry.second.owner == owner; });
        for (const auto &record : manifest.records) {
            generatedAssets[record.uuid] = record;
        }
        generatedManifests[owner] = manifest;
        return {};
    }

    bool AssetDataBase::RefreshOwner(const Uuid &owner)
    {
        std::lock_guard lock(assetMutex);
        const auto      source = idMap.find(owner);
        if (source == idMap.end()) {
            return false;
        }
        const auto &path = source->second->path;
        const auto  fs   = ResolveOwningFs(path);
        if (!fs) {
            return false;
        }
        manifests.Invalidate(fs, path.Parent());
        const auto            *entry = manifests.Get(fs, path.Parent()).Find(path.FileName());
        GeneratedAssetManifest data;
        const auto             parsed = ParseGeneratedAssets(owner, entry ? entry->generated : std::string{}, data);
        if (!parsed) {
            LOG_E("AssetDataBase", "%s: %s", path.GetStr().c_str(), parsed.message.c_str());
            return false;
        }
        const auto published = PublishGeneratedCatalog(owner, data);
        if (!published) {
            LOG_E("AssetDataBase", "%s: %s", path.GetStr().c_str(), published.message.c_str());
            return false;
        }
        return true;
    }

    ProductStatus AssetDataBase::UpdateGeneratedProducts(const Uuid                               &owner,
                                                         std::span<const GeneratedAssetDescriptor> requested,
                                                         const std::map<std::string, std::string> &defaults,
                                                         GeneratedAssetManifest                   &out)
    {
        std::lock_guard guard(assetMutex);
        const auto      source = idMap.find(owner);
        if (source == idMap.end()) {
            return {ProductError::NotFound, "generated asset source is not registered"};
        }
        const auto &path  = source->second->path;
        const auto  mount = FindMountIndex(path);
        if (mount < 0) {
            return {ProductError::NotFound, "generated source is not mounted"};
        }
        const auto &filesystem = mountFsList[mount];
        if (!mountInfos[mount].writable) {
            manifests.Invalidate(filesystem, path.Parent());
            const auto            *entry = manifests.Get(filesystem, path.Parent()).Find(path.FileName());
            GeneratedAssetManifest previous, next;
            if (entry && entry->id != owner) {
                return {ProductError::OwnerConflict, "read-only source identity differs"};
            }
            if (const auto status = ParseGeneratedAssets(owner, entry ? entry->generated : std::string{}, previous); !status) {
                return status;
            }
            if (const auto status = ReconcileGeneratedAssets(owner, previous, requested, defaults, next); !status) {
                return status;
            }
            for (const auto &record : next.records) {
                const bool known =
                    std::any_of(previous.records.begin(), previous.records.end(), [&record](const auto &old) { return old.uuid == record.uuid; });
                if (!known) {
                    return {ProductError::Unsupported, "read-only source requires committed generated child identities"};
                }
            }
            if (const auto status = PublishGeneratedCatalog(owner, next); !status) {
                return status;
            }
            out = std::move(next);
            return {};
        }
        FileLockPtr fileLock;
        if (const auto status = filesystem->AcquireFileLock(path.Parent() / FilePath("assets.jsonl.lock"), fileLock); !status) {
            return {status.error == FileOperationError::Busy ? ProductError::Busy : ProductError::IOError, status.message};
        }
        manifests.Invalidate(filesystem, path.Parent());
        auto        manifest = manifests.Get(filesystem, path.Parent());
        const auto *entry    = manifest.Find(path.FileName());
        if (!entry || entry->id != owner) {
            return {ProductError::InvalidData, "source manifest identity changed"};
        }
        GeneratedAssetManifest previous, next;
        if (const auto status = ParseGeneratedAssets(owner, entry->generated, previous); !status) {
            return status;
        }
        if (const auto status = ReconcileGeneratedAssets(owner, previous, requested, defaults, next); !status) {
            return status;
        }
        if (const auto status = CheckGeneratedCatalogConflicts(owner, next); !status) {
            return status;
        }
        auto updated      = *entry;
        updated.generated = SerializeGeneratedAssets(next);
        manifest.Set(updated);
        if (!manifests.Save(filesystem, path.Parent(), manifest)) {
            return {ProductError::IOError, "generated asset manifest save failed"};
        }
        if (const auto status = PublishGeneratedCatalog(owner, next); !status) {
            return status;
        }
        out = std::move(next);
        return {};
    }

    bool AssetDataBase::GetOwner(const Uuid &id, Uuid &out) const
    {
        std::lock_guard lock(assetMutex);
        if (idMap.contains(id)) {
            out = id;
            return true;
        }
        const auto child = generatedAssets.find(id);
        if (child == generatedAssets.end() || !child->second.active || !idMap.contains(child->second.owner)) {
            return false;
        }
        out = child->second.owner;
        return true;
    }

    bool AssetDataBase::GetGenerated(const Uuid &id, GeneratedAssetRecord &out) const
    {
        std::lock_guard lock(assetMutex);
        const auto      child = generatedAssets.find(id);
        if (child == generatedAssets.end()) {
            return false;
        }
        out = child->second;
        return true;
    }

    void AssetDataBase::ForEachGenerated(const std::function<void(const GeneratedAssetRecord &)> &fn) const
    {
        if (!fn) {
            return;
        }
        std::vector<GeneratedAssetRecord> records;
        {
            std::lock_guard lock(assetMutex);
            for (const auto &[id, record] : generatedAssets) {
                records.push_back(record);
            }
        }
        for (const auto &record : records) {
            fn(record);
        }
    }

    bool AssetDataBase::GetGeneratedDefaults(const Uuid &owner, std::map<std::string, Uuid> &out) const
    {
        std::lock_guard lock(assetMutex);
        const auto      manifest = generatedManifests.find(owner);
        if (manifest == generatedManifests.end()) {
            return false;
        }
        std::map<std::string, Uuid> result;
        for (const auto &[role, key] : manifest->second.defaults) {
            if (key == "$source") {
                result.emplace(role, owner);
                continue;
            }
            for (const auto &record : manifest->second.records) {
                if (record.key == key && record.active) {
                    result.emplace(role, record.uuid);
                }
            }
        }
        out = std::move(result);
        return true;
    }
} // namespace sky
