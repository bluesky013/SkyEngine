//
// Created on 2026/10/30.
//

#pragma once

#include <navigation/INavigationSystem.h>
#include <recast/NavigationSubSystemConfig.h>

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace sky::ai {
    class NavigationSystem final : public INavigationSystem {
    public:
        NavigationSystem() = default;
        ~NavigationSystem() override;
        bool                        Init(const NavigationSubSystemConfig &config, std::string &error, std::shared_ptr<const void> lease = {});
        const CounterPtr<NaviMesh> &GetNaviMesh() const override
        {
            return naviMesh;
        }
        void OnNavMeshChanged() override;
        bool SetupStreaming(const NaviMeshData &data) override;
        void SetStreamingEnabled(bool enable) override
        {
            streamingEnabled = enable;
        }
        void SetStreamingFocus(const Vector3 &position) override
        {
            focus = position;
        }
        bool SetStreamingRadii(float load, float unload) override;
        void SetLoadBudget(uint32_t count) override
        {
            loadBudgetPerTick = count;
        }
        uint32_t GetLoadedTileCount() const override
        {
            return static_cast<uint32_t>(loadedTiles.size());
        }
        uint32_t                                    GetPendingLoadCount() const override;
        void                                        AddGeometryProvider(INaviGeometryProvider *provider) override;
        void                                        RemoveGeometryProvider(INaviGeometryProvider *provider) override;
        const std::vector<INaviGeometryProvider *> &GetGeometryProviders() const override
        {
            return geometryProviders;
        }
        NaviPathQueryTaskPtr RequestPath(const Vector3 &start, const Vector3 &end, const NaviQueryFilterPtr &filter) override;
        void                 CancelPath(const NaviPathQueryTaskPtr &task) override;
        void                 SetQueryBudget(uint32_t count) override
        {
            queryBudget = count;
        }
        uint32_t GetActiveQueryCount() const override
        {
            return static_cast<uint32_t>(std::ranges::count_if(activeQueries, [](const auto &task) { return !task->IsFinished(); }));
        }

    private:
        void   OnAttachToWorld(World &world) override;
        void   OnDetachFromWorld(World &world) override;
        void   Tick(float time) override;
        void   CancelQueries();
        void   UpdateStreaming();
        double TileDistanceSquared(uint64_t key) const;
        struct TileRetry {
            uint32_t failures       = 0;
            uint32_t ticksRemaining = 0;
            bool     needsRemoval   = false;
        };
        void RetryTile(uint64_t key, TileRetry &retry, bool needsRemoval);

        std::shared_ptr<const void>                                    codeLease;
        CounterPtr<NaviMesh>                                           naviMesh;
        NaviAgentConfig                                                agent;
        NaviMeshResolution                                             resolution;
        uint32_t                                                       maxTiles = 256;
        NaviMeshBuildParams                                            buildParams;
        bool                                                           hasParams        = false;
        bool                                                           streamingEnabled = false;
        std::unordered_map<uint64_t, std::vector<NaviMeshTilePayload>> availableTiles;
        std::unordered_set<uint64_t>                                   loadedTiles;
        std::unordered_map<uint64_t, TileRetry>                        tileRetries;
        Vector3                                                        focus;
        float                                                          loadRadius        = 64.f;
        float                                                          unloadRadius      = 96.f;
        uint32_t                                                       loadBudgetPerTick = 4;
        std::vector<NaviPathQueryTaskPtr>                              activeQueries;
        uint32_t                                                       queryBudget = 4;
        std::vector<INaviGeometryProvider *>                           geometryProviders;
    };
} // namespace sky::ai
