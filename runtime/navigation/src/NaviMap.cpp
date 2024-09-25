//
// Created by blues on 2024/9/1.
//

#include <navigation/NaviMap.h>

namespace sky::ai {

    NaviMap* NaviMapFactory::CreateNaviMap()
    {
        return factory ? factory->CreateNaviMap() : nullptr;
    }

} // namespace sky::ai