//
// Created by Zach Lee on 2021/11/10.
//


#pragma once

#include <framework/interface/IEngine.h>
#include <framework/interface/IModule.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/window/NativeWindow.h>
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

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        bool Init(StartInfo&);

        void Mainloop();

        void Shutdown();

        void SetExit() override;

        const SettingRegistry& GetSettings() const override;

        const NativeWindow* GetViewport() const override;

        ApplicationImpl* GetImpl() const;

    private:
        void LoadDynamicModules(const StartInfo& start);

        void UnloadDynamicModules();

        ApplicationImpl* impl;
        Environment* env;
        std::vector<std::unique_ptr<DynamicModule>> dynLibs;
        std::vector<std::unique_ptr<IModule>> modules;
        SettingRegistry settings;

        std::unique_ptr<NativeWindow> nativeWindow;
    };

}
