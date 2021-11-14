//
// Created by Zach Lee on 2021/11/12.
//


#pragma once

#include <cstdint>
#include <string>

namespace sky {

    struct StartInfo {
        std::string appName;
    };

    class IEngineLoop {
    public:
        IEngineLoop() = default;
        virtual ~IEngineLoop() = default;

        virtual bool Init(const StartInfo&) = 0;

        virtual void Tick() = 0;

        virtual void DeInit() = 0;
    };


}