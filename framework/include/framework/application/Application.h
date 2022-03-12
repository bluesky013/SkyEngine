//
// Created by Zach Lee on 2021/11/10.
//


#pragma once

#include <framework/interface/IEngine.h>
#include <framework/interface/IModule.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <core/util/DynamicModule.h>
#include <memory>

namespace sky {

    class Environment;

    class ApplicationImpl {
    public:
        ApplicationImpl() = default;
        virtual ~ApplicationImpl() = default;

        static ApplicationImpl* Create();

        virtual void PumpMessages() = 0;

        virtual bool IsExit() const = 0;

        virtual void SetExit() = 0;
    };

    class Application : public ISystemNotify {
    public:
        Application();
        ~Application();

        bool Init(StartInfo&);

        void Mainloop();

        void Shutdown();

        void SetExit() override;

        const SettingRegistry& GetSettings() const override;

        IEngine* GetEngine() const;

        ApplicationImpl* GetImpl() const;

    private:
        void LoadDynamicModules(const StartInfo& start);

        void UnloadDynamicModules();

        struct Module {
            std::unique_ptr<DynamicModule> dynLib;
            std::unique_ptr<IModule> interface;
        };

        ApplicationImpl* impl;
        IEngine* engineInstance;
        Environment* env;
        std::vector<Module> modules;
        SettingRegistry settings;
    };

}
