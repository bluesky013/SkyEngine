//
// Created by Zach Lee on 2022/7/27.
//
#pragma once

#include <framework/interface/IModule.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>

#include <render/RenderScene.h>
#include <render/RenderView.h>
#include <render/RenderViewport.h>

namespace sky {
    class StaticMeshFeature;

    class RDSceneSample : public IModule {
    public:
        RDSceneSample() = default;
        ~RDSceneSample() = default;

        void Init() override;

        void Start() override;

        void Stop() override;

        void Tick(float delta) override;

    private:
        RDScenePtr scene;
        RDViewPtr mainCamera;
        RDViewportPtr viewport;

        StaticMeshFeature* smFeature = nullptr;
    };

}
