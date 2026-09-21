//
// Created by blues on 2024/9/1.
//

#pragma once

#include <framework/world/World.h>
#include <navigation/NaviMesh.h>
#include <navigation/NaviMeshAsset.h>
#include <navigation/NaviPath.h>

#include <core/math/Vector3.h>

#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace sky::ai {

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
        uint32_t GetLoadedTileCount() const { return static_cast<uint32_t>(loadedTiles.size()); }

    private:
        void OnAttachToWorld(World &world) override;
        void OnDetachFromWorld(World &world) override;
        void Tick(float time) override;

        void UpdateStreaming();

        CounterPtr<NaviMesh> naviMesh;

        NaviMeshBuildParams buildParams;
        bool                hasParams        = false;
        bool                streamingEnabled = false;

        std::unordered_map<uint64_t, const NaviMeshTilePayload *> availableTiles;
        std::unordered_set<uint64_t>                              loadedTiles;

        Vector3 focus;
        float   loadRadius   = 64.f;
        float   unloadRadius = 96.f;
    };

} // namespace sky::ai
