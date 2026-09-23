//
// Created by blues on 2024/9/1.
//

#pragma once

#include <framework/world/World.h>
#include <navigation/NaviMesh.h>
#include <navigation/NaviMeshAsset.h>
#include <navigation/NaviPath.h>
#include <navigation/NaviGeometryProvider.h>

#include <core/async/Task.h>
#include <core/math/Vector3.h>

#include <algorithm>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sky::ai {

    // Off-thread tile payload prefetch: copies the persisted bytes so the main thread only adds them.
    class NaviMeshTileLoadTask : public Task {
    public:
        void Setup(const NaviMeshTilePayload *payload) { source = payload; }
        const NaviMeshTilePayload &GetCopy() const { return copy; }
        bool IsFinished() const { return finished.load(); }

    protected:
        bool DoWork() override;

    private:
        const NaviMeshTilePayload *source = nullptr;
        NaviMeshTilePayload        copy;
        std::atomic_bool           finished{false};
    };

    // Asynchronous path query: runs QueryPath off the world tick; result is polled.
    class NaviPathQueryTask : public Task {
    public:
        void Setup(const CounterPtr<NaviMesh> &inMesh, const Vector3 &inStart, const Vector3 &inEnd, const NaviQueryFilterPtr &inFilter);

        const NaviPath &GetPath() const { return path; }
        bool IsFinished() const { return finished.load(); }
        bool IsCanceled() const { return canceled.load(); }
        void Cancel() { canceled.store(true); }

    protected:
        bool DoWork() override;

    private:
        CounterPtr<NaviMesh> mesh;
        Vector3              start;
        Vector3              end;
        NaviQueryFilterPtr   filter;
        NaviPathQueryParam   param;
        NaviPath             path;
        std::atomic_bool     finished{false};
        std::atomic_bool     canceled{false};
    };
    using NaviPathQueryTaskPtr = CounterPtr<NaviPathQueryTask>;

    class NavigationSystem : public IWorldSubSystem {
    public:
        NavigationSystem();
        ~NavigationSystem() override = default;

        static constexpr std::string_view NAME = "Navigation";

        const CounterPtr<NaviMesh> &GetNaviMesh() const { return naviMesh; }

        void OnNavMeshChanged();

        // Tile streaming: initialize from a Tiled asset manifest and page tiles around the focus.
        bool SetupStreaming(const NaviMeshData &data);
        void SetStreamingEnabled(bool enable) { streamingEnabled = enable; }
        void SetStreamingFocus(const Vector3 &position) { focus = position; }
        void SetStreamingRadii(float load, float unload) { loadRadius = load; unloadRadius = unload; }
        void SetLoadBudget(uint32_t tilesPerTick) { loadBudgetPerTick = tilesPerTick; }
        uint32_t GetLoadedTileCount() const { return static_cast<uint32_t>(loadedTiles.size()); }
        uint32_t GetPendingLoadCount() const { return static_cast<uint32_t>(pendingLoads.size()); }

        // Geometry providers: external sources (e.g. terrain) feeding nav mesh generation.
        void AddGeometryProvider(INaviGeometryProvider *provider)
        {
            if (provider != nullptr && std::find(geometryProviders.begin(), geometryProviders.end(), provider) == geometryProviders.end()) {
                geometryProviders.push_back(provider);
            }
        }
        void RemoveGeometryProvider(INaviGeometryProvider *provider)
        {
            geometryProviders.erase(std::remove(geometryProviders.begin(), geometryProviders.end(), provider), geometryProviders.end());
        }
        const std::vector<INaviGeometryProvider *> &GetGeometryProviders() const { return geometryProviders; }

        // Async path queries: budgeted submission, polled results, cancellation on mesh change.
        NaviPathQueryTaskPtr RequestPath(const Vector3 &start, const Vector3 &end, const NaviQueryFilterPtr &filter);
        void CancelPath(const NaviPathQueryTaskPtr &task);
        void SetQueryBudget(uint32_t maxConcurrent) { queryBudget = maxConcurrent; }
        uint32_t GetActiveQueryCount() const { return static_cast<uint32_t>(activeQueries.size()); }

    private:
        void OnAttachToWorld(World &world) override;
        void OnDetachFromWorld(World &world) override;
        void Tick(float time) override;

        void UpdateStreaming();

        CounterPtr<NaviMesh> naviMesh;

        NaviMeshBuildParams buildParams;
        bool                hasParams        = false;
        bool                streamingEnabled = false;

        std::unordered_map<uint64_t, const NaviMeshTilePayload *>  availableTiles;
        std::unordered_set<uint64_t>                               loadedTiles;
        std::unordered_map<uint64_t, CounterPtr<NaviMeshTileLoadTask>> pendingLoads;

        Vector3  focus;
        float    loadRadius   = 64.f;
        float    unloadRadius = 96.f;
        uint32_t loadBudgetPerTick = 4;

        std::vector<NaviPathQueryTaskPtr> activeQueries;
        uint32_t                          queryBudget = 4;

        std::vector<INaviGeometryProvider *> geometryProviders;
    };

} // namespace sky::ai
