//
// Created by blues on 2024/9/1.
//

#pragma once

#include <framework/world/World.h>
#include <navigation/NaviMap.h>
#include <memory>

namespace sky::ai {

    class NavigationSystem : public IWorldSubSystem {
    public:
        NavigationSystem() = default;
        ~NavigationSystem() override = default;

    private:
        std::unique_ptr<NaviMap> naviMap;
    };

} // namespace sky::ai
