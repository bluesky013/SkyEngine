//
// Created by Zach Lee on 2022/6/16.
//


#pragma once

#include <framework/interface/IModule.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/window/NativeWindow.h>
#include <render/Render.h>
#include <render/RenderScene.h>
#include <render/RenderViewport.h>

namespace sky::render {
    class NativeWindow;

    class Triangle : public IModule {
    public:
        Triangle() = default;
        ~Triangle() = default;

        void Init() override;

        void Start() override;

        void Stop() override;

        void Tick(float delta) override;

    private:
        RDScenePtr scene;
        RDViewPtr mainCamera;
        RDViewportPtr viewport;
    };

}