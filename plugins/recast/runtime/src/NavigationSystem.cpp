//
// Created on 2026/10/30.
//

#include <navigation/NavigationSystem.h>

#include <core/logger/Logger.h>
#include <framework/asset/AssetManager.h>
#include <navigation/NaviMeshAsset.h>
#include <navigation/NaviMeshFactory.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <limits>

namespace sky::ai {
    namespace {
        uint64_t TileKey(int32_t x, int32_t y)
        {
            return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) | static_cast<uint32_t>(y);
        }
        class SnapshotFilter final : public NaviQueryFilter {
        public:
            explicit SnapshotFilter(const NaviQueryFilter &source) : include(source.GetIncludeFlags()), exclude(source.GetExcludeFlags())
            {
                for (size_t i = 0; i < costs.size(); ++i)
                    costs[i] = source.GetAreaCost(static_cast<AreaId>(i));
            }
            void SetAreaCost(AreaId id, float cost) override
            {
                costs[id] = cost;
            }
            void SetIncludeFlags(FilterFlags value) override
            {
                include = value;
            }
            void SetExcludeFlags(FilterFlags value) override
            {
                exclude = value;
            }
            float GetAreaCost(AreaId id) const override
            {
                return costs[id];
            }
            FilterFlags GetIncludeFlags() const override
            {
                return include;
            }
            FilterFlags GetExcludeFlags() const override
            {
                return exclude;
            }

        private:
            std::array<float, static_cast<size_t>(std::numeric_limits<AreaId>::max()) + 1> costs;
            FilterFlags                                                                    include, exclude;
        };
        class PathQuery final : public NaviPathQueryTask {
        public:
            PathQuery(CounterPtr<NaviMesh> mesh, Vector3 start, Vector3 end, NaviQueryFilterPtr filter, std::shared_ptr<const void> lease)
                : lease(std::move(lease)), mesh(std::move(mesh)), start(start), end(end), filter(std::move(filter))
            {
            }
            bool IsFinished() const override
            {
                const auto value = state.load();
                return value == State::SUCCEEDED || value == State::FAILED || value == State::CANCELLED;
            }
            bool IsCanceled() const override
            {
                const auto value = state.load();
                return value == State::CANCEL_REQUESTED || value == State::CANCELLED;
            }
            void Cancel() override
            {
                auto expected = State::RUNNING;
                state.compare_exchange_strong(expected, State::CANCEL_REQUESTED);
            }
            const NaviPath &GetPath() const override
            {
                return IsFinished() ? path : pendingPath;
            }
            NaviQueryResult GetResult() const override
            {
                return state.load() == State::SUCCEEDED ? NaviQueryResult::SUCCESS : NaviQueryResult::FAILED;
            }

        private:
            bool DoWork() override
            {
                if (IsFinished())
                    return GetResult() == NaviQueryResult::SUCCESS;
                if (!IsCanceled() && mesh && filter) {
                    mesh->QueryPath(start, end, filter, {}, path);
                }
                auto       expected = State::RUNNING;
                const auto result   = path.result == NaviQueryResult::SUCCESS ? State::SUCCEEDED : State::FAILED;
                if (!state.compare_exchange_strong(expected, result)) {
                    path.Reset();
                    state.store(State::CANCELLED);
                }
                return GetResult() == NaviQueryResult::SUCCESS;
            }
            enum class State : uint8_t { RUNNING, CANCEL_REQUESTED, SUCCEEDED, FAILED, CANCELLED };
            std::shared_ptr<const void> lease;
            CounterPtr<NaviMesh>        mesh;
            Vector3                     start, end;
            NaviQueryFilterPtr          filter;
            NaviPath                    path;
            const NaviPath              pendingPath;
            std::atomic<State>          state{State::RUNNING};
        };
    } // namespace
    NavigationSystem::~NavigationSystem()
    {
        CancelQueries();
    }
    bool NavigationSystem::Init(const NavigationSubSystemConfig &config, std::string &error, std::shared_ptr<const void> lease)
    {
        if (!config.MakeSettings(agent, resolution, maxTiles, error))
            return false;
        codeLease = std::move(lease);
        if (!config.navMeshAsset)
            return true;
        auto asset = AssetManager::Get()->LoadAsset<NaviMesh>(config.navMeshAsset, AssetLoadPolicy::ProductsOnly);
        if (!asset) {
            error = "Navigation mesh asset could not be resolved";
            return false;
        }
        asset->BlockUntilLoaded();
        if (!asset->IsLoaded()) {
            error = "Navigation mesh asset failed to load";
            return false;
        }
        naviMesh = NaviMeshFactory::Get()->CreateNaviMesh();
        if (!naviMesh) {
            error = "Navigation backend is unavailable";
            return false;
        }
        naviMesh->SetMaxTiles(maxTiles);
        if (config.streaming) {
            if (!SetupStreaming(asset->Data())) {
                error = "Navigation streaming asset is invalid";
                return false;
            }
            streamingEnabled = true;
        } else if (!naviMesh->LoadData(asset->Data())) {
            error = "Navigation mesh asset could not be initialized";
            return false;
        }
        return true;
    }
    void NavigationSystem::OnAttachToWorld(World &world)
    {
        if (!naviMesh) {
            naviMesh = NaviMeshFactory::Get()->CreateNaviMesh();
            if (naviMesh) {
                naviMesh->SetAgentConfig(agent);
                naviMesh->SetResolution(resolution);
                naviMesh->SetMaxTiles(maxTiles);
            }
        }
        if (naviMesh) {
            naviMesh->navSystem = this;
            naviMesh->OnAttachToWorld(world);
        }
    }
    void NavigationSystem::OnDetachFromWorld(World &world)
    {
        CancelQueries();
        if (naviMesh) {
            naviMesh->OnDetachFromWorld(world);
            naviMesh->navSystem = nullptr;
            naviMesh            = nullptr;
        }
        availableTiles.clear();
        loadedTiles.clear();
        tileRetries.clear();
        hasParams = false;
    }
    bool NavigationSystem::SetStreamingRadii(float load, float unload)
    {
        if (!std::isfinite(load) || !std::isfinite(unload) || load < 0 || unload < load)
            return false;
        loadRadius   = load;
        unloadRadius = unload;
        return true;
    }
    bool NavigationSystem::SetupStreaming(const NaviMeshData &data)
    {
        if (!naviMesh || data.mode != NaviMeshExportMode::Tiled || !std::isfinite(data.params.resolution.tileSize) ||
            data.params.resolution.tileSize <= 0)
            return false;
        std::unordered_map<uint64_t, std::vector<NaviMeshTilePayload>> tiles;
        for (const auto &tile : data.tiles) {
            auto &layers = tiles[TileKey(tile.tx, tile.ty)];
            if (tile.data.empty() || std::ranges::any_of(layers, [&](const auto &value) { return value.layer == tile.layer; }))
                return false;
            layers.push_back(tile);
        }
        CancelQueries();
        if (!naviMesh->PrepareStreaming(data.params)) {
            hasParams = false;
            availableTiles.clear();
            loadedTiles.clear();
            tileRetries.clear();
            return false;
        }
        buildParams    = data.params;
        availableTiles = std::move(tiles);
        loadedTiles.clear();
        tileRetries.clear();
        hasParams = true;
        return true;
    }
    double NavigationSystem::TileDistanceSquared(uint64_t key) const
    {
        const auto   x    = static_cast<int32_t>(key >> 32);
        const auto   y    = static_cast<int32_t>(static_cast<uint32_t>(key));
        const double size = buildParams.resolution.tileSize;
        const double dx   = buildParams.bounds.min.x + (static_cast<double>(x) + .5) * size - focus.x;
        const double dz   = buildParams.bounds.min.z + (static_cast<double>(y) + .5) * size - focus.z;
        return dx * dx + dz * dz;
    }
    uint32_t NavigationSystem::GetPendingLoadCount() const
    {
        if (!streamingEnabled || !hasParams || !naviMesh || !std::isfinite(focus.x) || !std::isfinite(focus.z))
            return 0;
        const double radiusSquared = static_cast<double>(loadRadius) * loadRadius;
        return static_cast<uint32_t>(std::ranges::count_if(availableTiles, [&](const auto &entry) {
            return !loadedTiles.contains(entry.first) && TileDistanceSquared(entry.first) <= radiusSquared;
        }));
    }
    void NavigationSystem::RetryTile(uint64_t key, TileRetry &retry, bool needsRemoval)
    {
        retry.failures       = std::min(retry.failures + 1, 7u);
        retry.ticksRemaining = 1u << (retry.failures - 1);
        retry.needsRemoval   = needsRemoval;
        if (retry.failures == 1)
            LOG_E("Navigation", "Tile (%d,%d) streaming failed; removal pending: %u", static_cast<int32_t>(key >> 32),
                  static_cast<int32_t>(static_cast<uint32_t>(key)), static_cast<unsigned>(needsRemoval));
    }
    void NavigationSystem::UpdateStreaming()
    {
        if (!streamingEnabled || !hasParams || !naviMesh || !std::isfinite(focus.x) || !std::isfinite(focus.z))
            return;
        for (auto &[key, retry] : tileRetries)
            if (retry.ticksRemaining)
                --retry.ticksRemaining;
        const double loadSquared   = static_cast<double>(loadRadius) * loadRadius;
        const double unloadSquared = static_cast<double>(unloadRadius) * unloadRadius;
        for (auto it = loadedTiles.begin(); it != loadedTiles.end();) {
            const uint64_t key          = *it;
            const auto     retry        = tileRetries.find(key);
            const bool     needsRemoval = retry != tileRetries.end() && retry->second.needsRemoval;
            if ((TileDistanceSquared(key) > unloadSquared || needsRemoval) && (retry == tileRetries.end() || retry->second.ticksRemaining == 0)) {
                CancelQueries();
                if (naviMesh->RemoveTile({static_cast<int32_t>(key >> 32), static_cast<int32_t>(key)})) {
                    it = loadedTiles.erase(it);
                    tileRetries.erase(key);
                    continue;
                }
                RetryTile(key, tileRetries[key], true);
            }
            ++it;
        }
        for (auto &[key, retry] : tileRetries) {
            if (retry.needsRemoval && !retry.ticksRemaining && !loadedTiles.contains(key)) {
                CancelQueries();
                if (naviMesh->RemoveTile({static_cast<int32_t>(key >> 32), static_cast<int32_t>(static_cast<uint32_t>(key))}))
                    retry.needsRemoval = false;
                else
                    RetryTile(key, retry, true);
            }
        }
        std::vector<uint64_t> pending;
        for (const auto &[key, layers] : availableTiles) {
            const auto retry = tileRetries.find(key);
            if (!loadedTiles.contains(key) && TileDistanceSquared(key) <= loadSquared &&
                (retry == tileRetries.end() || (!retry->second.ticksRemaining && !retry->second.needsRemoval)))
                pending.push_back(key);
        }
        std::ranges::sort(pending, [&](auto a, auto b) {
            if (tileRetries.contains(a) != tileRetries.contains(b))
                return !tileRetries.contains(a);
            const double da = TileDistanceSquared(a), db = TileDistanceSquared(b);
            return da != db ? da < db : a < b;
        });
        uint32_t count = 0;
        for (const uint64_t key : pending) {
            if (count++ >= loadBudgetPerTick)
                break;
            CancelQueries();
            bool success = true;
            for (const auto &layer : availableTiles.at(key)) {
                if (!naviMesh->AddTile(layer)) {
                    success = false;
                    break;
                }
            }
            if (success) {
                loadedTiles.insert(key);
                tileRetries.erase(key);
            } else {
                const bool removed = naviMesh->RemoveTile({static_cast<int32_t>(key >> 32), static_cast<int32_t>(static_cast<uint32_t>(key))});
                RetryTile(key, tileRetries[key], !removed);
            }
        }
    }
    void NavigationSystem::AddGeometryProvider(INaviGeometryProvider *provider)
    {
        if (provider && std::find(geometryProviders.begin(), geometryProviders.end(), provider) == geometryProviders.end())
            geometryProviders.push_back(provider);
    }
    void NavigationSystem::RemoveGeometryProvider(INaviGeometryProvider *provider)
    {
        std::erase(geometryProviders, provider);
    }
    NaviPathQueryTaskPtr NavigationSystem::RequestPath(const Vector3 &start, const Vector3 &end, const NaviQueryFilterPtr &filter)
    {
        std::erase_if(activeQueries, [](const auto &task) { return task->IsFinished(); });
        if (!naviMesh || !filter || activeQueries.size() >= queryBudget)
            return nullptr;
        NaviQueryFilterPtr   snapshot = new SnapshotFilter(*filter);
        NaviPathQueryTaskPtr task     = new PathQuery(naviMesh, start, end, std::move(snapshot), codeLease);
        activeQueries.push_back(task);
        task->StartAsync();
        return task;
    }
    void NavigationSystem::CancelPath(const NaviPathQueryTaskPtr &task)
    {
        if (task)
            task->Cancel();
    }
    void NavigationSystem::CancelQueries()
    {
        for (const auto &task : activeQueries)
            task->Cancel();
        std::erase_if(activeQueries, [](const auto &task) { return task->IsFinished(); });
    }
    void NavigationSystem::Tick(float)
    {
        std::erase_if(activeQueries, [](const auto &task) { return task->IsFinished(); });
        UpdateStreaming();
    }
    void NavigationSystem::OnNavMeshChanged()
    {
        streamingEnabled = false;
        hasParams        = false;
        availableTiles.clear();
        loadedTiles.clear();
        tileRetries.clear();
        CancelQueries();
    }
} // namespace sky::ai
