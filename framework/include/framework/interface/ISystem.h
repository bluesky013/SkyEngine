//
// Created by Zach Lee on 2022/3/12.
//


#pragma once

namespace sky {

    class ISystemNotify {
    public:
        ISystemNotify() = default;
        virtual ~ISystemNotify() = default;

        virtual void SetExit() = 0;
    };

}