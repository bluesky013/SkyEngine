//
// Created by Zach Lee on 2021/11/10.
//


#pragma once

#include <application/window/NativeWindow.h>
#include <application/interface/EngineInterface.h>
#include <core/util/DynamicModule.h>
#include <memory>

namespace sky {

    class Application {
    public:
        Application();
        ~Application();

        class Impl {
        public:
            Impl() = default;
            virtual ~Impl() = default;

            static Impl* Create();

            virtual void PumpMessages() = 0;

            virtual bool IsExit() const = 0;
        };

        bool Init(const StartInfo&);

        void Mainloop();

        void Shutdown();

        NativeWindow* CreateNativeWindow(const NativeWindow::Descriptor&);

        IEngine* GetEngine() const;

    private:
        Impl* impl;
        IEngine* engineInstance;
        std::unique_ptr<DynamicModule> engine;
        std::vector<std::unique_ptr<DynamicModule>> modules;
    };

}
