//
// Created by Zach Lee on 2021/11/10.
//

#pragma once

#include <core/util/DynamicModule.h>
#include <framework/interface/IModule.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/application/SettingRegistry.h>
#include <memory>
#include <functional>
#include <vector>

namespace sky {

    class Environment;

    struct StartInfo {
        std::string              appName;
        std::vector<std::string> modules;
        SettingRegistry          setting;

        uint32_t windowWidth  = 1366;
        uint32_t windowHeight = 768;
        void*    mainWindow   = nullptr;
    };

    class Application : public ISystemNotify {
    public:
        Application();
        ~Application();

        Application(const Application &)            = delete;
        Application &operator=(const Application &) = delete;

        virtual bool Init(StartInfo &);

        virtual void Shutdown();

        virtual void PreTick() {};

        void Mainloop();

        void Loop();

        bool IsExit() const { return exit; }
        void SetExit() override;

        SettingRegistry &GetSettings() override;

        template <typename T>
        void BindTick(T &&val)
        {
            tickFn = std::forward<T>(val);
        }

        void RegisterModule(std::unique_ptr<IModule> &&module);

    protected:
        void LoadDynamicModules(const StartInfo &start);

        void UnloadDynamicModules();

        Environment                                *env;
        std::vector<std::unique_ptr<DynamicModule>> dynLibs;
        std::vector<std::unique_ptr<IModule>>       modules;
        SettingRegistry                             settings;
        std::function<void(float)>                  tickFn;
        bool                                        exit = false;
    };

} // namespace sky
