//
// Created by Zach Lee on 2021/12/23.
//

#pragma once

#include <engine/world/Component.h>

namespace sky {

    template <typename T>
    struct SHandle {
        using Type = T;
        uint32_t handle;
    };

    class IService {
    public:
        IService() = default;
        virtual ~IService() = default;

        virtual void OnTick(float time) {}
    };

}