//
// Created by blues on 2024/6/16.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/file/FileSystem.h>

#include <framework/asset/AssetCommon.h>
#include <framework/asset/Asset.h>
#include <framework/asset/AssetIndexFile.h>
#include <framework/asset/ISourceCatalog.h>

#include <unordered_map>
#include <mutex>

namespace sky {

    class AssetDependencyGraph;

    class AssetDataBase : public Singleton<AssetDataBase>, public ISourceCatalog {
    public:
        AssetDataBase() = default;
        ~AssetDataBase() override = default;

        void SetEngineFs(const NativeFileSystemPtr &fs);
        void SetWorkSpaceFs(const FileSystemPtr &fs);

        const NativeFileSystemPtr &GetEngineFs() const { return engineFs; }
        const FileSystemPtr &GetWorkSpaceFs() const { return workSpaceFs; }

        std::vector<AssetSourcePtr> Gather(const std::string_view& category);

        AssetSourcePtr RegisterAsset(const FilePath &path, bool build = true);

        AssetSourcePtr FindAsset(const Uuid &id);
        AssetSourcePtr FindAsset(const FilePath &path);
        void RemoveAsset(const Uuid &id);

        // Source-asset mutation (framework). Move keeps the UUID; duplicate assigns a new one.
        AssetSourcePtr MoveAsset(const FilePath &from, const FilePath &to);
        AssetSourcePtr DuplicateAsset(const FilePath &from, const FilePath &to);
        // Import an external file into the writable source mount, assign identity, optionally cook.
        AssetSourcePtr ImportAsset(const FilePath &sourceFile, const FilePath &dest, bool cook);

        FilePtr OpenFile(const AssetSourcePtr &src);
        FilePtr OpenFile(const FilePath &path);
        FilePtr CreateOrOpenFile(const FilePath &path);

        void SetMarkedName(const Uuid& id, const std::string &name);

        void Load();
        void Save();
        void Reset();
        // Rebuild the dev cache by scanning the writable source mount for builder-known extensions.
        void RebuildCacheFromScan();
        // Build one product per configured target for the given asset (multi-platform output).
        void BuildAllTargets(const Uuid &uuid);

        void Dump(std::ostream &stream);

        const std::unordered_map<Uuid, AssetSourcePtr> &GetSources() const { return idMap; }
        // Build the source dependency graph (editor/authoring side) from registered sources.
        void BuildDependencyGraph(AssetDependencyGraph &graph) const;

        // ISourceCatalog
        bool ResolvePath(const std::string &path, Uuid &out) const override;
        bool Exists(const Uuid &id) const override;
        bool GetType(const Uuid &id, std::string &out) const override;
        bool GetTarget(const Uuid &id, std::string &out) const override;
        bool GetSourcePath(const Uuid &id, std::string &out) const override;
    private:
        static Uuid CalculateUuidByPath(const FilePath &path);
        std::string QueryType(const std::string &ext) const;
        std::string CookJsonFor(const AssetSourcePtr &src) const;
        void RebuildMounts();
        FileSystemPtr ResolveOwningFs(const FilePath &path) const;
        void EnsureManifestEntry(const FileSystemPtr &fs, const FilePath &path, const Uuid &uuid);

        NativeFileSystemPtr engineFs;
        FileSystemPtr workSpaceFs;
        FileSystemPtr mountFs; // composed: workspace (writable) first, engine (read-only) second
        std::unordered_map<uint32_t, NativeFileSystemPtr> pluginFs;

        mutable std::recursive_mutex assetMutex;
        std::unordered_map<FilePath, Uuid> pathMap;
        std::unordered_map<Uuid, AssetSourcePtr> idMap;

        mutable AssetIndexFileCache manifests{"assets.jsonl", "file", "cook"};
    };

} // namespace sky
