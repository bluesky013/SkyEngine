//
// Created by blues on 2024/9/1.
//

#include <navigation/NavigationSystem.h>
#include <navigation/NaviMeshFactory.h>

namespace sky::ai {

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
    }

    void NavigationSystem::OnNavMeshChanged()
    {

    }

} // namespace sky::ai