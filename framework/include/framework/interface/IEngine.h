//
// Created by Zach Lee on 2022/3/12.
//


#pragma once

#include <string>
#include <vector>

namespace sky {

    class IWindowEvent;

    struct StartInfo {
        std::string appName;
        std::vector<std::string> modules;
    };

    class IEngine {
    public:
        IEngine() = default;
        virtual ~IEngine() = default;

        virtual bool Init(const StartInfo&) = 0;

        virtual void Tick(float) = 0;

        virtual void DeInit() = 0;

        virtual IWindowEvent* GetEventHandler() = 0;
    };

}