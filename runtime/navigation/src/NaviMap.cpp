//
// Created by blues on 2024/9/1.
//

#include <navigation/NaviMesh.h>

namespace sky::ai {

    NaviMesh* NaviMeshFactory::CreateNaviMap()
    {
        return factory ? factory->CreateNaviMesh() : nullptr;
    }

} // namespace sky::ai