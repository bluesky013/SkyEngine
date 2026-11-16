//
// Created by blues on 2024/6/16.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/file/FileSystem.h>

#include <framework/asset/Asset.h>
#include <framework/asset/AssetCommon.h>
#include <framework/asset/AssetIndexFile.h>
#include <framework/asset/ISourceCatalog.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace sky {

    class AssetDependencyGraph;

    struct AssetScanWarning {
        std::string mount;
        FilePath    path;
        size_t      line = 0;
        std::string message;
    };

    struct BlockedAssetDirectory {
        std::string mount;
        FilePath    directory;
        std::string reason;
        bool        recoverable = false;
    };

    struct FileScanReport {
        std::vector<FileScanError>         errors;
        std::vector<AssetScanWarning>      warnings;
        std::vector<BlockedAssetDirectory> blockedDirs;
        bool                               Succeeded() const
        {
            return errors.empty();
        }
        bool Complete() const
        {
            return Succeeded() && blockedDirs.empty();
        }
    };

    struct AssetDestination {
        std::string   mount;
        FileSystemPtr filesystem;
        FilePath      path;
    };

    struct AssetSourceSnapshot {
        AssetSourcePtr source;
        FileSystemPtr  filesystem;
    };

    struct AssetMutationResult {
        AssetCommitState                state = AssetCommitState::NotCommitted;
        AssetSourcePtr                  source;
        FileOperationStatus             status;
        std::string                     stage = "validate";
        std::vector<AssetRecoveryIssue> recovery;
        AssetMutationResult() = default;
        AssetMutationResult(FileOperationError error, std::string message) : status{error, std::move(message)}, stage("validate")
        {
        }
        AssetMutationResult(FileOperationStatus value, std::string failedStage) : status(std::move(value)), stage(std::move(failedStage))
        {
        }
        std::string Describe() const
        {
            return DescribeAssetOperation(status.message, state, stage, recovery);
        }
        explicit operator bool() const
        {
            return state == AssetCommitState::Committed && static_cast<bool>(status) && recovery.empty();
        }
    };

    class AssetDataBase : public Singleton<AssetDataBase>, public ISourceCatalog {
    public:
        AssetDataBase()           = default;
        ~AssetDataBase() override
        {
            Reset();
        }

        void SetEngineFs(const NativeFileSystemPtr &fs);
        void SetWorkSpaceFs(const FileSystemPtr &fs);

        const NativeFileSystemPtr &GetEngineFs() const
        {
            return engineFs;
        }
        const FileSystemPtr &GetWorkSpaceFs() const
        {
            return workSpaceFs;
        }

        // Ordered source mounts (id/display/writable); earlier mounts shadow later ones. Consumers
        // use this instead of probing filesystems or hardcoding mount names.
        const std::vector<AssetMount> &GetMounts() const
        {
            return mountInfos;
        }

        std::vector<AssetSourcePtr> Gather(const std::string_view &category);
        // Read-only, lock-safe enumeration of the registered sources. The sources are copied under
        // the asset mutex before the callback runs, so the callback may call back into the database
        // (and must not mutate it).
        void          ForEachSource(const std::function<void(const AssetSourcePtr &)> &fn) const;
        void          ForEachGenerated(const std::function<void(const GeneratedAssetRecord &)> &fn) const;
        ProductStatus UpdateGeneratedProducts(const Uuid                                         &owner,
                                              std::span<const GeneratedAssetDescriptor>           requested,
                                              const std::map<std::string, std::string>           &defaults,
                                              GeneratedAssetManifest                             &out,
                                              const std::shared_ptr<const AssetPublicationGuard> &publication = {});
        bool          GetGeneratedDefaults(const Uuid &owner, std::map<std::string, Uuid> &out) const;

        AssetSourcePtr RegisterAsset(const FilePath &path, bool build = true);
        AssetSourcePtr RegisterAsset(const std::string &mount, const FilePath &path, bool build);
        // Checked identity persistence for authoring saves; an unrelated manifest owner is never replaced.
        AssetMutationResult PersistSourceIdentity(const Uuid &source);

        // Source fields and their owning filesystem are captured together before any I/O or task submission.
        AssetSourceSnapshot CaptureSource(const Uuid &id) const;
        ProductStatus       PrepareBuildInput(const Uuid &id, AssetBuildRequest &out);
        AssetMutationResult WriteSource(const Uuid &id, std::string_view text);
        AssetSourcePtr FindAsset(const Uuid &id);
        AssetSourcePtr FindAsset(const FilePath &path);
        AssetSourcePtr FindAsset(const std::string &mount, const FilePath &path);
        // RemoveAsset unregisters identity; DeleteAsset also removes the source file.
        [[nodiscard]] AssetMutationResult RemoveAsset(const Uuid &id);
        [[nodiscard]] AssetMutationResult DeleteAsset(const Uuid &id);

        // Source-asset mutation (framework). Move keeps the UUID; duplicate assigns a new one.
        AssetMutationResult MoveAsset(const FilePath &from, const FilePath &to);
        AssetMutationResult CreateAsset(const std::string &mount, const FilePath &destination, std::span<const uint8_t> bytes);
        AssetMutationResult DuplicateAsset(const FilePath &from, const FilePath &to);
        // Import an external file into the writable source mount, assign identity, optionally cook.
        AssetMutationResult ImportAsset(const FilePath &sourceFile, const FilePath &dest, bool cook);
        FileOperationStatus ResolveWritableDestination(const FilePath &path, AssetDestination &out, const std::string &mount = {}) const;

        FilePtr OpenFile(const AssetSourcePtr &src);
        FilePtr OpenFile(const FilePath &path);
        FilePtr CreateOrOpenFile(const FilePath &path);

        void SetBuildDependencies(const Uuid &id, const std::vector<Uuid> &dependencies);
        bool SetBuildDependencies(const AssetBuildRequest &request);
        void SetMarkedName(const Uuid &id, const std::string &name);
        // The asset's raw cook block from its manifest entry (empty when absent).
        std::string GetCookJson(const Uuid &id) const;
        // Replace cook metadata only for an unchanged identity on its writable owning mount.
        AssetMutationResult SetCookJson(const Uuid &id, const std::string &cookJson);
        // Absolute OS path of a registered source's file (the framework owns the mount roots).
        bool GetAbsoluteSourcePath(const Uuid &id, std::string &out) const;

        [[nodiscard]] FileScanReport Load();
        void                         Save();
        void                         Reset();
        // Scan all source mounts; only healthy writable directories may acquire new identities.
        [[nodiscard]] FileScanReport RebuildCacheFromScan();
        // Seed legacy identities only in healthy writable directories; committed identities win.
        [[nodiscard]] FileScanReport MigrateLegacyIdentity();
        // Build one product per configured target for the given asset (multi-platform output).
        void BuildAllTargets(const Uuid &uuid);

        void Dump(std::ostream &stream);

        std::unordered_map<Uuid, AssetSourcePtr> GetSources() const;
        // Build the source dependency graph (editor/authoring side) from registered sources.
        void BuildDependencyGraph(AssetDependencyGraph &graph) const;

        // ISourceCatalog
        bool ResolvePath(const std::string &path, Uuid &out) const override;
        bool Exists(const Uuid &id) const override;
        bool GetType(const Uuid &id, std::string &out) const override;
        bool GetTarget(const Uuid &id, std::string &out) const override;
        bool GetSourcePath(const Uuid &id, std::string &out) const override;
        bool GetOwner(const Uuid &id, Uuid &out) const override;
        bool GetGenerated(const Uuid &id, GeneratedAssetRecord &out) const override;
        bool RefreshOwner(const Uuid &owner) override;

    private:
        void                  UnpublishSource(const AssetSourcePtr &source);
        AssetSourcePtr        FindSource(const Uuid &id) const;
        AssetSourcePtr        FindSource(const FilePath &path) const;
        AssetSourcePtr        FindSource(const std::string &mount, const FilePath &path) const;
        std::function<bool()> SourceCancellation() const;
        void                  InvalidatePath(const FilePath &path);
        FileOperationStatus WritableSource(const AssetSourcePtr &source, FileSystemPtr &out) const;
        AssetMutationResult   CreateSource(const AssetDestination       &destination,
                                           std::span<const uint8_t>      bytes,
                                           const std::string            &cook,
                                           const GeneratedAssetManifest &generated);
        using MountedPath      = std::pair<std::string, FilePath>;
        using SourceScanResult = std::variant<std::vector<MountedPath>, FileScanError>;
        SourceScanResult    ScanSources() const;
        FileScanReport      RegisterScannedSources(const std::vector<MountedPath> &sources);
        AssetMutationResult RemoveSource(const Uuid &id, bool deleteFile);
        static Uuid         CalculateUuidByPath(const FilePath &path);
        // Legacy `(bundle, path)` UUID, reproduced via the mount -> legacy `SourceAssetBundle` role
        // (engine mount -> ENGINE, workspace/other -> WORKSPACE). Used only by the migration.
        Uuid          CalculateLegacyUuid(const std::string &mount, const FilePath &path) const;
        bool          migrationMode = false;
        std::string   QueryType(const std::string &ext) const;
        std::string   CookJsonFor(const AssetSourceSnapshot &snapshot) const;
        void          RebuildMounts();
        FileSystemPtr ResolveOwningFs(const AssetSourcePtr &source) const;
        // Owning mount index for a logical path (or -1); index into mountInfos/mountFsList.
        int           FindMountIndex(const FilePath &path) const;
        int           FindMountIndex(const std::string &mount) const;
        ProductStatus PublishGeneratedCatalog(const Uuid &owner, const GeneratedAssetManifest &manifest);
        ProductStatus CheckGeneratedCatalogConflicts(const Uuid &owner, const GeneratedAssetManifest &manifest) const;

        NativeFileSystemPtr                               engineFs;
        FileSystemPtr                                     workSpaceFs;
        FileSystemPtr                                     mountFs; // composed: workspace (writable) first, engine (read-only) second
        std::unordered_map<uint32_t, NativeFileSystemPtr> pluginFs;

        std::vector<AssetMount>    mountInfos;  // public, ordered (parallel to mountFsList)
        std::vector<FileSystemPtr> mountFsList; // resolution order

        // Writers serialize independently; readers take only assetMutex. No file lock may wait for assetMutex.
        std::shared_ptr<std::recursive_mutex>                               mutationMutex = std::make_shared<std::recursive_mutex>();
        std::atomic<uint64_t>                                               cancellationEpoch{0};
        mutable std::recursive_mutex                                        assetMutex;
        std::unordered_map<std::string, std::unordered_map<FilePath, Uuid>> pathMap;
        std::unordered_map<Uuid, AssetSourcePtr>                            idMap;
        std::unordered_map<Uuid, GeneratedAssetRecord>                      generatedAssets;
        std::unordered_map<Uuid, GeneratedAssetManifest>                    generatedManifests;

        mutable AssetIndexFileCache manifests{"assets.jsonl", "file", "cook"};
    };

} // namespace sky
