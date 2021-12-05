//
// Created by Zach Lee on 2021/11/26.
//


#pragma once

#include <engine/SkyEngine.h>
#include <engine/world/Viewport.h>
#include <engine/world/World.h>
#include <framework/Application.h>

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