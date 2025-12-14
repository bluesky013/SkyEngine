//
// Created by Zach Lee on 2025/6/12.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <animation/core/AnimationConfigs.h>

namespace sky {

    class Animation;

    struct AnimContext {
        Animation* animation;
    };

    class AnimNode : public RefObject {
    public:
        AnimNode() = default;
        ~AnimNode() override = default;

        virtual void Tick(AnimContext& context, float deltaTime) {}
    };

    class AnimRootNode : public AnimNode {
    public:
        AnimRootNode() = default;
        ~AnimRootNode() override = default;
    };

} // namespace sky
