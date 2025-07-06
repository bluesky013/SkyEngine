//
// Created by Zach Lee on 2025/6/15.
//

#pragma once

#include <animation/core/AnimationTypes.h>

namespace sky {

    struct IAnimTransCond {
        IAnimTransCond() = default;
        virtual ~IAnimTransCond() = default;
        virtual bool Eval() const = 0;
    };

    template <typename T>
    struct AnimParameterCond : public IAnimTransCond {
        Name key;
        AnimComp comp;
        T refVal;

        bool Eval() const override
        {
            return true;
        }
    };

} // namespace sky
