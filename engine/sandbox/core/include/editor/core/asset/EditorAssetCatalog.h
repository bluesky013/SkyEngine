//
// Created on 2026/10/04.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/util/Uuid.h>
#include <framework/serialization/Any.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace sky {
    struct AssetSourceInfo;
    struct AssetBuildResult;
    class IAssetEvent;
    struct TypeNode;
} // namespace sky

namespace sky::editor {

    // Cook/product state of a source asset, derived from the asset build pipeline events.
    enum class AssetCookState : uint8_t {
        NotCooked = 0,
        Cooking,
        Ready,
        Failed,
    };

    // One source asset as surfaced to the editor. `path` is the logical path within the mount
    // namespace. `type` is the derived AssetTypeId (empty when the extension is unknown).
    struct EditorAssetItem {
        Uuid           uuid;
        std::string    name;
        std::string    path;
        std::string    type;
        std::string    mount; // owning mount key: "Project" (writable) or "Engine" (read-only)
        AssetCookState state = AssetCookState::NotCooked;
        std::string    error;
        bool           writable = false;
    };

    // A folder node of the virtual-path tree. Folders are derived from the registered sources'
    // logical paths; directories with no registered assets are not represented.
    struct EditorAssetFolder {
        std::string                    path;
        std::string                    name;
        std::string                    mount;
        bool                           writable = false;
        std::vector<EditorAssetFolder> folders;
        std::vector<EditorAssetItem>   items;
    };

    struct EditorCookSetting {
        std::string key;
        std::string value;
    };

    // Effective cook configuration for one asset (asset override x project preset x platform).
    // Per-target effective settings are surfaced by EditorAssetTargetInfo (see GetTargetInfos).
    struct EditorAssetCookConfig {
        std::string              activePlatform;
        std::string              activeTarget;
        std::vector<std::string> targets;
    };

    // Per-(asset, bundle) cook/product state used by the detail view.
    struct EditorAssetTargetInfo {
        std::string                    target;
        AssetCookState                 state = AssetCookState::NotCooked;
        std::string                    error;
        bool                           hasProduct = false;
        std::vector<EditorCookSetting> settings;
    };

    // Reflected cook settings for one (asset, target): the effective object (preset overlaid with the
    // asset override) bound to the generic reflected form, plus the global-preset baseline so the
    // form's "reset" restores the preset. `type` is the builder-declared settings type.
    struct EditorCookSettings {
        sky::Any             object;
        sky::Any             baseline;
        const sky::TypeNode *type = nullptr;

        bool IsValid() const
        {
            return type != nullptr && object.Data() != nullptr;
        }
    };

    // Toolkit-independent asset lookup used by asset-typed properties. Kept as a thin query
    // interface; the default implementation is EditorAssetCatalog.
    class IEditorAssetCatalog {
    public:
        virtual ~IEditorAssetCatalog() = default;

        virtual std::vector<EditorAssetItem> Gather(const std::string &type) const                 = 0;
        virtual bool                         GetType(const Uuid &uuid, std::string &outType) const = 0;
        virtual bool                         GetName(const Uuid &uuid, std::string &outName) const = 0;
    };

    // Process-wide (cross-DLL) asset catalog. Its folder tree is derived from the registered sources
    // in the framework AssetDataBase (the logical mount namespace), never from raw filesystem
    // listing. Render- and toolkit-independent (no ui/aurora/Qt).
    class EditorAssetCatalog : public sky::Singleton<EditorAssetCatalog>, public IEditorAssetCatalog {
        friend class sky::Singleton<EditorAssetCatalog>;

    public:
        using Observer = std::function<void()>;

        EditorAssetCatalog();
        ~EditorAssetCatalog() override;

        // ---- queries (IEditorAssetCatalog) ----
        std::vector<EditorAssetItem> Gather(const std::string &type) const override;
        bool                         GetType(const Uuid &uuid, std::string &outType) const override;
        bool                         GetName(const Uuid &uuid, std::string &outName) const override;

        // Roots of the virtual-path tree (one per mount that has registered assets).
        std::vector<EditorAssetFolder> GetRoots();
        // The folder at `path` with its immediate child folders and items (empty name = root).
        EditorAssetFolder ListFolder(const std::string &path);

        bool Find(const Uuid &uuid, EditorAssetItem &out) const;
        bool FindByPath(const std::string &path, EditorAssetItem &out) const;

        // Aggregate cook state across the asset's targets (failed if any target failed).
        AssetCookState GetCookState(const Uuid &uuid) const;
        // Per-bundle cook state.
        AssetCookState GetCookState(const Uuid &uuid, const std::string &bundle) const;
        bool           GetCookError(const Uuid &uuid, std::string &out) const;
        // Whether a product exists on disk for the asset/bundle (via the framework bundle lookup).
        bool HasProduct(const Uuid &uuid, const std::string &bundle) const;
        // Resolved targets with per-target state, product presence, and effective settings.
        std::vector<EditorAssetTargetInfo> GetTargetInfos(const Uuid &uuid) const;
        // Forward dependencies (assets this one references) and reverse dependents.
        std::vector<Uuid> GetDependencies(const Uuid &uuid) const;
        std::vector<Uuid> GetDependents(const Uuid &uuid) const;

        EditorAssetCookConfig GetCookConfig(const Uuid &uuid) const;
        bool                  IsWritable(const std::string &path) const;

        // Reflected cook settings for (asset, target): effective object + preset baseline + type.
        EditorCookSettings GetCookSettings(const Uuid &uuid, const std::string &target) const;
        // Persist a reflected-form edit: compute the sparse override vs the bundle preset and write it.
        bool ApplyCookSettings(const Uuid &uuid, const std::string &target, const Any &edited);

        // The full virtual-path tree (root folder), built in a single pass. A view flattens it.
        EditorAssetFolder GetTree() const;

        // Rebuild the cached snapshot from the AssetDataBase and notify observers.
        void Refresh();

        // ---- observers ----
        uint32_t AddObserver(Observer observer);
        void     RemoveObserver(uint32_t id);

        // Monotonic revision bumped whenever cook state changes (possibly off the UI thread).
        // Views can poll this to know when to re-read state without cross-thread callbacks.
        uint64_t GetStateRevision() const
        {
            return stateRevision.load(std::memory_order_relaxed);
        }

        // Record a finished build for (uuid, bundle). Used by the internal listener and tests.
        void OnBuildFinished(const Uuid &uuid, const std::string &bundle, bool success, const std::string &error);

        // Mark an asset's targets Cooking and bump the revision. Called by AssetCookService before it
        // triggers the framework cook so the tree/detail views update immediately.
        void BeginCook(const Uuid &uuid, const std::vector<std::string> &targets);

    private:
        struct BuildState {
            AssetCookState state = AssetCookState::NotCooked;
            std::string    error;
        };
        // Per asset: bundle -> build state.
        using CookStateMap = std::unordered_map<std::string, BuildState>;

        // Asset-event listener defined in the implementation (keeps the framework event interface
        // out of this public header).
        struct BuildListener;

        static AssetCookState AggregateState(const CookStateMap &states);

        void NotifyObservers();
        // Revision-keyed snapshot of the registered sources (structure + metadata + cook state).
        // Rebuilt only when the revision advances (mutation, build event, refresh).
        std::vector<EditorAssetItem> CollectItems() const;
        std::vector<EditorAssetItem> BuildItems() const;
        EditorAssetItem              MakeItem(const sky::AssetSourceInfo &source) const;

        // Resolved cook targets for a source: asset override, else project targets, else the active
        // platform's preset bundles.
        std::vector<std::string> ResolveTargets(const sky::AssetSourceInfo &source) const;
        std::string              ActivePlatform() const;
        void                     TrackCook(const Uuid &uuid);
        void                     RebuildGraph() const;
        void                     BumpRevision();

        mutable std::mutex                                  stateMutex;
        std::unordered_map<Uuid, CookStateMap>              cookStates;
        mutable std::unordered_map<Uuid, std::vector<Uuid>> dependents;
        mutable bool                                        graphBuilt = false;
        std::atomic<uint64_t>                               stateRevision{0};

        mutable std::mutex                   itemsMutex;
        mutable std::vector<EditorAssetItem> itemsCache;
        mutable uint64_t                     itemsRevision = ~0ULL;

        std::vector<Uuid> trackedCooks;
        // Internal asset-event listener (defined in the implementation).
        std::unique_ptr<BuildListener> listener;

        mutable std::mutex                     observerMutex;
        std::unordered_map<uint32_t, Observer> observers;
        uint32_t                               nextObserverId = 1;
    };

    IEditorAssetCatalog *GetEditorAssetCatalog();
    // Override the catalog used by GetEditorAssetCatalog() (null restores the singleton default).
    void SetEditorAssetCatalog(IEditorAssetCatalog *catalog);

} // namespace sky::editor
