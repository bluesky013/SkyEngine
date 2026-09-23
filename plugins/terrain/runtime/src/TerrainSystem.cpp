//
// Created on 2026/09/22.
//

#include <terrain/TerrainSystem.h>
#include <terrain/TerrainAddress.h>

#include <algorithm>
#include <cmath>

namespace sky::terrain {

    bool TerrainTileLoadTask::DoWork()
    {
        if (source == nullptr) {
            return false;
        }

        const TerrainLodPayload *payload = source->GetLod(lod);
        if (payload == nullptr) {
            return false;
        }

        copy = *payload;
        finished.store(true);
        return true;
    }

    bool TerrainSystem::Setup(const TerrainAssetData &data)
    {
        field.Clear();
        loadedTiles.clear();
        pendingLoads.clear();
        availableTiles.clear();
        generatedTiles.clear();
        pendingGenerates.clear();

        field.SetMeta(data.meta);
        manifest = data.manifest;

        for (const auto &tile : data.tiles) {
            availableTiles.emplace(tile.coord, &tile);
        }

        // Hole / no-data tiles are never loaded (no surface data).
        holeTiles.clear();
        for (const auto &info : manifest) {
            if (!info.hasData) {
                holeTiles.insert(info.coord);
                availableTiles.erase(info.coord);
            }
        }

        hasData = true;
        return true;
    }

    void TerrainSystem::OnTerrainChanged()
    {
        for (auto &entry : pendingLoads) {
            entry.second->ResetTask();
        }
        for (auto &entry : pendingGenerates) {
            entry.second->ResetTask();
        }
        pendingLoads.clear();
        pendingGenerates.clear();
        for (const auto &key : loadedTiles) {
            pendingRemoved.push_back(TerrainTileLodRef{key.coord, key.lod});
        }
        loadedTiles.clear();
        generatedTiles.clear();
        field.Clear();
    }

    uint32_t TerrainSystem::GetLoadedLodCount(uint32_t lod) const
    {
        uint32_t count = 0;
        for (const auto &key : loadedTiles) {
            if (key.lod == lod) {
                ++count;
            }
        }
        return count;
    }

    bool TerrainSystem::IsTileLoaded(const TerrainTileCoord &coord, uint32_t lod) const
    {
        return loadedTiles.find(TileLodKey{coord, lod}) != loadedTiles.end();
    }

    void TerrainSystem::UpdateStreaming()
    {
        if (!streamingEnabled || !hasData) {
            return;
        }
        if (availableTiles.empty() && !generationEnabled) {
            return;
        }

        const TerrainMeta &meta      = field.GetMeta();
        const float        tileWorld = meta.GetTileWorldSize();
        if (tileWorld <= 0.f) {
            return;
        }

        const uint32_t lodCount = meta.lodCount > 0 ? meta.lodCount : 1u;
        const float    half     = tileWorld * 0.5f;

        auto distanceSq = [&](const TerrainTileCoord &coord) {
            const Vector3 origin = TileToWorld(meta, coord);
            const float   dx = origin.x + half - focus.x;
            const float   dz = origin.z + half - focus.z;
            return dx * dx + dz * dz;
        };

        // Unload entries that left their LOD's band (too far, or moved into a finer annulus).
        for (auto iter = loadedTiles.begin(); iter != loadedTiles.end();) {
            const uint32_t q    = iter->lod;
            const float    d2   = distanceSq(iter->coord);
            const float    outU = LodOuterUnload(q);
            const float    inL  = LodInnerLoad(q);

            if (d2 > outU * outU || d2 < inL * inL) {
                if (q == 0) {
                    field.RemoveTile(iter->coord);
                }
                pendingRemoved.push_back(TerrainTileLodRef{iter->coord, q});
                iter = loadedTiles.erase(iter);
            } else {
                ++iter;
            }
        }

        const auto center = WorldToTile(meta, focus);

        // Generate missing tiles on demand (bounded to the outermost load radius).
        if (generationEnabled) {
            const auto radius = static_cast<int32_t>(std::ceil(LodOuterLoad(lodCount - 1) / tileWorld));
            for (int32_t y = center.y - radius; y <= center.y + radius; ++y) {
                for (int32_t x = center.x - radius; x <= center.x + radius; ++x) {
                    const TerrainTileCoord coord{x, y};
                    if (holeTiles.count(coord) != 0) {
                        continue;
                    }
                    if (availableTiles.count(coord) != 0 || generatedTiles.count(coord) != 0 ||
                        pendingGenerates.count(coord) != 0) {
                        continue;
                    }

                    auto task = CounterPtr<TerrainTileGenerateTask>(new TerrainTileGenerateTask());
                    task->Setup(&generateConfig, &meta, coord);
                    task->StartAsync();
                    pendingGenerates.emplace(coord, task);
                }
            }
        }

        // Apply finished generated tiles so they become addressable for loading.
        uint32_t generated = 0;
        for (auto iter = pendingGenerates.begin(); iter != pendingGenerates.end() && generated < loadBudgetPerTick;) {
            if (!iter->second->IsFinished()) {
                ++iter;
                continue;
            }

            TerrainTilePayload payload = iter->second->GetPayload();
            auto [ins, inserted] = generatedTiles.emplace(iter->first, std::move(payload));
            if (inserted) {
                availableTiles[iter->first] = &ins->second;
            }
            iter = pendingGenerates.erase(iter);
            ++generated;
        }

        // Scan each LOD's annulus and start async prefetches.
        for (uint32_t q = 0; q < lodCount; ++q) {
            const float outer = LodOuterLoad(q);
            const float inner = LodInnerLoad(q);
            if (outer <= inner) {
                continue;
            }

            const auto radius = static_cast<int32_t>(std::ceil(outer / tileWorld));
            for (int32_t y = center.y - radius; y <= center.y + radius; ++y) {
                for (int32_t x = center.x - radius; x <= center.x + radius; ++x) {
                    const TerrainTileCoord coord{x, y};
                    const TileLodKey       key{coord, q};

                    if (loadedTiles.count(key) != 0 || pendingLoads.count(key) != 0) {
                        continue;
                    }

                    const auto iter = availableTiles.find(coord);
                    if (iter == availableTiles.end() || iter->second->GetLodCount() <= q) {
                        continue;
                    }

                    const float d2 = distanceSq(coord);
                    if (d2 >= outer * outer || d2 < inner * inner) {
                        continue;
                    }

                    auto task = CounterPtr<TerrainTileLoadTask>(new TerrainTileLoadTask());
                    task->Setup(iter->second, q);
                    task->StartAsync();
                    pendingLoads.emplace(key, task);
                }
            }
        }

        // Apply finished prefetches on the main thread within the per-tick budget; drop drifted pendings.
        uint32_t applied = 0;
        for (auto iter = pendingLoads.begin(); iter != pendingLoads.end() && applied < loadBudgetPerTick;) {
            const TileLodKey &key  = iter->first;
            const float       d2   = distanceSq(key.coord);
            const float       outU = LodOuterUnload(key.lod);
            const float       inL  = LodInnerLoad(key.lod);

            if (d2 > outU * outU || d2 < inL * inL) {
                iter->second->ResetTask();
                iter = pendingLoads.erase(iter);
                continue;
            }

            if (!iter->second->IsFinished()) {
                ++iter;
                continue;
            }

            bool ok = true;
            if (key.lod == 0) {
                ok = field.AddTile(key.coord, iter->second->GetCopy());
            }
            if (ok) {
                loadedTiles.insert(key);
                pendingAdded.push_back(TerrainTileLodRef{key.coord, key.lod});
            }
            iter = pendingLoads.erase(iter);
            ++applied;
        }
    }

    void TerrainSystem::Tick(float time)
    {
        UpdateStreaming();
    }

    bool TerrainSystem::SampleRegionLod0(const AABB &bounds, ITerrainRegionSink &sink) const
    {
        TerrainTileCoord minCoord;
        TerrainTileCoord maxCoord;
        TileRangeForBounds(field.GetMeta(), bounds, minCoord, maxCoord);

        bool complete = true;
        for (int32_t y = minCoord.y; y <= maxCoord.y; ++y) {
            for (int32_t x = minCoord.x; x <= maxCoord.x; ++x) {
                const TerrainTileCoord coord{x, y};
                const float          *heights = nullptr;
                uint32_t              size    = 0;
                if (field.GetTileHeights(coord, heights, size)) {
                    sink.OnTerrainTileLod0(coord, field.GetMeta(), heights, size);
                } else {
                    complete = false;
                }
            }
        }
        return complete;
    }

    bool TerrainSystem::ConsumeResidencyDelta(std::vector<TerrainTileLodRef> &added, std::vector<TerrainTileLodRef> &removed)
    {
        const bool changed = !pendingAdded.empty() || !pendingRemoved.empty();
        added.swap(pendingAdded);
        removed.swap(pendingRemoved);
        pendingAdded.clear();
        pendingRemoved.clear();
        return changed;
    }

    void TerrainSystem::AddChangeListener(ITerrainChangeListener *listener)
    {
        if (listener == nullptr) {
            return;
        }
        if (std::find(changeListeners.begin(), changeListeners.end(), listener) == changeListeners.end()) {
            changeListeners.push_back(listener);
        }
    }

    void TerrainSystem::RemoveChangeListener(ITerrainChangeListener *listener)
    {
        changeListeners.erase(std::remove(changeListeners.begin(), changeListeners.end(), listener), changeListeners.end());
    }

    void TerrainSystem::NotifyTilesChanged(const std::vector<TerrainTileCoord> &coords)
    {
        for (const auto &c : coords) {
            for (auto iter = loadedTiles.begin(); iter != loadedTiles.end();) {
                if (iter->coord == c) {
                    if (iter->lod == 0) {
                        field.RemoveTile(c);
                    }
                    pendingRemoved.push_back(TerrainTileLodRef{iter->coord, iter->lod});
                    iter = loadedTiles.erase(iter);
                } else {
                    ++iter;
                }
            }

            const auto gen = generatedTiles.find(c);
            if (gen != generatedTiles.end()) {
                availableTiles.erase(c);
                generatedTiles.erase(gen);
            }
        }

        for (auto *listener : changeListeners) {
            listener->OnTerrainTilesChanged(coords);
        }
    }

    void TerrainSystem::OnAttachToWorld(World &world)
    {
    }

    void TerrainSystem::OnDetachFromWorld(World &world)
    {
        for (auto &entry : pendingLoads) {
            entry.second->ResetTask();
        }
        for (auto &entry : pendingGenerates) {
            entry.second->ResetTask();
        }
        pendingLoads.clear();
        pendingGenerates.clear();
        loadedTiles.clear();
        generatedTiles.clear();
        availableTiles.clear();
        field.Clear();
        hasData = false;
    }

} // namespace sky::terrain
