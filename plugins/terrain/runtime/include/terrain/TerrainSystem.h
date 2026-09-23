//
// Created on 2026/09/22.
//

#pragma once

#include <framework/world/World.h>

#include <terrain/TerrainAsset.h>
#include <terrain/TerrainField.h>
#include <terrain/TerrainGenerator.h>
#include <terrain/TerrainRegion.h>
#include <terrain/TerrainSystemInterface.h>
#include <terrain/TerrainTileKey.h>

#include <core/async/Task.h>

#include <atomic>
#include <unordered_map>
#include <unordered_set>

namespace sky::terrain {

    // Off-thread tile-LOD prefetch: copies the persisted bytes so the main thread only applies them.
    class TerrainTileLoadTask : public Task {
    public:
        void Setup(const TerrainTilePayload *payload, uint32_t inLod)
        {
            source = payload;
            lod    = inLod;
        }

        uint32_t                 GetLod() const { return lod; }
        const TerrainLodPayload &GetCopy() const { return copy; }
        bool                     IsFinished() const { return finished.load(); }

    protected:
        bool DoWork() override;

    private:
        const TerrainTilePayload *source = nullptr;
        uint32_t                  lod    = 0;
        TerrainLodPayload         copy;
        std::atomic_bool          finished{false};
    };

    // Per-world terrain runtime: owns the field + manifest and pages per-tile LODs around a focus position.
    class TerrainSystem : public IWorldSubSystem, public ITerrainSystem {
    public:
        TerrainSystem() = default;
        ~TerrainSystem() override = default;

        static constexpr std::string_view NAME = "Terrain";

        // Initializes metadata + the addressable tile manifest from a terrain asset payload.
        bool Setup(const TerrainAssetData &data);
        void OnTerrainChanged();

        void SetStreamingEnabled(bool enable) { streamingEnabled = enable; }
        void SetStreamingFocus(const Vector3 &position) { focus = position; }
        // Base (LOD0) radii; LOD q pages within [load * 2^(q-1), load * 2^q) with unload * 2^q hysteresis.
        void SetStreamingRadii(float load, float unload) { loadRadius = load; unloadRadius = unload; }
        void SetLoadBudget(uint32_t tilesPerTick) { loadBudgetPerTick = tilesPerTick; }
        void SetGenerationEnabled(bool enable) { generationEnabled = enable; }
        void SetGenerateConfig(const TerrainGenerateConfig &config) { generateConfig = config; }

        uint32_t GetLoadedTileCount() const override { return static_cast<uint32_t>(loadedTiles.size()); }
        uint32_t GetLoadedLodCount(uint32_t lod) const;
        uint32_t GetPendingLoadCount() const { return static_cast<uint32_t>(pendingLoads.size()); }
        bool     IsTileLoaded(const TerrainTileCoord &coord, uint32_t lod) const;

        const std::unordered_set<TileLodKey, TileLodHash> &GetLoadedTiles() const { return loadedTiles; }

        const ITerrainField &GetField() const override { return field; }
        const TerrainMeta &GetMeta() const override { return field.GetMeta(); }
        const TerrainTileManifest &GetManifest() const override { return manifest; }

        // Iterates resident LOD0 tile geometry over a world-space bounds. Returns false if any
        // overlapping tile has no resident LOD0 data (caller can defer/retry).
        bool SampleRegionLod0(const AABB &bounds, ITerrainRegionSink &sink) const override;

        void AddChangeListener(ITerrainChangeListener *listener) override;
        void RemoveChangeListener(ITerrainChangeListener *listener) override;
        void NotifyTilesChanged(const std::vector<TerrainTileCoord> &coords) override;

        void Tick(float time) override;
        void UpdateStreaming();

    private:
        void OnAttachToWorld(World &world) override;
        void OnDetachFromWorld(World &world) override;

        float LodOuterLoad(uint32_t lod) const { return loadRadius * static_cast<float>(1u << lod); }
        float LodOuterUnload(uint32_t lod) const { return unloadRadius * static_cast<float>(1u << lod); }
        float LodInnerLoad(uint32_t lod) const { return lod == 0 ? 0.f : LodOuterLoad(lod - 1u); }

        TerrainField        field;
        TerrainTileManifest manifest;

        std::vector<ITerrainChangeListener *> changeListeners;

        std::unordered_map<TerrainTileCoord, const TerrainTilePayload *, TerrainTileHash> availableTiles;
        std::unordered_set<TileLodKey, TileLodHash>                                       loadedTiles;
        std::unordered_map<TileLodKey, CounterPtr<TerrainTileLoadTask>, TileLodHash>      pendingLoads;

        std::unordered_map<TerrainTileCoord, TerrainTilePayload, TerrainTileHash>         generatedTiles;
        std::unordered_map<TerrainTileCoord, CounterPtr<TerrainTileGenerateTask>, TerrainTileHash> pendingGenerates;

        TerrainGenerateConfig generateConfig;
        bool     generationEnabled = false;
        bool     hasData          = false;
        bool     streamingEnabled = false;
        Vector3  focus;
        float    loadRadius        = 128.f;
        float    unloadRadius      = 192.f;
        uint32_t loadBudgetPerTick = 4;
    };

} // namespace sky::terrain
