//
// Created by Zach Lee on 2022/6/16.
//


#pragma once

#include <framework/interface/IModule.h>
#include <framework/window/NativeWindow.h>
#include <memory>

namespace sky {
    class NativeWindow;

    class Triangle : public IModule {
    public:
        Triangle() = default;
        ~Triangle() = default;

        void Init() override;

        void Start() override;

        void Stop() override;

        void Tick(float delta) override;
    };

}