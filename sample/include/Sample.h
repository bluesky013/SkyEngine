//
// Created by Zach Lee on 2021/11/26.
//


#pragma once

#include <SkyEngine.h>
#include <world/Viewport.h>
#include <world/World.h>
#include <application/Application.h>

namespace sky {

    class Sample : public IModule {
    public:
        Sample(Application& app) : application(app), engine(static_cast<SkyEngine&>(*app.GetEngine())) {}
        ~Sample() = default;

        void Start() override;

        void Stop() override;

        void Tick(float delta) override;

    private:
        Application& application;
        SkyEngine& engine;

        NativeWindow* nativeWindow = nullptr;
        Viewport* viewport = nullptr;
        World* world = nullptr;
    };

}