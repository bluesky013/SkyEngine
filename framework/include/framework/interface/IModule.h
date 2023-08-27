//
// Created by Zach Lee on 2022/3/12.
//

#pragma once

#include <core/environment/Environment.h>
#include <core/platform/Platform.h>

namespace sky {

    class NativeWindow;

    class IModule {
    public:
        IModule()          = default;
        virtual ~IModule() = default;

        virtual bool Init() { return true; }
        virtual void Start() {}
        virtual void Stop() {}
        virtual void Shutdown() {}

        virtual void Tick(float delta) = 0;
    };

} // namespace sky

#define REGISTER_MODULE(name)                                                                                                                        \
    extern "C" SKY_EXPORT sky::IModule *StartModule(sky::Environment *env)                                                                           \
    {                                                                                                                                                \
        sky::Environment::Attach(env);                                                                                                               \
        return new name();                                                                                                                           \
    }                                                                                                                                                \
    extern "C" SKY_EXPORT void StopModule()                                                                                                          \
    {                                                                                                                                                \
        sky::Environment::Detach();                                                                                                                  \
    }
