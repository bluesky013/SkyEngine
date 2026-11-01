//
// Created on 2026/10/30.
//

#pragma once

#include <core/async/Task.h>
#include <framework/world/World.h>
#include <navigation/NaviGeometryProvider.h>
#include <navigation/NaviMeshTypes.h>
#include <navigation/NaviPath.h>

namespace sky::ai {
    class NaviPathQueryTask : public Task {
    public:
        ~NaviPathQueryTask() override   = default;
        virtual bool IsFinished() const = 0;
        virtual bool IsCanceled() const = 0;
        virtual void Cancel()           = 0;
        // Read the completed result only after IsFinished() returns true.
        virtual const NaviPath &GetPath() const   = 0;
        virtual NaviQueryResult GetResult() const = 0;
    };
    using NaviPathQueryTaskPtr = CounterPtr<NaviPathQueryTask>;

    class INavigationSystem : public IWorldSubSystem {
    public:
        // Service/configuration calls belong to the world thread; poll/cancel returned tasks from any thread.
        static constexpr std::string_view   NAME                                        = "Navigation";
        virtual const CounterPtr<NaviMesh> &GetNaviMesh() const                         = 0;
        virtual void                        OnNavMeshChanged()                          = 0;
        virtual bool                        SetupStreaming(const NaviMeshData &data)    = 0;
        virtual void                        SetStreamingEnabled(bool enable)            = 0;
        virtual void                        SetStreamingFocus(const Vector3 &position)  = 0;
        virtual bool                        SetStreamingRadii(float load, float unload) = 0;
        virtual void                        SetLoadBudget(uint32_t tilesPerTick)        = 0;
        virtual uint32_t                    GetLoadedTileCount() const                  = 0;
        // Desired coordinates awaiting installation, including retry backoff; this is not a disk-IO count.
        virtual uint32_t                                    GetPendingLoadCount() const                                      = 0;
        virtual void                                        AddGeometryProvider(INaviGeometryProvider *provider)             = 0;
        virtual void                                        RemoveGeometryProvider(INaviGeometryProvider *provider)          = 0;
        virtual const std::vector<INaviGeometryProvider *> &GetGeometryProviders() const                                     = 0;
        virtual NaviPathQueryTaskPtr RequestPath(const Vector3 &start, const Vector3 &end, const NaviQueryFilterPtr &filter) = 0;
        virtual void                 CancelPath(const NaviPathQueryTaskPtr &task)                                            = 0;
        virtual void                 SetQueryBudget(uint32_t maxConcurrent)                                                  = 0;
        virtual uint32_t             GetActiveQueryCount() const                                                             = 0;
    };
} // namespace sky::ai
