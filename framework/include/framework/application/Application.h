//
// Created by Zach Lee on 2021/11/10.
//

#pragma once

#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/application/SettingRegistry.h>
#include <framework/application/ModuleManager.h>
#include <memory>
#include <functional>
#include <vector>

namespace sky {

    class Environment;

    class Application : public ISystemNotify {
    public:
        Application();
        ~Application() override = default;

        Application(const Application &)            = delete;
        Application &operator=(const Application &) = delete;

        virtual bool Init(int argc, char **argv);

        virtual void Shutdown();

        virtual void PreTick() {};

        void Mainloop();

        void Loop();

        bool IsExit() const { return exit; }
        void SetExit() override;

        template <typename T>
        void BindTick(T &&val)
        {
            tickFn = std::forward<T>(val);
        }

    protected:
        void SaveArgs(int argc, char **argv);

        Environment                    *env;
        std::unique_ptr<ModuleManager>  moduleManager;
        StartArguments                  arguments;
        std::function<void(float)>      tickFn;
        bool                            exit = false;

        virtual void ParseStartArgs() {}
        virtual void LoadConfigs() {}
        virtual void PreInit() {}
        virtual void PostInit() {}
    };

} // namespace sky
