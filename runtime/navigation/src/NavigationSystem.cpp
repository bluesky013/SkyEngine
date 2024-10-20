//
// Created by blues on 2024/9/1.
//

#include <navigation/NavigationSystem.h>
#include <navigation/NaviMeshFactory.h>

namespace sky::ai {

    NavigationSystem::NavigationSystem()
    {
        naviMesh = NaviMeshFactory::Get()->CreateNaviMesh();
    }

} // namespace sky::ai