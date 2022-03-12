//
// Created by Zach Lee on 2021/11/26.
//


#pragma once

#include <framework/interface/IModule.h>
#include <framework/window/NativeWindow.h>
#include <memory>

namespace sky {
    class NativeWindow;

    class Sample : public IModule {
    public:
        Sample() = default;
        ~Sample() = default;

        void Start() override;

        void Stop() override;

        void Tick(float delta) override;

    private:
        std::unique_ptr<NativeWindow> nativeWindow;
    };

}