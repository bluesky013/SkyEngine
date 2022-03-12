//
// Created by Zach Lee on 2021/11/12.
//


#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <core/platform/Platform.h>
#include <framework/environment/Environment.h>

namespace sky {

    class IEngine;
    class IWindowEvent;

    struct StartInfo {
        std::string appName;
        std::vector<std::string> modules;
    };

    class IModule {
    public:
        IModule() = default;
        virtual ~IModule() = default;

        virtual void Start() = 0;

        virtual void Stop() = 0;

        virtual void Tick(float delta) = 0;
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

#define REGISTER_MODULE(name) \
    extern "C" SKY_EXPORT sky::IModule* StartModule(sky::Environment* env) \
    { \
        sky::Environment::Attach(env);      \
        return new name();                  \
    } \
    extern "C" SKY_EXPORT void StopModule() \
    {                                       \
        sky::Environment::Detach();         \
    }