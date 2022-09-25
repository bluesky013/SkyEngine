//
// Created by Zach Lee on 2021/11/10.
//

#pragma once

#include <core/util/DynamicModule.h>
#include <framework/interface/IEngine.h>
#include <framework/interface/IModule.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <memory>

namespace sky {

    class Environment;

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

        void SetExit() override;

        const SettingRegistry &GetSettings() const override;

    protected:
        void LoadDynamicModules(const StartInfo &start);

        void UnloadDynamicModules();

        Environment                                *env;
        std::vector<std::unique_ptr<DynamicModule>> dynLibs;
        std::vector<std::unique_ptr<IModule>>       modules;
        SettingRegistry                             settings;
        bool                                        exit = false;
    };

} // namespace sky
