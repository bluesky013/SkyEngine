//
// Created by blues on 2024/9/1.
//

#pragma once

#include <render/resource/Technique.h>

namespace sky::phy {

    class PhysicsDebugDraw {
    public:
        PhysicsDebugDraw() = default;
        virtual ~PhysicsDebugDraw() = default;

        virtual void SetTechnique(const RDGfxTechPtr &tech) = 0;
    };

} // namespace sky::phy