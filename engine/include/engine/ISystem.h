//
// Created by Zach Lee on 2021/12/23.
//

#pragma once

#include <engine/world/Component.h>

namespace sky {

    class ISystem : public IComponentListener {
    public:
        ISystem() = default;
        virtual ~ISystem() = default;

        virtual void OnTick(float time) {}
    };

}