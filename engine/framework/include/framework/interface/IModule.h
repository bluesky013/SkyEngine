//
// Created by Zach Lee on 2022/3/12.
//

#pragma once

#include <core/environment/Environment.h>
#include <core/platform/Platform.h>
#include <list>#include <string>
#include <vector>

namespace sky {

    class NativeWindow;

    struct StartArguments {
        std::list<std::string>    values;
        std::vector<const char *> args;
    };

    class IModule {
    public:
        IModule()          = default;
        virtual ~IModule() = default;

        // Process-once phase: register process-global metadata (reflection types, asset types/
        // handlers, component groups, cook builders). `ModuleManager` runs this at most once per
        // process per module - it is NOT re-run on reload - so registrations that persist for the
        // process lifetime belong here, never in Init/Start/Shutdown.
        virtual void RegisterMeta()
        {
        }

        virtual bool Init(const StartArguments &args)
        {
            return true;
        }

        virtual void Shutdown()
        {
        }
        virtual void Start()
        {
        }

        virtual void PreTick(float delta){};
        virtual void Tick(float delta){};
        virtual void PostTick(float delta){};
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
// host (`SKY_MODULE_STATIC`), it exposes a uniquely-named factory instead of the
// `StartModule` dlopen shim, so several modules link together without symbol
// clashes; a generated registration cpp calls `sky_module_create_<tag>()` for each.
// `SKY_MODULE_TAG` is injected per module by `sky_add_module()` (the target name with
// non-identifier chars replaced by '_'), so the factory name always matches the
// generated registration without the module hand-writing a tag.
#if defined(SKY_MODULE_STATIC)
    #define SKY_MODULE_CAT_(a, b) a##b
    #define SKY_MODULE_CAT(a, b)  SKY_MODULE_CAT_(a, b)
    #define REGISTER_MODULE_TAGGED(name)                                                                                                             \
        extern "C" SKY_EXPORT sky::IModule *SKY_MODULE_CAT(sky_module_create_, SKY_MODULE_TAG)()                                                     \
        {                                                                                                                                            \
            return new name();                                                                                                                       \
        }                                                                                                                                            \
        extern "C" SKY_EXPORT void SKY_MODULE_CAT(sky_module_destroy_, SKY_MODULE_TAG)(sky::IModule * module)                                        \
        {                                                                                                                                            \
            delete module;                                                                                                                           \
        }
#else
    #define REGISTER_MODULE_TAGGED(name) REGISTER_MODULE(name)
#endif
