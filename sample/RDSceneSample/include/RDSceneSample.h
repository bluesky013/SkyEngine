//
// Created by Zach Lee on 2022/7/27.
//
#pragma once

#include <framework/interface/IModule.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>

#include <render/RenderScene.h>
#include <render/RenderViewport.h>
#include <render/RenderCamera.h>
#include <render/StaticMesh.h>

namespace sky {
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
        RDViewportPtr viewport;
        RDMaterialPtr material;
        RenderCamera* mainCamera = nullptr;
        StaticMesh* staticMesh = nullptr;
    };

}
