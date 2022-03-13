//
// Created by Zach Lee on 2022/3/12.
//


#pragma once

#include <core/platform/Platform.h>
#include <core/environment/Environment.h>

namespace sky {

    class IModule {
    public:
        IModule() = default;
        virtual ~IModule() = default;

        virtual void Init() = 0;

        virtual void Start() = 0;

        virtual void Stop() = 0;

        virtual void Tick(float delta) = 0;
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