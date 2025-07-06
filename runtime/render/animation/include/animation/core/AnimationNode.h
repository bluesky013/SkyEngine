//
// Created by Zach Lee on 2025/6/12.
//

#pragma once

#include <core/template/ReferenceObject.h>

namespace sky {

    class Animation;

    struct AnimContext {
        Animation* animation;
    };

    class AnimNode : public RefObject {
    public:
        AnimNode() = default;
        ~AnimNode() override = default;

        virtual void Update(AnimContext& context, float deltaTime) {}
    };

} // namespace sky
