//
// Created by Zach Lee on 2021/12/23.
//

#pragma once

namespace sky {

    class ISystem {
    public:
        ISystem() = default;
        virtual ~ISystem() = default;

        virtual void OnTick(float time) {}
    };

}