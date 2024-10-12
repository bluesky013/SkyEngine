//
// Created by blues on 2024/10/11.
//

#include <navigation/NaviMeshFactory.h>

namespace sky::ai {

    CounterPtr<NaviMesh> NaviMeshFactory::CreateNaviMap()
    {
        return factory ? factory->CreateNaviMesh() : nullptr;
    }

    CounterPtr<NaviMeshGenerator> NaviMeshFactory::CreateGenerator()
    {
        return factory ? factory->CreateGenerator() : nullptr;
    }

} // namespace sky::ai