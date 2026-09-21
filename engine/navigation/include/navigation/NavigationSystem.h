//
// Created by blues on 2024/9/1.
//

#pragma once

#include <framework/world/World.h>
#include <navigation/NaviMesh.h>
#include <navigation/NaviMeshAsset.h>
#include <navigation/NaviPath.h>

#include <core/async/Task.h>
#include <core/math/Vector3.h>

#include <atomic>
#include <memory>
#include <unordered_map>
#include <unordered_set>

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
    };

} // namespace sky::ai
