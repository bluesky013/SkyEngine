//
// Created by Zach Lee on 2021/11/10.
//

#pragma once

#include <core/util/DynamicModule.h>
#include <framework/interface/IEngine.h>
#include <framework/interface/IModule.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/window/NativeWindow.h>
#include <memory>

namespace sky {

    class Environment;

    class Application : public ISystemNotify {
    public:
        Application();
        ~Application();

        Application(const Application &)            = delete;
        Application &operator=(const Application &) = delete;

        bool Init(StartInfo &);

        void Mainloop();

        void Shutdown();

        void SetExit() override;

        const SettingRegistry &GetSettings() const override;

        const NativeWindow *GetViewport() const override;

    private:
        void LoadDynamicModules(const StartInfo &start);

        void UnloadDynamicModules();

        Environment                                *env;
        std::vector<std::unique_ptr<DynamicModule>> dynLibs;
        std::vector<std::unique_ptr<IModule>>       modules;
        SettingRegistry                             settings;
        std::unique_ptr<NativeWindow>               nativeWindow;
        bool                                        exit = false;
    };

} // namespace sky
