//
// Created by blues on 2024/9/1.
//

#pragma once

#include <framework/world/World.h>
#include <navigation/NaviMesh.h>
#include <memory>

namespace sky::ai {

    class NavigationSystem : public IWorldSubSystem {
    public:
        NavigationSystem();
        ~NavigationSystem() override = default;

        static constexpr std::string_view NAME = "Navigation";

        const CounterPtr<NaviMesh> &GetNaviMesh() const { return naviMesh; }

    private:
        CounterPtr<NaviMesh> naviMesh;
    };

} // namespace sky::ai
