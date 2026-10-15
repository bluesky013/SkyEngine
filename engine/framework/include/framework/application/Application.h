//
// Created by Zach Lee on 2021/11/10.
//

#pragma once

#include <framework/application/FrameClock.h>
#include <framework/application/ModuleManager.h>
#include <framework/application/SettingRegistry.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <functional>
#include <memory>
#include <vector>

namespace sky {

    class Environment;

    class Application : public ISystemNotify {
    public:
        Application();
        ~Application() override;

        Application(const Application &)            = delete;
        Application &operator=(const Application &) = delete;

        virtual bool Init(int argc, char **argv);

        virtual void PreTick()
        {
        }

        void Mainloop();

        void Loop();

        // When true, Mainloop() is a no-op: the platform host drives frames
        // itself (e.g. iOS via CADisplayLink), calling Loop() each frame.
        void SetExternalFrameLoop(bool value)
        {
            externalFrameLoop = value;
        }
        bool UsesExternalFrameLoop() const
        {
            return externalFrameLoop;
        }

        bool IsExit() const
        {
            return exit;
        }
        void SetExit() override;

        template <typename T>
        void BindTick(T &&val)
        {
            tickFn = std::forward<T>(val);
        }

    protected:
        void SaveArgs(int argc, char **argv);

        ModuleManager *GetModuleManager() const override
        {
            return moduleManager.get();
        }

        Environment                   *env;
        std::unique_ptr<ModuleManager> moduleManager;
        StartArguments                 arguments;
        std::function<void(float)>     tickFn;
        bool                           exit              = false;
        bool                           externalFrameLoop = false;
        FrameClock                     frameClock;

        virtual void ParseStartArgs()
        {
        }
        virtual bool LoadConfigs()
        {
            return true;
        }
        virtual bool PreInit()
        {
            return true;
        }
        virtual void PostInit()
        {
        }
    };

} // namespace sky
