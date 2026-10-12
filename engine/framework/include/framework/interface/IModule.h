//
// Created by Zach Lee on 2022/3/12.
//

#pragma once

#include <core/environment/Environment.h>
#include <core/platform/Platform.h>
#include <list>
#include <vector>
#include <string>

namespace sky {

    class NativeWindow;

    struct StartArguments {
        std::list<std::string> values;
        std::vector<const char*> args;
    };

    class IModule {
    public:
        IModule()          = default;
        virtual ~IModule() = default;

        virtual bool Init(const StartArguments &args) { return true; }
        virtual void Shutdown() {}
        virtual void Start() {}

        virtual void PreTick(float delta) {};
        virtual void Tick(float delta) {};
        virtual void PostTick(float delta) {};
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

// Module registration for static bundling. When the module is compiled into the
// host (`SKY_MODULE_STATIC`), it exposes a uniquely-named factory (tag = the
// module's target/module id) instead of the `StartModule` dlopen shim, so several
// modules can be linked together without symbol clashes. A generated registration
// cpp (from the plugin config) calls `sky_module_create_<tag>()` for each.
#if defined(SKY_MODULE_STATIC)
#define REGISTER_MODULE_TAGGED(tag, name)                                                                                                            \
    extern "C" SKY_EXPORT sky::IModule *sky_module_create_##tag()                                                                                    \
    {                                                                                                                                                \
        return new name();                                                                                                                           \
    }                                                                                                                                                \
    extern "C" SKY_EXPORT void sky_module_destroy_##tag(sky::IModule *module)                                                                        \
    {                                                                                                                                                \
        delete module;                                                                                                                               \
    }
#else
#define REGISTER_MODULE_TAGGED(tag, name) REGISTER_MODULE(name)
#endif
