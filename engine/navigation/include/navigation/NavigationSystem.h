//
// Created by blues on 2024/9/1.
//

#pragma once

#include <framework/world/World.h>
#include <navigation/NaviMesh.h>
#include <navigation/NaviPath.h>
#include <memory>

namespace sky::ai {

    class NavigationSystem : public IWorldSubSystem {
    public:
        NavigationSystem();
        ~NavigationSystem() override = default;

        static constexpr std::string_view NAME = "Navigation";

        const CounterPtr<NaviMesh> &GetNaviMesh() const { return naviMesh; }

        void OnNavMeshChanged();
    private:
        void OnAttachToWorld(World &world) override;
        void OnDetachFromWorld(World &world) override;

        CounterPtr<NaviMesh> naviMesh;
    };

} // namespace sky::ai
