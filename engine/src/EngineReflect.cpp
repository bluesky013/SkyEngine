//
// Created by Zach Lee on 2021/12/15.
//

#include <engine/SkyEngine.h>
#include <framework/serialization/CoreReflection.h>

namespace sky {

    void SkyEngine::Reflect()
    {
        CoreReflection();
        World::Reflect();
    }

} // namespace sky
