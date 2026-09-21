//
// Created by blues on 2024/9/1.
//

#include <navigation/NavigationSystem.h>
#include <navigation/NaviMeshFactory.h>

#include <cmath>

namespace sky::ai {

    namespace {
        uint64_t TileKey(int32_t x, int32_t y)
        {
            return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) | static_cast<uint32_t>(y);
        }
    } // namespace

    bool NaviMeshTileLoadTask::DoWork()
    {
        if (source == nullptr) {
            return false;
        }
        copy = *source;
        finished.store(true);
        return true;
    }

    NavigationSystem::NavigationSystem() = default;

    void NavigationSystem::OnAttachToWorld(World &world)
    {
        naviMesh = NaviMeshFactory::Get()->CreateNaviMesh();
        if (naviMesh != nullptr) {
            naviMesh->navSystem = this;
            naviMesh->OnAttachToWorld(world);
        }
    }

    void NavigationSystem::OnDetachFromWorld(World &world)
    {
        if (naviMesh != nullptr) {
            naviMesh->OnDetachFromWorld(world);
            naviMesh = nullptr;
        }

        availableTiles.clear();
        loadedTiles.clear();
        pendingLoads.clear();
    }

    bool NavigationSystem::SetupStreaming(const NaviMeshData &data)
    {
        if (naviMesh == nullptr) {
            return false;
        }

        availableTiles.clear();
        loadedTiles.clear();
        pendingLoads.clear();

        buildParams = data.params;
        hasParams   = true;

        for (const auto &tile : data.tiles) {
            availableTiles.emplace(TileKey(tile.tx, tile.ty), &tile);
        }

        return naviMesh->PrepareStreaming(data.params);
    }

    void NavigationSystem::UpdateStreaming()
    {
        if (!streamingEnabled || !hasParams || naviMesh == nullptr || availableTiles.empty()) {
            return;
        }

        const float tileSize = buildParams.resolution.tileSize > 0.f ? buildParams.resolution.tileSize : 1.f;
        const float originX  = buildParams.bounds.min.x;
        const float originZ  = buildParams.bounds.min.z;

        const auto centerX = static_cast<int32_t>(std::floor((focus.x - originX) / tileSize));
        const auto centerY = static_cast<int32_t>(std::floor((focus.z - originZ) / tileSize));

        const float unloadSq = unloadRadius * unloadRadius;
        const float loadSq   = loadRadius * loadRadius;

        for (auto iter = loadedTiles.begin(); iter != loadedTiles.end();) {
            const auto x  = static_cast<int32_t>(*iter >> 32);
            const auto y  = static_cast<int32_t>(*iter & 0xffffffff);
            const float dx = (static_cast<float>(x) + 0.5f) * tileSize - (focus.x - originX);
            const float dy = (static_cast<float>(y) + 0.5f) * tileSize - (focus.z - originZ);

            if (dx * dx + dy * dy > unloadSq) {
                naviMesh->RemoveTile({x, y});
                iter = loadedTiles.erase(iter);
            } else {
                ++iter;
            }
        }

        const auto radius = static_cast<int32_t>(std::ceil(loadRadius / tileSize));
        for (int32_t y = centerY - radius; y <= centerY + radius; ++y) {
            for (int32_t x = centerX - radius; x <= centerX + radius; ++x) {
                const uint64_t key = TileKey(x, y);
                if (loadedTiles.count(key) != 0 || pendingLoads.count(key) != 0) {
                    continue;
                }

                const auto iter = availableTiles.find(key);
                if (iter == availableTiles.end()) {
                    continue;
                }

                const float dx = (static_cast<float>(x) + 0.5f) * tileSize - (focus.x - originX);
                const float dy = (static_cast<float>(y) + 0.5f) * tileSize - (focus.z - originZ);
                if (dx * dx + dy * dy > loadSq) {
                    continue;
                }

                auto task = CounterPtr<NaviMeshTileLoadTask>(new NaviMeshTileLoadTask());
                task->Setup(iter->second);
                task->StartAsync();
                pendingLoads.emplace(key, task);
            }
        }

        // Apply finished prefetches on the main thread within a per-frame budget; drop ones that left range.
        uint32_t applied = 0;
        for (auto iter = pendingLoads.begin(); iter != pendingLoads.end() && applied < loadBudgetPerTick;) {
            const auto x = static_cast<int32_t>(iter->first >> 32);
            const auto y = static_cast<int32_t>(iter->first & 0xffffffff);

            const float dx = (static_cast<float>(x) + 0.5f) * tileSize - (focus.x - originX);
            const float dy = (static_cast<float>(y) + 0.5f) * tileSize - (focus.z - originZ);
            if (dx * dx + dy * dy > unloadSq) {
                iter->second->ResetTask();
                iter = pendingLoads.erase(iter);
                continue;
            }

            if (!iter->second->IsFinished()) {
                ++iter;
                continue;
            }

            if (naviMesh->AddTile(iter->second->GetCopy())) {
                loadedTiles.insert(iter->first);
            }
            iter = pendingLoads.erase(iter);
            ++applied;
        }
    }

    void NavigationSystem::Tick(float time)
    {
        UpdateStreaming();
    }

    void NavigationSystem::OnNavMeshChanged()
    {
        loadedTiles.clear();
    }

} // namespace sky::ai
