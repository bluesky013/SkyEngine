//
// Created by blues on 2024/8/2.
//

#pragma once

#include <core/template/ReferenceObject.h>

namespace sky {

    class Animation : public RefObject {
    public:
        Animation() = default;
        ~Animation() override = default;

        virtual void Tick(float delta) = 0;
    };

    class SimpleAnimation : public Animation {
    public:
        SimpleAnimation() = default;
        ~SimpleAnimation() override = default;

    protected:
        void Tick(float delta) override {}
    };
} // namespace sky