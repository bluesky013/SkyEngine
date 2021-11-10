//
// Created by Zach Lee on 2021/11/10.
//


#pragma once

#include "application/window/NativeWindow.h"

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

        bool Init();

        void Mainloop();

    private:
        Impl* impl;
        NativeWindow* window;
    };

}
