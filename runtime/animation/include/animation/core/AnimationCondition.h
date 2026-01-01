//
// Created by Zach Lee on 2025/6/15.
//

#pragma once

#include <animation/core/AnimationTypes.h>

namespace sky {

    template <typename T>
    struct TAnimParameterCond : public IAnimTransCond {
        TAnimParameter<T> *val = nullptr;
        T refVal;
        AnimComp comp;

        TAnimParameterCond(TAnimParameter<T> *inVal, const T& inRef, AnimComp inComp)
            : val(inVal)
            , refVal(inRef)
            , comp(inComp)
        {
        }

        void Update(float deltaTime) override
        {
            if (val != nullptr) {
                val->Update(deltaTime);
            }
        }

        bool Eval() const override
        {
            return val != nullptr && AnimCompEval<T>::Compare(comp, val->template EvalAs<T>(), refVal);
        }
    };

} // namespace sky
